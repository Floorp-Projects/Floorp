/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*
 * JS File object
 */

#ifndef _WINDOWS
#  include <strings.h>
#  include <stdio.h>
#  include <stdlib.h>
#  include <unistd.h>
#else
#  include "direct.h"
#endif

#include "jsapi.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsdate.h"
#include "jsdbgapi.h"
#include "jsemit.h"
#include "jsfun.h"
#include "jslock.h"
#include "jsobj.h"
#include "jsparse.h"
#include "jsscan.h"
#include "jsscope.h"
#include "jsscript.h"
#include "jsstr.h"
#include "jsutil.h" /* Added by JSIFY */

#if JS_HAS_FILE_OBJECT

/* NSPR dependencies */
#include "prio.h"
#include "prerror.h"
#include "jsutil.h" /* Added by JSIFY */

typedef enum JSFileErrNum {
#define MSG_DEF(name, number, count, exception, format) \
    name = number,
#include "jsfile.msg"
#undef MSG_DEF
    JSShellErr_Limit
#undef MSGDEF
} JSFileErrNum;

#define SPECIAL_FILE_STRING "Special File"
#define CURRENTDIR_PROPERTY "currentDir"
#define FILE_CONSTRUCTOR    "File"
#define PIPE_SYMBOL         '|'

#define ASCII   0
#define UTF8    1
#define UCS2    2

#define asciistring   "ascii"
#define utfstring     "binary"
#define unicodestring "unicode"

/* Platform dependent code goes here. */

#define MAX_PATH_LENGTH 1024 /* XXX dirty */
#define URL_PREFIX      "file://"

#ifdef XP_MAC
#  define LINEBREAK             "\012"
#  define LINEBREAK_LEN 1
#  define FILESEPARATOR         ':'
#  define FILESEPARATOR2        '\0'
#  define CURRENT_DIR "HARD DISK:Desktop Folder"

static JSBool
isAbsolute(char*name)
{
    return (name[0]!=FILESEPARATOR);
}

/* We assume: base is a valid absolute path, not ending with :
          name is a valid relative path. */
static char *
combinePath(JSContext *cx, char *base, char *name)
{
    char* tmp = (char*)JS_malloc(cx, strlen(base)+strlen(name)+2);

    if (!tmp)  return NULL;
    strcpy(tmp, base);
    i=strlen(base)-1;
    if (base[i]!=FILESEPARATOR) {
      tmp[i+1]=FILESEPARATOR;
      tmp[i+2]='\0';
    }
    strcat(tmp, name);
    return tmp;
}

/* Extracts the filename from a path. Returned string must be freed */
static char *
fileBaseName(JSContext *cx, char * pathname)
{
    jsint index, aux;
    char *basename;

    index=strlen(pathname)-1;
    aux=index;
    while ((index>=0)&&(pathname[index]!=FILESEPARATOR)) index--;
    basename = (char*)JS_malloc(cx, aux-index+1);
    if (!basename)  return NULL;
    strncpy(basename,&pathname[index+1],aux-index);
    basename[aux-index]='\0';
    return basename;
}

/* Extracts the directory name from a path. Returned string must be freed */
static char *
fileDirectoryName(JSContext *cx, char * pathname)
{
    jsint index;
    char *dirname;

    index=strlen(pathname)-1;
    while ((index>0)&&(pathname[index]!=FILESEPARATOR)) index--;

    if (index>=0) {
        dirname=(char*)JS_malloc(cx, index+2);
        if (!dirname) return NULL;
        strncpy(dirname, pathname, index);
        dirname[index]=FILESEPARATOR;
        dirname[index+1]='\0';
    } else
        dirname=JS_strdup(cx, pathname);
    return dirname;
}

#else
#  if defined(XP_PC) || defined(_WINDOWS) || defined(OS2)
#    define LINEBREAK           "\015\012"
#    define LINEBREAK_LEN       2
#    define FILESEPARATOR       '\\'
#    define FILESEPARATOR2      '/'
#    define CURRENT_DIR "c:\\"
#    define POPEN _popen

static JSBool
filenameHasAPipe(char *filename){
#ifdef XP_MAC
    /* pipes are not supported on the MAC */
    return JS_FALSE;
#else
    if(!filename) return JS_FALSE;
    return filename[0]==PIPE_SYMBOL || filename[strlen(filename)-1]==PIPE_SYMBOL;
#endif
}

static JSBool
isAbsolute(char*name)
{
    if (strlen(name)<2)
    return JS_FALSE;

    if (name[1]==':')
    return JS_TRUE;

    return JS_FALSE; /* First approximation, ignore "/tmp" case.. */
}

/* We assume: base is a valid absolute path, starting with x:, ending with /.
          name is a valid relative path, and may have a drive selector. */
static char *
combinePath(JSContext *cx, char *base, char*name)
{
    jsint i;
    char *tmp, *tmp1;

    if ((strlen(name)>=2)&&(name[1]==':')) { /* remove a drive selector if there's one.*/
    tmp1=&name[2];
    } else
    tmp1=name;

    tmp = (char*)JS_malloc(cx, strlen(base)+strlen(tmp1)+2);
    if (!tmp) return NULL;
    strcpy(tmp, base);
    i=strlen(base)-1;
    if ((base[i]!=FILESEPARATOR)&&(base[i]!=FILESEPARATOR2)) {
      tmp[i+1]=FILESEPARATOR;
      tmp[i+2]='\0';
    }
    strcat(tmp, tmp1);
    return tmp;
}

/* returned string must be freed */
static char *
fileBaseName(JSContext *cx, char * pathname)
{
    jsint index, aux;
    char *basename;

    /* First, get rid of the drive selector */
    if ((strlen(pathname)>=2)&&(pathname[1]==':')) {
        pathname=&pathname[2];
    }

    index=strlen(pathname)-1;
    while ((index>0)&&((pathname[index]==FILESEPARATOR)||
                       (pathname[index]==FILESEPARATOR2))) index--;
    aux=index;
    while ((index>=0)&&(pathname[index]!=FILESEPARATOR)&&
                      (pathname[index]!=FILESEPARATOR2)) index--;
    basename = (char*)JS_malloc(cx, aux-index+3);
    if (!basename)  return NULL;
    strncpy(basename,&pathname[index+1],aux-index);
    basename[aux-index]='\0';
    return basename;
}

/* returned string must be freed */
static char *
fileDirectoryName(JSContext *cx, char * pathname)
{
    jsint index;
    char  *dirname, *backpathname;
    char  drive='\0';

    backpathname=pathname;
    /* First, get rid of the drive selector */
    if ((strlen(pathname)>=2)&&(pathname[1]==':')) {
        drive=pathname[0];
        pathname=&pathname[2];
    }

    index=strlen(pathname)-1;
    while ((index>0)&&((pathname[index]==FILESEPARATOR)||
                       (pathname[index]==FILESEPARATOR2))) index--;
    while ((index>0)&&(pathname[index]!=FILESEPARATOR)&&
                      (pathname[index]!=FILESEPARATOR2)) index--;

    if (index>=0){
        dirname=(char*)JS_malloc(cx, index+4);
        if (!dirname)  return NULL;

        if (drive!='\0') {
            dirname[0]=toupper(drive);
            dirname[1]=':';
            strncpy(&dirname[2],pathname, index);
            dirname[index+2]=FILESEPARATOR;
            dirname[index+3]='\0';
        } else {
            strncpy(dirname, pathname, index);
            dirname[index]=FILESEPARATOR;
            dirname[index+1]='\0';
        }
    } else
        dirname=JS_strdup(cx, backpathname); /* may include drive selector */

    return dirname;
}

#  else
#    ifdef XP_UNIX
#      define LINEBREAK         "\012"
#      define LINEBREAK_LEN     1
#      define FILESEPARATOR     '/'
#      define FILESEPARATOR2    '\0'
#      define CURRENT_DIR "/"
#      define POPEN popen

static JSBool
isAbsolute(char*name)
{
    return (name[0]==FILESEPARATOR);
}

/* We assume: base is a valid absolute path[, ending with /]. name is a valid relative path. */
static char *
combinePath(JSContext *cx, char *base, char*name)
{
    jsint i;
    char * tmp;
    tmp = (char*)JS_malloc(cx, strlen(base)+strlen(name)+2);
    if (!tmp) return NULL;
    strcpy(tmp, base);
    i=strlen(base)-1;
    if (base[i]!=FILESEPARATOR) {
      tmp[i+1]=FILESEPARATOR;
      tmp[i+2]='\0';
    }
    strcat(tmp, name);
    return tmp;
}

/* returned string must be freed */
static char *
fileBaseName(JSContext *cx, char * pathname)
{
    jsint index, aux;
    char *basename;

    index=strlen(pathname)-1;
    while ((index>0)&&((pathname[index]==FILESEPARATOR)||
                       (pathname[index]==FILESEPARATOR2))) index--;
    aux=index;
    while ((index>=0)&&(pathname[index]!=FILESEPARATOR)&&
                      (pathname[index]!=FILESEPARATOR2)) index--;
    basename = (char*)JS_malloc(cx, aux-index+1);
    if (!basename)  return NULL;
    strncpy(basename,&pathname[index+1],aux-index);
    basename[aux-index]='\0';
    return basename;
}

