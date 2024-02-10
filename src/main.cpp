#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <allegro5/allegro5.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_audio.h>
#include <allegro5/allegro_acodec.h>
#include <allegro5/allegro_image.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_ttf.h>

extern "C"
{
  __declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
}

extern "C"
{
  __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}


#include <time.h>
#include <math.h>

#include <string.h>
#include "cJSON.h"

#define M_PI		3.14159265358979323846d


#define BUFFER_W 500
#define BUFFER_H 250


#define DISP_SCALE 3
#define DISP_W (BUFFER_W * DISP_SCALE)
#define DISP_H (BUFFER_H * DISP_SCALE)

long frames;
long score;
long difficulty;
int cargo_cnt;
int drone_cnt;
int gameOverStopTime;

bool menuMusic = true;
bool gameMusic = true;
bool isPlayingMenu = false;
bool fxPlay = true;

void must_init(bool test, const char *description)
{
    if(test) return;

    printf("couldn't initialize %s\n", description);
    exit(1);
}

int between(int lo, int hi)
{
    return lo + (rand() % (hi - lo));
}

float between_f(float lo, float hi)
{
    return lo + ((float)rand() / (float)RAND_MAX) * (hi - lo);
}

bool collide(int ax1, int ay1, int ax2, int ay2, int bx1, int by1, int bx2, int by2)
{
    if(ax1 > bx2) return false;
    if(ax2 < bx1) return false;
    if(ay1 > by2) return false;
    if(ay2 < by1) return false;

    return true;
}

ALLEGRO_DISPLAY* disp;
ALLEGRO_BITMAP* buffer;

void disp_init()
{
    al_set_new_display_option(ALLEGRO_SAMPLE_BUFFERS, 1, ALLEGRO_SUGGEST);
    al_set_new_display_option(ALLEGRO_SAMPLES, 8, ALLEGRO_SUGGEST);


    disp = al_create_display(DISP_W, DISP_H);
    must_init(disp, "display");
    al_set_window_title(disp, "Cosmic Combat");

    al_set_window_position(disp ,20 ,20);

    buffer = al_create_bitmap(BUFFER_W, BUFFER_H);
    must_init(buffer, "bitmap buffer");
}

void disp_deinit()
{
    al_destroy_bitmap(buffer);
    al_destroy_display(disp);
}

void disp_pre_draw()
{
    al_set_target_bitmap(buffer);
}

void disp_post_draw()
{
    al_set_target_backbuffer(disp);
    al_draw_scaled_bitmap(buffer, 0, 0, BUFFER_W, BUFFER_H, 0, 0, DISP_W, DISP_H, 0);

    al_flip_display();
}



#define KEY_SEEN     1
#define KEY_RELEASED 2
unsigned char key[ALLEGRO_KEY_MAX];

void keyboard_init()
{
    memset(key, 0, sizeof(key));
}


void keyboard_update(ALLEGRO_EVENT* event)
{
    switch(event->type)
    {
        case ALLEGRO_EVENT_TIMER:
            for(int i = 0; i < ALLEGRO_KEY_MAX; i++)
                key[i] &= KEY_SEEN;
            break;

        case ALLEGRO_EVENT_KEY_DOWN:
            key[event->keyboard.keycode] = KEY_SEEN | KEY_RELEASED;
            break;
        case ALLEGRO_EVENT_KEY_UP:
            key[event->keyboard.keycode] &= KEY_RELEASED;
            break;
    }
}



enum Direction {
    UP,
    DOWN,
    LEFT,
    RIGHT
};



typedef struct {
    char *key;
    char *value;
} KeyValuePair;

typedef struct {
    KeyValuePair *entries;
    size_t size;
} HashMap;

typedef struct {
    char *key;
    HashMap values;
} NestedHashMapEntry;

typedef struct {
    NestedHashMapEntry *entries;
    size_t size;
} NestedHashMap;


NestedHashMap myNestedHashMap;


int compareNestedHashMapEntries(const void *a, const void *b) {
    long aValue = atol(((NestedHashMapEntry *)a)->values.entries[0].value);
    long bValue = atol(((NestedHashMapEntry *)b)->values.entries[0].value);

    return (aValue > bValue) - (aValue < bValue);
}

void sortNestedHashMap(NestedHashMap *nestedHashMap) {
    qsort(nestedHashMap->entries, nestedHashMap->size, sizeof(NestedHashMapEntry), compareNestedHashMapEntries);
}

void writeNestedHashMapToJsonFile(const NestedHashMap *nestedHashMap, const char *filename) {
    cJSON *root = cJSON_CreateObject();

    for (size_t i = 0; i < nestedHashMap->size; ++i) {
        cJSON *nestedMap = cJSON_CreateObject();

        for (size_t j = 0; j < nestedHashMap->entries[i].values.size; ++j) {
            cJSON_AddItemToObject(nestedMap, nestedHashMap->entries[i].values.entries[j].key,
                                  cJSON_CreateString(nestedHashMap->entries[i].values.entries[j].value));
        }

        cJSON_AddItemToObject(root, nestedHashMap->entries[i].key, nestedMap);
    }

    // Open the file for writing
    FILE *file = fopen(filename, "w");
    if (!file) {
        fprintf(stderr, "Error opening file for writing: %s\n", filename);
        return;
    }

    // Write JSON data to the file
    char *jsonString = cJSON_Print(root);
    fprintf(file, "%s", jsonString);

    // Cleanup
    fclose(file);
    cJSON_free(jsonString);
    cJSON_Delete(root);
}

int readJsonFileToNestedHashMap(const char *filename, NestedHashMap *nestedHashMap) {
    // Open the file for reading
    FILE *file = fopen(filename, "r");
    if (!file) {
        FILE *file1 = fopen(filename, "w");
        fclose(file1);
        fprintf(stderr, "Error opening file for reading: %s\n", filename);
        return 1;
    }

    // Read the file into a cJSON object
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *jsonBuffer = (char *)malloc(fileSize + 1);
    fread(jsonBuffer, 1, fileSize, file);
    jsonBuffer[fileSize] = '\0';
    if (fileSize == 0){
        printf("File size is zero\n");
        return 2;
    }
    cJSON *root = cJSON_Parse(jsonBuffer);

    // Check for JSON parsing errors
    if (!root) {
        fprintf(stderr, "JSON parsing error: %s\n", cJSON_GetErrorPtr());
        fclose(file);
        free(jsonBuffer);
        return 3;
    }
    if (cJSON_GetArraySize(root) == 0){
        printf("Json Size : %d\n",cJSON_GetArraySize(root));
        return 4;
    }
    // Iterate through the cJSON object and populate the nested hashmap
    cJSON *value = NULL;
    cJSON *child = NULL;
    cJSON_ArrayForEach(value, root) {
        size_t index = nestedHashMap->size;
        nestedHashMap->entries = (NestedHashMapEntry*) realloc(nestedHashMap->entries, (index + 1) * sizeof(NestedHashMapEntry));

        nestedHashMap->entries[index].key = strdup(value->string);

        nestedHashMap->entries[index].values.size = cJSON_GetArraySize(value);
        nestedHashMap->entries[index].values.entries = (KeyValuePair*) malloc(nestedHashMap->entries[index].values.size * sizeof(KeyValuePair));

        size_t innerIndex = 0;
        cJSON_ArrayForEach(child, value) {
            nestedHashMap->entries[index].values.entries[innerIndex].key = strdup(child->string);
            nestedHashMap->entries[index].values.entries[innerIndex].value = strdup(child->valuestring);
            innerIndex++;
        }

        nestedHashMap->size++;
    }

    // Cleanup
    fclose(file);
    free(jsonBuffer);
    cJSON_Delete(root);
    return 0;
}


void freeNestedHashMap(NestedHashMap *nestedHashMap) {
    for (size_t i = 0; i < nestedHashMap->size; ++i) {
        free(nestedHashMap->entries[i].key);

        for (size_t j = 0; j < nestedHashMap->entries[i].values.size; ++j) {
            free(nestedHashMap->entries[i].values.entries[j].key);
            free(nestedHashMap->entries[i].values.entries[j].value);
        }

        free(nestedHashMap->entries[i].values.entries);
    }

    free(nestedHashMap->entries);
}

void printSortedHashMap(const NestedHashMap *nestedHashMap) {
    for (size_t i = 0; i < nestedHashMap->size; ++i) {
        printf("%s:\n", nestedHashMap->entries[i].key);

        for (size_t j = 0; j < nestedHashMap->entries[i].values.size; ++j) {
            printf("  %s: %s\n", nestedHashMap->entries[i].values.entries[j].key,
                   nestedHashMap->entries[i].values.entries[j].value);
        }
    }
}

void printSortedHashMapMe(const NestedHashMap *nestedHashMap, ALLEGRO_FONT *font, float x, float y) {
    int cnt = 0;
    int res = 0;
    int index = myNestedHashMap.size;
    sortNestedHashMap(&myNestedHashMap);
    for (size_t i = 0; i < nestedHashMap->size; ++i) {
        printf("%s:\n", nestedHashMap->entries[i].key);

        for (int j = 0; j < 1; ++j) {
            printf("  %s: %s\n", nestedHashMap->entries[i].values.entries[j].key,
                   nestedHashMap->entries[i].values.entries[j].value);

            al_draw_textf(font, al_map_rgb(255, 255, 255), x - 175, y - res + 475, 0, "%d - %s               %s                 %s", 10 - i, nestedHashMap->entries[i].key, nestedHashMap->entries[i].values.entries[0].value, nestedHashMap->entries[i].values.entries[1].value);
            cnt++;
            res += 40;
            if(cnt == 11)
            return;
        }
    }
}

int loadRecords(){
    myNestedHashMap.size = 0;
    myNestedHashMap.entries = NULL;
    myNestedHashMap.entries = (NestedHashMapEntry*) malloc(50 * sizeof(NestedHashMapEntry));
    return readJsonFileToNestedHashMap("records.json", &myNestedHashMap);
}


void addRecord(char *name, char *score, char *time){
    int index = myNestedHashMap.size;
    myNestedHashMap.entries[index].key = strdup(name);
    myNestedHashMap.entries[index].values.size = 2;
    myNestedHashMap.entries[index].values.entries = (KeyValuePair*) malloc(2 * sizeof(KeyValuePair));
    myNestedHashMap.entries[index].values.entries[0] = (KeyValuePair){"Score", strdup(score)};
    myNestedHashMap.entries[index].values.entries[1] = (KeyValuePair){"Time", strdup(time)};
    myNestedHashMap.size++;
}


void showRecords(){
    sortNestedHashMap(&myNestedHashMap);
    printSortedHashMap(&myNestedHashMap);
}


void saveRecordsToFile(){
    writeNestedHashMapToJsonFile(&myNestedHashMap, "records.json");
}


void showFirstFrag(ALLEGRO_FONT *font, float x, float y){
    sortNestedHashMap(&myNestedHashMap);
    int index = myNestedHashMap.size - 1;
    long highest = atol(myNestedHashMap.entries[index].values.entries[0].value);
    al_draw_textf(font, al_map_rgb(255, 255, 255), x, y, 0, "Highest Score:     %.7ld", highest);
}



