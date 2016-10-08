#include <redbus.h>
/*
#include <console.h>
*/
#include <sortron.h>
#include <retriever.h>
#include <stdlib.h>
#include <string.h>
#include <iox.h>


#define map_scratch() rb_map_device(10);

#define CLIPBOARD_SIZE 4096

char clipboard[CLIPBOARD_SIZE];

typedef struct MMU {
    unsigned char padding;
    unsigned char mode;
    unsigned char rootpage;
    unsigned char childpage;
    unsigned int childoffset;
    unsigned int rootoffset;
    unsigned int len;
    unsigned char special;
    unsigned char crystal;
} MMU;
MMU* mmu;

typedef struct CodeLine {
    char text[160];
    unsigned int previousSector;
    unsigned int nextSector;
    unsigned int magicNumber;
    unsigned int projectid;
    unsigned int fragmentid;
} CodeLine;

typedef struct CodeMeta {
    unsigned int magicNumber;
    unsigned int maxSector;
    unsigned int lastUsedSector;
    unsigned int projectid;
    unsigned int fragmentid;
} CodeMeta;
CodeMeta* codemeta;
CodeLine* codeline;

char word[256];
    char _conv[6];
    int __i = -1;
    int wordi = 0;
    int compi = 1;
    unsigned int blx = 0;
    unsigned int bly = 0;
    unsigned int rrblx = 0;
    char comment = 0;
    char instring = 0;
    char last = ' ';
    char isNumeric = 1;
    char include = 1;
    char firstword = 1;
    char lac = 0;
    char add = 0;
    unsigned char state = 0;
    unsigned char nexttwo;
    unsigned char nexttwoinc;
    unsigned char override = 0;

    unsigned int current_line = 1;

    unsigned int downers = 0;
    
typedef enum {
	IDLE 	  	= 0,
	READ_NAME 	= 1,
	WRITE_NAME 	= 2,
	READ_SERIAL = 3,
	READ 		= 4,
	WRITE 		= 5,
	FAIL 		= 0xFF 
} DiskCommand;

typedef struct RawRB {
    unsigned char data[256];
} RawRB;

typedef struct Disk {
	char sector[0x80];
	unsigned int sector_num;
	DiskCommand command;
        unsigned char offset;
        unsigned char len;
} Disk;
typedef struct Console {
	unsigned char line;
	unsigned char cursor_x;
	unsigned char cursor_y;
	unsigned char cursor_mode;

	char kb_start;
	char kb_pos;
	char kb_key;

	char blit_mode;
	char blit_start_x;
	char blit_start_y;
	char blit_offset_x;
	char blit_offset_y;
	char blit_width;
	char blit_height;
        
        unsigned char blitclr;
        unsigned char cmd;

	char display[0xA0];
        unsigned char colors[3];
        unsigned char bgblitclr;
        unsigned char mouseX;
        unsigned char mouseY;
        unsigned char pressedX;
        unsigned char pressedY;
        unsigned char charset;
        unsigned char releasedX;
        unsigned char releasedY;
        char scrollWheel;
} Console;

//character with inverted colors
#define inv(c) ((c) | 0x80)

#define fill(b, x, y, w, h) 		blit(1, b, 0, x, y, w, h)
#define invert(x, y, w, h) 			blit(2, 0, 0, x, y, w, h)
#define scroll(x, y, ox, oy, w, h) 	blit(3, x, y, ox, oy, w, h)
#define rscroll(x, y, ox, oy, w, h) 	blit(4, x, y, ox, oy, w, h)

#define upvert(x, y, w, h) 			blit(5, 0, 0, x, y, w, h)
#define downvert(x, y, w, h) 			blit(6, 0, 0, x, y, w, h)

#define RB 0x0300
#define LSTREAM 47
#define SCREEN_W 160
#define SCREEN_H 64

#define SCREEN_WIDTH SCREEN_W
#define SCREEN_HEIGHT SCREEN_H


#define LPROMPT 63
#define LF  13

#define map_console() rb_map_device(0x01);
#define map_iox() rb_map_device(0x03);
#define map_modem() rb_map_device(0x06);
#define map_active() rb_map_device(0x08);
#define map_matrix() rb_map_device(0x09);
#define map_hdd() rb_map_device(10);
#define map_mmu() rb_map_device(0);

#define map_matrix0() rb_map_device(0x08);
#define map_matrix1() rb_map_device(0x09);
#define map_matrix2() rb_map_device(0x0A);
#define map_matrix3() rb_map_device(0x0B);

Console* console;
Iox* iox;
char buffer[160];
unsigned int ffs_readindex = 0;

Disk* disk;
char conv[16];
char clmem[256];
char pwmem[32];
char pwlen = 0;

void blit(char command, char x, char y, char xo, char yo, char w, char h) {
    console->blit_start_x = x;
    console->blit_start_y = y;
    console->blit_offset_x = xo;
    console->blit_offset_y = yo;
    console->blit_width = w;
    console->blit_height = h;
    console->blit_mode = command;
    while (console->blit_mode != 0); //WAI
}

void blits(char* buffer, int x, int y, char shift) {
    unsigned char i = 0;
    int cursor = 0;
    int compare = 0xA0;
    char docodeline = y == bly;
    
    if (buffer[0] == 0) return;

    console->line = y;
    cursor = x;
    
    while (buffer[i] != 0 && i < 256) {
        if (buffer[i] == '\n' || cursor >= compare) {
            return;
        } else {
            console->display[cursor] = buffer[i] + shift;
            ++cursor;
        }
        ++i;
    }
}

void blitz(char* buffer, int x, int y, char shift) {
    unsigned char i = 0;
    int cursor = 0;
    int compare = 0xA0;
    char docodeline = y == bly;
    
    if (buffer[0] == 0) return;

    console->line = y;
    cursor = x;
    
    while (i < 256) {
        if (cursor >= compare) {
            return;
        } else {
            console->display[cursor] = buffer[i] + shift;
            ++cursor;
        }
        ++i;
    }
}

void blitc(char buffer, int x, int y, char shift) {
    console->line = y;
    console->display[x] = buffer + shift;
}

// BEGIN LIBSOCK
typedef struct Modem {
    char buffer[0xF0];
    unsigned char cmd0;
    unsigned char cmd1;
    unsigned char inlen;
    unsigned char outlen;
    unsigned char index;
    unsigned char mode;
} Modem;
Modem* sck;

void sck_disconnect(){
	sck->cmd1 = 2;
	while(sck->cmd1 == 2);
}
void sck_connect(char* address){
    sck_disconnect();
	strcpy(sck->buffer, address);
	sck->cmd0 = 1;
	while(sck->cmd0 == 1);
}

void sck_smallwrite(char* data, unsigned int len){
    sck->mode = 1;
    sck->outlen = len;
    memcpy(sck->buffer, data, len);
    sck->cmd1 = 4;
    while(sck->cmd1 == 4);
}

void sck_writebyte(unsigned char val){
    sck->mode = 1;
    sck->outlen = 1;
    sck->buffer[0] = (char)val;
    sck->cmd1 = 4;
    while(sck->cmd1 == 4);
}

void sck_writeshort(int i){
    sck->mode = 1;
    sck->outlen = 2;
    sck->buffer[0] = (char)((i >> 8) & 0xFF);
    sck->buffer[1] = (char)(i & 0xFF);
    sck->cmd1 = 4;
    while(sck->cmd1 == 4);
}

unsigned char sck_readbyte(){
    sck->mode = 0;
    sck->inlen = 1;
    sck->cmd0 = 8;
    while(sck->cmd0 == 8);
    return (unsigned char)sck->buffer[0];
}
unsigned int sck_readshort(){
    sck->mode = 0;
    sck->inlen = 2;
    sck->cmd0 = 8;
    while(sck->cmd0 == 8);
    return (((unsigned int)sck->buffer[0]) << 8) | (((unsigned int)sck->buffer[1]) & 0xFF);
}

void sck_smallread(char* data, unsigned int len){
    sck->mode = 0;
    sck->inlen = len;
    sck->cmd0 = 8;
    while(sck->cmd0 == 8);
    memcpy(data, sck->buffer, len);
}

// END LIBSOCK







/*
int oilBuckets = 0;
*/

int count = 0;

char qwop = 0;
char workchar = 0;

char mw = 0;
char mh = 0;
/*
int lastBuckets = 0;
*/

int curx = 0;
int cury = 0;
char curchar = 0;

int herpaderpa = 0;

void setpall(unsigned char index, unsigned char r, unsigned char g, unsigned char b){
    console->blitclr = index;
    console->colors[0] = r;
    console->colors[1] = g;
    console->colors[2] = b;
    console->cmd = 1;
    while(console->cmd == 1);
    //for(herpaderpa = 0; herpaderpa < 512; herpaderpa++);
}

RawRB* raw;
unsigned char disks[16];
unsigned char fstype[16];
unsigned char tempd[128];
unsigned char prompt[2];
unsigned int yinc = 0;
unsigned char mount = 64;