/* returned string must be freed */
static char *
fileDirectoryName(JSContext *cx, char * pathname)
{
    jsint index;
    char *dirname;

    index=strlen(pathname)-1;
    while ((index>0)&&((pathname[index]==FILESEPARATOR)||
                       (pathname[index]==FILESEPARATOR2))) index--;
    while ((index>0)&&(pathname[index]!=FILESEPARATOR)&&
                      (pathname[index]!=FILESEPARATOR2)) index--;

    if (index>=0) {
        dirname=(char*)JS_malloc(cx, index+2);
        if (!dirname)  return NULL;
        strncpy(dirname, pathname, index);
        dirname[index]=FILESEPARATOR;
        dirname[index+1]='\0';
    } else
        dirname=JS_strdup(cx, pathname);
    return dirname;
}

#    endif /* UNIX */
#  endif /* WIN */
#endif /* MAC */


/* returned string must be freed.. */
static char *
absolutePath(JSContext *cx, char * path)
{
    JSObject *obj;
    JSString *str;
    jsval prop;

    if (isAbsolute(path))
    return JS_strdup(cx, path);
    else {
        obj=JS_GetGlobalObject(cx);
        if (!JS_GetProperty(cx, obj, FILE_CONSTRUCTOR, &prop)) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                 JSFILEMSG_FILE_CONSTRUCTOR_UNDEFINED_ERROR);
            return JS_strdup(cx, path);
        }
        obj=JSVAL_TO_OBJECT(prop);
        if (!JS_GetProperty(cx, obj, CURRENTDIR_PROPERTY, &prop)) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                 JSFILEMSG_FILE_CURRENTDIR_UNDEFINED_ERROR);
            return JS_strdup(cx, path);
        }
        str=JS_ValueToString(cx, prop);
        if (!str ) {
            return JS_strdup(cx, path);
        }
        return combinePath(cx, JS_GetStringBytes(str), path); /* should we have an array of current dirs indexed by drive for windows? */
    }
}

/* this function should be in the platform specific side. */
/* :: is not handled. */
static char *
canonicalPath(JSContext *cx, char *path)
{
    char *tmp1, *tmp;
    char *base, *dir, *current, *result;
    jsint c;
    jsint back=0;
	unsigned int i=0, j=strlen(path)-1;

	/* TODO: this is probably optional */
	/* Remove possible spaces in the beginning and end */
	while(i<strlen(path)-1 && path[i]==' ') i++;
	while(j>=0 && path[j]==' ') j--;

	tmp = JS_malloc(cx, j-i+2);
	strncpy(tmp, &path[i], j-i+1);
	tmp[j-i+1]='\0';

	strcpy(path, tmp);
	JS_free(cx, tmp);

    /* pipe support */
    if(filenameHasAPipe(path))
        return JS_strdup(cx, path);
    /* file:// support */
    if(!strncmp(path, URL_PREFIX, strlen(URL_PREFIX)))
        return strdup(&path[strlen(URL_PREFIX)-1]);

    if (!isAbsolute(path))
        tmp1=absolutePath(cx, path);
    else
        tmp1=JS_strdup(cx, path);

    result=JS_strdup(cx, "");

    current=tmp1;

    base=fileBaseName(cx, current);
    dir=fileDirectoryName(cx, current);
    while (strcmp(dir, current)) {
        if (!strcmp(base,"..")) {
            back++;
        } else
        if(!strcmp(base,".")){

        } else {
            if (back>0)
                back--;
            else {
                tmp=result;
                result=JS_malloc(cx, strlen(base)+1+strlen(tmp)+1);
                if (!result) {
                    JS_free(cx, dir);
                    JS_free(cx, base);
                    JS_free(cx, current);
                    return NULL;
                }
                strcpy(result, base);
                c=strlen(result);
                if (strlen(tmp)>0) {
                    result[c]=FILESEPARATOR;
                    result[c+1]='\0';
                    strcat(result, tmp);
                }
                JS_free(cx, tmp);
            }
        }
        JS_free(cx, current);
        JS_free(cx, base);
        current=dir;
        base= fileBaseName(cx, current);
        dir=fileDirectoryName(cx, current);
    }
    tmp=result;
    result=JS_malloc(cx, strlen(dir)+1+strlen(tmp)+1);
    if (!result) {
        JS_free(cx, dir);
        JS_free(cx, base);
        JS_free(cx, current);
        return NULL;
    }
    strcpy(result, dir);
    c=strlen(result);
    if (strlen(tmp)>0) {
        if ((result[c-1]!=FILESEPARATOR)&&(result[c-1]!=FILESEPARATOR2)) {
            result[c]=FILESEPARATOR;
            result[c+1]='\0';
        }
        strcat(result, tmp);
    }
    JS_free(cx, tmp);
    JS_free(cx, dir);
    JS_free(cx, base);
    JS_free(cx, current);

    return result;
}

/* ---------------- text format conversion ------------------------- */
/* The following is ripped from libi18n/unicvt.c and include files.. */

/*
 * UTF8 defines and macros
 */
#define ONE_OCTET_BASE          0x00    /* 0xxxxxxx */
#define ONE_OCTET_MASK          0x7F    /* x1111111 */
#define CONTINUING_OCTET_BASE   0x80    /* 10xxxxxx */
#define CONTINUING_OCTET_MASK   0x3F    /* 00111111 */
#define TWO_OCTET_BASE          0xC0    /* 110xxxxx */
#define TWO_OCTET_MASK          0x1F    /* 00011111 */
#define THREE_OCTET_BASE        0xE0    /* 1110xxxx */
#define THREE_OCTET_MASK        0x0F    /* 00001111 */
#define FOUR_OCTET_BASE         0xF0    /* 11110xxx */
#define FOUR_OCTET_MASK         0x07    /* 00000111 */
#define FIVE_OCTET_BASE         0xF8    /* 111110xx */
#define FIVE_OCTET_MASK         0x03    /* 00000011 */
#define SIX_OCTET_BASE          0xFC    /* 1111110x */
#define SIX_OCTET_MASK          0x01    /* 00000001 */

#define IS_UTF8_1ST_OF_1(x) (( (x)&~ONE_OCTET_MASK  ) == ONE_OCTET_BASE)
#define IS_UTF8_1ST_OF_2(x) (( (x)&~TWO_OCTET_MASK  ) == TWO_OCTET_BASE)
#define IS_UTF8_1ST_OF_3(x) (( (x)&~THREE_OCTET_MASK) == THREE_OCTET_BASE)
#define IS_UTF8_1ST_OF_4(x) (( (x)&~FOUR_OCTET_MASK ) == FOUR_OCTET_BASE)
#define IS_UTF8_1ST_OF_5(x) (( (x)&~FIVE_OCTET_MASK ) == FIVE_OCTET_BASE)
#define IS_UTF8_1ST_OF_6(x) (( (x)&~SIX_OCTET_MASK  ) == SIX_OCTET_BASE)
#define IS_UTF8_2ND_THRU_6TH(x) \
                    (( (x)&~CONTINUING_OCTET_MASK  ) == CONTINUING_OCTET_BASE)
#define IS_UTF8_1ST_OF_UCS2(x) \
            IS_UTF8_1ST_OF_1(x) \
            || IS_UTF8_1ST_OF_2(x) \
            || IS_UTF8_1ST_OF_3(x)


#define MAX_UCS2            0xFFFF
#define DEFAULT_CHAR        0x003F  /* Default char is "?" */
#define BYTE_MASK           0xBF
#define BYTE_MARK           0x80


/* Function: one_ucs2_to_utf8_char
 *
 * Function takes one UCS-2 char and writes it to a UTF-8 buffer.
 * We need a UTF-8 buffer because we don't know before this
 * function how many bytes of utf-8 data will be written. It also
 * takes a pointer to the end of the UTF-8 buffer so that we don't
 * overwrite data. This function returns the number of UTF-8 bytes
 * of data written, or -1 if the buffer would have been overrun.
 */

#define LINE_SEPARATOR      0x2028
#define PARAGRAPH_SEPARATOR 0x2029
static int16 one_ucs2_to_utf8_char(unsigned char *tobufp,
        unsigned char *tobufendp, uint16 onechar)
{

     int16 numUTF8bytes = 0;

    if((onechar == LINE_SEPARATOR)||(onechar == PARAGRAPH_SEPARATOR))
    {
        strcpy((char*)tobufp, "\n");
        return strlen((char*)tobufp);;
    }

        if (onechar < 0x80) {               numUTF8bytes = 1;
        } else if (onechar < 0x800) {       numUTF8bytes = 2;
        } else if (onechar <= MAX_UCS2) {   numUTF8bytes = 3;
        } else { numUTF8bytes = 2;
                 onechar = DEFAULT_CHAR;
        }

        tobufp += numUTF8bytes;

        /* return error if we don't have space for the whole character */
        if (tobufp > tobufendp) {
            return(-1);
        }


        switch(numUTF8bytes) {

            case 3: *--tobufp = (onechar | BYTE_MARK) & BYTE_MASK; onechar >>=6;
                    *--tobufp = (onechar | BYTE_MARK) & BYTE_MASK; onechar >>=6;
                    *--tobufp = onechar |  THREE_OCTET_BASE;
                    break;

            case 2: *--tobufp = (onechar | BYTE_MARK) & BYTE_MASK; onechar >>=6;
                    *--tobufp = onechar | TWO_OCTET_BASE;
                    break;
            case 1: *--tobufp = (unsigned char)onechar;  break;
        }

        return(numUTF8bytes);
}