int showMenu (float FPS, ALLEGRO_EVENT_QUEUE *event_queueMenu, ALLEGRO_KEYBOARD_STATE keyStateMenu, ALLEGRO_TIMER* timer)
{

    int choice = 1;
    const float keyboard_FPS = 60.0;

    bool doneMenu = false, drawMenu = true;
    int dirMenu = DOWN;
    int frame_number = 1;
    char filename1[30];
    int frame_cnt= 0;

    al_clear_to_color(al_map_rgb(0, 0, 0));

    ALLEGRO_FONT *menuFont20 = al_load_font("menu\\menuFont.ttf", 20, 0);
    ALLEGRO_COLOR white = al_map_rgb(255, 255, 255);


    ALLEGRO_TIMER *keyboard_timer = al_create_timer(1.0 / keyboard_FPS);
    al_register_event_source(event_queueMenu, al_get_timer_event_source(keyboard_timer));

    ALLEGRO_BITMAP *button1 = al_load_bitmap("menu\\menuButton.png");
    ALLEGRO_BITMAP *button2 = al_load_bitmap("menu\\menuButton.png");
    ALLEGRO_BITMAP *button3 = al_load_bitmap("menu\\menuButton.png");
    ALLEGRO_BITMAP *button4 = al_load_bitmap("menu\\menuButton.png");
    ALLEGRO_BITMAP *button1Active = al_load_bitmap("menu\\menuButtonActive.png");
    ALLEGRO_BITMAP *button2Active = al_load_bitmap("menu\\menuButtonActive.png");
    ALLEGRO_BITMAP *button3Active = al_load_bitmap("menu\\menuButtonActive.png");
    ALLEGRO_BITMAP *button4Active = al_load_bitmap("menu\\menuButtonActive.png");

    al_start_timer(keyboard_timer);



    while(!doneMenu) {
        ALLEGRO_EVENT eventsMenu;
        al_wait_for_event(event_queueMenu, &eventsMenu);
        frame_cnt++;

        if(eventsMenu.type == ALLEGRO_EVENT_DISPLAY_CLOSE)
        {
            doneMenu = true;
        }
        else if(eventsMenu.type == ALLEGRO_EVENT_TIMER)
        {
            if(eventsMenu.timer.source == keyboard_timer)
            {
                al_get_keyboard_state(&keyStateMenu);

                if(al_key_down(&keyStateMenu, ALLEGRO_KEY_DOWN))
                {
                    if(choice != 4) choice += 1;
                    else choice = 1;
                    al_rest(0.25);
                }
                else if(al_key_down(&keyStateMenu, ALLEGRO_KEY_UP))
                {
                    if(choice != 1) choice -= 1;
                    else choice = 4;
                    al_rest(0.25);
                }
                else if(al_key_down(&keyStateMenu, ALLEGRO_KEY_ENTER))
                {
                    doneMenu = true;
                    continue;
                }

                drawMenu = true;
            }
            if(eventsMenu.timer.source == timer)
            {
                if((frame_cnt%45))
                {
                    al_clear_to_color(al_map_rgb(0, 0, 0));
                    snprintf(filename1, sizeof(filename1), "menu\\start-menu\\menu (%d).png", frame_number);
                    ALLEGRO_BITMAP *frame = al_load_bitmap(filename1);
                    al_draw_bitmap(frame, 0, 0, 0);
                    //al_flip_display();
                    al_destroy_bitmap(frame);
                    frame_number++;
                    if(frame_number == 45)
                        frame_number = 1;

                switch (choice)
                {
                    case 1:
                        al_draw_bitmap(button1Active, 340, 70, 0);
                        al_draw_bitmap(button2, 340, 170, 0);
                        al_draw_bitmap(button3, 340, 270, 0);
                        al_draw_bitmap(button4, 340, 370, 0);
                        al_draw_text(menuFont20, white, 340 + 42, 40 + 60, 0, "ARCADE");
                        al_draw_text(menuFont20, white, 340 + 30, 140 + 60, 0, "RECORDS");
                        al_draw_text(menuFont20, white, 340 + 35, 240 + 60, 0, "SETTING");
                        al_draw_text(menuFont20, white, 340 + 50, 340 + 60, 0, "QUIT");
                        break;
                    case 2:
                        al_draw_bitmap(button1, 340, 70, 0);
                        al_draw_bitmap(button2Active, 340, 170, 0);
                        al_draw_bitmap(button3, 340, 270, 0);
                        al_draw_bitmap(button4, 340, 370, 0);
                        al_draw_text(menuFont20, white, 340 + 37, 40 + 60, 0, "ARCADE");
                        al_draw_text(menuFont20, white, 340 + 35, 140 + 60, 0, "RECORDS");
                        al_draw_text(menuFont20, white, 340 + 35, 240 + 60, 0, "SETTING");
                        al_draw_text(menuFont20, white, 340 + 50, 340 + 60, 0, "QUIT");
                        break;
                    case 3:
                        al_draw_bitmap(button1, 340, 70, 0);
                        al_draw_bitmap(button2, 340, 170, 0);
                        al_draw_bitmap(button3Active, 340, 270, 0);
                        al_draw_bitmap(button4, 340, 370, 0);
                        al_draw_text(menuFont20, white, 340 + 37, 40 + 60, 0, "ARCADE");
                        al_draw_text(menuFont20, white, 340 + 30, 140 + 60, 0, "RECORDS");
                        al_draw_text(menuFont20, white, 340 + 39, 240 + 60, 0, "SETTING");
                        al_draw_text(menuFont20, white, 340 + 50, 340 + 60, 0, "QUIT");
                        break;
                    case 4:
                        al_draw_bitmap(button1, 340, 70, 0);
                        al_draw_bitmap(button2, 340, 170, 0);
                        al_draw_bitmap(button3, 340, 270, 0);
                        al_draw_bitmap(button4Active, 340, 370, 0);
                        al_draw_text(menuFont20, white, 340 + 37, 40 + 60, 0, "ARCADE");
                        al_draw_text(menuFont20, white, 340 + 30, 140 + 60, 0, "RECORDS");
                        al_draw_text(menuFont20, white, 340 + 35, 240 + 60, 0, "SETTING");
                        al_draw_text(menuFont20, white, 340 + 55, 340 + 60, 0, "QUIT");
                        break;
                }
                drawMenu = false;

                al_flip_display();
                }
            }
        }
    }

        bool running = true;
        frame_number = 1;
        char filename[30];

        if(choice == 1)
        {
            while (running) {
                ALLEGRO_EVENT event;
                al_wait_for_event(event_queueMenu, &event);

                if (event.type == ALLEGRO_EVENT_TIMER) {
                    snprintf(filename, sizeof(filename), "menu\\start-arcade\\%d.png", frame_number);
                    ALLEGRO_BITMAP *frame = al_load_bitmap(filename);
                    if (!frame) {
                        fprintf(stderr, "Failed to load image %s.\n", filename);
                        running = false;
                        break;
                    }

                    al_draw_bitmap(frame, 0, 0, 0);
                    al_flip_display();
                    al_destroy_bitmap(frame);

                    if (++frame_number > 99) {
                        running = false;
                    }
                } else if (event.type == ALLEGRO_EVENT_DISPLAY_CLOSE) {
                    running = false;
                }
            }
        }



    al_destroy_bitmap(button1);
    al_destroy_bitmap(button2);
    al_destroy_bitmap(button3);
    al_destroy_bitmap(button4);

    al_destroy_bitmap(button1Active);
    al_destroy_bitmap(button2Active);
    al_destroy_bitmap(button3Active);
    al_destroy_bitmap(button4Active);

    al_destroy_timer(keyboard_timer);
    al_destroy_font(menuFont20);

    return choice;
}