void makebox(int x, int y, int w, int h){
    int i;
    console->line = y;
    console->display[x] = 16;
    console->display[x+w] = 14;
    for(i = x+1; i < x + w; i ++){
        console->display[i] = 26;
    }
    console->line = y+h;
    console->display[x] = 13;
    console->display[x+w] = 20;
    for(i = x+1; i < x + w; i ++){
        console->display[i] = 26;
    }
    for(i = y+1; i < y+h; i++){
        console->line = i;
        console->display[x] = 15;
        console->display[x+w] = 15;
    }
}

#define UNK_WORD_COLOR 103
#define C_KEYWORD_COLOR 58
#define STRING_COLOR 43
#define COMMENT_COLOR 48
#define SYMBOL_COLOR 15
#define NUMBER_COLOR 76
#define C_ACTION_KEYWORD_COLOR 0x2D
#define INCLUDE_COLOR 44
#define VAR_COLOR UNK_WORD_COLOR
#define CHILD_COLOR 68
#define FUNCTION_COLOR 65

#define CONSOLE_HEIGHT 6


char needsRepaint[64];
char lineLens[64];
char lineStates[64];

void printcons(char* chr){
    //scroll(x, y, ox, oy, w, h);
    scroll(1, SCREEN_HEIGHT - CONSOLE_HEIGHT, 1, SCREEN_HEIGHT - CONSOLE_HEIGHT - 1, SCREEN_WIDTH - 2, CONSOLE_HEIGHT-1);
    fill(0x20, 1, SCREEN_HEIGHT-2 , SCREEN_W-2, 1);
    blits(chr, 1, SCREEN_HEIGHT - 2, 0);
}
void printconz(char* chr){
    //scroll(x, y, ox, oy, w, h);
    scroll(1, SCREEN_HEIGHT - CONSOLE_HEIGHT, 1, SCREEN_HEIGHT - CONSOLE_HEIGHT - 1, SCREEN_WIDTH - 2, CONSOLE_HEIGHT-1);
    fill(0x20, 1, SCREEN_HEIGHT-2 , SCREEN_W-2, 1);
    blitz(chr, 1, SCREEN_HEIGHT - 2, 0);
}
unsigned char blit_colored_c_line(short x, short y, short linenum, unsigned char lastState, char resetregs){
    //state = lastState;
    
    char inmlc = 0;
    char iic = y-1;
    //if(y > (short)0x0037) return lastState;
    while(iic != 4){
        if(lineLens[iic] != 6) break;
        iic--;
    }
    state = lineStates[iic];
    if(state == COMMENT_COLOR)inmlc = 1;
    if(resetregs){
        blx = x;
        bly = y;
        wordi = 0;
        compi = 1;
        comment = 0;
        instring = 0;
        last = ' ';
        isNumeric = 1;
        include = 1;
        firstword = 1;
        lac = 0;
        add = 0;
        rrblx = 0;
    }
    __i = -1;
    console->line = y;
    if(lineLens[y] != -1){
        itoa(lineLens[y], _conv, 10);
        while(add++<6)
            if(_conv[add] == 0) break;
        console->blitclr = 15;
        
        blits("     ", 1, y, 0);
        blits(_conv, 6-add, y, 0);
    }
    add = 0;
    if(state == COMMENT_COLOR) comment = 1;
    while(__i++<(int)lineLens[y]){
        char c = console->display[__i + x];
        if(last == '/'){
            if(c == '/'){
                comment = 1;
                console->blitclr = COMMENT_COLOR;
                blitc('/', blx-1, bly, 0);
            }else if(c == '*'){
                inmlc = 1;
                state = COMMENT_COLOR;
                comment = 1;
                console->blitclr = COMMENT_COLOR;
                blitc('/', blx-1, bly, 0);
            }
        }
        include = 1;
        override = 0;
        switch(c & 0x7F){
            case '\'':
            case '"':
                if(comment || last == '\\'){
                    word[wordi] = c;
                }else{
                    instring = !instring;
                    
                    console->blitclr = STRING_COLOR;
                    word[wordi] = 0;
                    blits(word, blx, bly, 0);
                    blx += wordi;
                    blitc(c, blx, bly, 0);
                    blx++;
                    wordi = -1;
                    isNumeric = 1;
                    firstword = 0;
                    console->blitclr = SYMBOL_COLOR; // wtf
                }
                lac = c;
                break;
            case ' ':
                if(wordi == 0){wordi=-1;blx++;break;}
            case '{':
            case '}':
            case '(':
            case ')':
            case ';':
            case ':':
            case '-':
            case '+':
            case '=':
            case '[':
            case ']':
            case '/':
            case '<':
            case '>':
            case '.':
            case ',':
            case '*':
            case '~':
            case '&':
            case '!':
            case 0:
                if(wordi==0){
                    wordi = -1;
                    isNumeric = 1;
                    //if(c != 0)
                    //    blitc(c, blx, bly, 0);
                    console->display[blx] = console->display[blx];
                    blx++;
                    break;
                }
                if(c=='(')
                    add = FUNCTION_COLOR-UNK_WORD_COLOR; 
                else if(add == FUNCTION_COLOR-UNK_WORD_COLOR && c != ')'){
                    add = VAR_COLOR-UNK_WORD_COLOR; 
                }
                else if(c == ')') 
                    add = 0;
                else if(add && add != VAR_COLOR-UNK_WORD_COLOR) add = 0;
                
                word[wordi] = 0;
                switch(word[0]){
                    case 'W':
                        if(strcmp(word, "WHITE") == 0){
                            console->blitclr = 15;
                        } else console->blitclr = UNK_WORD_COLOR+add;
                        break;
                    case 'O':
                        if(strcmp(word, "ORANGE") == 0){
                            console->blitclr = 0x2A;
                        } else console->blitclr = UNK_WORD_COLOR+add;
                        break;
                    case 'M':
                        if(strcmp(word, "MAGENTA") == 0){
                            console->blitclr = 0x0D;
                        } else console->blitclr = UNK_WORD_COLOR+add;
                        break;    
                    case 'L':
                        if(strcmp(word, "LBLUE") == 0){
                            console->blitclr = 0x35;
                        }else if(strcmp(word, "LIME") == 0){
                            console->blitclr = 0x30;
                        }else if(strcmp(word, "LGRAY") == 0){
                            console->blitclr = 0x1C;
                        }else console->blitclr = UNK_WORD_COLOR+add;
                        break;
                    case 'Y':
                        if(strcmp(word, "YELLOW") == 0){
                            console->blitclr = 0x0E;
                        } else console->blitclr = UNK_WORD_COLOR+add;
                        break;
                    case 'P':
                        if(strcmp(word, "PINK") == 0){
                            console->blitclr = 0x3E;
                        } else if(strcmp(word, "PURPLE") == 0){
                            console->blitclr = 0x23;
                        }else console->blitclr = UNK_WORD_COLOR+add;
                        break;
                    case 'G':
                        if(strcmp(word, "GRAY") == 0){
                            console->blitclr = 0x17;
                        } else if(strcmp(word, "GREEN") == 0){
                            console->blitclr = 0x02;
                        }else console->blitclr = UNK_WORD_COLOR+add;
                        break;
                    case 'C':
                        if(strcmp(word, "CYAN") == 0){
                            console->blitclr = 0x0B;
                        } else console->blitclr = UNK_WORD_COLOR+add;
                        break;
                    case 'B':
                        if(strcmp(word, "BLUE") == 0){
                            console->blitclr = 0x20;
                        } else if(strcmp(word, "BROWN") == 0){
                            console->blitclr = 0x06;
                        } else if(strcmp(word, "BLACK") == 0){
                            console->blitclr = 0x14;
                        }else console->blitclr = UNK_WORD_COLOR+add;
                        break;
                    case 'R':
                        if(strcmp(word, "RED") == 0){
                            console->blitclr = 0x28;
                        } else console->blitclr = UNK_WORD_COLOR+add;
                        break;
                        
                        
                        
                        
                        
                    case 't':
                        if(strcmp(word, "typedef") == 0){
                            console->blitclr = C_ACTION_KEYWORD_COLOR;
                        }else{
                             console->blitclr = UNK_WORD_COLOR+add;
                        }
                        break;
                    case 'b':
                        if(strcmp(word, "break") == 0){
                            console->blitclr = C_ACTION_KEYWORD_COLOR;
                        }else if(strcmp(word, "byte") == 0){
                            console->blitclr = C_KEYWORD_COLOR;
                        }else console->blitclr = UNK_WORD_COLOR+add;
                        break;
                    case 'e':
                        if(strcmp(word, "else") == 0){
                            console->blitclr = C_ACTION_KEYWORD_COLOR;
                        } else console->blitclr = UNK_WORD_COLOR+add;
                        break;
                    case 'd':
                        if(strcmp(word, "default") == 0){
                            console->blitclr = C_ACTION_KEYWORD_COLOR;
                        } else console->blitclr = UNK_WORD_COLOR+add;
                        break;
                    case '#':
                        console->blitclr = INCLUDE_COLOR;
                        break;
                    case 'v':
                        if(strcmp(word, "void") == 0){
                            console->blitclr = C_KEYWORD_COLOR;
                        } else console->blitclr = UNK_WORD_COLOR+add;
                        break;
                    case 'i':
                        if(strcmp(word, "int") == 0){
                            console->blitclr = C_KEYWORD_COLOR;
                        }else if(strcmp(word, "if") == 0){
                            console->blitclr = C_ACTION_KEYWORD_COLOR;
                        }else console->blitclr = UNK_WORD_COLOR+add;
                        break;
                    case 's':
                        if(strcmp(word, "short") == 0){
                            console->blitclr = C_KEYWORD_COLOR;
                        }else if(strcmp(word, "struct") == 0){
                            console->blitclr = C_KEYWORD_COLOR;
                        }else if(strcmp(word, "switch") == 0){
                            console->blitclr = C_ACTION_KEYWORD_COLOR;
                        }else if(strcmp(word, "signed") == 0){
                            console->blitclr = C_ACTION_KEYWORD_COLOR;
                        }else console->blitclr = UNK_WORD_COLOR+add;
                        break;
                    case 'c':
                        if(strcmp(word, "char") == 0){
                            console->blitclr = C_KEYWORD_COLOR;
                        }else if(strcmp(word, "case") == 0){
                            console->blitclr = C_ACTION_KEYWORD_COLOR;
                        }else console->blitclr = UNK_WORD_COLOR+add;
                        break;
                    case 'l':
                        if(strcmp(word, "long") == 0){
                            console->blitclr = C_KEYWORD_COLOR;
                        }else console->blitclr = UNK_WORD_COLOR+add;
                        break;
                    case 'f':
                        if(strcmp(word, "for") == 0){
                            console->blitclr = C_ACTION_KEYWORD_COLOR;
                        }else console->blitclr = UNK_WORD_COLOR+add;
                        break;
                    case 'r':
                        if(strcmp(word, "return") == 0){
                            console->blitclr = C_ACTION_KEYWORD_COLOR;
                        }else console->blitclr = UNK_WORD_COLOR+add;
                        break;
                    case 'w':
                        if(strcmp(word, "while") == 0){
                            console->blitclr = C_ACTION_KEYWORD_COLOR;
                        }else console->blitclr = UNK_WORD_COLOR+add;
                        break;
                    case 'u':
                        if(strcmp(word, "unsigned") == 0){
                            console->blitclr = C_KEYWORD_COLOR;
                        }else console->blitclr = UNK_WORD_COLOR+add;
                        break;
                    default:
                        console->blitclr = UNK_WORD_COLOR+add;
                        break;
                }
                rrblx = 0;
                if(include || wordi > 0x0000){
                    if(comment)console->blitclr = COMMENT_COLOR;
                    else if(instring) console->blitclr = STRING_COLOR;
                    else if(isNumeric) console->blitclr = NUMBER_COLOR;
                    else if(nexttwoinc){
                        if(nexttwoinc != 2)console->blitclr = nexttwo;
                        else if(nexttwoinc == 3){
                            console->blitclr = nexttwo;
                            nexttwoinc = 2;
                        }
                        nexttwoinc--;
                    }
                    blits(word, blx, bly, 0);

                    if(c == 0) {lineStates[y] = state;return state;}

                    blx += wordi;

                    if(!comment && !instring)
                        console->blitclr = SYMBOL_COLOR;
                    
                    console->display[blx] = console->display[blx];
                    blx++;
                    wordi = -1;
                    isNumeric = 1;
                    firstword = 0;
                }else{
                    wordi = -1;
                    isNumeric = 1;
                    
                    console->display[blx] = console->display[blx];
                    blx++;
                }
                
                break;
            default:
                if(isNumeric)
                    if(!(c > 47 && c < 58))
                        isNumeric = 0;
                word[wordi] = c;
                break;
                
        }
        if(state == COMMENT_COLOR && c == '/' && last == '*'){
            state = 0;
            comment = 0;
            inmlc = 0;
            console->blitclr = COMMENT_COLOR;
            console->display[blx-1] = '/';
        }
        last = c;
        wordi++;
    }
    lineStates[y] = console->blitclr == COMMENT_COLOR ? (inmlc?COMMENT_COLOR:0) : console->blitclr;
    return console->blitclr;
}