/*
 * utf8_to_ucs2_char
 *
 * Convert a utf8 multibyte character to ucs2
 *
 * inputs: pointer to utf8 character(s)
 *         length of utf8 buffer ("read" length limit)
 *         pointer to return ucs2 character
 *
 * outputs: number of bytes in the utf8 character
 *          -1 if not a valid utf8 character sequence
 *          -2 if the buffer is too short
 */
static int16
utf8_to_ucs2_char(const unsigned char *utf8p, int16 buflen, uint16 *ucs2p)
{
    uint16 lead, cont1, cont2;

    /*
     * Check for minimum buffer length
     */
    if ((buflen < 1) || (utf8p == NULL)) {
        return -2;
    }
    lead = (uint16) (*utf8p);

    /*
     * Check for a one octet sequence
     */
    if (IS_UTF8_1ST_OF_1(lead)) {
        *ucs2p = lead & ONE_OCTET_MASK;
        return 1;
    }

    /*
     * Check for a two octet sequence
     */
    if (IS_UTF8_1ST_OF_2(*utf8p)) {
        if (buflen < 2)
            return -2;
        cont1 = (uint16) *(utf8p+1);
        if (!IS_UTF8_2ND_THRU_6TH(cont1))
            return -1;
        *ucs2p =  (lead & TWO_OCTET_MASK) << 6;
        *ucs2p |= cont1 & CONTINUING_OCTET_MASK;
        return 2;
    }

    /*
     * Check for a three octet sequence
     */
    else if (IS_UTF8_1ST_OF_3(lead)) {
        if (buflen < 3)
            return -2;
        cont1 = (uint16) *(utf8p+1);
        cont2 = (uint16) *(utf8p+2);
        if (   (!IS_UTF8_2ND_THRU_6TH(cont1))
            || (!IS_UTF8_2ND_THRU_6TH(cont2)))
            return -1;
        *ucs2p =  (lead & THREE_OCTET_MASK) << 12;
        *ucs2p |= (cont1 & CONTINUING_OCTET_MASK) << 6;
        *ucs2p |= cont2 & CONTINUING_OCTET_MASK;
        return 3;
    }
    else { /* not a valid utf8/ucs2 character */
        return -1;
    }
}

/* ------------------- file methods declared here  ------------------------- */
/* a few forward declarations... */
static JSClass file_class;
JSObject* NewFileObject(JSContext *cx, char *bytes);
JSObject* NewFileObjectFromFILE(JSContext *cx, FILE *f, char *filename, JSBool open);
static JSBool file_close(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

typedef struct JSFile {
    char        *path;          /* the path to the file. */
    PRFileDesc* handle;         /* the handle for the file, if opened.  */
    FILE        *nativehandle;  /* native handle, for stuff NSPR doesn't do. */
    JSBool      opened;
    JSString    *linebuffer;    /* temp buffer used by readln. */
    int32       mode;           /* mode used to open the file: read, write, append, create, etc.. */
    int32       type;           /* Asciiz, utf, unicode */
    char        byteBuffer[3];  /* bytes read in advance by js_FileRead ( UTF8 encoding ) */
    jsint       nbBytesInBuf;   /* number of bytes stored in the buffer above */
    jschar      charBuffer;     /* character read in advance by readln ( mac files only ) */
    JSBool      charBufferUsed; /* flag indicating if the buffer above is being used */
    JSBool      randomAccess;   /* can the file be randomly accessed? false for stdin, and
                                 UTF-encoded files. */
    JSBool      autoflush;      /* should we force a flush for each line break? */
} JSFile;

/* ------------------------------- Helper functions ---------------------------- */
/* Ripped off from lm_win.c .. */
/* where is strcasecmp?.. for now, it's case sensitive..
 *
 * strcasecmp is in strings.h, but on windows it's called _stricmp...
 * will need to #ifdef this
 * */
static int32
file_has_option(char *options, char *name)
{
    char *comma, *equal;
    int32 found = 0;

    for (;;) {
        comma = strchr(options, ',');
        if (comma) *comma = '\0';
        equal = strchr(options, '=');
        if (equal) *equal = '\0';
        if (strcmp(options, name) == 0) {
            if (!equal || strcmp(equal + 1, "yes") == 0)
                found = 1;
            else
                found = atoi(equal + 1);
        }
        if (equal) *equal = '=';
        if (comma) *comma = ',';
        if (found || !comma)
            break;
        options = comma + 1;
    }
    return found;
}


static void
js_ResetBuffers(JSFile * file)
{
    file->charBufferUsed=JS_FALSE;
    file->nbBytesInBuf=0;
}

/* Buffered version of PR_Read. Used by js_FileRead */
static int32
js_BufferedRead(JSFile * f, char*buf, int32 len)
{
    int32 count=0;
    while (f->nbBytesInBuf>0&&len>0) {
        buf[0]=f->byteBuffer[0];
        f->byteBuffer[0]=f->byteBuffer[1];
        f->byteBuffer[1]=f->byteBuffer[2];
        f->nbBytesInBuf--;
        len--;
        buf+=1;
        count++;
    }
    if (len>0) {
        if (f->handle) {
            count+=PR_Read(f->handle, buf, len);
        } else {
            count+=fread(buf, 1, len, f->nativehandle);
        }
    }
    return count;
}

static int32
js_FileRead(JSContext *cx, JSFile * f, jschar*buf, int32 len, int32 mode)
{
    unsigned char*aux;
    int32 count, i;
    jsint remainder;
    unsigned char utfbuf[3];

    if (f->charBufferUsed) {
      buf[0]=f->charBuffer;
      buf++;
      len--;
      f->charBufferUsed=JS_FALSE;
    }

    switch (mode) {
    case ASCII:
      aux = (unsigned char*)JS_malloc(cx, len);
      if (!aux) {
        return 0;
      }
      count=js_BufferedRead(f, aux, len);
      if (count==-1) {
        JS_free(cx, aux);
        return 0;
      }
      for (i=0;i<len;i++) {
        buf[i]=(jschar)aux[i];
      }
      JS_free(cx, aux);
      break;
    case UTF8:
        remainder = 0;
        for (count=0;count<len;count++) {
            i=js_BufferedRead(f, utfbuf+remainder, 3-remainder);
            if (i<=0) {
                return count;
            }
            i=utf8_to_ucs2_char(utfbuf, (int16)i, &buf[count] );
            if (i<0) {
                return count;
            } else {
                if (i==1) {
                    utfbuf[0]=utfbuf[1];
                    utfbuf[1]=utfbuf[2];
                    remainder=2;
                } else
                if (i==2) {
                    utfbuf[0]=utfbuf[2];
                    remainder=1;
                } else
                if (i==3)
                    remainder=0;
            }
        }
        while (remainder>0) {
            f->byteBuffer[f->nbBytesInBuf]=utfbuf[0];
            f->nbBytesInBuf++;
            utfbuf[0]=utfbuf[1];
            utfbuf[1]=utfbuf[2];
            remainder--;
        }
      break;
    case UCS2:
      count=js_BufferedRead(f,(char*)buf, len*2)>>1;
      if (count==-1) {
          return 0;
      }
      break;
    }
    return count;
}

static int32
js_FileSkip(JSFile * f, int32 len, int32 mode)
{
    int32 count, i;
    jsint remainder;
    unsigned char utfbuf[3];
    jschar tmp;

    switch (mode) {
    case ASCII:
        if (f->handle) {
            count=PR_Seek(f->handle, len, PR_SEEK_CUR);
        } else {
            count=fseek(f->nativehandle, len, SEEK_CUR);
        }
        break;
    case UTF8:
        remainder = 0;
        for (count=0;count<len;count++) {
            i=js_BufferedRead(f, utfbuf+remainder, 3-remainder);
            if (i<=0) {
                return 0;
            }
            i=utf8_to_ucs2_char(utfbuf, (int16)i, &tmp );
            if (i<0) {
                return 0;
            } else {

                if (i==1) {
                    utfbuf[0]=utfbuf[1];
                    utfbuf[1]=utfbuf[2];
                    remainder=2;
                } else
                if (i==2) {
                    utfbuf[0]=utfbuf[2];
                    remainder=1;
                } else
                if (i==3)
                    remainder=0;
            }
        }
        while (remainder>0) {
            f->byteBuffer[f->nbBytesInBuf]=utfbuf[0];
            f->nbBytesInBuf++;
            utfbuf[0]=utfbuf[1];
            utfbuf[1]=utfbuf[2];
            remainder--;
        }
      break;
    case UCS2:
        if (f->handle) {
            count=PR_Seek(f->handle, len*2, PR_SEEK_CUR)/2;
        } else {
            count=fseek(f->nativehandle, len*2, SEEK_CUR)/2;
        }
        break;
    }
    return count;
}

static int32
js_FileWrite(JSContext *cx, JSFile* f, jschar*buf, int32 len, int32 mode)
{
    unsigned char*aux;
    int32 count, i,j;
    unsigned char*utfbuf;
    switch (mode) {
    case ASCII:
        aux = (unsigned char*)JS_malloc(cx, len);
        if (!aux)  return 0;

        for (i=0; i<len; i++) {
            aux[i]=buf[i]%256;
        }
        if (f->handle) {
            count = PR_Write(f->handle, aux, len);
        } else {
            count = fwrite(aux, 1, len, f->nativehandle);
        }
        if (count==-1) {
            JS_free(cx, aux);
            return 0;
        }
        JS_free(cx, aux);
        break;
    case UTF8:
        utfbuf = (unsigned char*)JS_malloc(cx, len*3);
        if (!utfbuf)  return 0;
        i=0;
        for (count=0;count<len;count++) {
            j=one_ucs2_to_utf8_char(utfbuf+i, utfbuf+len*3, buf[count]);
            if (j==-1) {
                JS_free(cx, utfbuf);
                return 0;
            }
            i+=j;
        }
        if (f->handle) {
            j = PR_Write(f->handle, utfbuf, i);
        } else {
            j = fwrite(utfbuf, 1, i, f->nativehandle);
        }
        if (j<i) {
            JS_free(cx, utfbuf);
            return 0;
        }
        JS_free(cx, utfbuf);
      break;
    case UCS2:
        if (f->handle) {
            count = PR_Write(f->handle, buf, len*2)>>1;
        } else {
            count = fwrite(buf, 1, len*2, f->nativehandle)>>1;
        }
        if (count==-1) {
            return 0;
        }
        break;
    }
    return count;
}

/* -------------------------------- File methods ------------------------------ */
static JSBool
file_toString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSFile *file;

    file = JS_GetInstancePrivate(cx, obj, &file_class, NULL);

    *rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, file->path));
    return JS_TRUE;
}