int showMenuPause (float FPS, ALLEGRO_EVENT_QUEUE *event_queueMenu, ALLEGRO_KEYBOARD_STATE keyStateMenu, ALLEGRO_TIMER* timer)
{
    int wholeMenu = 0;


    int choice = 1;
    int settingChoice = 1;
    const float keyboard_FPS = 60.0;

    bool doneMenu = false, drawMenu = true;
    int dirMenu = DOWN;


    ALLEGRO_FONT *menuFont20 = al_load_font("menu\\menuFont.ttf", 20, 0);
    ALLEGRO_COLOR white = al_map_rgb(255, 255, 255);

    ALLEGRO_BITMAP *menuBackground = al_load_bitmap("menuPause\\menuBackground.png");
    ALLEGRO_BITMAP *pauseSign = al_load_bitmap("menuPause\\Pause.png");

    int menuBackground_W = 522;
    int menuBackground_H = 600;

    ALLEGRO_TIMER *keyboard_timer = al_create_timer(1.0 / keyboard_FPS);
    al_register_event_source(event_queueMenu, al_get_timer_event_source(keyboard_timer));

    ALLEGRO_BITMAP *button1Active = al_load_bitmap("menuPause\\Table.png");
    ALLEGRO_BITMAP *button2Active = al_load_bitmap("menuPause\\Table.png");
    ALLEGRO_BITMAP *button3Active = al_load_bitmap("menuPause\\Table.png");
    ALLEGRO_BITMAP *button1 = al_load_bitmap("menuPause\\TableActive.png");
    ALLEGRO_BITMAP *button2 = al_load_bitmap("menuPause\\TableActive.png");
    ALLEGRO_BITMAP *button3 = al_load_bitmap("menuPause\\TableActive.png");

    ALLEGRO_BITMAP *Ok = al_load_bitmap("menuPause\\Ok_BTN.png");
    ALLEGRO_BITMAP *OkActive = al_load_bitmap("menuPause\\Ok_BTN_ACTIVE.png");
    ALLEGRO_BITMAP *Close = al_load_bitmap("menuPause\\Close_BTN.png");
    ALLEGRO_BITMAP *CloseActive = al_load_bitmap("menuPause\\Close_BTN_ACTIVE.png");

    al_start_timer(keyboard_timer);

    while(!wholeMenu)
    {
        while(!doneMenu) {
            ALLEGRO_EVENT eventsMenu;
            al_wait_for_event(event_queueMenu, &eventsMenu);

            if(eventsMenu.type == ALLEGRO_EVENT_DISPLAY_CLOSE)
            {
                doneMenu = true;
            }
            else if(eventsMenu.type == ALLEGRO_EVENT_TIMER)
            {
                if(eventsMenu.timer.source == keyboard_timer)
                {
                    al_get_keyboard_state(&keyStateMenu);

                    if(al_key_down(&keyStateMenu, ALLEGRO_KEY_DOWN))
                    {
                        if(choice != 3) choice += 1;
                        else choice = 1;
                        al_rest(0.25);
                    }
                    else if(al_key_down(&keyStateMenu, ALLEGRO_KEY_UP))
                    {
                        if(choice != 1) choice -= 1;
                        else choice = 3;
                        al_rest(0.25);
                    }
                    else if(al_key_down(&keyStateMenu, ALLEGRO_KEY_ENTER))
                    {
                        doneMenu = true;
                        continue;
                    }

                    drawMenu = true;
                }
                if(eventsMenu.timer.source == timer)
                {
                    al_draw_bitmap(menuBackground, (BUFFER_W/2)*DISP_SCALE - menuBackground_W/2, (BUFFER_H/2)*DISP_SCALE - menuBackground_H/2, 0);
                    al_draw_bitmap(pauseSign, (BUFFER_W/2)*DISP_SCALE - (300/2), (BUFFER_H/2)*DISP_SCALE - menuBackground_H/2 + 12, 0);
                    switch (choice)
                    {
                        case 1:
                            al_draw_bitmap(button1Active, BUFFER_W/2*DISP_SCALE - (100), BUFFER_H/2*DISP_SCALE - 100, 0);
                            al_draw_bitmap(button2, BUFFER_W/2*DISP_SCALE - (100), BUFFER_H/2*DISP_SCALE, 0);
                            al_draw_bitmap(button3, BUFFER_W/2*DISP_SCALE - (100), BUFFER_H/2*DISP_SCALE + 100, 0);
                            al_draw_text(menuFont20, white, BUFFER_W/2*DISP_SCALE - (100) + 43 + 15, BUFFER_H/2*DISP_SCALE + 30 - 112, 0, "RESUME");
                            al_draw_text(menuFont20, white, BUFFER_W/2*DISP_SCALE - (100) + 39 + 15, BUFFER_H/2*DISP_SCALE + 30 - 11, 0, "SETTING");
                            al_draw_text(menuFont20, white, BUFFER_W/2*DISP_SCALE - (100) + 54 + 15, BUFFER_H/2*DISP_SCALE + 30 + 89, 0, "QUIT");
                            break;
                        case 2:
                            al_draw_bitmap(button1, BUFFER_W/2*DISP_SCALE - (100), BUFFER_H/2*DISP_SCALE - 100, 0);
                            al_draw_bitmap(button2Active, BUFFER_W/2*DISP_SCALE - (100), BUFFER_H/2*DISP_SCALE, 0);
                            al_draw_bitmap(button3, BUFFER_W/2*DISP_SCALE - (100), BUFFER_H/2*DISP_SCALE + 100, 0);
                            al_draw_text(menuFont20, white, BUFFER_W/2*DISP_SCALE - (100) + 39 + 15, BUFFER_H/2*DISP_SCALE + 30 - 112, 0, "RESUME");
                            al_draw_text(menuFont20, white, BUFFER_W/2*DISP_SCALE - (100) + 43 + 15, BUFFER_H/2*DISP_SCALE + 30 - 11, 0, "SETTING");
                            al_draw_text(menuFont20, white, BUFFER_W/2*DISP_SCALE - (100) + 54 + 15, BUFFER_H/2*DISP_SCALE + 30 + 89, 0, "QUIT");
                            break;
                        case 3:
                            al_draw_bitmap(button1, BUFFER_W/2*DISP_SCALE - (100), BUFFER_H/2*DISP_SCALE - 100, 0);
                            al_draw_bitmap(button2, BUFFER_W/2*DISP_SCALE - (100), BUFFER_H/2*DISP_SCALE, 0);
                            al_draw_bitmap(button3Active, BUFFER_W/2*DISP_SCALE - (100), BUFFER_H/2*DISP_SCALE + 100, 0);
                            al_draw_text(menuFont20, white, BUFFER_W/2*DISP_SCALE - (100) + 39 + 15, BUFFER_H/2*DISP_SCALE + 30 - 112, 0, "RESUME");
                            al_draw_text(menuFont20, white, BUFFER_W/2*DISP_SCALE - (100) + 39 + 15, BUFFER_H/2*DISP_SCALE + 30 - 11, 0, "SETTING");
                            al_draw_text(menuFont20, white, BUFFER_W/2*DISP_SCALE - (100) + 58 + 15, BUFFER_H/2*DISP_SCALE + 30 + 89, 0, "QUIT");
                            break;
                    }
                    drawMenu = false;

                    al_flip_display();
                }
            }
        }
        al_rest(0.25);
        doneMenu = false;
        if(choice == 1)
            wholeMenu = 1;
        else if(choice == 2)
        {
            while(!doneMenu) {
                ALLEGRO_EVENT eventsMenu;
                al_wait_for_event(event_queueMenu, &eventsMenu);

                if(eventsMenu.type == ALLEGRO_EVENT_DISPLAY_CLOSE)
                {
                    doneMenu = true;
                }
                else if(eventsMenu.type == ALLEGRO_EVENT_TIMER)
                {
                    if(eventsMenu.timer.source == keyboard_timer)
                    {
                        al_get_keyboard_state(&keyStateMenu);

                        if(al_key_down(&keyStateMenu, ALLEGRO_KEY_DOWN))
                        {
                            if(settingChoice != 3) settingChoice += 1;
                            else settingChoice = 1;
                            al_rest(0.25);
                        }
                        else if(al_key_down(&keyStateMenu, ALLEGRO_KEY_UP))
                        {
                            if(settingChoice != 1) settingChoice -= 1;
                            else settingChoice = 3;
                            al_rest(0.25);
                        }
                        else if(al_key_down(&keyStateMenu, ALLEGRO_KEY_ENTER))
                        {
                            if(settingChoice == 3)
                                doneMenu = true;
                            continue;
                        }
                        else if(al_key_down(&keyStateMenu, ALLEGRO_KEY_RIGHT) || al_key_down(&keyStateMenu, ALLEGRO_KEY_LEFT))
                        {
                            if(settingChoice == 1)
                                gameMusic = (!gameMusic);
                            else if(settingChoice == 2)
                                fxPlay = (!fxPlay);

                            al_rest(0.25);
                        }

                        drawMenu = true;
                    }
                    if(eventsMenu.timer.source == timer)
                    {
                        al_draw_bitmap(menuBackground, (BUFFER_W/2)*DISP_SCALE - menuBackground_W/2, (BUFFER_H/2)*DISP_SCALE - menuBackground_H/2, 0);
                        al_draw_bitmap(pauseSign, (BUFFER_W/2)*DISP_SCALE - (300/2), (BUFFER_H/2)*DISP_SCALE - menuBackground_H/2 + 12, 0);
                        switch (settingChoice)
                        {
                            case 1:
                                al_draw_bitmap(button1Active, BUFFER_W/2*DISP_SCALE - (200), BUFFER_H/2*DISP_SCALE - 100, 0);
                                al_draw_bitmap(button2, BUFFER_W/2*DISP_SCALE - (200), BUFFER_H/2*DISP_SCALE, 0);
                                al_draw_bitmap(button3, BUFFER_W/2*DISP_SCALE - (200), BUFFER_H/2*DISP_SCALE + 100, 0);
                                al_draw_text(menuFont20, white, BUFFER_W/2*DISP_SCALE - (200) + 42 + 30, BUFFER_H/2*DISP_SCALE + 30 - 112, 0, "MUSIC");
                                al_draw_text(menuFont20, white, BUFFER_W/2*DISP_SCALE - (200) + 30 + 55, BUFFER_H/2*DISP_SCALE + 30 - 11, 0, "FX");
                                al_draw_text(menuFont20, white, BUFFER_W/2*DISP_SCALE - (200) + 35 + 38, BUFFER_H/2*DISP_SCALE + 30 + 89, 0, "BACK");
                                if(gameMusic)
                                {
                                    al_draw_bitmap(OkActive, BUFFER_W/2*DISP_SCALE - (200) + 250, BUFFER_H/2*DISP_SCALE - 100, 0);
                                    al_draw_bitmap(Close, BUFFER_W/2*DISP_SCALE - (200) + 350, BUFFER_H/2*DISP_SCALE - 100, 0);
                                }
                                else
                                {
                                    al_draw_bitmap(Ok, BUFFER_W/2*DISP_SCALE - (200) + 250, BUFFER_H/2*DISP_SCALE - 100, 0);
                                    al_draw_bitmap(CloseActive, BUFFER_W/2*DISP_SCALE - (200) + 350, BUFFER_H/2*DISP_SCALE - 100, 0);
                                }
                                if(fxPlay)
                                {
                                    al_draw_bitmap(OkActive, BUFFER_W/2*DISP_SCALE - (200) + 250, BUFFER_H/2*DISP_SCALE, 0);
                                    al_draw_bitmap(Close, BUFFER_W/2*DISP_SCALE - (200) + 350, BUFFER_H/2*DISP_SCALE, 0);
                                }
                                else
                                {
                                    al_draw_bitmap(Ok, BUFFER_W/2*DISP_SCALE - (200) + 250, BUFFER_H/2*DISP_SCALE, 0);
                                    al_draw_bitmap(CloseActive, BUFFER_W/2*DISP_SCALE - (200) + 350, BUFFER_H/2*DISP_SCALE, 0);
                                }
                                break;
                            case 2:
                                al_draw_bitmap(button1, BUFFER_W/2*DISP_SCALE - (200), BUFFER_H/2*DISP_SCALE - 100, 0);
                                al_draw_bitmap(button2Active, BUFFER_W/2*DISP_SCALE - (200), BUFFER_H/2*DISP_SCALE, 0);
                                al_draw_bitmap(button3, BUFFER_W/2*DISP_SCALE - (200), BUFFER_H/2*DISP_SCALE + 100, 0);
                                al_draw_text(menuFont20, white, BUFFER_W/2*DISP_SCALE - (200) + 37 + 30, BUFFER_H/2*DISP_SCALE + 30 - 112, 0, "MUSIC");
                                al_draw_text(menuFont20, white, BUFFER_W/2*DISP_SCALE - (200) + 35 + 55, BUFFER_H/2*DISP_SCALE + 30 - 11, 0, "FX");
                                al_draw_text(menuFont20, white, BUFFER_W/2*DISP_SCALE - (200) + 35 + 38, BUFFER_H/2*DISP_SCALE + 30 + 89, 0, "BACK");
                                if(gameMusic)
                                {
                                    al_draw_bitmap(OkActive, BUFFER_W/2*DISP_SCALE - (200) + 250, BUFFER_H/2*DISP_SCALE - 100, 0);
                                    al_draw_bitmap(Close, BUFFER_W/2*DISP_SCALE - (200) + 350, BUFFER_H/2*DISP_SCALE - 100, 0);
                                }
                                else
                                {
                                    al_draw_bitmap(Ok, BUFFER_W/2*DISP_SCALE - (200) + 250, BUFFER_H/2*DISP_SCALE - 100, 0);
                                    al_draw_bitmap(CloseActive, BUFFER_W/2*DISP_SCALE - (200) + 350, BUFFER_H/2*DISP_SCALE - 100, 0);
                                }
                                if(fxPlay)
                                {
                                    al_draw_bitmap(OkActive, BUFFER_W/2*DISP_SCALE - (200) + 250, BUFFER_H/2*DISP_SCALE, 0);
                                    al_draw_bitmap(Close, BUFFER_W/2*DISP_SCALE - (200) + 350, BUFFER_H/2*DISP_SCALE, 0);
                                }
                                else
                                {
                                    al_draw_bitmap(Ok, BUFFER_W/2*DISP_SCALE - (200) + 250, BUFFER_H/2*DISP_SCALE, 0);
                                    al_draw_bitmap(CloseActive, BUFFER_W/2*DISP_SCALE - (200) + 350, BUFFER_H/2*DISP_SCALE, 0);
                                }
                                break;
                            case 3:
                                al_draw_bitmap(button1, BUFFER_W/2*DISP_SCALE - (200), BUFFER_H/2*DISP_SCALE - 100, 0);
                                al_draw_bitmap(button2, BUFFER_W/2*DISP_SCALE - (200), BUFFER_H/2*DISP_SCALE, 0);
                                al_draw_bitmap(button3Active, BUFFER_W/2*DISP_SCALE - (200), BUFFER_H/2*DISP_SCALE + 100, 0);
                                al_draw_text(menuFont20, white, BUFFER_W/2*DISP_SCALE - (200) + 37 + 30, BUFFER_H/2*DISP_SCALE + 30 - 112, 0, "MUSIC");
                                al_draw_text(menuFont20, white, BUFFER_W/2*DISP_SCALE - (200) + 30 + 55, BUFFER_H/2*DISP_SCALE + 30 - 11, 0, "FX");
                                al_draw_text(menuFont20, white, BUFFER_W/2*DISP_SCALE - (200) + 39 + 38, BUFFER_H/2*DISP_SCALE + 30 + 89, 0, "BACK");
                                if(gameMusic)
                                {
                                    al_draw_bitmap(OkActive, BUFFER_W/2*DISP_SCALE - (200) + 250, BUFFER_H/2*DISP_SCALE - 100, 0);
                                    al_draw_bitmap(Close, BUFFER_W/2*DISP_SCALE - (200) + 350, BUFFER_H/2*DISP_SCALE - 100, 0);
                                }
                                else
                                {
                                    al_draw_bitmap(Ok, BUFFER_W/2*DISP_SCALE - (200) + 250, BUFFER_H/2*DISP_SCALE - 100, 0);
                                    al_draw_bitmap(CloseActive, BUFFER_W/2*DISP_SCALE - (200) + 350, BUFFER_H/2*DISP_SCALE - 100, 0);
                                }
                                if(fxPlay)
                                {
                                    al_draw_bitmap(OkActive, BUFFER_W/2*DISP_SCALE - (200) + 250, BUFFER_H/2*DISP_SCALE, 0);
                                    al_draw_bitmap(Close, BUFFER_W/2*DISP_SCALE - (200) + 350, BUFFER_H/2*DISP_SCALE, 0);
                                }
                                else
                                {
                                    al_draw_bitmap(Ok, BUFFER_W/2*DISP_SCALE - (200) + 250, BUFFER_H/2*DISP_SCALE, 0);
                                    al_draw_bitmap(CloseActive, BUFFER_W/2*DISP_SCALE - (200) + 350, BUFFER_H/2*DISP_SCALE, 0);
                                }
                                break;
                        }
                        drawMenu = false;

                        al_flip_display();
                    }
                }
            }
            al_rest(0.25);
        }
        else if(choice == 3)
        {
            wholeMenu = 1;
        }
        if(settingChoice == 3)
            doneMenu = false;
    }



    al_destroy_bitmap(menuBackground);
    al_destroy_bitmap(pauseSign);
    al_destroy_bitmap(button1);
    al_destroy_bitmap(button2);
    al_destroy_bitmap(button3);
    al_destroy_bitmap(button1Active);
    al_destroy_bitmap(button2Active);
    al_destroy_bitmap(button3Active);
    al_destroy_bitmap(Ok);
    al_destroy_bitmap(OkActive);
    al_destroy_bitmap(Close);
    al_destroy_bitmap(CloseActive);
    al_destroy_timer(keyboard_timer);
    al_destroy_font(menuFont20);

    return choice;
}