unsigned short __state = 0;
unsigned short laststate = 0;
unsigned int largestLine = 0;

typedef struct NBTShort {
    unsigned int value;
} NBTShort;
NBTShort* Nshort;

short what_the_actual_fuck_hack = 1;
unsigned short inkrm = 0;

char menuOpened = 0;
char openedMenuHeight = 0;
int __COM_read8bitstring(){
    int len = sck_readbyte();
    if(len == 0) return clmem[0]=0;
    sck->mode = 0;
    sck->inlen = len;
    sck->cmd0 = 8;
    while(sck->cmd0 == 8);
    memcpy(clmem, sck->buffer, len);
    clmem[len] = 0;
    return len;
}

void __COM_SET_OSL(int lineid){
    if(lineLens[lineid] == 6) {
        map_modem();
        sck_writebyte(3);
        sck_writebyte(0);
        sck_writebyte(lineid);
        return;
    }
    map_console();
    console->line = lineid;
    memcpy(clmem, console->display + 7, 158);
    //printcons(clmem);
    map_modem();
    sck_writebyte(3);
    sck_writebyte(lineLens[lineid]-6);
    sck_writebyte(lineid);
    sck_smallwrite(clmem, lineLens[lineid]-6);
}

void __COM_GET_OSL(int lineid){
    sck_writebyte(4);
    sck_writebyte(lineid);
    lineLens[lineid] = __COM_read8bitstring() + 6;
    map_console();
    blits(clmem, 7, lineid, 0);
    map_modem();
    needsRepaint[lineid] = 1;
}

void __COM_GET_OSL_SET(int from, int to){
    int e = to - from, lineid;
    sck_writebyte(5);
    sck_writebyte(from);
    sck_writebyte(to);
    while(e != 0){
        lineid = sck_readbyte();
        lineLens[lineid] = __COM_read8bitstring() + 6;
        map_console();
        blits(clmem, 7, lineid, 0);
        map_modem();
        needsRepaint[lineid] = 1;
        e--;
    }
}
char* prg = (char*) 0x500;
int lof;
void __COM_read1packet(){
    int len, i, llen;
    switch(sck_readbyte()){
        case 22: // execute binary data
            len = sck_readshort();
            map_console();
            printcons("Receiving binary data to execute...");
            map_mmu();
            mmu->childpage = 2;
            mmu->len = 0xFFFF;
                //map_console();
                //blits("Copying data to child page...", 1, 2, 128);
                //map_mmu();
            
            map_modem();
            i = 0;
            llen = len / 64;
            while(i != llen){
                //map_console();
                //printcons("Receiving 32 byte long segment...");
                map_modem();
                sck->mode = 0;
                sck->inlen = 64;
                sck->cmd0 = 8;
                while(sck->cmd0 == 8);
                memcpy(clmem, sck->buffer, 64);
                //map_console();
                //printcons("Copying segment to memory...");
                //map_console();
                //printconz(clmem);
                
                map_mmu();
                mmu->mode = 1; // read main write sec
                memcpy(prg + lof, clmem, 64);
                mmu->mode = 0; // read main write sec
                //map_console();
                //printcons("done");
               // map_modem();
                lof += 64;
                i++;
            }
            //rb_map_device(10);
            //FFS_read(0x500, lengthof+256, lindex);//0x3780
            map_console();
            printcons("Done! booting...");
            map_mmu();
            mmu->mode = 0;
            //mmu->special = 1;
            mmu->rootpage = 2; // boot page 2
            map_console();
            printcons("AFTERBOOT?? THIS SHOULD NEVER HAPPEN!!!");
            break;
        case 4: // set file 
            __COM_read8bitstring();
            lof = sck_readshort();
            len = sck_readbyte();
            map_console();
            //blits("                                                                       ", 21, 1, 0);
            fill(0x20, 21, 1, 128, 1);
            blits(clmem, 21, 1, 0);
/*
            
            
            itoa(len, conv, 10);
            blits(conv, 21, 2, 0);
*/
            map_modem();
            i=0;
            while(i!=len){
                llen = __COM_read8bitstring() + 6;
                map_console();
                blits(clmem, 7, i+5, 0);
                lineLens[i+5] = llen;
                needsRepaint[i+5] = 1;
                map_modem();
                i++;
            }
            break;
        default: break;
    }
}
void openMenu(char men){
    short i;
    char e=5;
    map_console();
    downvert(0,0,160,64);
    while(e != (char)13){
        __COM_SET_OSL((int)e);
        e++;
    }
    map_console();
    menuOpened = men;
    switch(men){
        case 4:
            console->blitclr = 15;
            
            openedMenuHeight = 5;
            makebox(23,4,20,openedMenuHeight);
/*
            console->line = 4;
            console->display[0] = 11;
            console->display[20] = 18;
            console->display[7] = 17;
            console->display[14] = 17;
            console->line = 4+openedMenuHeight;
            console->display[0] = 11;
            console->display[6] = 18;
*/
            
            fill(0x20, 24, 5, 19, openedMenuHeight-1);
            blits("Build", 25, 5, 0);
            blits("Build+Run", 25, 6, 0);
            blits("Build+Write", 25, 7, 0);
            blits("Clean", 25, 8, 0);
            break;
        case 2:
            console->blitclr = 15;
            
            openedMenuHeight = 5;
            makebox(0,4,20,openedMenuHeight);
/*
            console->line = 4;
            console->display[0] = 11;
            console->display[20] = 18;
            console->display[7] = 17;
            console->display[14] = 17;
            console->line = 4+openedMenuHeight;
            console->display[0] = 11;
            console->display[6] = 18;
*/
            
            fill(0x20, 1, 5, 19, openedMenuHeight-1);
            blits("...", 2, 5, 0);
            blits("...", 2, 6, 0);
            blits("", 2, 7, 0);
            blits("Anus", 2, 8, 0);
            break;
        case 1:
            console->blitclr = 15;
            
            openedMenuHeight = 5;
            makebox(0,4,20,openedMenuHeight);
            console->line = 4;
            console->display[0] = 11;
            console->display[20] = 18;
            console->display[7] = 17;
            console->display[14] = 17;
            console->line = 4+openedMenuHeight;
            console->display[0] = 11;
            console->display[6] = 18;
            
            fill(0x20, 1, 5, 19, openedMenuHeight-1);
            blits("New...", 2, 5, 0);
            blits("Open...", 2, 6, 0);
            blits("Save", 2, 7, 0);
            blits("Test", 2, 8, 0);
            
            break;
    }
    
    
}