static JSBool
file_open(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSFile		*file;
    JSString	*strmode, *strtype;
    char		*ctype;
    char		*mode;
    int32       mask;
    int32       type;
    int         len;

    file = JS_GetInstancePrivate(cx, obj, &file_class, NULL);

    /* A native file that is already open */
    if(file->opened && file->nativehandle){
        return JS_TRUE;
    }

    /* Close before proceeding */
    if (file->opened) {
        file_close(cx, obj, 0, NULL, rval);
    }

    /* Path not defined */
    if (!file->path) return JS_FALSE;
    len = strlen(file->path);

    /* Mode */
    if (argc>=2){
      strmode = JS_ValueToString(cx, argv[1]);
      if (!strmode){
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                JSFILEMSG_SECOND_ARGUMENT_OPEN_NOT_STRING_ERROR, argv[1]);
        return JS_FALSE;
      }
      mode = JS_strdup(cx, JS_GetStringBytes(strmode));
    }else{
        if(file->path[0]==PIPE_SYMBOL){
            mode = JS_strdup(cx, "readOnly");                   /* pipe default mode */
        }else
        if(file->path[len-1]==PIPE_SYMBOL){
            mode = JS_strdup(cx, "writeOnly");                  /* pipe default mode */
        }else{
            mode = JS_strdup(cx, "readWrite, append, create");  /* non-destructive, permissive defaults. */
        }
    }

    /* Process the mode */
    mask=0;
    /* TODO: this is pretty ugly, BTW, we walk thru the string too many times */
    mask|=(file_has_option(mode, "readOnly"))?  PR_RDONLY       :   0;
    mask|=(file_has_option(mode, "writeOnly"))? PR_WRONLY       :   0;
    mask|=(file_has_option(mode, "readWrite"))? PR_RDWR         :   0;
    mask|=(file_has_option(mode, "append"))?    PR_APPEND       :   0;
    mask|=(file_has_option(mode, "create"))?    PR_CREATE_FILE  :   0;
    mask|=(file_has_option(mode, "truncate"))?  PR_TRUNCATE     :   0;
    mask|=(file_has_option(mode, "replace"))?   PR_TRUNCATE     :   0;

    if ((mask&(PR_RDONLY|PR_WRONLY))==0) mask|=PR_RDWR;

    file->autoflush|=(file_has_option(mode, "autoflush"));

    JS_free(cx, mode);

    /* Type */
    if (argc>=1) {
      strtype = JS_ValueToString(cx, argv[0]);
      if (!strtype) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                JSFILEMSG_FIRST_ARGUMENT_OPEN_NOT_STRING_ERROR, argv[0]);
        return JS_FALSE;
      }
      ctype = JS_GetStringBytes(strtype);
    }else{
      ctype = "";
    }

    if (!strcmp(ctype, asciistring))
        type=ASCII;
    else
    if (!strcmp(ctype, unicodestring))
        type=UCS2;
    else
        type=UTF8;

    /* Save the relevant fields */
    file->type=type;
    file->mode=mask;
    file->nativehandle=NULL;
    file->randomAccess=(type!=UTF8);

    /*
		Deal with pipes here. We can't use NSPR for pipes,
		so we have to use POPEN
	*/
    if(file->path[0]==PIPE_SYMBOL || file->path[len-1]==PIPE_SYMBOL){
        if(file->path[0]==PIPE_SYMBOL && file->path[len-1]){
			JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
				JSFILEMSG_BIDIRECTIONAL_PIPE_NOT_SUPPORTED);
				return JS_FALSE;
        }else{
            char pipemode[3];
            if(file->path[0] == PIPE_SYMBOL){
                if(mask & (PR_RDONLY | PR_APPEND | PR_CREATE_FILE | PR_TRUNCATE)){
					JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
						JSFILEMSG_OPEN_MODE_NOT_SUPPORTED_WITH_PIPES,
						mode, file->path);
                    return JS_FALSE;
                }
                /* open(SPOOLER, "| cat -v | lpr -h 2>/dev/null") -- pipe for writing */
                pipemode[0] = 'w';
                pipemode[1] = file->type==UTF8?'b':'t';
                pipemode[2] = '\0';
                file->nativehandle = POPEN(&file->path[1], pipemode);
            }else
            if(file->path[len-1] == PIPE_SYMBOL){
                char *command = JS_malloc(cx, len);

                strncpy(command, file->path, len-1);
				command[len-1]='\0';
                /* open(STATUS, "netstat -an 2>&1 |") */
                pipemode[0] = 'r';
                pipemode[1] = file->type==UTF8?'b':'t';
                pipemode[2] = '\0';
                file->nativehandle = POPEN(command, pipemode);
				JS_free(cx, command);
            }
        }
    }else{
        /* what about the permissions?? Java seems to ignore the problem.. */
        file->handle = PR_Open(file->path, mask, 0644);
    }

    js_ResetBuffers(file);

    /* Set the open flag and return result */
    if (file->handle==NULL && file->nativehandle==NULL){
		/* TODO: error */
        return JS_FALSE;
    }else{
        file->opened=JS_TRUE;
        return JS_TRUE;
    }
}

static JSBool
file_close(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSFile  *file;
    JSStatus status;

    file = JS_GetInstancePrivate(cx, obj, &file_class, NULL);

    status=(file->handle)?PR_Close(file->handle):fclose(file->nativehandle);

    if (status!=0){
      /* TODO: error reporting */
      return JS_FALSE;
    }
    /* Reset values */
    file->handle=NULL;
    file->nativehandle=NULL;
    file->opened=JS_FALSE;
    js_ResetBuffers(file);
    *rval=JSVAL_TRUE;

    return JS_TRUE;
}

static JSBool
file_exists(JSFile*file)
{
    JSStatus status;
    status = PR_Access(file->path, PR_ACCESS_EXISTS);
    return (status==PR_SUCCESS);
}

static JSBool
file_canRead(JSFile*file)
{
    JSStatus status;
    status = PR_Access(file->path, PR_ACCESS_READ_OK);
    return (status==PR_SUCCESS) /* TODO: && (file->mode&PR_RDONLY) */;
}

static JSBool
file_canWrite(JSFile*file)
{
    JSStatus status;
    status = PR_Access(file->path, PR_ACCESS_WRITE_OK);
    return (status==PR_SUCCESS) /* TODO: && (file->mode&PR_WRONLY) */;
}

static JSBool
file_isFile(JSFile*file)
{

    JSStatus status;
    PRFileInfo info;
    if (file->opened&&file->handle) {
        status=PR_GetOpenFileInfo(file->handle,&info);
    } else {
        status=PR_GetFileInfo(file->path,&info);
    }
    if (status==JS_FAILURE)
        return JS_FALSE;
    return (info.type==PR_FILE_FILE);
}