int gameOverMenu (float FPS, ALLEGRO_EVENT_QUEUE *event_queueMenu, ALLEGRO_KEYBOARD_STATE keyStateMenu, ALLEGRO_TIMER* timer)
{
    int choice = 1;
    int settingChoice = 1;
    const float keyboard_FPS = 60.0;

    bool doneMenu = false, drawMenu = true;
    int dirMenu = DOWN;

    char name[4] = {'A', 'A', 'A', '\0'};


    ALLEGRO_FONT *menuFont20 = al_load_font("menu\\menuFont.ttf", 20, 0);
    ALLEGRO_FONT *menuFont30 = al_load_font("menu\\menuFont.ttf", 30, 0);
    ALLEGRO_COLOR white = al_map_rgb(255, 255, 255);

    ALLEGRO_BITMAP *menuBackground = al_load_bitmap("menuPause\\menuBackground.png");
    ALLEGRO_BITMAP *Lose = al_load_bitmap("menuPause\\LOSE.png");

    int menuBackground_W = 522;
    int menuBackground_H = 600;

    ALLEGRO_TIMER *keyboard_timer = al_create_timer(1.0 / keyboard_FPS);
    al_register_event_source(event_queueMenu, al_get_timer_event_source(keyboard_timer));

    ALLEGRO_BITMAP *button1 = al_load_bitmap("menuPause\\TableActive.png");
    ALLEGRO_BITMAP *button2 = al_load_bitmap("menuPause\\TableActive.png");
    ALLEGRO_BITMAP *button1Active = al_load_bitmap("menuPause\\Table.png");
    ALLEGRO_BITMAP *button2Active = al_load_bitmap("menuPause\\Table.png");

    ALLEGRO_BITMAP *input = al_load_bitmap("menuPause\\button.png");
    ALLEGRO_BITMAP *inputActive = al_load_bitmap("menuPause\\buttonActive.png");

    al_start_timer(keyboard_timer);
    int check = 0;
    int i = 0;

    while(1)
    {
        char temp = 'A';
        ALLEGRO_EVENT eventsMenu;
        al_wait_for_event(event_queueMenu, &eventsMenu);


        if(eventsMenu.type == ALLEGRO_EVENT_DISPLAY_CLOSE)
        {
            doneMenu = true;
        }
        else if(eventsMenu.type == ALLEGRO_EVENT_TIMER)
        {
            if(eventsMenu.timer.source == keyboard_timer)
            {
                al_get_keyboard_state(&keyStateMenu);

                if(al_key_down(&keyStateMenu, ALLEGRO_KEY_UP))
                {
                    if (check != 0) check -= 1;
                    else check = 25;
                    al_rest(0.25);
                }
                else if(al_key_down(&keyStateMenu, ALLEGRO_KEY_DOWN))
                {
                    if(check != 25) check += 1;
                    else check = 0;
                    al_rest(0.25);
                }
                else if(al_key_down(&keyStateMenu, ALLEGRO_KEY_ENTER))
                 {
                    name[i] = 65 + check;
                    check = 0;
                    i++;
                    if(choice != 4) choice++;
                    al_rest(0.25);
                    continue;
                }

                drawMenu = true;
            }
            if(eventsMenu.timer.source == timer)
            {
                al_draw_bitmap(menuBackground, (BUFFER_W/2)*DISP_SCALE - menuBackground_W/2, (BUFFER_H/2)*DISP_SCALE - menuBackground_H/2, 0);
                al_draw_bitmap(Lose, (BUFFER_W/2)*DISP_SCALE - (450/2), (BUFFER_H/2)*DISP_SCALE - menuBackground_H/2 + 12, 0);
                al_draw_text(menuFont20, white, BUFFER_W/2*DISP_SCALE + (-5), BUFFER_H/2*DISP_SCALE + 200, ALLEGRO_ALIGN_CENTRE, "ENTER YOUR NICK:\n(use arrow keys to choose)");
                switch (choice)
                {
                    case 1:
                        al_draw_bitmap(inputActive, BUFFER_W/2*DISP_SCALE - (125), BUFFER_H/2*DISP_SCALE - 0, 0);
                        al_draw_bitmap(input, BUFFER_W/2*DISP_SCALE + (-25), BUFFER_H/2*DISP_SCALE - 0, 0);
                        al_draw_bitmap(input, BUFFER_W/2*DISP_SCALE + (75), BUFFER_H/2*DISP_SCALE - 0, 0);
                        al_draw_textf(menuFont20, white, BUFFER_W/2*DISP_SCALE - (105), BUFFER_H/2*DISP_SCALE + 20, 0, "%c", 65 + check);
                        al_draw_text(menuFont20, white, BUFFER_W/2*DISP_SCALE + (-5), BUFFER_H/2*DISP_SCALE + 20, 0, "A");
                        al_draw_text(menuFont20, white, BUFFER_W/2*DISP_SCALE + (95), BUFFER_H/2*DISP_SCALE + 20, 0, "A");
                        break;
                    case 2:
                        al_draw_bitmap(input, BUFFER_W/2*DISP_SCALE - (125), BUFFER_H/2*DISP_SCALE - 0, 0);
                        al_draw_bitmap(inputActive, BUFFER_W/2*DISP_SCALE + (-25), BUFFER_H/2*DISP_SCALE - 0, 0);
                        al_draw_bitmap(input, BUFFER_W/2*DISP_SCALE + (75), BUFFER_H/2*DISP_SCALE - 0, 0);
                        al_draw_textf(menuFont20, white, BUFFER_W/2*DISP_SCALE - (105), BUFFER_H/2*DISP_SCALE + 20, 0,"%c", name[0]);
                        al_draw_textf(menuFont20, white, BUFFER_W/2*DISP_SCALE + (-5), BUFFER_H/2*DISP_SCALE + 20, 0, "%c", 65 + check);
                        al_draw_text(menuFont20, white, BUFFER_W/2*DISP_SCALE + (95), BUFFER_H/2*DISP_SCALE + 20, 0, "A");
                        break;
                    case 3:
                        al_draw_bitmap(input, BUFFER_W/2*DISP_SCALE - (125), BUFFER_H/2*DISP_SCALE - 0, 0);
                        al_draw_bitmap(input, BUFFER_W/2*DISP_SCALE + (-25), BUFFER_H/2*DISP_SCALE - 0, 0);
                        al_draw_bitmap(inputActive, BUFFER_W/2*DISP_SCALE + (75), BUFFER_H/2*DISP_SCALE - 0, 0);
                        al_draw_textf(menuFont20, white, BUFFER_W/2*DISP_SCALE - (105), BUFFER_H/2*DISP_SCALE + 20, 0,"%c", name[0]);
                        al_draw_textf(menuFont20, white, BUFFER_W/2*DISP_SCALE + (-5), BUFFER_H/2*DISP_SCALE + 20, 0, "%c", name[1]);
                        al_draw_textf(menuFont20, white, BUFFER_W/2*DISP_SCALE + (95), BUFFER_H/2*DISP_SCALE + 20, 0, "%c", 65 + check);
                        break;
                }
                drawMenu = false;
                al_flip_display();
            }
        }
            if(choice == 4)
                break;
    }

    char str[8];
    sprintf(str, "%ld", score);

    time_t now;
    struct tm *tm_info;
    char buffer[80];

    // Get current time
    time(&now);
    // Convert time to local time
    tm_info = localtime(&now);

    // Format the time into a string: "YYYY-MM-DD HH:MM:SS"
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);


    addRecord(name, str, buffer);
    saveRecordsToFile();

    choice = 1;


    while(!doneMenu) {
        ALLEGRO_EVENT eventsMenu;
        al_wait_for_event(event_queueMenu, &eventsMenu);

        if(eventsMenu.type == ALLEGRO_EVENT_DISPLAY_CLOSE)
        {
            doneMenu = true;
        }
        else if(eventsMenu.type == ALLEGRO_EVENT_TIMER)
        {
            if(eventsMenu.timer.source == keyboard_timer)
            {
                al_get_keyboard_state(&keyStateMenu);

                if(al_key_down(&keyStateMenu, ALLEGRO_KEY_RIGHT))
                {
                    if(choice != 2) choice += 1;
                    else choice = 1;
                    al_rest(0.25);
                }
                else if(al_key_down(&keyStateMenu, ALLEGRO_KEY_LEFT))
                {
                    if(choice != 1) choice -= 1;
                    else choice = 2;
                    al_rest(0.25);
                }
                else if(al_key_down(&keyStateMenu, ALLEGRO_KEY_ENTER))
                {
                    doneMenu = true;
                    al_rest(0.25);
                    continue;
                }

                drawMenu = true;
            }
            if(eventsMenu.timer.source == timer)
            {
                al_draw_bitmap(menuBackground, (BUFFER_W/2)*DISP_SCALE - menuBackground_W/2, (BUFFER_H/2)*DISP_SCALE - menuBackground_H/2, 0);
                al_draw_bitmap(Lose, (BUFFER_W/2)*DISP_SCALE - (450/2), (BUFFER_H/2)*DISP_SCALE - menuBackground_H/2 + 12, 0);
                al_draw_textf(menuFont30, white, BUFFER_W/2*DISP_SCALE - 200, BUFFER_H/2*DISP_SCALE - 150, 0, "Your Score:          %07ld", score);
                showFirstFrag(menuFont30, BUFFER_W/2*DISP_SCALE -200, BUFFER_H/2*DISP_SCALE - 50);
                switch (choice)
                {
                    case 1:
                        al_draw_bitmap(button1Active, BUFFER_W/2*DISP_SCALE - (200), BUFFER_H/2*DISP_SCALE + 200, 0);
                        al_draw_bitmap(button2, BUFFER_W/2*DISP_SCALE + 10, BUFFER_H/2*DISP_SCALE + 200, 0);
                        al_draw_text(menuFont20, white, BUFFER_W/2*DISP_SCALE - (100) , BUFFER_H/2*DISP_SCALE + 215, ALLEGRO_ALIGN_CENTRE, "Menu");
                        al_draw_text(menuFont20, white, BUFFER_W/2*DISP_SCALE + (110) , BUFFER_H/2*DISP_SCALE + 215, ALLEGRO_ALIGN_CENTRE, "Play again");
                        break;
                    case 2:
                        al_draw_bitmap(button1, BUFFER_W/2*DISP_SCALE - (200), BUFFER_H/2*DISP_SCALE + 200, 0);
                        al_draw_bitmap(button2Active, BUFFER_W/2*DISP_SCALE + (10), BUFFER_H/2*DISP_SCALE + 200, 0);
                        al_draw_text(menuFont20, white, BUFFER_W/2*DISP_SCALE - (100) , BUFFER_H/2*DISP_SCALE + 215, ALLEGRO_ALIGN_CENTRE, "Menu");
                        al_draw_text(menuFont20, white, BUFFER_W/2*DISP_SCALE + (110) , BUFFER_H/2*DISP_SCALE + 215, ALLEGRO_ALIGN_CENTRE, "Play again");
                        break;
                }
                drawMenu = false;
                al_flip_display();
            }
        }
    }
    al_rest(0.25);

    al_destroy_bitmap(menuBackground);
    al_destroy_bitmap(Lose);
    al_destroy_bitmap(button1);
    al_destroy_bitmap(button2);
    al_destroy_bitmap(button1Active);
    al_destroy_bitmap(button2Active);
    al_destroy_bitmap(input);
    al_destroy_bitmap(inputActive);
    al_destroy_timer(keyboard_timer);
    al_destroy_font(menuFont20);


    return choice;
}


int showRecord (float FPS, ALLEGRO_EVENT_QUEUE *event_queueMenu, ALLEGRO_KEYBOARD_STATE keyStateMenu, ALLEGRO_TIMER* timer)
{
    int wholeMenu = 0;


    int choice = 1;
    int settingChoice = 1;
    const float keyboard_FPS = 60.0;

    bool doneMenu = false, drawMenu = true;
    int dirMenu = DOWN;


    ALLEGRO_FONT *menuFont20 = al_load_font("menu\\menuFont.ttf", 20, 0);
    ALLEGRO_FONT *menuFont50 = al_load_font("menu\\menuFont.ttf", 40, 0);
    ALLEGRO_COLOR white = al_map_rgb(255, 255, 255);

    ALLEGRO_BITMAP *menuBackground = al_load_bitmap("menuPause\\menuBackgroundRecord.png");
    ALLEGRO_BITMAP *recordSign = al_load_bitmap("menuPause\\Record.png");

    int menuBackground_W = 1044;
    int menuBackground_H = 600;

    ALLEGRO_TIMER *keyboard_timer = al_create_timer(1.0 / keyboard_FPS);
    al_register_event_source(event_queueMenu, al_get_timer_event_source(keyboard_timer));

    ALLEGRO_BITMAP *button1Active = al_load_bitmap("menuPause\\TableActive.png");

    al_start_timer(keyboard_timer);

    while(!doneMenu) {
        ALLEGRO_EVENT eventsMenu;
        al_wait_for_event(event_queueMenu, &eventsMenu);

        if(eventsMenu.type == ALLEGRO_EVENT_DISPLAY_CLOSE)
        {
            doneMenu = true;
        }
        else if(eventsMenu.type == ALLEGRO_EVENT_TIMER)
        {
            if(eventsMenu.timer.source == keyboard_timer)
            {
                al_get_keyboard_state(&keyStateMenu);
                if(al_key_down(&keyStateMenu, ALLEGRO_KEY_ENTER))
                {
                    doneMenu = true;
                    continue;
                }

                drawMenu = true;
            }
            if(eventsMenu.timer.source == timer)
            {
                al_draw_bitmap(menuBackground, (BUFFER_W/2)*DISP_SCALE - menuBackground_W/2, (BUFFER_H/2)*DISP_SCALE - menuBackground_H/2, 0);
                al_draw_bitmap(recordSign, (BUFFER_W/2)*DISP_SCALE - (300/2), (BUFFER_H/2)*DISP_SCALE - menuBackground_H/2 + 20, 0);
                switch (choice)
                {
                    case 1:
                        al_draw_bitmap(button1Active, BUFFER_W/2*DISP_SCALE - (100), BUFFER_H/2*DISP_SCALE + 220, 0);
                        al_draw_text(menuFont20, white, BUFFER_W/2*DISP_SCALE - (100) + 70, BUFFER_H/2*DISP_SCALE + 235, 0, "Back");
                        break;
                }
                printSortedHashMapMe(&myNestedHashMap, menuFont50, ((BUFFER_W/2)*DISP_SCALE) - 300, ((BUFFER_H/2)*DISP_SCALE) - 300);
                drawMenu = false;

                al_flip_display();
            }
        }
    }
    al_rest(0.25);

    al_destroy_bitmap(menuBackground);
    al_destroy_bitmap(recordSign);
    al_destroy_bitmap(button1Active);
    al_destroy_timer(keyboard_timer);
    al_destroy_font(menuFont20);

    return choice;
}