void closemenus(){
    int e, i=SCREEN_W;
    console->bgblitclr = 0;
    fill(0x20, 0, 4, 158, openedMenuHeight+1);
    e=5;
    map_modem();
    //while(e != 13){
    //    __COM_GET_OSL((int)e);
    //    e++;
    //}
    __COM_GET_OSL_SET(5, 13);//__COM_GET_OSL_SET(13, 55);
    map_console();
    menuOpened = 0;
    console->line = 4;
    while(i-->0){
        console->display[i] = 26;
    }
    console->display[6] = 18;
    console->display[7] = 17;
    console->display[14] = 17;
    console->display[24] = 17;
    console->display[32] = 17;
    console->display[39] = 17;
    console->display[0] = 11;
    console->display[SCREEN_W-1] = 19;
    //console->cursor_x = 7;
    //console->cursor_y = 5;
    i = 4;
    while(i++ != 15){
        console->line = i;
        console->display[0] = 15;
        console->display[6] = 15;
    }
}

void buildAndRun(){
    int e;
    printcons("Clicked build and run!");
    downvert(0,0,160,64);
    map_modem();
    e=13;
    while(e != (short)56){
        __COM_SET_OSL((int)e);
        e++;
    }
    //__COM_GET_OSL_SET(13, 55);
    sck_writebyte(45); // build&run
    __COM_read1packet();
    map_console();
}

void execmenu(char menuid, char entry){
    short i,e,swch=0, v, k;
    char resp[32];
    switch(menuid){
        case 4:
            switch(entry){
                case 1: // build & run
                    buildAndRun();
                    break;
                default: break;
            }
            break;
        case 1:
            switch(entry){
                case 3: // test
                    break;
            }
            break;
    }
    closemenus();
}

char lhi = 0;
char llhi = -1;
char selindex = -1;
char dragstartx = 0;
char dragstarty = 0;
char linvx = 0;
char linvy = 0;
char linvx_h = 0;
char linvy_h = 0;

char lselx = 0;
char lsely = 0;
char lseltargx = 0;
char lseltargy = 0;

void selinvert(char _fx, char _fy, char _tx, char _ty, char rpt, char remove){
    char fx = _fx, fy = _fy, tx = _tx, ty = _ty, x, y;
    if(remove){
        // TODO MAKE THIS BETTER
        lselx = 0;
        lsely = 0;
        lseltargx = 0;
        lseltargy = 0;
        downvert(0,0,160,64);
        return;
    }
    if(ty < fy){
        if(1)return;
        ty = _fy;
        fy = _ty;
    }
    lselx = fx;
    lsely = fy;
    lseltargx = tx;
    lseltargy = ty;
    x = fx;
    y = fy;

    if(fy != ty){
        while(y != ty){
            upvert(x, y, 160, 1);
            if(rpt)needsRepaint[y] = 1;
            x = 7;
            y++;
        }
    }else{
        if(tx < fx){
            tx = _fx;
            fx = _tx;
        }
        x = fx;
    }
    
    //if(!(tx-x)) return;
    if(rpt)needsRepaint[y] = 1;
    upvert(x, y, (tx-x)+1, 1);
}

char ldsx = 0;
char ldsy = 0;

int idles = 0;

void maus(){
    unsigned char cx = 0;
    unsigned char cy = 0;
    unsigned char inkr = 0;
    cx = console->pressedX;
    cy = console->pressedY;
    if(dragstartx != 0){
        idles = 0;
        if(linvx_h != console->mouseX || linvy_h != console->mouseY){
            
            selinvert(dragstartx, dragstarty, linvx, linvy, 0, 1);
            linvx = console->mouseX;
            linvy = console->mouseY;
            console->cursor_x = linvx;
            console->cursor_y = linvy;
            selinvert(dragstartx, dragstarty, linvx, linvy, 0, 0);
            linvx_h = console->mouseX;
            linvy_h = console->mouseY;
        }

        if(console->releasedX != 0){
            printcons("Released");
            //printf("lol");
            if((dragstartx == (console->releasedX)) && dragstarty == console->releasedY){
                //printcons("Univerting the bullshit");

                console->line = dragstarty;
                //console->display[dragstartx+1]-=0x80;
                ldsx = 0;
                ldsy = 0;
                //selinvert(ldsx+1, ldsy, linvx, linvy, 1, 1);
                needsRepaint[dragstarty] = 1;
                console->releasedX = 0;
                dragstartx = 0;

            }else if(dragstarty > linvy || dragstartx > linvx){
                console->line = dragstarty;
                //console->display[dragstartx]-=0x80;
                needsRepaint[dragstarty] = 1;
/*
                ldsx = 0;
                ldsy = 0;
*/              ldsx = dragstartx;
                ldsy = dragstarty;
                //selinvert(ldsx+1, ldsy, linvx, linvy, 1, 1);
                console->releasedX = 0;
                dragstartx = 0;
            }else{
                ldsx = dragstartx;
                ldsy = dragstarty;
                console->releasedX = 0;
                dragstartx = 0;
            }
        }

    }
    if(cx != 0){
        if(cy>4&&menuOpened != 0 && selindex != -1){
            execmenu(menuOpened, selindex);
        }else
        if(cy > 0x01 && cy < 0x05){
            if(!menuOpened){
                if(cx > 0x00 && cx < 0x07){
                    openMenu(1);
                }else if(cx>0x06 && cx < 0x0E){
                    openMenu(2);
                }else if(cx>0x0E && cx < 0x18){
                    openMenu(3);
                }else if(cx > 0x18 && cx < 0x20){
                    openMenu(4);
                }else if(cx > 0x20 && cx < 0x27){
                    openMenu(5);
                }
            }
        }else{
            console->cursor_x = cx;
            console->cursor_y = cy;
        }
        if(ldsx != 0){
            printcons("inverting");
            selinvert(ldsx+1, ldsy, linvx, linvy, 1, 1);
            ldsx = 0;
        }
        dragstartx = console->pressedX;
        dragstarty = console->pressedY;
        linvx = dragstartx;
        linvy = dragstarty;
        linvx_h = dragstartx;
        linvy_h = dragstarty;
        console->pressedX = 0;
        console->pressedY = 0;
    }
    switch(menuOpened){
        case 4:
            if(console->mouseX > 23 && console->mouseX < 45){
                lhi = console->mouseY - 5;
                if(lhi < 4){
                    inkr = 0;
                    console->line = console->mouseY;
                    console->bgblitclr = 132;
                    while(inkr++ != 19){
                        console->display[inkr+23] = console->display[inkr+23];
                    }
                    selindex = lhi;
                }else{
                    lhi = -1;
                    selindex = -1;
                }
                
                if(llhi != -1 && llhi != lhi){
                    inkr = 0;
                    console->line = llhi + 5;
                    console->bgblitclr = 0;
                    while(inkr++ != 19){
                        console->display[inkr+23] = console->display[inkr+23];
                    }
                    llhi = lhi;
                }
                
                
            }else{
                selindex = -1;
            }
            break;
        case 1:
            if(console->mouseX > 1 && console->mouseX < 20){
                lhi = console->mouseY - 5;
                if(lhi < 4){
                    inkr = 0;
                    console->line = console->mouseY;
                    console->bgblitclr = 132;
                    while(inkr++ != 19){
                        console->display[inkr] = console->display[inkr];
                    }
                    selindex = lhi;
                }else{
                    lhi = -1;
                    selindex = -1;
                }
                
                if(llhi != -1 && llhi != lhi){
                    inkr = 0;
                    console->line = llhi + 5;
                    console->bgblitclr = 0;
                    while(inkr++ != 19){
                        console->display[inkr] = console->display[inkr];
                    }
                    llhi = lhi;
                }
                
                
            }else{
                selindex = -1;
            }
            break;
    }
}