static JSBool
file_isDirectory(JSFile*file)
{
    JSStatus status;
    PRFileInfo info;
    if (file->opened && file->handle){
      status=PR_GetOpenFileInfo(file->handle,&info);
    }else{
          status=PR_GetFileInfo(file->path,&info);
    }

    if (status==JS_FAILURE)
        return JS_FALSE;
    return (info.type==PR_FILE_DIRECTORY);
}

static JSBool
file_remove(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSFile  *file;
    JSStatus status;

    file = JS_GetInstancePrivate(cx, obj, &file_class, NULL);

    if (file_isDirectory(file)) {
      status = PR_RmDir(file->path);
    } else {
      status = PR_Delete(file->path);
    }
    if (status==PR_SUCCESS) {
        file->opened=JS_FALSE; /* I'm not sure I should do that.. */
        *rval = JSVAL_TRUE;
    } else {
        *rval = JSVAL_FALSE;
    }
    return JS_TRUE;
}

/* Raw PR-based function. No text processing. Just raw data copying. */
static JSBool
file_copyTo(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSFile  *file;
    JSStatus status;
    char*str;
    PRFileDesc* handle;
    PRFileInfo info;
    char* buffer;
    uint32 count;

    if (argc!=1)
    return JS_FALSE;

    file = JS_GetInstancePrivate(cx, obj, &file_class, NULL);

    str=JS_GetStringBytes(JS_ValueToString(cx, argv[0]));

    if (file->opened) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                JSFILEMSG_CANNOT_COPY_OPENED_FILE_ERROR, file->path);
        *rval = JSVAL_FALSE;
        return JS_TRUE;
    }

    handle=PR_Open(str, PR_WRONLY|PR_CREATE_FILE|PR_TRUNCATE, 0644 );
    file->handle=PR_Open(file->path, PR_RDONLY, 0644);

    status=PR_GetOpenFileInfo(file->handle,&info);
    if (status==JS_FAILURE) {
      JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
              JSFILEMSG_CANNOT_ACCESS_FILE_INFO_ERROR, file->path);
      *rval = JSVAL_FALSE;
      return JS_TRUE;
    }

    buffer=JS_malloc(cx, info.size);
    count=PR_Read(file->handle, buffer, info.size);
    if (count!=info.size) {
        JS_free(cx, buffer);
        PR_Close(file->handle);
        PR_Close(handle);
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
              JSFILEMSG_COPY_READ_ERROR, file->path);
        *rval = JSVAL_FALSE;
        return JS_TRUE;
    }
    count=PR_Write(handle, buffer, info.size);
    if (count!=info.size) {
        JS_free(cx, buffer);
        PR_Close(file->handle);
        PR_Close(handle);
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
              JSFILEMSG_COPY_WRITE_ERROR, file->path);
        *rval=JSVAL_FALSE;
        return JS_TRUE;
    }
    JS_free(cx, buffer);
    PR_Close(file->handle);
    PR_Close(handle);

    *rval = JSVAL_TRUE;
    return JS_TRUE;
}

static JSBool
file_renameTo(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSFile  *file;
    JSStatus status;
    char*str;

    if (argc<1) {
      JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
           JSFILEMSG_RENAME_EXPECTS_ONE_ARG_ERROR);
      return JS_FALSE;
    }

    file = JS_GetInstancePrivate(cx, obj, &file_class, NULL);

    str=JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
    status = PR_Rename(file->path, str);
    if (status==JS_FAILURE)
      *rval = JSVAL_FALSE;
    else
      *rval = JSVAL_TRUE;

    return JS_TRUE;
}

static JSBool
file_flush(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSFile  *file;
    JSStatus status;

    file = JS_GetInstancePrivate(cx, obj, &file_class, NULL);

    if (!file->opened) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
           JSFILEMSG_CANNOT_FLUSH_CLOSE_FILE_ERROR, file->path);
        return JS_FALSE;
    }
    if (file->handle) {
        status = PR_Sync(file->handle);
    } else {
        status = fflush(file->nativehandle);
    }
    if (status==JS_FAILURE)
      *rval = JSVAL_FALSE;
    else
      *rval = JSVAL_TRUE;

    return JS_TRUE;
}

/* write(s) methods..
    First, check if the file is open.
    If not, try to open it with default values.
      ( here, append mode.. )
*/
static JSBool
file_write(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    uintN i;
    JSFile  *file;
    JSString *str;
    int32 count;

    *rval = BOOLEAN_TO_JSVAL(JS_FALSE);
    file = JS_GetInstancePrivate(cx, obj, &file_class, NULL);

    if (!file->opened) {
        JSString *type,*mask;
        jsval v[2];
        jsval rval;
        JSBool b;
        type= JS_NewStringCopyZ(cx, utfstring);
        mask= JS_NewStringCopyZ(cx, "create, append, writeOnly");
        v[0]=STRING_TO_JSVAL(type);
        v[1]=STRING_TO_JSVAL(mask);
        b=file_open(cx, obj, 2,v,&rval);
        if (!file->opened) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                JSFILEMSG_CANNOT_OPEN_WRITING_ERROR, file->path);
            return JS_FALSE;
        }
    }

    for (i=0;i<argc;i++) {
        str=JS_ValueToString(cx, argv[i]);
        count=js_FileWrite(cx, file, JS_GetStringChars(str),JS_GetStringLength(str),file->type);
        if (count==-1)
          return JS_TRUE;
    }

    *rval = JSVAL_TRUE;
    return JS_TRUE;
}

static JSBool
file_writeln(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSFile      *file;
    JSString    *str;
    JSBool      b;
    jsint         count;

    file = JS_GetInstancePrivate(cx, obj, &file_class, NULL);

    b=file_write(cx, obj, argc, argv, rval);
    if (!b)
    return JS_FALSE;

    str=JS_NewStringCopyZ(cx, LINEBREAK);
    count=js_FileWrite(cx, file, JS_GetStringChars(str),JS_GetStringLength(str),file->type);

    if (file->autoflush)
        file_flush(cx, obj, 0,NULL, NULL);

    *rval= BOOLEAN_TO_JSVAL(count>=0);
    return JS_TRUE;
}

static JSBool
file_writeAll(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSFile      *file;
    jsuint       iter;
    jsuint       limit;
    JSObject    *array;
    JSObject    *elem;
    jsval       elemval;

    file = JS_GetInstancePrivate(cx, obj, &file_class, NULL);

    if (argc<1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
            JSFILEMSG_WRITEALL_EXPECTS_ONE_ARG_ERROR);
        return JS_FALSE;
    }

    if (!JS_IsArrayObject(cx, JSVAL_TO_OBJECT(argv[0]))) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
            JSFILEMSG_FIRST_ARGUMENT_WRITEALL_NOT_ARRAY_ERROR);
        return JS_FALSE;
    }
    array=JSVAL_TO_OBJECT(argv[0]);
    if (!JS_GetArrayLength(cx, array, &limit)) {
        return JS_FALSE; /* not supposed to happen */
    }


    for (iter=0; iter<limit; iter++) {
      if (!JS_GetElement(cx, array, iter, &elemval))
          return JS_FALSE;
      elem=JSVAL_TO_OBJECT(elemval);
      file_writeln(cx, obj, 1,&elemval, rval);
    }

    *rval = JSVAL_TRUE;
    return JS_TRUE;
}

static JSBool
file_read(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSFile      *file;
    JSString    *str;
    int32       want, count;
    jschar      *buf;

    file = JS_GetInstancePrivate(cx, obj, &file_class, NULL);

    if (!file->opened) {
        JSString *type,*mask;
        jsval v[2];
        jsval rval;
        JSBool b;
        type= JS_NewStringCopyZ(cx, utfstring);
        mask= JS_NewStringCopyZ(cx, "readOnly");
        v[0]=STRING_TO_JSVAL(type);
        v[1]=STRING_TO_JSVAL(mask);
        b=file_open(cx, obj, 2,v,&rval);
        if (!file->opened) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                JSFILEMSG_CANNOT_OPEN_FILE_ERROR, file->path);
            return JS_FALSE;
        }
    }

    if (!JS_ValueToInt32(cx, argv[0], &want))
    return JS_FALSE;

    /* want=(want>262144)?262144:want; * arbitrary size limitation */

    buf = JS_malloc(cx, want*sizeof buf[0]);
    if (!buf)
    return JS_FALSE;

    count= js_FileRead(cx, file, buf, want, file->type);
    if (count>0) {
      str = JS_NewUCStringCopyN(cx, buf, count);
      *rval=STRING_TO_JSVAL(str);
      JS_free(cx, buf);
    } else {
      JS_free(cx, buf);
      *rval=JSVAL_NULL;
      return JS_TRUE;
    }

    return JS_TRUE;
}