double calcDegree(int shipX, int shipY, int alienX, int alienY) {
    double dy = shipY - alienY;
    double dx = shipX - alienX;
    return atan2(dy, dx); // atan2 automatically handles division by zero and returns the correct angle in radians
}




#define SHIP_W 30
#define SHIP_H 39

#define SHIP_SHOT_W 2
#define SHIP_SHOT_H 9

#define LIFE_W 6
#define LIFE_H 6

const int ALIEN_W[] = {14, 20, 45, 15, 22, 24, 20};
const int ALIEN_H[] = { 9, 24, 27, 15, 20, 21, 20};

#define ALIEN_BUG_W             ALIEN_W[0]
#define ALIEN_BUG_H             ALIEN_H[0]
#define ALIEN_WARRIOR_W         ALIEN_W[1]
#define ALIEN_WARRIOR_H         ALIEN_H[1]
#define ALIEN_THICCBOI_W        ALIEN_W[2]
#define ALIEN_THICCBOI_H        ALIEN_H[2]
#define ALIEN_MINE_W            ALIEN_W[3]
#define ALIEN_MINE_H            ALIEN_H[3]
#define ALIEN_BOMBER_W          ALIEN_W[4]
#define ALIEN_BOMBER_H          ALIEN_H[4]
#define ALIEN_CARGO_SHIP_W      ALIEN_W[5]
#define ALIEN_CARGO_SHIP_H      ALIEN_H[5]
#define ALIEN_DRONE_W           ALIEN_W[6]
#define ALIEN_DRONE_H           ALIEN_H[6]

#define ALIEN_SHOT_W                4
#define ALIEN_SHOT_H                4
#define ALIEN_SHOT_BOMBER_W         12
#define ALIEN_SHOT_BOMBER_H         11
#define ALIEN_CARGO_SHIP_HEALTH_W   32
#define ALIEN_CARGO_SHIP_HEALTH_H   32
#define ALIEN_CARGO_SHIP_BULLET_W   32
#define ALIEN_CARGO_SHIP_BULLET_H   32
#define ALIEN_CARGO_SHIP_SHIELD_W   32
#define ALIEN_CARGO_SHIP_SHIELD_H   32


#define EXPLOSION_FRAMES 4
#define SPARKS_FRAMES    3

typedef enum ALIEN_TYPE
{
    ALIEN_TYPE_BUG = 0,
    ALIEN_TYPE_WARRIOR,
    ALIEN_TYPE_THICCBOI,
    ALIEN_TYPE_MINE,
    ALIEN_TYPE_BOMBER,
    ALIEN_TYPE_CARGO_SHIP,
    ALIEN_TYPE_DRONE,
    ALIEN_TYPE_N
} ALIEN_TYPE;

typedef struct ALIEN
{
    int x, y;
    ALIEN_TYPE type;
    int shot_timer;
    int shot_power;
    int blink;
    int life;
    int RorL;
    bool used;
} ALIEN;

#define ALIENS_N 18
ALIEN aliens[ALIENS_N];


#define SHIP_SPEED 3
#define SHIP_HEALTH 20;
#define SHIP_MAX_X (BUFFER_W - SHIP_W)
#define SHIP_MAX_Y (BUFFER_H - SHIP_H)

typedef struct SHIP
{
    int x, y;
    int shot_timer;
    int lives;
    int respawn_timer;
    int invincible_timer;
    int ship_shot_power;
    int shield_timer;
} SHIP;
SHIP ship;



typedef struct SPRITES
{
    ALLEGRO_BITMAP* _sheet;

    ALLEGRO_BITMAP* shipFull;
    ALLEGRO_BITMAP* shipSlight;
    ALLEGRO_BITMAP* shipBase;
    ALLEGRO_BITMAP* shipVery;

    ALLEGRO_BITMAP* ship_mask;
    ALLEGRO_BITMAP* shield;

    ALLEGRO_BITMAP* ship_shot[2];
    ALLEGRO_BITMAP* ship_shot_mask[2];
    ALLEGRO_BITMAP* life;

    ALLEGRO_BITMAP* alien[ALIEN_TYPE_N];
    ALLEGRO_BITMAP* alien_shot[5];
    ALLEGRO_BITMAP* MINE[4];

    ALLEGRO_BITMAP* alien_mask[ALIEN_TYPE_N];
    ALLEGRO_BITMAP* alien_shot_mask[5];
    ALLEGRO_BITMAP* MINE_mask[4];

    ALLEGRO_BITMAP* explosion[EXPLOSION_FRAMES];
    ALLEGRO_BITMAP* sparks[SPARKS_FRAMES];

    ALLEGRO_BITMAP* powerup[4];
} SPRITES;
SPRITES sprites;

ALLEGRO_BITMAP* sprite_grab(int x, int y, int w, int h)
{
    ALLEGRO_BITMAP* sprite = al_create_sub_bitmap(sprites._sheet, x, y, w, h);
    must_init(sprite, "sprite grab");
    return sprite;
}

ALLEGRO_BITMAP* create_collision_mask(ALLEGRO_BITMAP* sprite);

void sprites_init()
{
    sprites._sheet = al_load_bitmap("graphic\\GENERAL\\spritesheet.png");
    must_init(sprites._sheet, "spritesheet");

    sprites.shipFull = al_load_bitmap("graphic\\MAIN_SHIP\\new4.png");

    sprites.ship_mask = create_collision_mask(sprites.shipFull);

    sprites.life = sprite_grab(0, 14, LIFE_W, LIFE_H);

    sprites.ship_shot[0] = sprite_grab(13, 0, SHIP_SHOT_W, SHIP_SHOT_H);
    sprites.ship_shot[1] = sprite_grab(16, 0, SHIP_SHOT_W, SHIP_SHOT_H);

    for (int i = 0; i < 2; i++) {
        sprites.ship_shot_mask[i] = create_collision_mask(sprites.ship_shot[i]);
    }

    sprites.shield = al_load_bitmap("graphic\\MAIN_SHIP\\SHIELD.png");

    sprites.alien[0] = sprite_grab(19, 0, ALIEN_BUG_W, ALIEN_BUG_H);
    sprites.alien[1] = al_load_bitmap("graphic\\WARRIOR\\WARRIOR.png");
    sprites.alien[2] = sprite_grab(0, 21, ALIEN_THICCBOI_W, ALIEN_THICCBOI_H);
    sprites.alien[3] = al_load_bitmap("graphic\\SPACE_BOMB\\1.png");
    sprites.alien[4] = al_load_bitmap("graphic\\BOMBER\\BOMBER.png");
    sprites.alien[5] = al_load_bitmap("graphic\\CARGO_SHIP\\CARGO_SHIP.png");
    sprites.alien[6] = al_load_bitmap("graphic\\DRONE\\DRONE.png");

    for (int i = 0; i < ALIEN_TYPE_N; i++) {
        sprites.alien_mask[i] = create_collision_mask(sprites.alien[i]);
    }


    sprites.MINE[0] = al_load_bitmap("graphic\\SPACE_BOMB\\1.png");
    sprites.MINE[1] = al_load_bitmap("graphic\\SPACE_BOMB\\2.png");
    sprites.MINE[2] = al_load_bitmap("graphic\\SPACE_BOMB\\3.png");
    sprites.MINE[3] = al_load_bitmap("graphic\\SPACE_BOMB\\4.png");


    sprites.alien_shot[0] = sprite_grab(13, 10, ALIEN_SHOT_W, ALIEN_SHOT_H);
    sprites.alien_shot[1] = al_load_bitmap("graphic\\BOMBER\\BOMB.png");
    sprites.alien_shot[2] = al_load_bitmap("graphic\\CARGO_SHIP\\HEALTH.png");
    sprites.alien_shot[3] = al_load_bitmap("graphic\\CARGO_SHIP\\BULLET.png");
    sprites.alien_shot[4] = al_load_bitmap("graphic\\CARGO_SHIP\\SHIELD.png");

    sprites.alien_shot_mask[0] = create_collision_mask(sprites.alien_shot[0]);
    sprites.alien_shot_mask[1] = create_collision_mask(sprites.alien_shot[1]);
    sprites.alien_shot_mask[2] = create_collision_mask(sprite_grab(0, 0, ALIEN_CARGO_SHIP_HEALTH_W, ALIEN_CARGO_SHIP_HEALTH_H));
    sprites.alien_shot_mask[3] = create_collision_mask(sprite_grab(0, 0, ALIEN_CARGO_SHIP_BULLET_W, ALIEN_CARGO_SHIP_BULLET_H));
    sprites.alien_shot_mask[4] = create_collision_mask(sprite_grab(0, 0, ALIEN_CARGO_SHIP_SHIELD_W, ALIEN_CARGO_SHIP_SHIELD_H));

    sprites.explosion[0] = sprite_grab(33, 10, 9, 9);
    sprites.explosion[1] = sprite_grab(43, 9, 11, 11);
    sprites.explosion[2] = sprite_grab(46, 21, 17, 18);
    sprites.explosion[3] = sprite_grab(46, 40, 17, 17);

    sprites.sparks[0] = sprite_grab(34, 0, 10, 8);
    sprites.sparks[1] = sprite_grab(45, 0, 7, 8);
    sprites.sparks[2] = sprite_grab(54, 0, 9, 8);

    sprites.powerup[0] = sprite_grab(0, 49, 9, 12);
    sprites.powerup[1] = sprite_grab(10, 49, 9, 12);
    sprites.powerup[2] = sprite_grab(20, 49, 9, 12);
    sprites.powerup[3] = sprite_grab(30, 49, 9, 12);
}

void sprites_deinit()
{
    al_destroy_bitmap(sprites.shipFull);
    al_destroy_bitmap(sprites.shield);

    al_destroy_bitmap(sprites.ship_mask);

    for (int i = 0; i < 2; i++) {
        al_destroy_bitmap(sprites.ship_shot_mask[i]);
    }

    al_destroy_bitmap(sprites.ship_shot[0]);
    al_destroy_bitmap(sprites.ship_shot[1]);

    al_destroy_bitmap(sprites.alien[0]);
    al_destroy_bitmap(sprites.alien[1]);
    al_destroy_bitmap(sprites.alien[2]);
    al_destroy_bitmap(sprites.alien[3]);
    al_destroy_bitmap(sprites.alien[4]);
    al_destroy_bitmap(sprites.alien[5]);
    al_destroy_bitmap(sprites.alien[6]);

    for (int i = 0; i < ALIEN_TYPE_N; i++) {
        al_destroy_bitmap(sprites.alien_mask[i]);
    }

    for(int i = 0; i < 4; i++)
    {
        al_destroy_bitmap(sprites.MINE[i]);
    }


    al_destroy_bitmap(sprites.alien_shot_mask[0]);
    al_destroy_bitmap(sprites.alien_shot_mask[1]);
    al_destroy_bitmap(sprites.alien_shot_mask[2]);
    al_destroy_bitmap(sprites.alien_shot_mask[3]);
    al_destroy_bitmap(sprites.alien_shot_mask[4]);

    al_destroy_bitmap(sprites.alien_shot[1]);
    al_destroy_bitmap(sprites.alien_shot[2]);
    al_destroy_bitmap(sprites.alien_shot[3]);
    al_destroy_bitmap(sprites.alien_shot[4]);

    al_destroy_bitmap(sprites.sparks[0]);
    al_destroy_bitmap(sprites.sparks[1]);
    al_destroy_bitmap(sprites.sparks[2]);

    al_destroy_bitmap(sprites.explosion[0]);
    al_destroy_bitmap(sprites.explosion[1]);
    al_destroy_bitmap(sprites.explosion[2]);
    al_destroy_bitmap(sprites.explosion[3]);

    al_destroy_bitmap(sprites.powerup[0]);
    al_destroy_bitmap(sprites.powerup[1]);
    al_destroy_bitmap(sprites.powerup[2]);
    al_destroy_bitmap(sprites.powerup[3]);

    al_destroy_bitmap(sprites._sheet);
}


ALLEGRO_SAMPLE* sample_shot;
ALLEGRO_SAMPLE* sample_gun;
ALLEGRO_SAMPLE* sample_health;
ALLEGRO_SAMPLE* sample_shield;
ALLEGRO_SAMPLE* sample_explode[2];
ALLEGRO_SAMPLE* sample_gameMusic;

void audio_init()
{
    al_install_audio();
    al_init_acodec_addon();
    al_reserve_samples(150);

    sample_shot = al_load_sample("audio\\shot.flac");
    must_init(sample_shot, "shot sample");

    sample_explode[0] = al_load_sample("audio\\explode1.flac");
    must_init(sample_explode[0], "explode[0] sample");
    sample_explode[1] = al_load_sample("audio\\explode2.flac");
    must_init(sample_explode[1], "explode[1] sample");

    sample_gun = al_load_sample("audio\\gun.ogg");
    must_init(sample_shot, "gun sample");
    sample_shield = al_load_sample("audio\\shield.ogg");
    must_init(sample_shot, "sheild sample");
    sample_health = al_load_sample("audio\\health.ogg");
    must_init(sample_shot, "health sample");

    sample_gameMusic = al_load_sample("audio\\gameMusic.ogg");
    must_init(sample_gameMusic, "game Music sample");
}