unsigned int sectorids[64];
unsigned int nextFreeSector;

void __COM_POP_BOTTOM(){
    int len;
    map_modem();
    sck_writebyte(7);
    if(sck_readbyte() == 7){
        if(len=__COM_read8bitstring()){
            map_console();
            lineLens[55] = len + 6;
            needsRepaint[55] = 1;
            blits(clmem, 7, 55, 0);
        }
    }
    map_console();
}

void scrollLineLensUp(int max){
    int i = max+1;
    while(i != 63){
        needsRepaint[i-1] = needsRepaint[i];
        lineLens[i-1] = lineLens[i];
        lineStates[i-1] = lineStates[i];
        i++;
    }
    needsRepaint[63] = 0;
    lineLens[63] = 6;
    lineStates[63] = 0;
    
}

void scrollLineLensDown(int max, int rst){
    int i = 63;
    while(i != max){
        needsRepaint[i+1] = needsRepaint[i];
        lineLens[i+1] = lineLens[i];
        lineStates[i+1] = lineStates[i];
        i--;
    }
    needsRepaint[max+1] = 0;
    lineStates[max+1] = 0;
    lineLens[max+1] = rst;
}

void sendline(char c, int len){
    map_modem();
    sck_writebyte(c);
    sck_writebyte(len);
    sck_smallwrite(clmem, len); 
    map_console();
}

void insertNewLineBelow(){
    int i = console->cursor_y,e;
    if(console->cursor_y == 55){
        if(lineLens[5] > (char)6){
            e=lineLens[5]-6;
            console->line = 5;
            downvert(0,0,160,64);
            memcpy(clmem, console->display + 7, e);
            clmem[e] = 0;
            //printcons(clmem);
            sendline(2, e);
        }else{
            map_modem();
            sck_writebyte(9);
            map_console();
        }
        //printcons("new line but at bottom!!!!!!");
        scroll(7, 6, 7, 5, 158, 50);
        scrollLineLensUp(5);
        fill(0x20, 7, 55, 152, 1);
        console->cursor_y = 54;
    }else{
        if(lineLens[55] > (char)6){
            downvert(0,0,160,64);
            e=lineLens[55]-6;
            console->line = 55;
            memcpy(clmem, console->display + 7, e);
            clmem[e] = 0;
            //printcons(clmem);
            sendline(1, e);
            console->line = i;
        }else{
            map_modem();
            sck_writebyte(9);
            map_console();
        }
        rscroll(7, console->cursor_y+1, 7, console->cursor_y+2, 158, 54-console->cursor_y);
        scrollLineLensDown(console->cursor_y, 6);
        fill(0x20, 7, console->cursor_y+1, 152, 1);
    }
}


void handlenextline(char resetX){
    int i = 0;
    needsRepaint[console->cursor_y] = 1;
    if(resetX){
        
        if(console->cursor_x < (unsigned char)(lineLens[console->cursor_y]+1)){
            i = lineLens[console->cursor_y] - console->cursor_x;
            insertNewLineBelow();
            scroll(console->cursor_x, console->cursor_y, console->cursor_x, console->cursor_y+1, 160, 1);
            scroll(console->cursor_x, console->cursor_y+1, 7, console->cursor_y+1, 160, 1);
            lineLens[console->cursor_y+1] = i + 7;
            lineLens[console->cursor_y] -= i + 1;
            fill(0x20, console->cursor_x, console->cursor_y, 160, 1);
        }else{
            insertNewLineBelow();
            lineLens[console->cursor_y+1] = 6;
        }
        console->cursor_x = 7;
    }
    console->cursor_y++;
}

void __COM_TRYPOPTOP(){
    int len;
    map_modem();
    sck_writebyte(8);
    if(sck_readbyte() == 8){
        if(len = __COM_read8bitstring()){
            memcpy(buffer, clmem, 160);
            // pop the line
            map_console();
            console->cursor_y--;
            insertNewLineBelow();
            console->cursor_y++;
            blits(buffer, 7, console->cursor_y, 0);
            lineLens[console->cursor_y] = len + 6;
            lineStates[console->cursor_y] = 0;
            needsRepaint[console->cursor_y] = 1;
        }
    }
    map_console();
}

void handlepreviousline(char resetX){
    int i,v;
    needsRepaint[console->cursor_y] = 1;
    if(console->cursor_y > 5){
        
        if(resetX){
            v = lineLens[console->cursor_y-1]+1;
            if(console->cursor_x < (unsigned char)(lineLens[console->cursor_y]+1)){ /* THIS NEEDS FIXING */
                i = lineLens[console->cursor_y] - console->cursor_x;
                if(lineLens[console->cursor_y-1] + i >= (int)0x009E){ // prevent moving stuff into oblivion
                    console->cursor_y--;
                    return;
                }
                rscroll(console->cursor_x, console->cursor_y, v, console->cursor_y, 152, 1);
                rscroll(v, console->cursor_y, v, console->cursor_y-1, 152, 1);
                
                fill(0x20, 7, console->cursor_y, 152, 1);
                scroll(7, console->cursor_y+1, 7, console->cursor_y, 152, 55-(console->cursor_y-1)); // THIS IS BORNK
                lineLens[console->cursor_y] = 6;
                console->cursor_x = v;
                lineLens[console->cursor_y-1] += i + 1;
                fill(0x20, 7, 55, 152, 1);
                scrollLineLensUp(console->cursor_y);
                __COM_POP_BOTTOM();
            }else{
                scroll(7, console->cursor_y+1, 7, console->cursor_y, 152, 55-(console->cursor_y-1));
                console->cursor_x = v;
                fill(0x20, 7, 55, 152, 1);
                scrollLineLensUp(console->cursor_y);
                __COM_POP_BOTTOM();
            }
            
        }
        console->cursor_y--;
    }else{
        __COM_TRYPOPTOP();
    }
}
char temporaryLine[160];

int copyToClipboard(){
    int bytes = 0;
    int x = 0, y = 0;
    //lselx = fx;
    //lsely = fy;
    //lseltargx = tx;
    //lseltargy = ty;
    
    //itoa(lselx, conv, 10);
    //printcons(conv);
    //itoa(lsely, conv, 10);
    //printcons(conv);
    ///itoa(lseltargx, conv, 10);
    //printcons(conv);
    //itoa(lseltargy, conv, 10);
    //printcons(conv);
    y = lsely;
    console->line = y;
    x = lselx;
    while(1){
        //printcons("Copying line");
        console->line = y;
        while(y == lseltargy?x<lseltargx+1:(x < lineLens[y]+1)){
            clipboard[bytes++] = console->display[x] & 0x7F;
            x++;
        }
        x = 7;
        clipboard[bytes++] = '\r';
        
        y++;
        if(y == lseltargy+1) break;
    }
    clipboard[bytes-1] = 0;
    
    return bytes-1;
}
char __avoid_bksp = 0;
void tryCMD(){
    int v = lineLens[console->cursor_y] - 7;
    char* tok;
    char* arg;
    unsigned char cval;
    memcpy(clmem, console->display + 8, v);
    clmem[v] = 0;
    printcons("Tried to exec:");
    //printcons(clmem);
    tok = strtok(clmem, " ");
    arg = strtok(NULL, " ");
    printcons(tok);
    printcons(arg);
    switch(tok[0]){
        case 'c':
            if(strcmp(tok, "clock") == 0){
                cval = (unsigned char)atoi(arg);
                printcons("Setting clock speed!");
                map_mmu();
                mmu->crystal = 33;
                mmu->crystal = 41;
                mmu->crystal = cval;
/*
                rb_unmap();
*/  map_console();
                blits("//Set clock speed!", 7, console->cursor_y, 0); 
                lineLens[console->cursor_y] = 24;   
            }
            break;
        default:
            break;
    }    
}
void hkey(char c){
    int i;
    char e;
    //itoa(c, conv, 10);
    //printcons(conv);
    switch(c){
        case 24: // CTRL+X
            copyToClipboard();
            hkey(8);
            downvert(0,0,160,64);
            break;
        case 22: // CTRL+V
            i = 0;
            while(1){
                if(i == CLIPBOARD_SIZE || clipboard[i] == 0) break;
                hkey(clipboard[i]);
                //printcons(clipboard+i);
                i++;
            }
            break;
        case 3: // CTRL+C
            i = copyToClipboard();
            itoa(i, conv, 10);
            printcons(conv);
            printcons(clipboard);
            break;
        case 18: // CTRL+R
            printcons("CTRL+R run!");
            ++console->kb_start;
            e = 5;
            downvert(0,0,160,64);
            while(e != (char)13){
                __COM_SET_OSL((int)e);
                e++;
            }
            map_console();
            buildAndRun();
            break;
        case '\t':
            hkey(' ');
            hkey(' ');
            hkey(' ');
            hkey(' ');
            break;
        case '\r':
            if(console->display[7] == '`')
                tryCMD();
            else{
                handlenextline(1);
                console->line = console->cursor_y;
            }
            break;
        case 128: // up
            handlepreviousline(0);
            console->line = console->cursor_y;
            break;
        case 131: // right
            if(console->cursor_x < 157) console->cursor_x++;
            else{
                handlenextline(0);
                console->cursor_x = 7;
                console->line = console->cursor_y;
            }
            break;
        case 130: // left
            if(console->cursor_x > 7) console->cursor_x--;
            break;
        case 129: // down
            if(console->cursor_y == 55){
                insertNewLineBelow();
                __COM_POP_BOTTOM();
                console->cursor_y++;
            }else{
                handlenextline(0);
            }
            console->line = console->cursor_y;
            break;

        case 8: // backspace
            if(!__avoid_bksp && lselx){
                __avoid_bksp = 1;
                console->cursor_x = lseltargx+1;
                console->cursor_y = lseltargy;
                console->line = lseltargy;
                while(!(console->cursor_y == lsely && (console->cursor_x == lselx+1 || console->cursor_x == 7))){
                    hkey(8);
                }
                
                downvert(0,0,160,64);
                if(console->cursor_x == 7){
                    hkey('r'); // ugly hack
                }
                lselx = 0;
                lsely = 0;
                lseltargx = 0;
                lseltargy = 0;
                __avoid_bksp = 0;
            }
            if(console->cursor_x > 7){
                if(console->cursor_x < (unsigned char)(lineLens[console->cursor_y]+1)){
                    i = console->display[console->cursor_x]; // wtf
                    scroll(console->cursor_x+1, console->cursor_y, console->cursor_x, console->cursor_y, 160, 1);
                    lineLens[console->cursor_y]--;
                    console->cursor_x--;
                    console->display[console->cursor_x] = i; // wtf
                }else{
                    console->cursor_x--;
                    lineLens[console->cursor_y] = console->cursor_x-1;
                    console->blitclr = 15;
                    console->display[console->cursor_x] = ' ';
                }
            }else{
                handlepreviousline(1);
                console->line = console->cursor_y;
            }
            break;

        default:
            if(console->cursor_x < (unsigned char)(lineLens[console->cursor_y]+1)){
                rscroll(console->cursor_x, console->cursor_y, console->cursor_x+1, console->cursor_y, 160, 1);
                lineLens[console->cursor_y]++;
            }else{
                lineLens[console->cursor_y] = console->cursor_x;
            }
            console->display[console->cursor_x] = c;
            ++console->cursor_x;
            needsRepaint[console->cursor_y] = 1;
            if(console->cursor_x>157){
                console->display[console->cursor_x] = '\\';
                handlenextline(1);
            }
            break;
    }
}