static JSBool
file_readln(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSFile      *file;
    JSString    *str;
    jschar      *buf;
    int32       offset;
    intN        room;
    jschar      data, data2;
    JSBool  endofline;

    file = JS_GetInstancePrivate(cx, obj, &file_class, NULL);

    if (!file->opened){
        JSString *type,*mask;
        jsval v[2];
        jsval rval;
        JSBool b;
        type= JS_NewStringCopyZ(cx, utfstring);
        mask= JS_NewStringCopyZ(cx, "readOnly");
        v[0]=STRING_TO_JSVAL(type);
        v[1]=STRING_TO_JSVAL(mask);
        b=file_open(cx, obj, 2,v,&rval);
        if (!file->opened) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                JSFILEMSG_CANNOT_OPEN_FILE_ERROR, file->path);
            return JS_FALSE;
        }
    }

    if (!file->linebuffer) {
        buf = JS_malloc(cx, 128*(sizeof data));
        if (!buf) return JS_FALSE;
        file->linebuffer=JS_NewUCString(cx, buf, 128);
    }
    room=JS_GetStringLength(file->linebuffer);
    offset=0;

    /* XXX TEST ME!! */
    for(;;) {
        if (!js_FileRead(cx, file,&data, 1,file->type)) {
            endofline=JS_FALSE;
            goto loop;
        }
        switch (data) {
        case '\n' :
            endofline=JS_TRUE;
            goto loop;
        case '\r' :
            if (!js_FileRead(cx, file,&data2, 1,file->type)) {
                endofline=JS_TRUE;
                goto loop;
            }
            if (data2!='\n') { /* we read one char to far. buffer it. */
                file->charBuffer=data2;
                file->charBufferUsed=JS_TRUE;
            }
            endofline=JS_TRUE;
            goto loop;
        default:
            if (--room < 0) {
                buf = JS_malloc(cx, (offset+ 128)*sizeof data);
                if (!buf) return JS_FALSE;
                room = 127;
                memcpy(buf, JS_GetStringChars(file->linebuffer),
                    JS_GetStringLength(file->linebuffer));
                /* what follows may not be the cleanest way. */
                file->linebuffer->chars = buf;
                file->linebuffer->length= offset+128;
            }
            file->linebuffer->chars[offset++] = data;
            break;
        }
    }
loop:
    file->linebuffer->chars[offset]=0;
    if ((endofline==JS_TRUE)) {
    str = JS_NewUCStringCopyN(cx, JS_GetStringChars(file->linebuffer),
          offset);
    *rval=STRING_TO_JSVAL(str);
    } else
    *rval = JSVAL_NULL;
    return JS_TRUE;
}

static JSBool
file_readAll(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSFile      *file;
    PRFileInfo  info;
    JSStatus    status;
    JSObject    *array;
    jsint         len;
    jsval       line;
    JSBool      ok=JS_TRUE;


    file = JS_GetInstancePrivate(cx, obj, &file_class, NULL);

    array=JS_NewArrayObject(cx, 0, NULL);
    *rval = OBJECT_TO_JSVAL(array);
    len = 0;


    if (!file->opened) {
        JSString *type,*mask;
        jsval v[2];
        jsval rval;
        JSBool b;
        type= JS_NewStringCopyZ(cx, utfstring);
        mask= JS_NewStringCopyZ(cx, "readOnly");
        v[0]=STRING_TO_JSVAL(type);
        v[1]=STRING_TO_JSVAL(mask);
        b=file_open(cx, obj, 2,v,&rval);
        if (!file->opened) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                JSFILEMSG_CANNOT_OPEN_FILE_ERROR, file->path);
            return JS_FALSE;
        }
    }

    if (file->handle) {
        status=PR_GetOpenFileInfo(file->handle,&info);
    } else {
        status=PR_GetFileInfo(file->path,&info);
    }

    if (status==JS_FAILURE)  return JS_FALSE;

    if (file->handle) {
        while (ok&&(info.size>(JSUint32)PR_Seek(file->handle, 0, PR_SEEK_CUR))) {
            ok=file_readln(cx, obj, 0,NULL,&line);
            JS_SetElement(cx, array, len, &line);
            len++;
        }
    } else {
        while (ok&&(info.size>(JSUint32)fseek(file->nativehandle, 0, SEEK_CUR))) {
            ok=file_readln(cx, obj, 0,NULL,&line);
            JS_SetElement(cx, array, len, &line);
            len++;
        }
    }

    return JS_TRUE;
}

static JSBool
file_skip(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSFile      *file;
    int32       toskip, count;

    file = JS_GetInstancePrivate(cx, obj, &file_class, NULL);

    if (!file->opened) {
        JSString *type,*mask;
        jsval v[2];
        jsval rval;
        JSBool b;
        type= JS_NewStringCopyZ(cx, utfstring);
        mask= JS_NewStringCopyZ(cx, "readOnly");
        v[0]=STRING_TO_JSVAL(type);
        v[1]=STRING_TO_JSVAL(mask);
        b=file_open(cx, obj, 2,v,&rval);
        if (!file->opened) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                JSFILEMSG_CANNOT_OPEN_FILE_ERROR, file->path);
            return JS_FALSE;
        }
    }

    if (!JS_ValueToInt32(cx, argv[0], &toskip))
    return JS_FALSE;

    count= js_FileSkip(file, toskip, file->type);
    if (count!=toskip)  return JS_FALSE;

    return JS_TRUE;
}

static JSBool
file_list(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    PRDir *dir;
    PRDirEntry *entry;
    JSFile * file;
    JSObject *array;
    JSObject *each;
    JSFile *eachObj;
    jsint len;
    jsval v;
    JSRegExp *re = NULL;
    JSFunction *func = NULL;
    JSString * tmp;
    size_t index;
    jsval args[1];
    char* aux;


    if (argc>0) {
        if (JSVAL_IS_REGEXP(cx, argv[0])) {
          re = JS_GetPrivate(cx, JSVAL_TO_OBJECT(argv[0]));
        } else
        if (JSVAL_IS_FUNCTION(cx, argv[0])) {
          func = JS_GetPrivate(cx, JSVAL_TO_OBJECT(argv[0]));
        }
    }

    file = JS_GetInstancePrivate(cx, obj, &file_class, NULL);

    if (!file_isDirectory(file)) {
        *rval = JSVAL_FALSE;
        return JS_TRUE; /* or return an empty array? or do a readAll?  */
    }
    dir=PR_OpenDir(file->path);
    /* Create JSArray here... */
    array=JS_NewArrayObject(cx, 0, NULL);
    *rval = OBJECT_TO_JSVAL(array);
    len=0;

    entry=NULL;
    if (dir==NULL)  return JS_TRUE;

    while ((entry=PR_ReadDir(dir, PR_SKIP_BOTH))!=NULL) {
        /* First, check if we have a filter */
        if (re!=NULL) {
            tmp=JS_NewStringCopyZ(cx, entry->name);
            index=0;
            js_ExecuteRegExp(cx, re, tmp, &index, JS_TRUE, &v);
            if (v==JSVAL_NULL) {
                continue;
            }

        }
        if (func!=NULL) {
            tmp=JS_NewStringCopyZ(cx, entry->name);
            args[0]=STRING_TO_JSVAL(tmp);
            JS_CallFunction(cx, obj, func, 1, args, &v);
            if (v==JSVAL_FALSE) {
                continue;
            }
        }
        aux = combinePath(cx, file->path, (char*)entry->name);

        each=NewFileObject(cx, aux);
        JS_free(cx, aux);
        if (!each)
        return JS_FALSE;
        eachObj = JS_GetInstancePrivate(cx, each, &file_class, NULL);
        if (!eachObj)
        return JS_FALSE;
        v=OBJECT_TO_JSVAL(each);
        JS_SetElement(cx, array, len, &v);
        JS_SetProperty(cx, array, entry->name, &v); /* accessible by name.. make sense I think.. */
        len++;
    }
    PR_CloseDir(dir);

    return JS_TRUE;
}

static JSBool
file_mkdir(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSFile * file;
    JSStatus status;
    char * str;
    JSObject*newobj;


    file = JS_GetInstancePrivate(cx, obj, &file_class, NULL);

    str = fileDirectoryName(cx, file->path);
    if (strcmp(str, file->path)) {
        newobj=NewFileObject(cx, str);
        if (!file_mkdir(cx, newobj, argc, argv, rval)) {
            return JS_FALSE;
        }
    }
    JS_free(cx, str);

    status=PR_MkDir(file->path, 0755);
    if (status==JS_FAILURE)
        *rval=JSVAL_FALSE;
    else
        *rval=JSVAL_TRUE;
    return JS_TRUE;
}

static JSFunctionSpec file_functions[] = {
    { "toString",       file_toString, 0},
    { "open",           file_open, 0},
    { "close",          file_close, 0},
    { "remove",         file_remove, 0},
    { "copyTo",         file_copyTo, 0},
    { "renameTo",       file_renameTo, 0},
    { "flush",          file_flush, 0},
    { "skip",           file_skip, 0},
    { "read",           file_read, 0},
    { "readln",         file_readln, 0},
    { "readAll",        file_readAll, 0},
    { "write",          file_write, 0},
    { "writeln",        file_writeln, 0},
    { "writeAll",       file_writeAll, 0},
    { "list",           file_list, 0},
    { "mkdir",          file_mkdir, 0},
    {0}
};