void audio_deinit()
{
    al_destroy_sample(sample_shot);
    al_destroy_sample(sample_explode[0]);
    al_destroy_sample(sample_explode[1]);
    al_destroy_sample(sample_gun);
    al_destroy_sample(sample_shield);
    al_destroy_sample(sample_health);
    al_destroy_sample(sample_gameMusic);
}

typedef struct FX
{
    int x, y;
    int frame;
    bool spark;
    bool used;
} FX;

#define FX_N 128
FX fx[FX_N];

void fx_init()
{
    for(int i = 0; i < FX_N; i++)
        fx[i].used = false;
}

void fx_add(bool spark, int x, int y)
{
    if(!spark && fxPlay)
        al_play_sample(sample_explode[between(0, 2)], 0.75, 0, 1, ALLEGRO_PLAYMODE_ONCE, NULL);

    for(int i = 0; i < FX_N; i++)
    {
        if(fx[i].used)
            continue;

        fx[i].x = x;
        fx[i].y = y;
        fx[i].frame = 0;
        fx[i].spark = spark;
        fx[i].used = true;
        return;
    }
}

void fx_update()
{
    for(int i = 0; i < FX_N; i++)
    {
        if(!fx[i].used)
            continue;

        fx[i].frame++;

        if((!fx[i].spark && (fx[i].frame == (EXPLOSION_FRAMES * 2)))
        || ( fx[i].spark && (fx[i].frame == (SPARKS_FRAMES * 2)))
        )
            fx[i].used = false;
    }
}

void fx_draw()
{
    for(int i = 0; i < FX_N; i++)
    {
        if(!fx[i].used)
            continue;

        int frame_display = fx[i].frame / 2;
        ALLEGRO_BITMAP* bmp =
            fx[i].spark
            ? sprites.sparks[frame_display]
            : sprites.explosion[frame_display]
        ;

        int x = fx[i].x - (al_get_bitmap_width(bmp) / 2);
        int y = fx[i].y - (al_get_bitmap_height(bmp) / 2);
        al_draw_bitmap(bmp, x, y, 0);
    }
}


typedef struct SHOT
{
    int x, y, dx, dy;
    int frame;
    int type;
    int shot_power;
    bool ship;
    bool used;
} SHOT;

#define SHOTS_N 128
SHOT shots[SHOTS_N];

void shots_init()
{
    for(int i = 0; i < SHOTS_N; i++)
    {
        shots[i].used = false;
        shots[i].type = -1;
    }
}

bool shots_add(bool ship, bool straight, int x, int y, int type, int shot_power)
{
    if(fxPlay)
    {
        al_play_sample(
            sample_shot,
            0.3,
            0,
            ship ? 1.0 : between_f(1.5, 1.6),
            ALLEGRO_PLAYMODE_ONCE,
            NULL
        );
    }

    for(int i = 0; i < SHOTS_N; i++)
    {
        if(shots[i].used)
            continue;

        shots[i].ship = ship;
        shots[i].type = type;
        shots[i].shot_power = shot_power;


        if(ship)
        {
            shots[i].x = x - (SHIP_SHOT_W / 2);
            shots[i].y = y;
        }
        else if(type == ALIEN_TYPE_CARGO_SHIP)
        {
            shots[i].x = x - (ALIEN_SHOT_W / 2);
            shots[i].y = y - (ALIEN_SHOT_H / 2);

            shots[i].dx = 0;
            shots[i].dy = 0;

            int cargo = between(0, 3);
            if(cargo == 0)
            {
                shots[i].shot_power = -1;
            }
            if(cargo == 1)
            {
                shots[i].shot_power = -2;
            }
            if(cargo == 2)
            {
                shots[i].shot_power = -3;
            }
        }
        else // alien
        {
            shots[i].x = x - (ALIEN_SHOT_W / 2);
            shots[i].y = y - (ALIEN_SHOT_H / 2);

            if(straight)
            {
                shots[i].dx = 0;
                shots[i].dy = 2;
            }
            else
            {

                shots[i].dx = between(-2, 2);
                shots[i].dy = between(-2, 2);
            }

            if(type == ALIEN_TYPE_WARRIOR)
            {
                shots[i].dy = 3;
            }

            // if the shot has no speed, don't bother
            if(!shots[i].dx && !shots[i].dy)
                return true;

            shots[i].frame = 0;
        }

        shots[i].frame = 0;
        shots[i].used = true;

        return true;
    }
    return false;
}

void shots_update()
{
    for(int i = 0; i < SHOTS_N; i++)
    {
        if(!shots[i].used)
            continue;

        if(shots[i].ship)
        {
            shots[i].y -= 5;

            if(shots[i].y < -SHIP_SHOT_H)
            {
                shots[i].used = false;
                continue;
            }
        }
        else // alien
        {
            shots[i].x += shots[i].dx;
            shots[i].y += shots[i].dy;

            if((shots[i].x < -ALIEN_SHOT_W)
            || (shots[i].x > BUFFER_W)
            || (shots[i].y < -ALIEN_SHOT_H)
            || (shots[i].y > BUFFER_H)
            ) {
                shots[i].used = false;
                continue;
            }
        }

        shots[i].frame++;
    }
}


bool ship_collide(int x, int y, int w, int h);
bool check_pixel_collision(int x1, int y1, ALLEGRO_BITMAP *mask1, int x2, int y2, ALLEGRO_BITMAP *mask2);
bool checkCircleCollision(float x1, float y1, float r1, float x2, float y2, float r2);

int shots_collide(bool ships, int x, int y, int w, int h, int num)
{
    for(int i = 0; i < SHOTS_N; i++)
    {
        if(!shots[i].used)
            continue;

        // don't collide with one's own shots
        if(shots[i].ship == ships)
            continue;

        int sw, sh;
        if(ships)
        {
            if(shots[i].type == ALIEN_TYPE_BOMBER)
            {
                sw = ALIEN_SHOT_BOMBER_W;
                sh = ALIEN_SHOT_BOMBER_H;
            }
            else if(shots[i].type == ALIEN_TYPE_CARGO_SHIP)
            {
                sw = ALIEN_CARGO_SHIP_HEALTH_W;
                sh = ALIEN_CARGO_SHIP_HEALTH_H;
            }
            else
            {
                sw = ALIEN_SHOT_W;
                sh = ALIEN_SHOT_H;
            }
        }
        else
        {
            sw = SHIP_SHOT_W;
            sh = SHIP_SHOT_H;
        }


        if(ship.shield_timer != 0 && ships)
        {
            if(shots[i].shot_power != -3 || shots[i].shot_power != -2 || shots[i].shot_power != -1)
            {
                if(checkCircleCollision(ship.x + SHIP_W/2, ship.y + SHIP_H/2, 32, shots[i].x + ALIEN_SHOT_W/2, shots[i].y + ALIEN_SHOT_H/2, ((ALIEN_SHOT_W > ALIEN_SHOT_H) ? ALIEN_SHOT_W : ALIEN_SHOT_H)/2))
                {
                    fx_add(true, shots[i].x + (sw / 2), shots[i].y + (sh / 2));
                    shots[i].used = false;
                    return shots[i].shot_power;
                }
            }
        }


        if(collide(x, y, x+w, y+h, shots[i].x, shots[i].y, shots[i].x+sw, shots[i].y+sh))
        {
            if(ships)
            {
                if (check_pixel_collision(x, y, sprites.ship_mask, shots[i].x, shots[i].y, sprites.alien_shot_mask[0])
                    || check_pixel_collision(x, y, sprites.ship_mask, shots[i].x, shots[i].y, sprites.alien_shot_mask[1])
                    || check_pixel_collision(x, y, sprites.ship_mask, shots[i].x, shots[i].y, sprites.alien_shot_mask[2])
                    || check_pixel_collision(x, y, sprites.ship_mask, shots[i].x, shots[i].y, sprites.alien_shot_mask[3])
                    || check_pixel_collision(x, y, sprites.ship_mask, shots[i].x, shots[i].y, sprites.alien_shot_mask[4]))
                {
                    fx_add(true, shots[i].x + (sw / 2), shots[i].y + (sh / 2));
                    shots[i].used = false;
                    return shots[i].shot_power;
                }
            }

            else
            {
                if (check_pixel_collision(x, y, sprites.alien_mask[num], shots[i].x, shots[i].y, sprites.ship_shot[1]) || check_pixel_collision(x, y, sprites.alien_mask[num], shots[i].x, shots[i].y, sprites.ship_shot[0]))
                {
                    fx_add(true, shots[i].x + (sw / 2), shots[i].y + (sh / 2));
                    shots[i].used = false;
                    return shots[i].shot_power;
                }
            }
        }
    }

    return 0;
}


ALLEGRO_BITMAP* create_collision_mask(ALLEGRO_BITMAP* sprite) {
    int width = al_get_bitmap_width(sprite);
    int height = al_get_bitmap_height(sprite);
    ALLEGRO_BITMAP* mask = al_create_bitmap(width, height);

    al_set_target_bitmap(mask);
    al_clear_to_color(al_map_rgba(0, 0, 0, 0)); // Clear mask to fully transparent

    al_lock_bitmap(sprite, ALLEGRO_PIXEL_FORMAT_ARGB_8888, ALLEGRO_LOCK_READONLY);
    al_lock_bitmap(mask, ALLEGRO_PIXEL_FORMAT_ARGB_8888, ALLEGRO_LOCK_WRITEONLY);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            ALLEGRO_COLOR color = al_get_pixel(sprite, x, y);
            if (color.a > 0.5)  // Adjust threshold based on your sprites
                al_put_pixel(x, y, al_map_rgb(255, 255, 255)); // Mark as solid
        }
    }

    al_unlock_bitmap(sprite);
    al_unlock_bitmap(mask);
    al_set_target_bitmap(al_get_backbuffer(disp)); // Reset target bitmap

    return mask;
}

bool check_pixel_collision(int x1, int y1, ALLEGRO_BITMAP *mask1, int x2, int y2, ALLEGRO_BITMAP *mask2) {
    int w1 = al_get_bitmap_width(mask1);
    int h1 = al_get_bitmap_height(mask1);
    int w2 = al_get_bitmap_width(mask2);
    int h2 = al_get_bitmap_height(mask2);

    // Calculate the intersection rectangle
    int x_start = fmax(x1, x2);
    int y_start = fmax(y1, y2);
    int x_end = fmin(x1 + w1, x2 + w2);
    int y_end = fmin(y1 + h1, y2 + h2);

    for (int y = y_start; y < y_end; y++) {
        for (int x = x_start; x < x_end; x++) {
            // Get mask pixel colors at current position
            ALLEGRO_COLOR color1 = al_get_pixel(mask1, x - x1, y - y1);
            ALLEGRO_COLOR color2 = al_get_pixel(mask2, x - x2, y - y2);

            // Check if both pixels are solid (non-transparent)
            if (color1.a != 0 && color2.a != 0) {
                return true; // Collision detected
            }
        }
    }

    return false; // No collision
}

bool checkCircleCollision(float x1, float y1, float r1, float x2, float y2, float r2)
{
   float dx = x1-x2;
   float dy = y1-y2;
   float dist = sqrt(dx*dx + dy*dy);
   return dist < (r1+r2);
}

void shots_draw()
{
    for(int i = 0; i < SHOTS_N; i++)
    {
        if(!shots[i].used)
            continue;

        int frame_display = (shots[i].frame / 2) % 2;

        if(shots[i].ship)
            al_draw_bitmap(sprites.ship_shot[frame_display], shots[i].x, shots[i].y, 0);
        else if(shots[i].type == ALIEN_TYPE_BOMBER)
        {
            al_draw_bitmap(sprites.alien_shot[1], shots[i].x, shots[i].y, 0);
        }
        else if(shots[i].type == ALIEN_TYPE_CARGO_SHIP && shots[i].shot_power == -1)
        {
            al_draw_bitmap_region(sprites.alien_shot[2], (frames % 15) * 32 + 0, 0, 32, 32, shots[i].x, shots[i].y, 0);
        }
        else if(shots[i].type == ALIEN_TYPE_CARGO_SHIP && shots[i].shot_power == -2)
        {
            al_draw_bitmap_region(sprites.alien_shot[3], (frames % 15) * 32 + 0, 0, 32, 32, shots[i].x, shots[i].y, 0);
        }
        else if(shots[i].type == ALIEN_TYPE_CARGO_SHIP && shots[i].shot_power == -3)
        {
            al_draw_bitmap_region(sprites.alien_shot[4], (frames % 15) * 32 + 0, 0, 32, 32, shots[i].x, shots[i].y, 0);
        }
        else // alien
        {
            ALLEGRO_COLOR tint =
                frame_display
                ? al_map_rgb_f(1, 1, 1)
                : al_map_rgb_f(0.5, 0.5, 0.5)
            ;
            al_draw_tinted_bitmap(sprites.alien_shot[0], tint, shots[i].x, shots[i].y, 0);
        }
    }
}