void kbrd(){
    unsigned char i = 0;
    unsigned int scrl = (unsigned int) (console->scrollWheel & 0xFF);
    
    if(scrl != 0){
        itoa(scrl, conv, 16);
        printcons(conv);
        console->scrollWheel = 0;
        if(scrl < (unsigned int)0x0080){
            while(scrl != 0){
                if(console->pressedX) break;
                if(scrl % 3 == 1)hkey(0x80);
                scrl--;
            }
        }else{
            scrl -= 0x80;
            scrl = 0x80 - scrl;
            while(scrl != 0){
                if(console->pressedX) break;
                if(scrl % 3 == 1)hkey(0x81);
                scrl--;
            }
        }
        console->scrollWheel = 0;
        //while(1);
    }
    maus();
    
    console->line = console->cursor_y;
    while(console->kb_pos != console->kb_start){
        
        idles = 0;
        hkey(console->kb_key);
        ++console->kb_start;
        ++i;
    }
    if(i != 0){
        needsRepaint[console->cursor_y] = 1;
    }
    if(idles++==128){
        idles = 0;
        while(i != 64 && console->kb_pos == console->kb_start && !console->pressedX && !console->scrollWheel){
            if(needsRepaint[i]){
                console->line = i;
                //memcpy(temporaryLine, console->display+7, 160-7);
                blit_colored_c_line(7, i, i, 0, 1);
                needsRepaint[i] = 0;
            }
            i++;
        }
    }
}

void __IDE_3rdMAIN(){
    map_console();
    fill(0x20, 10, 5, 140, 40);
    printcons("IDE Main entry point!");
    console->pressedX = 0;
    console->pressedY = 0;
    console->cursor_x = 7;
    console->cursor_y = 5;
    map_modem();
    __COM_read1packet(); // continue handshake
    map_console();
    while(1){
        kbrd();
    }
    while(1);
}

void __COM_CONNECT(){
    int i = 43, done = 0;
    console->line = 14;
    
    while(i != 159){
        switch(console->display[i]){
            case 0:
            case ' ':
            case 1:
                done = 1;
                break;
            default:break;
        }
        if(done) break;
        i++;
    }
    memcpy(clmem, console->display + 43, i - 43);
    clmem[i-43] = 0;
    printcons("Connecting to:");
    printcons(clmem);
    map_modem();
    sck_connect(clmem);
    sck_writebyte(0xBE);
    sck_writebyte(0xEF);
    map_console();
    i=56;
    console->line = 17;
    done = 0;
    while(i != 159){
        switch(console->display[i]){
            case 0:
            case ' ':
            case 1:
                done = 1;
                break;
            default:break;
        }
        if(done) break;
        i++;
    }
    memcpy(clmem, console->display + 56, i - 56);
    clmem[i-56] = 0;
    map_modem();
    //56
    if(sck_readbyte() == 0xCA){
        if(sck_readbyte() == 0xFE){
            sck_writebyte(0x69);
            sck_writebyte(i-56);
            sck_smallwrite(clmem, i-56);
            sck_writebyte(pwlen);
            sck_smallwrite(pwmem, pwlen);
            switch(sck_readbyte()){
                case 14: // login success
                    map_console();
                    printcons("Connected, handshake complete!");
                    __IDE_3rdMAIN();
                    break;
                default:
                case 26: // invalid details
                    map_console();
                    printcons("Invalid login details! Cannot continue!");
                    break;
                case 27: // would you like to create this account
                    map_console();
                    printcons("Invalid login details! Cannot continue!");
                    break;
            }
            
            while(1);
        }
    }
    map_console();
    printcons("Connection error!");
    
}

void __COM_cli(unsigned int i){
    switch(i){
        case 1:
            printcons("Clicked yes!");
            __COM_CONNECT();
            break;
        case 2:
            printcons("Clicked no!");
            console->bgblitclr = 0;
            makebox(40, 10, 80, 4);
            console->bgblitclr = 200;
            fill(0x20, 41, 11, 79, 3);
            console->blitclr = 40;
            blits("CRITICAL ERROR", 74, 11, 0);
            console->blitclr = 15;
            blits("The system cannot continue with this configuration!", 56, 12, 0);
            blits("Please reconfigure disks and try again.", 62, 13, 0);
            console->bgblitclr = 0;
            fill(0x20, 40, 15, 81, 3);
            while(1);
            break;
    }
}
void __COM_rst(unsigned int i){
    unsigned int e;
    switch(i){
        case 1:
            console->bgblitclr = 190;//200
            e = 98;
            console->line = 15-2;
            while(e != 110){
                console->display[e] = console->display[e];
                e++;
            }
            break;
        case 2:
            console->bgblitclr = NUMBER_COLOR/4;//200
            e = 110;
            console->line = 15-2;
            while(e != 120){
                console->display[e] = console->display[e];
                e++;
            }
            break;
        case 3:
            console->bgblitclr = 190;//200
            e = 110;
            console->line = 15;
            while(e != 120){
                console->display[e] = console->display[e];
                e++;
            }
            break;
    }
}