enum file_tinyid {
    FILE_LENGTH             = -2,
    FILE_PARENT             = -3,
    FILE_PATH               = -4,
    FILE_NAME               = -5,
    FILE_ISDIR              = -6,
    FILE_ISFILE             = -7,
    FILE_EXISTS             = -8,
    FILE_CANREAD            = -9,
    FILE_CANWRITE           = -10,
    FILE_OPENED             = -11,
    FILE_TYPE               = -12,
    FILE_MODE               = -13,
    FILE_CREATED            = -14,
    FILE_MODIFIED           = -15,
    FILE_SIZE               = -16,
    FILE_RANDOMACCESS       = -17,
    FILE_POSITION           = -18

};

static JSPropertySpec file_props[] = {
    {"length",      FILE_LENGTH,    JSPROP_ENUMERATE | JSPROP_READONLY },
    {"parent",      FILE_PARENT,    JSPROP_ENUMERATE | JSPROP_READONLY },
    {"path",        FILE_PATH,      JSPROP_ENUMERATE | JSPROP_READONLY },
    {"name",        FILE_NAME,      JSPROP_ENUMERATE | JSPROP_READONLY },
    {"isDirectory", FILE_ISDIR,     JSPROP_ENUMERATE | JSPROP_READONLY },
    {"isFile",      FILE_ISFILE,    JSPROP_ENUMERATE | JSPROP_READONLY },
    {"exists",      FILE_EXISTS,    JSPROP_ENUMERATE | JSPROP_READONLY },
    {"canRead",     FILE_CANREAD,   JSPROP_ENUMERATE | JSPROP_READONLY },
    {"canWrite",    FILE_CANWRITE,  JSPROP_ENUMERATE | JSPROP_READONLY },
    {"opened",      FILE_OPENED,    JSPROP_ENUMERATE | JSPROP_READONLY },
    {"type",        FILE_TYPE,      JSPROP_ENUMERATE | JSPROP_READONLY },
    {"mode",        FILE_MODE,      JSPROP_ENUMERATE | JSPROP_READONLY },
    {"creationTime",FILE_CREATED,   JSPROP_ENUMERATE | JSPROP_READONLY },
    {"lastModified",FILE_MODIFIED,  JSPROP_ENUMERATE | JSPROP_READONLY },
    {"size",        FILE_SIZE,      JSPROP_ENUMERATE | JSPROP_READONLY },
    {"randomAccess",FILE_RANDOMACCESS,  JSPROP_ENUMERATE | JSPROP_READONLY },
    {"position",    FILE_POSITION,  JSPROP_ENUMERATE },
    {0}
};


static JSBool JS_DLL_CALLBACK
file_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSFile * file;
    char *str;
    jsint tiny;
    JSStatus status;
    PRFileInfo info;
    PRExplodedTime  expandedTime;
    JSObject * tmp;
/*     int aux, index; */
    JSObject * newobj;
    JSString * newstring;
    JSBool flag;

    tiny=JSVAL_TO_INT(id);
    file= JS_GetInstancePrivate(cx, obj, &file_class, NULL);

    switch (tiny) {
    case FILE_PARENT:
        str=fileDirectoryName(cx, file->path);
        newobj=NewFileObject(cx, str);
        JS_free(cx, str);
        *vp = OBJECT_TO_JSVAL(newobj);
        break;
    case FILE_PATH:
        *vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, file->path));
        break;
    case FILE_NAME:
        str=fileBaseName(cx, file->path);
        newstring=JS_NewString(cx, str, strlen(str));
        *vp = STRING_TO_JSVAL(newstring);
        break;
    case FILE_ISDIR:
        *vp = BOOLEAN_TO_JSVAL(file_isDirectory(file));
        break;
    case FILE_ISFILE:
        *vp = BOOLEAN_TO_JSVAL(file_isFile(file));
        break;
    case FILE_EXISTS:
        *vp = BOOLEAN_TO_JSVAL(file_exists(file));
        break;
    case FILE_CANREAD:
        *vp = BOOLEAN_TO_JSVAL(file_canRead(file));
        break;
    case FILE_CANWRITE:
        *vp = BOOLEAN_TO_JSVAL(file_canWrite(file));
        break;
    case FILE_OPENED:
        *vp = BOOLEAN_TO_JSVAL(file->opened);
        break;
    case FILE_TYPE:
        switch (file->type) {
        case ASCII:
            *vp = STRING_TO_JSVAL(JS_NewString(cx, asciistring, strlen(asciistring)));
            break;
        case UTF8:
            *vp = STRING_TO_JSVAL(JS_NewString(cx, utfstring, strlen(utfstring)));
            break;
        case UCS2:
            *vp = STRING_TO_JSVAL(JS_NewString(cx, unicodestring, strlen(unicodestring)));
            break;
        default:
            ; /* time to panic, or to do nothing.. */
        }
        break;
    case FILE_MODE:
        str=(char*)JS_malloc(cx, 200); /* Big enough to contain all the modes. */
        str[0]='\0';
        flag=JS_FALSE;

        if ((file->mode&PR_RDONLY)==PR_RDONLY) {
            if (flag) strcat(str,",");
            strcat(str,"readOnly");
            flag=JS_TRUE;
        }
        if ((file->mode&PR_WRONLY)==PR_WRONLY) {
            if (flag) strcat(str,",");
            strcat(str,"writeOnly");
            flag=JS_TRUE;
        }
        if ((file->mode&PR_RDWR)==PR_RDWR) {
            if (flag) strcat(str,",");
            strcat(str,"readWrite");
            flag=JS_TRUE;
        }
        if ((file->mode&PR_APPEND)==PR_APPEND) {
            if (flag) strcat(str,",");
            strcat(str,"append");
            flag=JS_TRUE;
        }
        if ((file->mode&PR_CREATE_FILE)==PR_CREATE_FILE) {
            if (flag) strcat(str,",");
            strcat(str,"create");
            flag=JS_TRUE;
        }
        if ((file->mode&PR_TRUNCATE)==PR_TRUNCATE) {
            if (flag) strcat(str,",");
            strcat(str,"replace");
            flag=JS_TRUE;
        }
        if (file->autoflush) {
            if (flag) strcat(str,",");
            strcat(str,"autoflush");
            flag=JS_TRUE;
        }
        newstring=JS_NewStringCopyZ(cx, str);
        *vp = STRING_TO_JSVAL(newstring);
        JS_free(cx, str);
        break;
    case FILE_CREATED:
        if (file->opened&&file->handle) {
          status=PR_GetOpenFileInfo(file->handle,&info);
        } else {
          status=PR_GetFileInfo(file->path,&info);
        }
        if (status==JS_FAILURE)
          break;

        PR_ExplodeTime(info.creationTime, PR_LocalTimeParameters, &expandedTime);
        tmp=js_NewDateObject(cx,    expandedTime.tm_year,
                                    expandedTime.tm_month,
                                    expandedTime.tm_mday,
                                    expandedTime.tm_hour,
                                    expandedTime.tm_min,
                                    expandedTime.tm_sec );
        *vp = OBJECT_TO_JSVAL(tmp);
        break;
    case FILE_MODIFIED:
        if (file->opened&&file->handle) {
          status=PR_GetOpenFileInfo(file->handle,&info);
        } else {
          status=PR_GetFileInfo(file->path,&info);
        }
        if (status==JS_FAILURE)
          break;

        PR_ExplodeTime(info.modifyTime, PR_LocalTimeParameters, &expandedTime);
        tmp=js_NewDateObject(cx,    expandedTime.tm_year,
                                    expandedTime.tm_month,
                                    expandedTime.tm_mday,
                                    expandedTime.tm_hour,
                                    expandedTime.tm_min,
                                    expandedTime.tm_sec );
        *vp = OBJECT_TO_JSVAL(tmp);
        break;
    case FILE_LENGTH:
        if (file_isDirectory(file)) { /* XXX debug me */
          PRDir *dir;
          PRDirEntry *entry;
          jsint count;

          dir=PR_OpenDir(file->path);
          if (dir!=NULL)
            entry=PR_ReadDir(dir, PR_SKIP_BOTH);
          else
            break;

          count=0;
          while (entry!=NULL) {
            count++;
            entry=PR_ReadDir(dir, PR_SKIP_BOTH);
          }
          PR_CloseDir(dir);
          *vp = INT_TO_JSVAL(count);
          break;
        }

        if (file->opened&&file->handle) {
            status=PR_GetOpenFileInfo(file->handle,&info);
        } else {
            status=PR_GetFileInfo(file->path,&info);
        }
        if (status!=JS_FAILURE)
          *vp = INT_TO_JSVAL(info.size);
        break;
    case FILE_RANDOMACCESS:
        *vp = BOOLEAN_TO_JSVAL(file->randomAccess);
        break;
    case FILE_POSITION:
        if (file->opened) {
            if (file->handle) {
                *vp = INT_TO_JSVAL(PR_Seek(file->handle, 0, PR_SEEK_CUR));
            } else {
                *vp = INT_TO_JSVAL(fseek(file->nativehandle, 0, SEEK_CUR));
            }
        } else {
          *vp = JSVAL_VOID;
        }
        break;
    default:
        /*break;*/
		/* this is some other property -- try to use the dir["file"] syntax */
		if(file_isDirectory(file)){
			PRDir *dir = NULL;
			PRDirEntry *entry = NULL;
			char* prop_name = JS_GetStringBytes(JS_ValueToString(cx, id));

			dir=PR_OpenDir(file->path);
			if(!dir) return JS_TRUE;

            while((entry=PR_ReadDir(dir, PR_SKIP_NONE))!=NULL){
				if(!strcmp(entry->name, prop_name)){
					str = combinePath(cx, file->path, prop_name);
                    *vp = OBJECT_TO_JSVAL(NewFileObject(cx, str));
					JS_free(cx, str);
					return JS_TRUE;
				}
			}
		}
    }
    return JS_TRUE;
}