void ship_init()
{
    ship.x = (BUFFER_W / 2) - (SHIP_W / 2);
    ship.y = (BUFFER_H) - (SHIP_H / 2);
    ship.shot_timer = 0;
    ship.lives = SHIP_HEALTH;
    ship.respawn_timer = 0;
    ship.invincible_timer = 120;
    ship.ship_shot_power = 2;
    ship.shield_timer = 0;
}

void ship_update()
{
    /*if(ship.ship_shot_power == 0)
        ship.lives = -1;*/

    if(ship.lives < 0)
        return;

    if(ship.respawn_timer)
    {
        ship.respawn_timer--;
        return;
    }

    if(key[ALLEGRO_KEY_LEFT])
        ship.x -= SHIP_SPEED;
    if(key[ALLEGRO_KEY_RIGHT])
        ship.x += SHIP_SPEED;
    if(key[ALLEGRO_KEY_UP])
        ship.y -= SHIP_SPEED;
    if(key[ALLEGRO_KEY_DOWN])
        ship.y += SHIP_SPEED;

    if(ship.x < 0)
        ship.x = 0;
    if(ship.y < 0)
        ship.y = 0;

    if(ship.x > SHIP_MAX_X)
        ship.x = SHIP_MAX_X;
    if(ship.y > SHIP_MAX_Y)
        ship.y = SHIP_MAX_Y;

    int damage = 0;

    if(ship.invincible_timer)
        ship.invincible_timer--;
    else if(ship_collide(ship.x, ship.y, SHIP_W, SHIP_H))
    {
        if(ship.shield_timer == 0)
        {
            int x = ship.x + (SHIP_W / 2);
            int y = ship.y + (SHIP_H / 2);
            fx_add(false, x, y);
            fx_add(false, x+4, y+2);
            fx_add(false, x-2, y-4);
            fx_add(false, x+1, y-5);

            ship.lives--;
            if(ship.ship_shot_power > 1)
                ship.ship_shot_power--;

            ship.respawn_timer = 90;
            ship.invincible_timer = 180;
        }
    }
    else if(damage = shots_collide(true, ship.x, ship.y, SHIP_W, SHIP_H, 0))
    {
        if(ship.shield_timer == 0)
        {
            if(damage > 0)
            {
                int x = ship.x + (SHIP_W / 2);
                int y = ship.y + (SHIP_H / 2);
                fx_add(false, x, y);
                fx_add(false, x+4, y+2);
                fx_add(false, x-2, y-4);
                fx_add(false, x+1, y-5);

                    ship.lives -= damage;
                if(ship.ship_shot_power > 1)
                    ship.ship_shot_power--;
                ship.respawn_timer = 90;
                ship.invincible_timer = 180;
            }
            else if(damage == -1)
            {
                if(fxPlay)
                {
                    al_play_sample(
                        sample_health,
                        1.0,
                        0,
                        1.0,
                        ALLEGRO_PLAYMODE_ONCE,
                        NULL
                    );
                }
                if(ship.lives < 20)
                    ship.lives -= damage;
                ship.invincible_timer = 30;
            }
            else if(damage == -2)
            {
                if(fxPlay)
                {
                    al_play_sample(
                        sample_gun,
                        1.0,
                        0,
                        1.0,
                        ALLEGRO_PLAYMODE_ONCE,
                        NULL
                    );
                }
                if(ship.ship_shot_power < 4)
                    ship.ship_shot_power++;
            }
            else if(damage == -3)
            {
                if(fxPlay)
                {
                    al_play_sample(
                        sample_shield,
                        1.0,
                        0,
                        1.0,
                        ALLEGRO_PLAYMODE_ONCE,
                        NULL
                    );
                }
                ship.shield_timer = 600;
            }
        }

    }

    if(ship.shield_timer != 0)
        ship.shield_timer--;

    if(ship.shot_timer)
        ship.shot_timer--;
    else if(key[ALLEGRO_KEY_SPACE])
    {
        int x = ship.x + (SHIP_W / 2);
        if(shots_add(true, false, x, ship.y, -1, ship.ship_shot_power))
            ship.shot_timer = 5;
    }
}

void ship_draw()
{
    if(ship.lives < 0)
        return;
    if(ship.respawn_timer)
        return;
    if(((ship.invincible_timer / 2) % 3) == 1)
        return;

    if((ship.lives) >= 0 && (ship.ship_shot_power) >= 0)
        {
        int orig_width = al_get_bitmap_width(sprites.shipFull);
        int orig_height = al_get_bitmap_height(sprites.shipFull);

        int new_width = orig_width * 1;
        int new_height = orig_height * 1;

        al_draw_scaled_bitmap(sprites.shipFull,
                      0, 0,
                      orig_width, orig_height,
                      ship.x, ship.y,
                      new_width, new_height,
                      0);
        }
        if(ship.shield_timer != 0)
            al_draw_bitmap_region(sprites.shield, (frames % 15) * 64 + 0, 0, 64, 64, (ship.x + SHIP_W/2) - 32, (ship.y + SHIP_H/2) - 32, 0);
}


void aliens_init()
{
    for(int i = 0; i < ALIENS_N; i++)
        aliens[i].used = false;
}

bool ship_collide(int x, int y, int w, int h)
{
    for(int i = 0; i < ALIENS_N; i++)
    {
        if(!aliens[i].used)
            continue;

        /*if(aliens[i].type == HELP)
            continue;*/

        int sw, sh;
        sw = ALIEN_W[aliens[i].type];
        sh = ALIEN_H[aliens[i].type];

        if(ship.shield_timer != 0)
        {
            if(checkCircleCollision(ship.x + SHIP_W/2, ship.y + SHIP_H/2, 32, aliens[i].x + ALIEN_W[aliens[i].type]/2, aliens[i].y + ALIEN_H[aliens[i].type]/2, ((ALIEN_W[aliens[i].type] > ALIEN_H[aliens[i].type]) ? ALIEN_W[aliens[i].type] : ALIEN_H[aliens[i].type])/2))
                {
                    fx_add(false, (aliens[i].x + x)/2, (aliens[i].y + y)/2);
                    if(aliens[i].type == ALIEN_TYPE_MINE)
                        aliens[i].used = false;
                    else if(aliens[i].type == ALIEN_TYPE_CARGO_SHIP)
                    {
                        shots_add(false, false, aliens[i].x + ALIEN_W[aliens[i].type], aliens[i].y + ALIEN_H[aliens[i].type], aliens[i].type, -1);
                        aliens[i].used = false;
                    }
                    else if(aliens[i].type == ALIEN_TYPE_DRONE)
                    {
                        aliens[i].life = 0;
                        aliens[i].used = false;
                        drone_cnt--;
                    }
                    else
                        aliens[i].life = 0;

                    return true;
                }
        }

        else if(collide(x, y, x + w, y + h, aliens[i].x, aliens[i].y, aliens[i].x + sw, aliens[i].y + sh))                                                                    //collide(x, y, x+(w*DISP_SCALE)-8, y+(h*DISP_SCALE)-8, aliens[i].x, aliens[i].y, aliens[i].x+sw, aliens[i].y+sh)
        {
            if (check_pixel_collision(x, y, sprites.ship_mask, aliens[i].x, aliens[i].y, sprites.alien_mask[aliens[i].type])) {
                fx_add(false, (aliens[i].x + x)/2, (aliens[i].y + y)/2);
                if(aliens[i].type == ALIEN_TYPE_MINE)
                    aliens[i].used = false;
                else if(aliens[i].type == ALIEN_TYPE_CARGO_SHIP)
                {
                    shots_add(false, false, aliens[i].x + ALIEN_W[aliens[i].type], aliens[i].y + ALIEN_H[aliens[i].type], aliens[i].type, -1);
                    aliens[i].used = false;
                }
                else if(aliens[i].type == ALIEN_TYPE_DRONE)
                {
                    aliens[i].life = 0;
                    aliens[i].used = false;
                    drone_cnt--;
                }
                else
                    aliens[i].life = 0;

                return true;
            }
        }
    }
    return false;
}


void aliens_update()
{
    int new_quota =
        (frames % 180)
        ? 0
        : between(6, 8)
    ;
    int new_x = between(10, BUFFER_W-50);

    for(int i = 0; i < ALIENS_N; i++)
    {
        if(!aliens[i].used)
        {
            // if this alien is unused, should it spawn?
            if(new_quota > 0)
            {
                new_x += between(40, 300);
                if(new_x > (BUFFER_W - 60))
                    new_x -= (BUFFER_W - 60);


                aliens[i].x = new_x;
                aliens[i].y = between(-250, -200);
                aliens[i].type = (ALIEN_TYPE)between(0, ALIEN_TYPE_N);

                if(drone_cnt != 0)
                {
                   while(aliens[i].type == ALIEN_TYPE_DRONE)
                        aliens[i].type = (ALIEN_TYPE)between(0, ALIEN_TYPE_N);

                    if(aliens[i].type == ALIEN_TYPE_CARGO_SHIP)
                        cargo_cnt++;
                }

                if((aliens[i].type == ALIEN_TYPE_CARGO_SHIP) || (aliens[i].type == ALIEN_TYPE_DRONE))
                {
                    if(aliens[i].type == ALIEN_TYPE_CARGO_SHIP)
                        cargo_cnt++;
                    else if(aliens[i].type == ALIEN_TYPE_DRONE)
                    {
                        drone_cnt++;
                    }
                }


                if(cargo_cnt > 2 && aliens[i].type == ALIEN_TYPE_CARGO_SHIP)
                {
                    while(aliens[i].type == ALIEN_TYPE_CARGO_SHIP || aliens[i].type == ALIEN_TYPE_DRONE)
                        aliens[i].type = (ALIEN_TYPE)between(0, ALIEN_TYPE_N);
                    cargo_cnt--;
                }
                if((frames % 900) == 0)
                {
                    cargo_cnt = 0;
                }



                aliens[i].shot_timer = between(1, 79);
                aliens[i].blink = 0;
                aliens[i].used = true;

                switch(aliens[i].type)
                {
                    case ALIEN_TYPE_BUG:
                        aliens[i].life = 4;
                        aliens[i].shot_power = 1;
                        break;
                    case ALIEN_TYPE_WARRIOR:
                        aliens[i].life = 2;
                        aliens[i].shot_power = 1;
                        break;
                    case ALIEN_TYPE_THICCBOI:
                        aliens[i].life = 12;
                        aliens[i].shot_power = 2;
                        break;
                    case ALIEN_TYPE_MINE:
                        aliens[i].life = INT_MAX;
                        aliens[i].shot_power = 3;
                        break;
                    case ALIEN_TYPE_BOMBER:
                        if(between(0, 2))
                        {
                            aliens[i].RorL = 0;
                            aliens[i].x = 0;
                            aliens[i].y = between(0, 50);
                        }
                        else
                        {
                            aliens[i].RorL = 1;
                            aliens[i].x = BUFFER_W;
                            aliens[i].y = between(0, 50);
                        }
                        aliens[i].life = 8;
                        aliens[i].shot_power = 3;
                        aliens[i].shot_timer = 1;
                        break;
                    case ALIEN_TYPE_CARGO_SHIP:
                        aliens[i].life = 1;
                        aliens[i].shot_power = 0;
                        break;
                    case ALIEN_TYPE_DRONE:
                        aliens[i].life = 1;
                        aliens[i].shot_power = 0;
                        break;
                }

                new_quota--;
            }
            continue;
        }

        switch(aliens[i].type)
        {
            case ALIEN_TYPE_BUG:
                if(frames % 2)
                {
                    aliens[i].y++;
                }
                break;

            case ALIEN_TYPE_WARRIOR:
                aliens[i].y++;
                break;

            case ALIEN_TYPE_THICCBOI:
                if(!(frames % 4))
                    aliens[i].y++;
                break;
            case ALIEN_TYPE_MINE:
                aliens[i].y += 2;
                break;
            case ALIEN_TYPE_BOMBER:
                if(aliens[i].RorL == 0)
                {
                    if((frames % 3) == 0)
                    {
                        aliens[i].y++;
                    }
                    if((frames % 4) != 0)
                    {
                        aliens[i].x++;
                    }
                }
                else
                {
                    if((frames % 3) == 0)
                    {
                        aliens[i].y++;
                    }
                    if((frames % 4) != 0)
                    {
                        aliens[i].x -= 2;
                    }
                }
                break;
            case ALIEN_TYPE_CARGO_SHIP:
                if(frames % 2)
                {
                    aliens[i].y++;
                }
                break;
            case ALIEN_TYPE_DRONE:
                int dx = (ship.x + SHIP_W/2) - aliens[i].x;
                int dy = (ship.y + SHIP_H/2) - aliens[i].y;
                if(dx > 0)
                    aliens[i].x++;
                else
                    aliens[i].x--;

                if(dy > 0)
                    aliens[i].y++;
                else
                    aliens[i].y--;
                break;
        }

        if(aliens[i].y >= BUFFER_H)
        {
            aliens[i].used = false;
            continue;
        }

        if(aliens[i].x >= BUFFER_W)
        {
            if(aliens[i].type != ALIEN_TYPE_DRONE)
                aliens[i].used = false;

            continue;
        }

        if(aliens[i].blink)
            aliens[i].blink--;

        if(shots_collide(false, aliens[i].x, aliens[i].y, ALIEN_W[aliens[i].type], ALIEN_H[aliens[i].type], aliens[i].type))
        {
            aliens[i].life -= ship.ship_shot_power;
            aliens[i].blink = 4;

            if(aliens[i].type == ALIEN_TYPE_CARGO_SHIP)
            {
                shots_add(false, false, aliens[i].x + ALIEN_W[aliens[i].type], aliens[i].y + ALIEN_H[aliens[i].type], aliens[i].type, -1);
            }
            else if(aliens[i].type == ALIEN_TYPE_DRONE)
            {
                aliens[i].used = false;
                drone_cnt--;
            }
        }

        int cx = aliens[i].x + (ALIEN_W[aliens[i].type] / 2);
        int cy = aliens[i].y + (ALIEN_H[aliens[i].type] / 2);

        if(aliens[i].life <= 0)
        {
            fx_add(false, cx, cy);

            switch(aliens[i].type)
            {
                case ALIEN_TYPE_BUG:
                    score += 200;
                    break;

                case ALIEN_TYPE_WARRIOR:
                    score += 150;
                    break;

                case ALIEN_TYPE_THICCBOI:
                    score += 800;
                    fx_add(false, cx-10, cy-4);
                    fx_add(false, cx+4, cy+10);
                    fx_add(false, cx+8, cy+8);
                    break;
                case ALIEN_TYPE_MINE:
                    fx_add(false, cx+3, cy+3);
                    fx_add(false, cx-5, cy-3);
                    break;
                case ALIEN_TYPE_BOMBER:
                    score += 500;
                    fx_add(false, cx, cy);
                    fx_add(false, cx+4, cy-3);
                    break;
                case ALIEN_TYPE_CARGO_SHIP:
                    score += 0;
                    fx_add(false, cx, cy);
                    break;
                case ALIEN_TYPE_DRONE:
                    score += 100;
                    fx_add(false, cx, cy);
                    break;
            }

            aliens[i].used = false;
            continue;
        }

        aliens[i].shot_timer--;
        if(aliens[i].shot_timer == 0)
        {
            switch(aliens[i].type)
            {
                case ALIEN_TYPE_BUG:
                    shots_add(false, false, cx, cy, ALIEN_TYPE_BUG, aliens[i].shot_power);
                    aliens[i].shot_timer = 150;
                    break;
                case ALIEN_TYPE_WARRIOR:
                    shots_add(false, true, cx, aliens[i].y, ALIEN_TYPE_WARRIOR, aliens[i].shot_power);
                    aliens[i].shot_timer = 70;
                    break;
                case ALIEN_TYPE_THICCBOI:
                    shots_add(false, true, cx-5, cy, ALIEN_TYPE_THICCBOI, aliens[i].shot_power);
                    shots_add(false, true, cx+5, cy, ALIEN_TYPE_THICCBOI, aliens[i].shot_power);
                    shots_add(false, true, cx-5, cy + 8, ALIEN_TYPE_THICCBOI, aliens[i].shot_power);
                    shots_add(false, true, cx+5, cy + 8, ALIEN_TYPE_THICCBOI, aliens[i].shot_power);
                    aliens[i].shot_timer = 200;
                    break;
                case ALIEN_TYPE_MINE:
                    aliens[i].shot_timer = INT_MAX;
                    break;
                case ALIEN_TYPE_BOMBER:
                    shots_add(false, true, cx, cy, aliens[i].type, aliens[i].shot_power);
                    aliens[i].shot_timer = ((aliens[i].RorL) ? 85 : 120);
                    break;
                case ALIEN_TYPE_CARGO_SHIP:
                    break;
                case ALIEN_TYPE_DRONE:
                    break;
            }
        }
    }
}