void handle_connection_init(){
    int i=0, e=0, d=0;
    char f = 0, q=0, m=0;
    map_console();

    console->bgblitclr = 200;//200
    fill(0x20, 41, 11, 79, 11);
    console->blitclr = 0x2B;
    console->bgblitclr = 0;//200
    blits("C IDE Suite v0.1", 55, 10, 0);
    console->charset = 1;
    while(f != 48){
        console->blitclr = (f/16)+0x29;
        //if(i % (int)16 < (int)12){
        if((f % 16) < 13)
            switch(f){
                case 0x26:
                case 0x27:
                case 0x25:
                case 0x19:
                case 0x0C:
                case 0x2C:
                    break;
                default:
                    
                    //if(((int)(i % 16)) <= ((int)12)){
                        blitc(f, f % 16+41, f/16+7, 0);
                    //}
                    
                    break;
            }
        //}
        f++;
    }
    console->charset = 0;
    i=0;
    console->blitclr = 0x2A;
    //console->bgblitclr = 0;//200
    blits("C IDE Suite v0.1 (cc65 compiler)", 56, 8, 0);
    console->bgblitclr = 200;//200
    //console->charset = 0;
    console->blitclr = NUMBER_COLOR;
    blits("INFO: ", 42, 11,0);
    console->blitclr = 15;
    blits("You must connect to a build server before continuing!", 49, 11,0);
    i = 40;
    console->line = 12;
    while(i++ != 119){
        console->display[i] = 26;
    }
    //blits("The currently selected disk is not formatted. Would you like to format it now?", 41, 13,0);
    makebox(109, 14-2, 11, 2);
    console->bgblitclr = NUMBER_COLOR/4;//200
    blits("   Info    ", 110, 15-2,0);
    console->bgblitclr = 0;
    makebox(97, 14-2, 12, 2);
    console->bgblitclr = 190;//200
    blits("  Connect  ", 98, 15-2,0);
    console->bgblitclr = 0;//200
    makebox(40, 10, 80, 12);

    console->line = 16-2;
/*
    console->display[97] = 17; // 10
*/
    console->display[109] = 17;
/*
    console->line = 12-2;
    console->display[40] = 11;
    console->display[120] = 19;
*/

    console->line = 14-2;
    console->display[120] = 19;
    console->bgblitclr = 200;
    console->display[109] = 18;
    console->pressedX = 0;
    console->pressedY = 0;
    console->line = 12;
    i = 40;
    while(i++ != 119){
        console->display[i] = 26;
    }
    console->bgblitclr = 0;
    console->display[40] = 11;
    console->display[120] = 19;
    
    console->bgblitclr = 200;
    i = 42;
    console->blitclr = 249; 
    console->line = 13;
    console->charset = 1;
    while(i++ != 94){
        console->display[i] = 0x47;
    }
    
    console->line = 14;
    console->display[42] = 0x44;//1
    console->display[95] = 0x45;//1
    
    console->line = 15;
    
    i = 42;
    while(i++ != 94){
        console->display[i] = 0x46;
    }
    
    console->charset = 0;
    console->line = 16;
   
    i=55;
    console->charset = 1;
    while(i++ != 94){
        console->display[i] = 0x47;
    }
    
    console->line = 17;
    console->display[55] = 0x44;//1
    console->display[95] = 0x45;//1
    
    console->line = 18;
    
    i = 55;
    while(i++ != 94){
        console->display[i] = 0x46;
    }
    
    console->charset = 0;
    console->line = 19;
   
    i=55;
    console->charset = 1;
    while(i++ != 94){
        console->display[i] = 0x47;
    }
    
    console->line = 20;
    console->display[55] = 0x44;//1
    console->display[95] = 0x45;//1
    
    console->line = 21;
    
    i = 55;
    while(i++ != 94){
        console->display[i] = 0x46;
    }
    
    console->charset = 0;
    console->bgblitclr = 0;
    console->blitclr = 15;
/*
    blits("209.195.113.136:23214                               ", 43, 14,0);
*/
    blits("digida.ca:23214                                     ", 43, 14,0);
    blits("Username:", 45, 17,0);
    blits("Password:", 45, 20,0);
    console->bgblitclr = 0;
    blits("steve                                  ", 56, 17,0);
    blits("******                                 ", 56, 20,0);
    console->cursor_y = 14;
    console->cursor_x = 64;
    console->pressedX = 0;
    console->pressedY = 0;
    __COM_CONNECT(); // REMOVE THIS AUTOCONNET
    while(1){
        f=0;
/*
        if(d++==1024){
            if(m){
                q++;
            }else{
                q--;
            }
            if(q%3==2)m=!m;
            d=0;
            console->charset = 1;
            while(f != 48){
                console->blitclr = (f/16+q)%3+0x29;
                //if(i % (int)16 < (int)12){
                if((f % 16) < 13)
                    switch(f){
                        case 0x26:
                        case 0x27:
                        case 0x25:
                        case 0x19:
                        case 0x0C:
                        case 0x2C:
                            break;
                        default:

                            //if(((int)(i % 16)) <= ((int)12)){
                                blitc(f, f % 16+41, f/16+7, 0);
                            //}

                            break;
                    }
                //}
                f++;
            }
            console->charset = 0;
        }
*/
        
        console->line = console->cursor_y;
        console->bgblitclr = 0;
        console->blitclr = 15;
        while(console->kb_pos != console->kb_start){
            switch(console->kb_key){
                case 8:
                    if(console->cursor_x > (unsigned char)43)console->display[--console->cursor_x] = ' ';
                    break;
                case '\r':
                    console->kb_start = console->kb_pos;
                    __COM_CONNECT();
                    break;
                default:
                    if(console->cursor_y ==20 && console->cursor_x > 55 && console->cursor_x <85){
                        pwlen = console->cursor_x - 56;
                        pwmem[pwlen] = console->kb_key;
                        pwlen++;
                        console->display[console->cursor_x++] = '*';
                    }else{
                        console->display[console->cursor_x++] = console->kb_key; 
                    }
                    
                    //console->cursor_x++;
                    break;
            }
            ++console->kb_start;
        }
        
        if(console->mouseY > 13-2 && console->mouseY < 17-2){
            if(console->mouseX > 97 && console->mouseX < 109){
                if(i != 1){
                    if(i != 0){
                        __COM_rst(i);
                    }
                    i = 1;
                    console->bgblitclr = 250;//200
                    e = 98;
                    console->line = 15-2;
                    while(e != 109){
                        console->display[e] = console->display[e];
                        e++;
                    }
                }
            }else if(console->mouseX > 109 && console->mouseX < 120){
                if(i != 2){
                    if(i != 0){
                        __COM_rst(i);
                    }
                    i = 2;
                    console->bgblitclr = 248;
                    e = 110;
                    console->line = 15-2;
                    while(e != 120){
                        console->display[e] = console->display[e];
                        e++;
                    }
                }
            }else if(i != 0){
                __COM_rst(i);
                i = 0;
            }
        }else if (i != 0){
            __COM_rst(i);
            i = 0;
        }
        if(console->pressedY == 17){
            console->cursor_y =17;
            console->cursor_x = console->pressedX;
            //console->line = 17;
            console->pressedY = 0;
            console->pressedX = 0;
        }
        if(console->pressedY == 20){
            console->cursor_y =20;
            console->cursor_x = console->pressedX;
            //console->line = 20;
            console->pressedY = 0;
            console->pressedX = 0;
        }
        if(console->pressedY == 14){
            console->cursor_y =14;
            console->cursor_x = console->pressedX;
            //console->line = 14;
            
            console->pressedY = 0;
            console->pressedX = 0;
        }
        if(i != 0 && console->pressedY > 13-2 && console->pressedY < 17-2){
            __COM_cli(i);
            console->pressedY = 0;
        }
    }
}

void __IDE_MAIN(){
    int i = 0;
    
    while(i!=64){
        lineLens[i] = 6;
        lineStates[i] = 0;
        needsRepaint[i] = 0;
        i++;
    };
    handle_connection_init();
    while(1);
    
}

void loadsrc(){
    __IDE_MAIN();
}