static JSBool JS_DLL_CALLBACK
file_setProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSFile *file;
    jsint slot;
    int32 offset;
    int32 count;

    file = JS_GetInstancePrivate(cx, obj, &file_class, NULL);

    if (JSVAL_IS_STRING(id)){
        return JS_TRUE;
    }

    slot = JSVAL_TO_INT(id);

    switch (slot) {
    case FILE_POSITION:
        if (file->randomAccess) {
            offset=JSVAL_TO_INT(*vp);
            if (file->handle) {
                count = PR_Seek( file->handle, offset, PR_SEEK_SET);
            } else {
                count = fseek( file->nativehandle, offset, SEEK_SET);
            }
            js_ResetBuffers(file);
            *vp = INT_TO_JSVAL(count);
        }
        break;
    default:
        break;
    }

    return JS_TRUE;
}

static void
file_finalize(JSContext *cx, JSObject *obj)
{
    JSFile *file;
    file = JS_GetInstancePrivate(cx, obj, &file_class, NULL);
    JS_free(cx, file->path);
    if (file->linebuffer)
        JS_free(cx, file->linebuffer);
    JS_free(cx, file);
}

static JSClass file_class = {
    FILE_CONSTRUCTOR, JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,  JS_PropertyStub,  file_getProperty,  file_setProperty,
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   file_finalize
};

/*
    Allocates memory for the file object, sets fields to defaults.
*/
static JSFile*
file_init(JSContext *cx, JSObject *obj, char *bytes)
{
    JSFile      *file;

    file=JS_malloc(cx, sizeof *file);
    if (!file) {
        /* TODO: report error */
        return NULL;
    }
    memset(file, 0 , sizeof *file);

    /* canonize the path */
    file->path=canonicalPath(cx, bytes);

    file->opened=JS_FALSE;
    file->handle=NULL;
    file->nativehandle=NULL;
    file->nbBytesInBuf=0;
    file->charBufferUsed=JS_FALSE;
    file->randomAccess=JS_TRUE; /* innocent until proven guilty */

    if (!JS_SetPrivate(cx, obj, file)) {
        JS_free(cx, file);
        return NULL;
    }

    return file;
}

/* Returns a JSObject */
JSObject*
NewFileObject(JSContext *cx, char *bytes)
{
    JSObject    *obj;
    JSFile      *file;

    obj = JS_NewObject(cx, &file_class, NULL, NULL);
    if (!obj) return NULL;
    file=file_init(cx, obj, bytes);
    if(!file) return NULL;
    return obj;
}

JSObject*
NewFileObjectFromPipe(JSContext *cx, char *bytes)
{
	return NULL;
}

/* Internal function, used for cases which NSPR file support doesn't cover */
JSObject*
NewFileObjectFromFILE(JSContext *cx, FILE *nativehandle, char *filename, JSBool open)
{
    JSObject *obj;
    JSFile   *file;

    obj = JS_NewObject(cx, &file_class, NULL, NULL);
    if (!obj) return NULL;
    file = file_init(cx, obj, filename);
    if(!file) return NULL;

    file->nativehandle=nativehandle;
    file->path=strdup(filename);
    file->opened=open;      /* maybe need to make a parameter */
    return obj;
}

/*
    Real file constructor that is called from JavaScript.
    Basically, does error processing and calls file_init.
*/
static JSBool
File(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSString *str;
    JSFile   *file;

    str=(argc==0)?JS_InternString(cx, ""):JS_ValueToString(cx, argv[0]);

    if (!str){
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
            JSFILEMSG_FIRST_ARGUMENT_CONSTRUCTOR_NOT_STRING_ERROR, argv[0]);
        return JS_FALSE;
    }

    file = file_init(cx, obj, JS_GetStringBytes(str));
    if (!file) {
        *rval=JSVAL_VOID;
        return JS_FALSE;
    }

    return JS_TRUE;
}

/*
    File.currentDir = new File("D:\") or File.currentDir = "D:\"
*/
static JSBool
file_currentDirSetter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSObject *rhsObject;
    char     *path;
    JSFile   *file;

    /* Look at the rhs and extract a file object from it */
    if (JSVAL_IS_OBJECT(*vp)){
        if (JS_InstanceOf(cx, rhsObject, &file_class, NULL)){
            file = JS_GetInstancePrivate(cx, rhsObject, &file_class, NULL);
            /* Braindamaged rhs -- just return the old value */
            if (file && (!file_exists(file) || !file_isDirectory(file))){
                JS_GetProperty(cx, obj, CURRENTDIR_PROPERTY, vp);
                return JS_FALSE;
            }else{
                rhsObject = JSVAL_TO_OBJECT(*vp);
                chdir(file->path);
                return JS_TRUE;
            }
        }else
            return JS_FALSE;
    }else{
        path      = JS_GetStringBytes(JS_ValueToString(cx, *vp));
        rhsObject = NewFileObject(cx, path);
        if (!rhsObject)  return JS_FALSE;

        file = JS_GetInstancePrivate(cx, rhsObject, &file_class, NULL);
        if (!file || !file_exists(file) || !file_isDirectory(file)){
            JS_GetProperty(cx, obj, CURRENTDIR_PROPERTY, vp);
        }else{
            *vp=OBJECT_TO_JSVAL(rhsObject);
            chdir(path);
        }
    }
    return JS_TRUE;
}

JSObject*
js_InitFileClass(JSContext *cx, JSObject* obj)
{
    JSObject *file, *ctor;
    JSObject *afile;
    JSFile   *fileObj;
    jsval    vp;
    char     *currentdir;

    file = JS_InitClass(cx, obj, NULL, &file_class, File, 1,
        file_props, file_functions, NULL, NULL);
    if (!file) return NULL;

    ctor = JS_GetConstructor(cx, file);
    if (!ctor)  return NULL;

	/* Define CURRENTDIR property. We are doing this to get a
	slash at the end of the current dir */
    afile = NewFileObject(cx, CURRENT_DIR);
    currentdir= JS_malloc(cx, MAX_PATH_LENGTH);
    currentdir= getcwd(currentdir, MAX_PATH_LENGTH);
    afile = NewFileObject(cx, currentdir);
    JS_free(cx, currentdir);
    vp = OBJECT_TO_JSVAL(afile);
    JS_DefinePropertyWithTinyId(cx, ctor, CURRENTDIR_PROPERTY, 0, vp,
                JS_PropertyStub, file_currentDirSetter, JSPROP_ENUMERATE);

	/* Code to create stdin, stdout, and stderr. Insert in the appropriate place. */
	/* Define input */
    afile=NewFileObjectFromFILE(cx, stdin, SPECIAL_FILE_STRING, JS_TRUE);
    if (!afile) return NULL;
    fileObj = JS_GetInstancePrivate(cx, afile, &file_class, NULL);
    fileObj->randomAccess=JS_FALSE;
    vp=OBJECT_TO_JSVAL(afile);
    JS_SetProperty(cx, ctor, "input", &vp);

	/* Define output */
    afile=NewFileObjectFromFILE(cx, stdout, SPECIAL_FILE_STRING, JS_TRUE);
    if (!afile) return NULL;
    fileObj = JS_GetInstancePrivate(cx, afile, &file_class, NULL);
    fileObj->randomAccess=JS_FALSE;
    vp=OBJECT_TO_JSVAL(afile);
    JS_SetProperty(cx, ctor, "output", &vp);

	/* Define error */
    afile=NewFileObjectFromFILE(cx, stderr, SPECIAL_FILE_STRING, JS_TRUE);
    if (!afile)
    return NULL;
    fileObj = JS_GetInstancePrivate(cx, afile, &file_class, NULL);
    fileObj->randomAccess=JS_FALSE;
    vp=OBJECT_TO_JSVAL(afile);
    JS_SetProperty(cx, ctor, "error", &vp);

    return file;
}
#endif /* JS_HAS_FILE_OBJECT */