void aliens_draw()
{
    for(int i = 0; i < ALIENS_N; i++)
    {
        if(!aliens[i].used)
            continue;
        if(aliens[i].blink > 2)
            continue;


        if(aliens[i].type == ALIEN_TYPE_MINE)
        {
            if((frames % 60) < 15)
                sprites.alien[aliens[i].type] = sprites.MINE[0];
            else if((frames % 60) < 30)
                sprites.alien[aliens[i].type] = sprites.MINE[1];
            else if((frames % 60) < 45)
                sprites.alien[aliens[i].type] = sprites.MINE[2];
            else
                sprites.alien[aliens[i].type] = sprites.MINE[3];
        }
        if(aliens[i].type == ALIEN_TYPE_DRONE)
            al_draw_rotated_bitmap(sprites.alien[aliens[i].type], ALIEN_DRONE_W/2, ALIEN_DRONE_H/2, aliens[i].x+5, aliens[i].y+5, calcDegree(ship.x + SHIP_W/2, ship.y + SHIP_H/2, aliens[i].x + ALIEN_W[6], aliens[i].y + ALIEN_H[6]), 0);
        else
            al_draw_bitmap(sprites.alien[aliens[i].type], aliens[i].x, aliens[i].y, 0);
    }
}



typedef struct STAR
{
    float y;
    float speed;
} STAR;

#define STARS_N ((BUFFER_W / 2) - 1)
STAR stars[STARS_N];

void stars_init()
{
    for(int i = 0; i < STARS_N; i++)
    {
        stars[i].y = between_f(0, BUFFER_H);
        stars[i].speed = between_f(0.1, 1);
    }
}

void stars_update()
{
    for(int i = 0; i < STARS_N; i++)
    {
        stars[i].y += stars[i].speed;
        if(stars[i].y >= BUFFER_H)
        {
            stars[i].y = 0;
            stars[i].speed = between_f(0.1, 2.0);
        }
    }
}

void stars_draw()
{
    float star_x = 1.5;
    for(int i = 0; i < STARS_N; i++)
    {
        float l = stars[i].speed * 0.8;
        al_draw_pixel(star_x, stars[i].y, al_map_rgb_f(l,l,l));
        star_x += 2;
    }
}

ALLEGRO_FONT* font;
long score_display;

void hud_init()
{
    font = al_create_builtin_font();
    must_init(font, "font");

    score_display = 0;
}

void hud_deinit()
{
    al_destroy_font(font);
}

void hud_update()
{
    if(frames % 2)
        return;

    for(long i = 5; i > 0; i--)
    {
        long diff = 1 << i;
        if(score_display <= (score - diff))
            score_display += diff;
    }
}

bool hud_draw()
{
    al_draw_textf(
        font,
        al_map_rgb_f(1,1,1),
        1, 1,
        0,
        "%07ld",
        score_display
    );

    int spacing = LIFE_W + 1;
    for(int i = 0; i < ship.lives; i++)
        al_draw_bitmap(sprites.life, 1 + (i * spacing), 10, 0);

    al_draw_textf(
    font,
    al_map_rgb_f(1,1,1),
    1, 17,
    0,
    "GUN lvl %d",
    ship.ship_shot_power);

    if(ship.lives < 0)
    {
        al_draw_text(
            font,
            al_map_rgb_f(1,1,1),
            BUFFER_W / 2, BUFFER_H / 2,
            ALLEGRO_ALIGN_CENTER,
            "G A M E  O V E R"
        );
        gameOverStopTime++;
        if(gameOverStopTime > 360)
        return true;
    }

    return false;
}


int main()
{

    loadRecords();

    must_init(al_init(), "allegro");
    must_init(al_install_keyboard(), "keyboard");

    const float FPS = 60.0;
    bool gameRunning = false;
    int choice = 0;


    ALLEGRO_TIMER* timer = al_create_timer(1.0 / FPS);
    must_init(timer, "timer");

    ALLEGRO_EVENT_QUEUE* queue = al_create_event_queue();
    must_init(queue, "queue");

    disp_init();

    audio_init();

    al_init_font_addon();
    al_init_ttf_addon();

    must_init(al_init_image_addon(), "image");
    sprites_init();

    hud_init();

    must_init(al_init_primitives_addon(), "primitives");

    must_init(al_install_audio(), "audio");
    must_init(al_init_acodec_addon(), "audio codecs");
    must_init(al_reserve_samples(16), "reserve samples");

    ALLEGRO_BITMAP* starter = al_load_bitmap("graphic\\Starter.jpg");
    al_draw_bitmap(starter, 0, 0, 0);
    al_flip_display();

    al_rest(5.0);

    al_register_event_source(queue, al_get_keyboard_event_source());
    al_register_event_source(queue, al_get_display_event_source(disp));
    al_register_event_source(queue, al_get_timer_event_source(timer));

    while(!gameRunning)
    {
        keyboard_init();

        frames = 0;
        int starter_time = 180;
        int cnt = 0;

        time_t startTime, endTime;
        double elapsedTime;

        bool done = false;
        bool redraw = true;
        int playAgain = 0;
        ALLEGRO_EVENT event;

        al_start_timer(timer);

        ALLEGRO_KEYBOARD_STATE keyState;


        if(choice != 10)
            choice = showMenu(FPS, queue, keyState, timer);


        if(choice == 4)
            gameRunning = true;

        al_rest(0.5);

        if(choice == 1)
        {
            fx_init();
            shots_init();
            ship_init();
            aliens_init();
            stars_init();

            ALLEGRO_SAMPLE_ID id;
            if(gameMusic)
            {
                al_play_sample(sample_gameMusic, 0.75, 0, 1, ALLEGRO_PLAYMODE_LOOP, &id);
                isPlayingMenu = true;
            }


            startTime = time(NULL);
            score = 0;
            bool hudValue = false;
            while(1)
            {
                int breaker = 0;

                al_wait_for_event(queue, &event);

                long int currentFPS = FPS;

                if(cnt++ < starter_time)
                    {
                        continue;
                    }
                switch(event.type)
                {
                    case ALLEGRO_EVENT_TIMER:
                        hud_update();
                        fx_update();
                        stars_update();
                        ship_update();
                        aliens_update();
                        shots_update();



                        if(key[ALLEGRO_KEY_ESCAPE])
                        {
                            breaker = showMenuPause(FPS, queue, keyState, timer);
                            if(!gameMusic)
                            {
                                al_stop_sample(&id);
                                isPlayingMenu = false;
                            }
                            else if(!isPlayingMenu)
                            {
                                al_play_sample(sample_gameMusic, 0.75, 0, 1, ALLEGRO_PLAYMODE_LOOP, &id);
                                isPlayingMenu = true;
                            }

                            key[ALLEGRO_KEY_ESCAPE] = 0;
                        }

                            redraw = true;
                            frames++;
                            break;

                    case ALLEGRO_EVENT_DISPLAY_CLOSE:
                        done = true;
                        break;
                }

                if(breaker == 3)
                    break;

                if(done)
                    break;

                if((frames % currentFPS) == 0)
                    score += SHIP_SPEED;

                keyboard_update(&event);

                if(redraw && al_is_event_queue_empty(queue))
                {
                    disp_pre_draw();
                    al_clear_to_color(al_map_rgb(0,0,0));

                    stars_draw();
                    ship_draw();
                    shots_draw();
                    aliens_draw();
                    fx_draw();

                    hudValue = hud_draw();

                    disp_post_draw();
                    redraw = false;
                }
                if(hudValue)
                {
                    score -= 9;
                    score_display -= 9;
                    playAgain = gameOverMenu(FPS, queue, keyState, timer);
                    hudValue = false;
                }
                if(playAgain == 1)
                {

                    fx_init();
                    shots_init();
                    hud_init();
                    ship_init();
                    aliens_init();
                    stars_init();


                    startTime = time(NULL);
                    score = 0;
                    score_display = 0;
                    playAgain = 0;
                    choice = 10;
                    gameOverStopTime = 0;
                    continue;
                }
                else if(playAgain == 2)
                {
                    choice = 0;
                    break;
                }
                endTime = time(NULL);
                elapsedTime = difftime(endTime, startTime);
            }
            al_stop_sample(&id);
        }
        else if (choice == 2)
        {
            showRecord(FPS, queue, keyState, timer);
        }

        gameMusic = true;
        isPlayingMenu = false;
        score = 0;
        score_display = 0;
    }



    sprites_deinit();
    hud_deinit();
    audio_deinit();
    disp_deinit();
    al_destroy_timer(timer);
    al_destroy_event_queue(queue);

    return 0;
}