void initui(){
    short i;
    console->blitclr = 15;
    fill(0x20, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    
    console->bgblitclr = 0;
    i = SCREEN_H;
    while(i-->0){
        console->line = i;
        console->display[6] = 15;
    }
    blits(" RPC C IDE ~ cc65 ~ DISCONNECTED", 1, 1, 0);
    
    blits("\x0F File \x0F Edit \x0F Project \x0F Build \x0F Help \x0F", 0, 3, 0);

    makebox(0,0,SCREEN_W-1, SCREEN_H-1);
    i=SCREEN_W;
    makebox(0,0,SCREEN_W-1, 2);
    
    console->line = 2;
    console->display[0] = 11;
    console->display[SCREEN_W-1] = 19;
    console->display[7] = 18;
    console->display[14] = 18;
    console->display[24] = 18;
    console->display[32] = 18;
    console->display[39] = 18;
    console->line = SCREEN_H - CONSOLE_HEIGHT-2;
    while(i-->0){
        console->display[i] = 26;
    }
    console->display[6] = 17;
    console->display[0] = 11;
    console->display[SCREEN_W-1] = 19;
    console->line = SCREEN_H;
    while(console->line-->SCREEN_H-CONSOLE_HEIGHT-1){
        console->display[6] = ' ';
    }
    i = SCREEN_W;
    console->line = 4;
    while(i-->0){
        console->display[i] = 26;
    }
    console->display[6] = 18;
    console->display[7] = 17;
    console->display[14] = 17;
    console->display[24] = 17;
    console->display[32] = 17;
    console->display[39] = 17;
    console->display[0] = 11;
    console->display[SCREEN_W-1] = 19;
    console->cursor_x = 7;
    console->cursor_y = 5;
    blx = 7;
    bly = 5;
}

void hack(){
    unsigned char state = 0;
    initui();
    loadsrc();
    console->releasedX = 0;
    console->releasedY = 0;
    console->pressedX = 0;
    console->pressedY = 0;
    while(1) kbrd();
}

unsigned char paldat[1024] = {
    0, 0, 0, 0,
    1, 0, 0, 170,
    2, 0, 170, 0,
    3, 0, 170, 170,
    4, 170, 0, 0,
    5, 170, 0, 170,
    6, 170, 85, 0,
    7, 170, 170, 170,
    8, 85, 85, 85,
    9, 85, 85, 255,
    10, 85, 255, 85,
    11, 85, 255, 255,
    12, 255, 85, 85,
    13, 255, 85, 255,
    14, 255, 255, 85,
    15, 255, 255, 255,
    16, 0, 0, 0,
    17, 16, 16, 16,
    18, 32, 32, 32,
    19, 53, 53, 53,
    20, 69, 69, 69,
    21, 85, 85, 85,
    22, 101, 101, 101,
    23, 117, 117, 117,
    24, 138, 138, 138,
    25, 154, 154, 154,
    26, 170, 170, 170,
    27, 186, 186, 186,
    28, 202, 202, 202,
    29, 223, 223, 223,
    30, 239, 239, 239,
    31, 255, 255, 255,
    32, 0, 0, 255,
    33, 65, 0, 255,
    34, 130, 0, 255,
    35, 190, 0, 255,
    36, 255, 0, 255,
    37, 255, 0, 190,
    38, 255, 0, 130,
    39, 255, 0, 65,
    40, 255, 0, 0,
    41, 255, 65, 0,
    42, 255, 130, 0,
    43, 255, 190, 0,
    44, 255, 255, 0,
    45, 190, 255, 0,
    46, 130, 255, 0,
    47, 65, 255, 0,
    48, 0, 255, 0,
    49, 0, 255, 65,
    50, 0, 255, 130,
    51, 0, 255, 190,
    52, 0, 255, 255,
    53, 0, 190, 255,
    54, 0, 130, 255,
    55, 0, 65, 255,
    56, 130, 130, 255,
    57, 158, 130, 255,
    58, 190, 130, 255,
    59, 223, 130, 255,
    60, 255, 130, 255,
    61, 255, 130, 223,
    62, 255, 130, 190,
    63, 255, 130, 158,
    64, 255, 130, 130,
    65, 255, 158, 130,
    66, 255, 190, 130,
    67, 255, 223, 130,
    68, 255, 255, 130,
    69, 223, 255, 130,
    70, 190, 255, 130,
    71, 158, 255, 130,
    72, 130, 255, 130,
    73, 130, 255, 158,
    74, 130, 255, 190,
    75, 130, 255, 223,
    76, 130, 255, 255,
    77, 130, 223, 255,
    78, 130, 190, 255,
    79, 130, 158, 255,
    80, 186, 186, 255,
    81, 202, 186, 255,
    82, 223, 186, 255,
    83, 239, 186, 255,
    84, 255, 186, 255,
    85, 255, 186, 239,
    86, 255, 186, 223,
    87, 255, 186, 202,
    88, 255, 186, 186,
    89, 255, 202, 186,
    90, 255, 223, 186,
    91, 255, 239, 186,
    92, 255, 255, 186,
    93, 239, 255, 186,
    94, 223, 255, 186,
    95, 202, 255, 186,
    96, 186, 255, 186,
    97, 186, 255, 202,
    98, 186, 255, 223,
    99, 186, 255, 239,
    100, 186, 255, 255,
    101, 186, 239, 255,
    102, 186, 223, 255,
    103, 186, 202, 255,
    104, 0, 0, 113,
    105, 28, 0, 113,
    106, 57, 0, 113,
    107, 85, 0, 113,
    108, 113, 0, 113,
    109, 113, 0, 85,
    110, 113, 0, 57,
    111, 113, 0, 28,
    112, 113, 0, 0,
    113, 113, 28, 0,
    114, 113, 57, 0,
    115, 113, 85, 0,
    116, 113, 113, 0,
    117, 85, 113, 0,
    118, 57, 113, 0,
    119, 28, 113, 0,
    120, 0, 113, 0,
    121, 0, 113, 28,
    122, 0, 113, 57,
    123, 0, 113, 85,
    124, 0, 113, 113,
    125, 0, 85, 113,
    126, 0, 57, 113,
    127, 0, 28, 113,
    128, 57, 57, 113,
    129, 69, 57, 113,
    130, 85, 57, 113,
    131, 97, 57, 113,
    132, 113, 57, 113,
    133, 113, 57, 97,
    134, 113, 57, 85,
    135, 113, 57, 69,
    136, 113, 57, 57,
    137, 113, 69, 57,
    138, 113, 85, 57,
    139, 113, 97, 57,
    140, 113, 113, 57,
    141, 97, 113, 57,
    142, 85, 113, 57,
    143, 69, 113, 57,
    144, 57, 113, 57,
    145, 57, 113, 69,
    146, 57, 113, 85,
    147, 57, 113, 97,
    148, 57, 113, 113,
    149, 57, 97, 113,
    150, 57, 85, 113,
    151, 57, 69, 113,
    152, 81, 81, 113,
    153, 89, 81, 113,
    154, 97, 81, 113,
    155, 105, 81, 113,
    156, 113, 81, 113,
    157, 113, 81, 105,
    158, 113, 81, 97,
    159, 113, 81, 89,
    160, 113, 81, 81,
    161, 113, 89, 81,
    162, 113, 97, 81,
    163, 113, 105, 81,
    164, 113, 113, 81,
    165, 105, 113, 81,
    166, 97, 113, 81,
    167, 89, 113, 81,
    168, 81, 113, 81,
    169, 81, 113, 89,
    170, 81, 113, 97,
    171, 81, 113, 105,
    172, 81, 113, 113,
    173, 81, 105, 113,
    174, 81, 97, 113,
    175, 81, 89, 113,
    176, 0, 0, 65,
    177, 16, 0, 65,
    178, 32, 0, 65,
    179, 49, 0, 65,
    180, 65, 0, 65,
    181, 65, 0, 49,
    182, 65, 0, 32,
    183, 65, 0, 16,
    184, 65, 0, 0,
    185, 65, 16, 0,
    186, 65, 32, 0,
    187, 65, 49, 0,
    188, 65, 65, 0,
    189, 49, 65, 0,
    190, 32, 65, 0,
    191, 16, 65, 0,
    192, 0, 65, 0,
    193, 0, 65, 16,
    194, 0, 65, 32,
    195, 0, 65, 49,
    196, 0, 65, 65,
    197, 0, 49, 65,
    198, 0, 32, 65,
    199, 0, 16, 65,
    200, 32, 32, 65,
    201, 40, 32, 65,
    202, 49, 32, 65,
    203, 57, 32, 65,
    204, 65, 32, 65,
    205, 65, 32, 57,
    206, 65, 32, 49,
    207, 65, 32, 40,
    208, 65, 32, 32,
    209, 65, 40, 32,
    210, 65, 49, 32,
    211, 65, 57, 32,
    212, 65, 65, 32,
    213, 57, 65, 32,
    214, 49, 65, 32,
    215, 40, 65, 32,
    216, 32, 65, 32,
    217, 32, 65, 40,
    218, 32, 65, 49,
    219, 32, 65, 57,
    220, 32, 65, 65,
    221, 32, 57, 65,
    222, 32, 49, 65,
    223, 32, 40, 65,
    224, 45, 45, 65,
    225, 49, 45, 65,
    226, 53, 45, 65,
    227, 61, 45, 65,
    228, 65, 45, 65,
    229, 65, 45, 61,
    230, 65, 45, 53,
    231, 65, 45, 49,
    232, 65, 45, 45,
    233, 65, 49, 45,
    234, 65, 53, 45,
    235, 65, 61, 45,
    236, 65, 65, 45,
    237, 61, 65, 45,
    238, 53, 65, 45,
    239, 49, 65, 45,
    240, 45, 65, 45,
    241, 45, 65, 49,
    242, 45, 65, 53,
    243, 45, 65, 61,
    244, 45, 65, 65,
    245, 45, 61, 65,
    246, 45, 53, 65,
    247, 45, 49, 65,
    248, 128, 128, 128,
    249, 190, 190, 190,
    250, 32, 140, 0,
    251, 140, 32, 0,
    252, 0, 0, 0,
    253, 0, 0, 0,
    254, 0, 0, 0,
    255, 0, 0, 0,
};

void main(){
    unsigned int xinc = 0;
    unsigned int magic = 0;
    
    rb_set_window((void*) RB);
    prompt[1] = ':';
    
    disk = (Disk*) RB;
    console = (Console*) RB;
    raw = (RawRB*) RB;
    Nshort = (NBTShort*) RB;
    sck = (Modem*) RB;
    codeline = (CodeLine*) &clmem;
    codemeta = (CodeMeta*) RB;
    mmu = (MMU*) RB;
    map_console();
    map_hdd();
    map_mmu();
    rb_enable();
    map_hdd();
    map_mmu();
    map_console();
    
    fill(0x20, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    console->cursor_mode = 0;
    console->cursor_y = 0;
    console->cursor_x = 0;
    console->blitclr = 15;
    console->bgblitclr = 0;
    console->charset = 0;
    xinc = 0;
    while(xinc != 256){
        magic = xinc * 4;
        setpall(paldat[magic], paldat[magic+1], paldat[magic+2], paldat[magic+3]);
        xinc++;
    }
    console->blitclr = 0x33;
    if(1){
        hack();
        return;
    }
    
    
}
