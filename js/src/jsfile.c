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
#if JS_HAS_FILE_OBJECT

#ifndef _WINDOWS
#  include <strings.h>
#  include <stdio.h>
#  include <stdlib.h>
#  include <unistd.h>
#else
#  include "direct.h"
#  include <io.h>
#  include <sys/types.h>
#  include <sys/stat.h>
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

#define MAX_PATH_LENGTH 1024
#define MSG_SIZE        256
#define URL_PREFIX      "file://"

/* assumes we don't have leading/trailing spaces */
static JSBool
filenameHasAPipe(const char *filename)
{
#ifdef XP_MAC
    /* pipes are not supported on the MAC */
    return JS_FALSE;
#else
    if(!filename) return JS_FALSE;
    return filename[0]==PIPE_SYMBOL || filename[strlen(filename)-1]==PIPE_SYMBOL;
#endif
}

/* Platform dependent code goes here. */
#ifdef XP_MAC
#  define LINEBREAK             "\012"
#  define LINEBREAK_LEN 1
#  define FILESEPARATOR         ':'
#  define FILESEPARATOR2        '\0'
#  define CURRENT_DIR "HARD DISK:Desktop Folder"

static JSBool
isAbsolute(char *name)
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
    i = strlen(base)-1;
    if (base[i]!=FILESEPARATOR) {
      tmp[i+1] = FILESEPARATOR;
      tmp[i+2] = '\0';
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

    index = strlen(pathname)-1;
    aux = index;
    while ((index>=0)&&(pathname[index]!=FILESEPARATOR)) index--;
    basename = (char*)JS_malloc(cx, aux-index+1);
    if (!basename)  return NULL;
    strncpy(basename, &pathname[index+1], aux-index);
    basename[aux-index] = '\0';
    return basename;
}

/* Extracts the directory name from a path. Returned string must be freed */
static char *
fileDirectoryName(JSContext *cx, char * pathname)
{
    jsint index;
    char *dirname;

    index = strlen(pathname)-1;
    while ((index>0)&&(pathname[index]!=FILESEPARATOR)) index--;

    if (index>=0) {
        dirname = (char*)JS_malloc(cx, index+2);
        if (!dirname) return NULL;
        strncpy(dirname, pathname, index);
        dirname[index] = FILESEPARATOR;
        dirname[index+1] = '\0';
    } else
        dirname = JS_strdup(cx, pathname);
    return dirname;
}

#else
#  if defined(XP_PC) || defined(_WINDOWS) || defined(OS2)
#    define LINEBREAK           "\015\012"
#    define LINEBREAK_LEN       2
#    define FILESEPARATOR       '\\'
#    define FILESEPARATOR2      '/'
#    define CURRENT_DIR "c:\\"
#    define POPEN   _popen
#    define PCLOSE  _pclose

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
    tmp1 = &name[2];
    } else
    tmp1 = name;

    tmp = (char*)JS_malloc(cx, strlen(base)+strlen(tmp1)+2);
    if (!tmp) return NULL;
    strcpy(tmp, base);
    i = strlen(base)-1;
    if ((base[i]!=FILESEPARATOR)&&(base[i]!=FILESEPARATOR2)) {
      tmp[i+1] = FILESEPARATOR;
      tmp[i+2] = '\0';
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
        pathname = &pathname[2];
    }

    index = strlen(pathname)-1;
    while ((index>0)&&((pathname[index]==FILESEPARATOR)||
                       (pathname[index]==FILESEPARATOR2))) index--;
    aux = index;
    while ((index>=0)&&(pathname[index]!=FILESEPARATOR)&&
                      (pathname[index]!=FILESEPARATOR2)) index--;
    basename = (char*)JS_malloc(cx, aux-index+3);
    if (!basename)  return NULL;
    strncpy(basename, &pathname[index+1], aux-index);
    basename[aux-index] = '\0';
    return basename;
}

/* returned string must be freed */
static char *
fileDirectoryName(JSContext *cx, char * pathname)
{
    jsint index;
    char  *dirname, *backpathname;
    char  drive = '\0';

    backpathname = pathname;
    /* First, get rid of the drive selector */
    if ((strlen(pathname)>=2)&&(pathname[1]==':')) {
        drive = pathname[0];
        pathname = &pathname[2];
    }

    index = strlen(pathname)-1;
    while ((index>0)&&((pathname[index]==FILESEPARATOR)||
                       (pathname[index]==FILESEPARATOR2))) index--;
    while ((index>0)&&(pathname[index]!=FILESEPARATOR)&&
                      (pathname[index]!=FILESEPARATOR2)) index--;

    if (index>=0){
        dirname = (char*)JS_malloc(cx, index+4);
        if (!dirname)  return NULL;

        if (drive!='\0') {
            dirname[0] = toupper(drive);
            dirname[1] = ':';
            strncpy(&dirname[2], pathname, index);
            dirname[index+2] = FILESEPARATOR;
            dirname[index+3] = '\0';
        } else {
            strncpy(dirname, pathname, index);
            dirname[index] = FILESEPARATOR;
            dirname[index+1] = '\0';
        }
    } else
        dirname = JS_strdup(cx, backpathname); /* may include drive selector */

    return dirname;
}

#  else
#    ifdef XP_UNIX
#      define LINEBREAK         "\012"
#      define LINEBREAK_LEN     1
#      define FILESEPARATOR     '/'
#      define FILESEPARATOR2    '\0'
#      define CURRENT_DIR "/"
#      define POPEN   popen
#      define PCLOSE  pclose

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
    i = strlen(base)-1;
    if (base[i]!=FILESEPARATOR) {
      tmp[i+1] = FILESEPARATOR;
      tmp[i+2] = '\0';
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

    index = strlen(pathname)-1;
    while ((index>0)&&((pathname[index]==FILESEPARATOR)||
                       (pathname[index]==FILESEPARATOR2))) index--;
    aux = index;
    while ((index>=0)&&(pathname[index]!=FILESEPARATOR)&&
                      (pathname[index]!=FILESEPARATOR2)) index--;
    basename = (char*)JS_malloc(cx, aux-index+1);
    if (!basename)  return NULL;
    strncpy(basename, &pathname[index+1], aux-index);
    basename[aux-index] = '\0';
    return basename;
}

/* returned string must be freed */
static char *
fileDirectoryName(JSContext *cx, char * pathname)
{
    jsint index;
    char *dirname;

    index = strlen(pathname)-1;
    while ((index>0)&&((pathname[index]==FILESEPARATOR)||
                       (pathname[index]==FILESEPARATOR2))) index--;
    while ((index>0)&&(pathname[index]!=FILESEPARATOR)&&
                      (pathname[index]!=FILESEPARATOR2)) index--;

    if (index>=0) {
        dirname = (char*)JS_malloc(cx, index+2);
        if (!dirname)  return NULL;
        strncpy(dirname, pathname, index);
        dirname[index] = FILESEPARATOR;
        dirname[index+1] = '\0';
    } else
        dirname = JS_strdup(cx, pathname);
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

    if (isAbsolute(path)){
        return JS_strdup(cx, path);
    }else{
        obj = JS_GetGlobalObject(cx);
        if (!JS_GetProperty(cx, obj, FILE_CONSTRUCTOR, &prop)) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                 JSFILEMSG_FILE_CONSTRUCTOR_UNDEFINED_ERROR);
            return JS_strdup(cx, path);
        }
        obj = JSVAL_TO_OBJECT(prop);
        if (!JS_GetProperty(cx, obj, CURRENTDIR_PROPERTY, &prop)) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                 JSFILEMSG_FILE_CURRENTDIR_UNDEFINED_ERROR);
            return JS_strdup(cx, path);
        }
        str = JS_ValueToString(cx, prop);
        if (!str ) {
            return JS_strdup(cx, path);
        }
        return combinePath(cx, JS_GetStringBytes(str), path); /* should we have an array of current dirs indexed by drive for windows? */
    }
}

/* this function should be in the platform specific side. */
/* :: is not handled. */
/* plus symbolic links are not handled. We should let the OS handle all this */
static char *
canonicalPath(JSContext *cx, char *path)
{
    char *tmp1, *tmp;
    char *base, *dir, *current, *result;
    jsint c;
    jsint back = 0;
    unsigned int i = 0, j = strlen(path)-1;

	/* TODO: this is probably optional */
	/* Remove possible spaces in the beginning and end */
	while(i<strlen(path)-1 && path[i]==' ') i++;
	while(j>=0 && path[j]==' ') j--;

	tmp = JS_malloc(cx, j-i+2);
	strncpy(tmp, &path[i], j-i+1);
    tmp[j-i+1] = '\0';

	strcpy(path, tmp);
	JS_free(cx, tmp);

    /* pipe support */
    if(filenameHasAPipe(path))
        return JS_strdup(cx, path);
    /* file:// support */
    if(!strncmp(path, URL_PREFIX, strlen(URL_PREFIX)))
        return strdup(&path[strlen(URL_PREFIX)-1]);

    if (!isAbsolute(path))
        tmp1 = absolutePath(cx, path);
    else
        tmp1 = JS_strdup(cx, path);

    result = JS_strdup(cx, "");

    current = tmp1;

    base = fileBaseName(cx, current);
    dir = fileDirectoryName(cx, current);
    /* TODO: MAC??? */
    while (strcmp(dir, current)) {
        if (!strcmp(base, "..")) {
            back++;
        } else
        if(!strcmp(base, ".")){

        } else {
            if (back>0)
                back--;
            else {
                tmp = result;
                result = JS_malloc(cx, strlen(base)+1+strlen(tmp)+1);
                if (!result) {
                    JS_free(cx, dir);
                    JS_free(cx, base);
                    JS_free(cx, current);
                    return NULL;
                }
                strcpy(result, base);
                c = strlen(result);
                if (strlen(tmp)>0) {
                    result[c] = FILESEPARATOR;
                    result[c+1] = '\0';
                    strcat(result, tmp);
                }
                JS_free(cx, tmp);
            }
        }
        JS_free(cx, current);
        JS_free(cx, base);
        current = dir;
        base =  fileBaseName(cx, current);
        dir = fileDirectoryName(cx, current);
    }
    tmp = result;
    result = JS_malloc(cx, strlen(dir)+1+strlen(tmp)+1);
    if (!result) {
        JS_free(cx, dir);
        JS_free(cx, base);
        JS_free(cx, current);
        return NULL;
    }
    strcpy(result, dir);
    c = strlen(result);
    if (strlen(tmp)>0) {
        if ((result[c-1]!=FILESEPARATOR)&&(result[c-1]!=FILESEPARATOR2)) {
            result[c] = FILESEPARATOR;
            result[c+1] = '\0';
        }
        strcat(result, tmp);
    }
    JS_free(cx, tmp);
    JS_free(cx, dir);
    JS_free(cx, base);
    JS_free(cx, current);

    return result;
}
/* ----------------------------- New file manipulation procesures -------------------------- */
static JSBool
js_isAbsolute(const char *name)
{
#ifdef XP_PC
    return (strlen(name)>1)?((name[1]==':')?JS_TRUE:JS_FALSE):JS_FALSE;
#else
    return (name[0]
#   ifdef XP_UNIX
            ==
#   else
            !=
#   endif
            FILESEPARATOR)?JS_TRUE:JS_FALSE;
#endif
}

/*
    Concatinates base and name to produce a valid filename.
    Returned string must be freed.
*/
static char*
js_combinePath(JSContext *cx, const char *base, const char *name)
{
    int len = strlen(base)-1;
    char* result = (char*)JS_malloc(cx, strlen(base)+strlen(name)+2);

    if (!result)  return NULL;

    strcpy(result, base);
#ifdef XP_PC
    /* TODO: Do we need this ??? */
    if ((strlen(name)>=2)&&(name[1]==':')) { /* remove a drive selector if there's one.*/
        name = &name[2];
    }
#endif

    if (base[len]!=FILESEPARATOR
#ifdef XP_PC
            && base[len]!=FILESEPARATOR2
#endif
            ) {
      result[len+1] = FILESEPARATOR;
      result[len+2] = '\0';
    }
    strcat(result, name);
    return result;
}

/* Extract the last component from a path name. Returned string must be freed */
static char *
js_fileBaseName(JSContext *cx, const char *pathname)
{
    jsint index, aux;
    char *result;
#ifdef XP_PC
    /* First, get rid of the drive selector */
    if ((strlen(pathname)>=2)&&(pathname[1]==':')) {
        pathname = &pathname[2];
    }
#endif
    index = strlen(pathname)-1;
    /* remove trailing separators -- don't necessarily need to check for FILESEPARATOR2, but that's fine */
    while ((index>0)&&((pathname[index]==FILESEPARATOR)||
                       (pathname[index]==FILESEPARATOR2))) index--;
    aux = index;
    /* now find the next separator */
    while ((index>=0)&&(pathname[index]!=FILESEPARATOR)&&
                      (pathname[index]!=FILESEPARATOR2)) index--;
    /* allocate and copy */
    result = (char*)JS_malloc(cx, aux-index+1);
    if (!result)  return NULL;
    strncpy(result, &pathname[index+1], aux-index);
    result[aux-index] = '\0';
    return result;
}

/*
    Returns everytynig bu the last component from a path name.
    Returned string must be freed. Returned string must be freed.
*/
static char *
js_fileDirectoryName(JSContext *cx, const char *pathname)
{
    jsint index;
    char  *result;
#ifdef XP_PC
    char  drive = '\0';
    const char *oldpathname = pathname;

    /* First, get rid of the drive selector */
    if ((strlen(pathname)>=2)&&(pathname[1]==':')) {
        drive = pathname[0];
        pathname = &pathname[2];
    }
#endif
    index = strlen(pathname)-1;
    while ((index>0)&&((pathname[index]==FILESEPARATOR)||
                       (pathname[index]==FILESEPARATOR2))) index--;
    while ((index>0)&&(pathname[index]!=FILESEPARATOR)&&
                      (pathname[index]!=FILESEPARATOR2)) index--;

    if (index>=0){
        result = (char*)JS_malloc(cx, index+4);
        if (!result)  return NULL;
#ifdef XP_PC
        if (drive!='\0') {
            result[0] = toupper(drive);
            result[1] = ':';
            strncpy(&result[2], pathname, index);
        }else{
            strncpy(result, pathname, index);
        }
#else
        strncpy(dirname, pathname, index);
#endif
        /* add terminating separator */
        index = strlen(result)-1;
        result[index] = FILESEPARATOR;
        result[index+1] = '\0';
    } else{
#ifdef XP_PC
        result = JS_strdup(cx, oldpathname); /* may include drive selector */
#else
        result = JS_strdup(cx, pathname);
#endif
    }

    return result;
}

static char *
js_absolutePath(JSContext *cx, const char * path)
{
    JSObject *obj;
    JSString *str;
    jsval prop;

    if (js_isAbsolute(path)){
        return JS_strdup(cx, path);
    }else{
        obj = JS_GetGlobalObject(cx);
        if (!JS_GetProperty(cx, obj, FILE_CONSTRUCTOR, &prop)) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                 JSFILEMSG_FILE_CONSTRUCTOR_UNDEFINED_ERROR);
            return JS_strdup(cx, path);
        }
        obj = JSVAL_TO_OBJECT(prop);
        if (!JS_GetProperty(cx, obj, CURRENTDIR_PROPERTY, &prop)) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                 JSFILEMSG_FILE_CURRENTDIR_UNDEFINED_ERROR);
            return JS_strdup(cx, path);
        }
        str = JS_ValueToString(cx, prop);
        if (!str ) {
            return JS_strdup(cx, path);
        }
        return js_combinePath(cx, JS_GetStringBytes(str), path); /* should we have an array of current dirs indexed by drive for windows? */
    }
}

/* Side effect: will remove spaces in the beginning/end of the filename */
static char *
js_canonicalPath(JSContext *cx, const char *oldpath)
{
    char *tmp1, *tmp;
    char *path = oldpath;
    char *base, *dir, *current, *result;
    jsint c;
    jsint back = 0;
    unsigned int i = 0, j = strlen(path)-1;

	/* TODO: this is probably optional */
	/* Remove possible spaces in the beginning and end */
    while(i<strlen(path)-1 && path[i]==' ') i++;
	while(j>=0 && path[j]==' ') j--;

	tmp = JS_malloc(cx, j-i+2);
	strncpy(tmp, &path[i], j-i+1);
    tmp[j-i+1] = '\0';

	strcpy(path, tmp);
    JS_free(cx, tmp);

    /* pipe support */
    if(filenameHasAPipe(path))
        return JS_strdup(cx, path);
    /* file:// support */
    if(!strncmp(path, URL_PREFIX, strlen(URL_PREFIX)))
        return js_canonicalPath(cx, &path[strlen(URL_PREFIX)-1]);

    if (!js_isAbsolute(path))
        path = js_absolutePath(cx, path);
    else
        path = JS_strdup(cx, path);

    result = JS_strdup(cx, "");

    current = path;

    base = js_fileBaseName(cx, current);
    dir = js_fileDirectoryName(cx, current);

    /* TODO: MAC??? */
    while (strcmp(dir, current)) {
        if (!strcmp(base, "..")) {
            back++;
        } else
        if(!strcmp(base, ".")){
            /* ??? */
        } else {
            if (back>0)
                back--;
            else {
                tmp = result;
                result = JS_malloc(cx, strlen(base)+1+strlen(tmp)+1);
                if (!result) {
                    JS_free(cx, dir);
                    JS_free(cx, base);
                    JS_free(cx, current);
                    return NULL;
                }
                strcpy(result, base);
                c = strlen(result);
                if (strlen(tmp)>0) {
                    result[c] = FILESEPARATOR;
                    result[c+1] = '\0';
                    strcat(result, tmp);
                }
                JS_free(cx, tmp);
            }
        }
        JS_free(cx, current);
        JS_free(cx, base);
        current = dir;
        base =  js_fileBaseName(cx, current);
        dir = js_fileDirectoryName(cx, current);
    }

    tmp = result;
    result = JS_malloc(cx, strlen(dir)+1+strlen(tmp)+1);
    if (!result) {
        JS_free(cx, dir);
        JS_free(cx, base);
        JS_free(cx, current);
        return NULL;
    }
    strcpy(result, dir);
    c = strlen(result);
    if (strlen(tmp)>0) {
        if ((result[c-1]!=FILESEPARATOR)&&(result[c-1]!=FILESEPARATOR2)) {
            result[c] = FILESEPARATOR;
            result[c+1] = '\0';
        }
        strcat(result, tmp);
    }
    JS_free(cx, tmp);
    JS_free(cx, dir);
    JS_free(cx, base);
    JS_free(cx, current);

    return result;
}


/* ---------------- Text format conversion ------------------------- */
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

typedef struct JSFile {
    char        *path;          /* the path to the file. */
    JSBool      isOpen;
    JSString    *linebuffer;    /* temp buffer used by readln. */
    int32       mode;           /* mode used to open the file: read, write, append, create, etc.. */
    int32       type;           /* Asciiz, utf, unicode */
    char        byteBuffer[3];  /* bytes read in advance by js_FileRead ( UTF8 encoding ) */
    jsint       nbBytesInBuf;   /* number of bytes stored in the buffer above */
    jschar      charBuffer;     /* character read in advance by readln ( mac files only ) */
    JSBool      charBufferUsed; /* flag indicating if the buffer above is being used */
    JSBool      hasRandomAccess;   /* can the file be randomly accessed? false for stdin, and
                                 UTF-encoded files. */
    JSBool      hasAutoflush;      /* should we force a flush for each line break? */
    JSBool      isNative;       /* if the file is using OS-specific file FILE type */

    union{                      /* anonymous union */
        PRFileDesc *handle;        /* the handle for the file, if open.  */
        FILE       *nativehandle;  /* native handle, for stuff NSPR doesn't do. */
    };
    JSBool      isAPipe;        /* if the file is really an OS pipe */
} JSFile;

/* a few forward declarations... */
static JSClass file_class;
JS_EXPORT_API(JSObject*) js_NewFileObject(JSContext *cx, char *filename);
JS_EXPORT_API(JSObject*) js_NewFileObjectFromFILEFromFILE(JSContext *cx, FILE *f, char *filename, JSBool open);
static JSBool file_open(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
static JSBool file_close(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

/* ------------------------------- Helper functions ---------------------------- */
/* Ripped off from lm_win.c .. */
/* where is strcasecmp?.. for now, it's case sensitive..
 *
 * strcasecmp is in strings.h, but on windows it's called _stricmp...
 * will need to #ifdef this
*/

static int32
js_FileHasOption(JSContext *cx, char *oldoptions, char *name)
{
    char *comma, *equal, *current;
    char *options = JS_strdup(cx, oldoptions);
    int32 found = 0;

	current = options;
    for (;;) {
        comma = strchr(current, ', ');
        if (comma) *comma = '\0';
        equal = strchr(current, ' = ');
        if (equal) *equal = '\0';
        if (strcmp(current, name) == 0) {
            if (!equal || strcmp(equal + 1, "yes") == 0)
                found = 1;
            else
                found = atoi(equal + 1);
        }
        if (equal) *equal = '=';
        if (comma) *comma = ',';
        if (found || !comma)
            break;
        current = comma + 1;
    }
    JS_free(cx, options);
    return found;
}

static void
js_ResetBuffers(JSFile * file)
{
    file->charBufferUsed = JS_FALSE;
    file->nbBytesInBuf = 0;
}

/* Reset file attributes */
static void
js_ResetAttributes(JSFile * file){
    file->isOpen = JS_FALSE;
    file->handle = NULL;
    file->nativehandle = NULL;
    file->hasRandomAccess = JS_TRUE; /* innocent until proven guilty */
	file->hasAutoflush = JS_FALSE;
    file->isNative = JS_FALSE;
    file->isAPipe = JS_FALSE;
    js_ResetBuffers(file);
}

static JSBool
js_FileOpen(JSContext *cx, JSObject *obj, JSFile *file, char *mode){
    JSString *type, *mask;
    jsval v[2];
    jsval rval;

    type =  JS_NewStringCopyZ(cx, utfstring);
    mask =  JS_NewStringCopyZ(cx, mode);
    v[0] = STRING_TO_JSVAL(type);
    v[1] = STRING_TO_JSVAL(mask);

    if (!file_open(cx, obj, 2, v, &rval)) {
		/* TODO: do we need error reporting here? */
        return JS_FALSE;
    }
}

/* Buffered version of PR_Read. Used by js_FileRead */
static int32
js_BufferedRead(JSFile * f, char *buf, int32 len)
{
    int32 count = 0;

    while (f->nbBytesInBuf>0&&len>0) {
        buf[0] = f->byteBuffer[0];
        f->byteBuffer[0] = f->byteBuffer[1];
        f->byteBuffer[1] = f->byteBuffer[2];
        f->nbBytesInBuf--;
        len--;
        buf+=1;
        count++;
    }

    if (len>0) {
        count+= (!f->isNative)?
                    PR_Read(f->handle, buf, len):
                    fread(buf, 1, len, f->nativehandle);
    }
    return count;
}

static int32
js_FileRead(JSContext *cx, JSFile * file, jschar*buf, int32 len, int32 mode)
{
    unsigned char*aux;
    int32 count, i;
    jsint remainder;
    unsigned char utfbuf[3];

    if (file->charBufferUsed) {
      buf[0] = file->charBuffer;
      buf++;
      len--;
      file->charBufferUsed = JS_FALSE;
    }

    switch (mode) {
    case ASCII:
      aux = (unsigned char*)JS_malloc(cx, len);
      if (!aux) {
        return 0;
      }
      count = js_BufferedRead(file, aux, len);
      if (count==-1) {
        JS_free(cx, aux);
        return 0;
      }
      for (i = 0;i<len;i++) {
        buf[i] = (jschar)aux[i];
      }
      JS_free(cx, aux);
      break;
    case UTF8:
        remainder = 0;
        for (count = 0;count<len;count++) {
            i = js_BufferedRead(file, utfbuf+remainder, 3-remainder);
            if (i<=0) {
                return count;
            }
            i = utf8_to_ucs2_char(utfbuf, (int16)i, &buf[count] );
            if (i<0) {
                return count;
            } else {
                if (i==1) {
                    utfbuf[0] = utfbuf[1];
                    utfbuf[1] = utfbuf[2];
                    remainder = 2;
                } else
                if (i==2) {
                    utfbuf[0] = utfbuf[2];
                    remainder = 1;
                } else
                if (i==3)
                    remainder = 0;
            }
        }
        while (remainder>0) {
            file->byteBuffer[file->nbBytesInBuf] = utfbuf[0];
            file->nbBytesInBuf++;
            utfbuf[0] = utfbuf[1];
            utfbuf[1] = utfbuf[2];
            remainder--;
        }
      break;
    case UCS2:
      count = js_BufferedRead(file, (char*)buf, len*2)>>1;
      if (count==-1) {
          return 0;
      }
      break;
    }

    if(count==-1){
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
            JSFILEMSG_READ_FAILED, file->path);
    }

    return count;
}

static int32
js_FileSkip(JSContext *cx, JSFile * file, int32 len, int32 mode)
{
    int32 count, i;
    jsint remainder;
    unsigned char utfbuf[3];
    jschar tmp;

    switch (mode) {
    case ASCII:
        count = (!file->isNative)?
                PR_Seek(file->handle, len, PR_SEEK_CUR):
                fseek(file->nativehandle, len, SEEK_CUR);
        break;
    case UTF8:
        remainder = 0;
        for (count = 0;count<len;count++) {
            i = js_BufferedRead(file, utfbuf+remainder, 3-remainder);
            if (i<=0) {
                return 0;
            }
            i = utf8_to_ucs2_char(utfbuf, (int16)i, &tmp );
            if (i<0) {
                return 0;
            } else {

                if (i==1) {
                    utfbuf[0] = utfbuf[1];
                    utfbuf[1] = utfbuf[2];
                    remainder = 2;
                } else
                if (i==2) {
                    utfbuf[0] = utfbuf[2];
                    remainder = 1;
                } else
                if (i==3)
                    remainder = 0;
            }
        }
        while (remainder>0) {
            file->byteBuffer[file->nbBytesInBuf] = utfbuf[0];
            file->nbBytesInBuf++;
            utfbuf[0] = utfbuf[1];
            utfbuf[1] = utfbuf[2];
            remainder--;
        }
      break;
    case UCS2:
        count = (!file->isNative)?
                PR_Seek(file->handle, len*2, PR_SEEK_CUR)/2:
                fseek(file->nativehandle, len*2, SEEK_CUR)/2;
        break;
    }

    if(count==-1){
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
            JSFILEMSG_SKIP_FAILED, file->path);
    }

    return count;
}

static int32
js_FileWrite(JSContext *cx, JSFile *file, jschar *buf, int32 len, int32 mode)
{
    unsigned char   *aux;
    int32           count, i, j;
    unsigned char   *utfbuf;

    switch (mode) {
    case ASCII:
        aux = (unsigned char*)JS_malloc(cx, len);
        if (!aux)  return 0;

        for (i = 0; i<len; i++) {
            aux[i]=buf[i]%256;
        }

        count = (!file->isNative)?
                    PR_Write(file->handle, aux, len):
                    fwrite(aux, 1, len, file->nativehandle);

        if (count==-1) {
            JS_free(cx, aux);
            return 0;
        }
        JS_free(cx, aux);
        break;
    case UTF8:
        utfbuf = (unsigned char*)JS_malloc(cx, len*3);
        if (!utfbuf)  return 0;
        i = 0;
        for (count = 0;count<len;count++) {
            j = one_ucs2_to_utf8_char(utfbuf+i, utfbuf+len*3, buf[count]);
            if (j==-1) {
                JS_free(cx, utfbuf);
                return 0;
            }
            i+=j;
        }
        j = (!file->isNative)?
                PR_Write(file->handle, utfbuf, i):
                fwrite(utfbuf, 1, i, file->nativehandle);

        if (j<i) {
            JS_free(cx, utfbuf);
            return 0;
        }
        JS_free(cx, utfbuf);
      break;
    case UCS2:
        count = (!file->isNative)?
                PR_Write(file->handle, buf, len*2)>>1:
                fwrite(buf, 1, len*2, file->nativehandle)>>1;

        if (count==-1) {
            return 0;
        }
        break;
    }
    if(count==-1){
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
            JSFILEMSG_WRITE_FAILED, file->path);
    }
    return count;
}

/* ----------------------------- non-file access methods ------------------------- */
static JSBool
js_exists(JSFile *file)
{
    /* SECURITY */
    if(!file->isNative){
        return (PR_Access(file->path, PR_ACCESS_EXISTS)==PR_SUCCESS);
    }else{
        return (access(file->path, PR_ACCESS_EXISTS)==PR_SUCCESS);
    }
}

static JSBool
js_canRead(JSFile *file)
{
    if(file->isOpen&&!(file->mode&PR_RDONLY)) return JS_FALSE;
    /* SECURITY */
    if(!file->isNative){
        return (PR_Access(file->path, PR_ACCESS_READ_OK)==PR_SUCCESS);
    }else{
        return (access(file->path, PR_ACCESS_READ_OK)==PR_SUCCESS);
    }
}

static JSBool
js_canWrite(JSFile *file)
{
    if(file->isOpen&&!(file->mode&PR_WRONLY)) return JS_FALSE;
    /* SECURITY */
    if(!file->isNative){
        return (PR_Access(file->path, PR_ACCESS_WRITE_OK)==PR_SUCCESS);
    }else{
        return (access(file->path, PR_ACCESS_WRITE_OK)==PR_SUCCESS);
    }
}

static JSBool
js_isFile(JSFile *file)
{
    /* SECURITY */
    if(!file->isNative){
        PRFileInfo info;

        if ((file->isOpen)?
                        PR_GetOpenFileInfo(file->handle, &info):
                        PR_GetFileInfo(file->path, &info)!=PR_SUCCESS){
            /* TODO: report error */
            return JS_FALSE;
        }else
            return (info.type==PR_FILE_FILE);
    }else{
        struct stat buf;
        if(stat(file->path, &buf)){
            /* TODO: report error */
            return JS_FALSE;
        }
        return (buf.st_mode&_S_IFREG);
    }
}

static JSBool
js_isDirectory(JSFile *file)
{
    /* SECURITY */
    if(!file->isNative){
        PRFileInfo info;

        if ((file->isOpen)?
                        PR_GetOpenFileInfo(file->handle, &info):
                        PR_GetFileInfo(file->path, &info)!=PR_SUCCESS){
            /* TODO: report error */
            return JS_FALSE;
        }else
            return (info.type==PR_FILE_DIRECTORY);
    }else{
        struct stat buf;
        if(stat(file->path, &buf )){
            /* TODO: report error */
            return JS_FALSE;
        }
        return (buf.st_mode&_S_IFDIR);
    }
}

static int
js_size(JSContext *cx, JSFile *file)
{
    /* SECURITY */
    if(!file->isNative){
        PRFileInfo info;

        if ((file->isOpen)?
                        PR_GetOpenFileInfo(file->handle, &info):
                        PR_GetFileInfo(file->path, &info)!=PR_SUCCESS){
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                JSFILEMSG_CANNOT_ACCESS_FILE_STATUS, file->path);
            return -1;
        }else
            return info.size;
    }else{
        struct stat buf;
        if(stat(file->path, &buf)){
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                JSFILEMSG_CANNOT_ACCESS_FILE_STATUS, file->path);
            return -1;
        }else
            return buf.st_size;
    }
}


/* -------------------------------- File methods ------------------------------ */
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
    if(file->isOpen && file->isNative){
        JS_ReportWarning(cx, "Native file %s is already open, proceeding", file->path);
        *rval = JSVAL_TRUE;
        return JS_TRUE;
    }

    /* Close before proceeding */
    if (file->isOpen) {
        JS_ReportWarning(cx, "File %s is already open, we will close it and reopen, proceeding");
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
    mask = 0;
    /* TODO: this is pretty ugly, BTW, we walk thru the string too many times */
    mask|=(js_FileHasOption(cx, mode, "readOnly"))?  PR_RDONLY       :   0;
    mask|=(js_FileHasOption(cx, mode, "writeOnly"))? PR_WRONLY       :   0;
    mask|=(js_FileHasOption(cx, mode, "readWrite"))? PR_RDWR         :   0;
    mask|=(js_FileHasOption(cx, mode, "append"))?    PR_APPEND       :   0;
    mask|=(js_FileHasOption(cx, mode, "create"))?    PR_CREATE_FILE  :   0;
    mask|=(js_FileHasOption(cx, mode, "replace"))?   PR_TRUNCATE     :   0;

    if ((mask&(PR_RDONLY|PR_WRONLY))==0) mask|=PR_RDWR;

    file->hasAutoflush|=(js_FileHasOption(cx, mode, "hasAutoflush"));

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
        type = ASCII;
    else
    if (!strcmp(ctype, unicodestring))
        type = UCS2;
    else
        type = UTF8;

    /* Save the relevant fields */
    file->type = type;
    file->mode = mask;
    file->nativehandle = NULL;
    file->hasRandomAccess = (type!=UTF8);

    /*
		Deal with pipes here. We can't use NSPR for pipes,
        so we have to use POPEN.
	*/
    if(file->path[0]==PIPE_SYMBOL || file->path[len-1]==PIPE_SYMBOL){
        if(file->path[0]==PIPE_SYMBOL && file->path[len-1]){
			JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
				JSFILEMSG_BIDIRECTIONAL_PIPE_NOT_SUPPORTED);
				return JS_FALSE;
        }else{
            char pipemode[3];
            /* special SECURITY */

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
                command[len-1] = '\0';
                /* open(STATUS, "netstat -an 2>&1 |") */
                pipemode[0] = 'r';
                pipemode[1] = file->type==UTF8?'b':'t';
                pipemode[2] = '\0';
                file->nativehandle = POPEN(command, pipemode);
				JS_free(cx, command);
            }
            /* set the flags */
            file->isNative = JS_TRUE;
            file->isAPipe = JS_TRUE;
        }
    }else{
        /* SECURITY */
        /* TODO: what about the permissions?? Java seems to ignore the problem.. */
        file->handle = PR_Open(file->path, mask, 0644);
    }

    js_ResetBuffers(file);

    /* Set the open flag and return result */
    if (file->handle==NULL && file->nativehandle==NULL){
        file->isOpen = JS_FALSE;
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
            JSFILEMSG_OPEN_FAILED, file->path);
        return JS_FALSE;
    }else{
        file->isOpen = JS_TRUE;
        *rval = JSVAL_TRUE;
        return JS_TRUE;
    }
}

static JSBool
file_close(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSFile  *file;

    /* SECURITY ? */
    file = JS_GetInstancePrivate(cx, obj, &file_class, NULL);

    if(!file->isOpen){
        JS_ReportWarning(cx, "File %s is not open, can't close it, proceeding", file->path);
    }

    if(!file->isAPipe){
        if ((!file->isNative)?
                PR_Close(file->handle):
                fclose(file->nativehandle)!=0){
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                    JSFILEMSG_CLOSE_FAILED, file->path);

            return JS_FALSE;
        }
    }else{
        if(PCLOSE(file->nativehandle)==-1){
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                JSFILEMSG_PCLOSE_FAILED, file->path);
            return JS_FALSE;
        }
    }

    js_ResetAttributes(file);
    *rval = JSVAL_TRUE;

    return JS_TRUE;
}


static JSBool
file_remove(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSFile  *file = JS_GetInstancePrivate(cx, obj, &file_class, NULL);;

    /* SECURITY */
    if(!file->isNative){
        if ((js_isDirectory(file) ? PR_RmDir(file->path) : PR_Delete(file->path))==PR_SUCCESS) {
            js_ResetAttributes(file);
            *rval = JSVAL_TRUE;
        } else {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                JSFILEMSG_REMOVE_FAILED, file->path);
            return JS_FALSE;
        }
    }else{
        if ((js_isDirectory(file) ? rmdir(file->path) : remove(file->path))==PR_SUCCESS) {
            js_ResetAttributes(file);
            *rval = JSVAL_TRUE;
        } else {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                JSFILEMSG_REMOVE_FAILED, file->path);
            return JS_FALSE;
        }
    }
}

/* Raw PR-based function. No text processing. Just raw data copying. */
static JSBool
file_copyTo(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSFile      *file;
    char        *dest = NULL;
    PRFileDesc  *handle = NULL;
    char        *buffer;
    uint32      count, size;
    JSBool      fileInitiallyOpen=JS_FALSE;

    /* SECURITY */

    if (argc!=1){
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
            JSFILEMSG_EXPECTS_ONE_ARG_ERROR, "copyTo", argc);
        goto out;
    }

    file = JS_GetInstancePrivate(cx, obj, &file_class, NULL);

    dest = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));

    /* SECURITY */
    /* make sure we are not reading a file open for writing */
    if (file->isOpen && (file->mode&PR_WRONLY)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                JSFILEMSG_CANNOT_COPY_FILE_OPEN_FOR_WRITING_ERROR, file->path);
        return JS_FALSE;
    }

    /* open a closed file */
    if(!file->isOpen){
        js_FileOpen(cx, obj, file, "readOnly");
    }else
        fileInitiallyOpen = JS_TRUE;

    if (file->handle==NULL && file->nativehandle==NULL){
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
            JSFILEMSG_OPEN_FAILED, file->path);
        goto out;
    }else
        file->isOpen = JS_TRUE;

    handle = PR_Open(dest, PR_WRONLY|PR_CREATE_FILE|PR_TRUNCATE, 0644);

    if(!handle){
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
            JSFILEMSG_OPEN_FAILED, dest);
        goto out;
    }

    if ((size=js_size(cx, file))!=-1) {
        goto out;
    }

    buffer = JS_malloc(cx, size);

    count = (!file->isNative)?
            PR_Read(file->handle, buffer, size):
            fread(buffer, 1, size, file->nativehandle);

    /* reading panic */
    if (count!=size) {
        JS_free(cx, buffer);
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
              JSFILEMSG_COPY_READ_ERROR, file->path);
        goto out;
    }

    count = PR_Write(handle, buffer, size);

    /* writing panic */
    if (count!=size) {
        JS_free(cx, buffer);
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
              JSFILEMSG_COPY_WRITE_ERROR, file->path);
        goto out;
    }

    JS_free(cx, buffer);

    if(((!file->isNative)?PR_Close(file->handle):fclose(file->nativehandle))!=PR_SUCCESS){
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
              JSFILEMSG_CLOSE_FAILED, file->path);
        goto out;
    }

    if(PR_Close(handle)!=PR_SUCCESS){
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
              JSFILEMSG_CLOSE_FAILED, dest);
        goto out;
    }

    *rval = JSVAL_TRUE;
    return JS_TRUE;
out:
    if(file->isOpen && !fileInitiallyOpen){
        if((!file->isNative)?PR_Close(file->handle):fclose(file->nativehandle)!=PR_SUCCESS){
            JS_ReportWarning(cx, "Can't close %s, proceeding", file->path);
        }
    }

    if(handle && PR_Close(handle)!=PR_SUCCESS){
        JS_ReportWarning(cx, "Can't close %s, proceeding", dest);
    }

    *rval = JSVAL_FALSE;
    return JS_FALSE;
}

static JSBool
file_renameTo(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSFile  *file;
    char    *dest;

    if (argc<1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
            JSFILEMSG_EXPECTS_ONE_ARG_ERROR, "rename", argc);
        *rval = JSVAL_TRUE;
        return JS_FALSE;
    }

    file = JS_GetInstancePrivate(cx, obj, &file_class, NULL);

    dest = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));

    /* SECURITY */
    if (((!file->isNative)?PR_Rename(file->path, dest):rename(file->path, dest))==PR_SUCCESS){
        *rval = JSVAL_TRUE;
        return JS_TRUE;
    }else{
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
            JSFILEMSG_RENAME_FAILED, file->path, dest);
        *rval = JSVAL_FALSE;
        return JS_FALSE;
    }
}

static JSBool
file_flush(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSFile *file;

    file = JS_GetInstancePrivate(cx, obj, &file_class, NULL);

    if (!file->isOpen) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
           JSFILEMSG_CANNOT_FLUSH_CLOSE_FILE_ERROR, file->path);
        goto out;
    }

    /* SECURITY */
    if ((!file->isNative)?PR_Sync(file->handle):fflush(file->nativehandle)==PR_SUCCESS)
      *rval = JSVAL_TRUE;
    else{
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
           JSFILEMSG_CANNOT_FLUSH, file->path);
       goto out;
    }
out:
    *rval = JSVAL_FALSE;
    return JS_FALSE;
}

static JSBool
file_write(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    uintN       i;
    JSFile      *file;
    JSString    *str;
    int32       count;

    file = JS_GetInstancePrivate(cx, obj, &file_class, NULL);

    if(!js_CanWrite(file)){
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
            JSFILEMSG_CANNOT_WRITE, file->path);
        goto out;
    }

    /* SECURITY */
    /* open file if necessary */
    if (!file->isOpen) {
        js_FileOpen(cx, obj, file, "create, append, writeOnly");
    }

    for (i = 0; i<argc; i++) {
        str = JS_ValueToString(cx, argv[i]);
        count = js_FileWrite(cx, file, JS_GetStringChars(str), JS_GetStringLength(str), file->type);
        if (count==-1){
          *rval = JSVAL_FALSE;
          return JS_FALSE;
        }
    }

    *rval = JSVAL_TRUE;
    return JS_TRUE;
out:
    *rval = JSVAL_FALSE;
    return JS_FALSE;
}

static JSBool
file_writeln(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSFile      *file;
    JSString    *str;

    file = JS_GetInstancePrivate(cx, obj, &file_class, NULL);

    if(!js_CanWrite(file)){
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
            JSFILEMSG_CANNOT_WRITE, file->path);
        goto out;
    }

    /* don't report an error here */
    if(!file_write(cx, obj, argc, argv, rval))  return JS_FALSE;
    /* don't do security here -- we passed the check in file_write */
    str = JS_NewStringCopyZ(cx, LINEBREAK);

    if (js_FileWrite(cx, file, JS_GetStringChars(str), JS_GetStringLength(str), file->type)==-1){
        *rval = JSVAL_FALSE;
        return JS_FALSE;
    }

    /* eol causes flush if hasAutoflush is turned on */
    if (file->hasAutoflush)
        file_flush(cx, obj, 0, NULL, NULL);

    *rval =  JSVAL_TRUE;
    return JS_TRUE;
out:
    *rval = JSVAL_FALSE;
    return JS_FALSE;
}

static JSBool
file_writeAll(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSFile      *file;
    jsuint      i;
    jsuint      limit;
    JSObject    *array;
    JSObject    *elem;
    jsval       elemval;

    file = JS_GetInstancePrivate(cx, obj, &file_class, NULL);

    if(!js_CanWrite(file)){
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
            JSFILEMSG_CANNOT_WRITE, file->path);
        goto out;
    }

    /* SECURITY */

    if (argc<1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
            JSFILEMSG_WRITEALL_EXPECTS_ONE_ARG_ERROR);
        goto out;
    }

    if (!JS_IsArrayObject(cx, JSVAL_TO_OBJECT(argv[0]))) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
            JSFILEMSG_FIRST_ARGUMENT_WRITEALL_NOT_ARRAY_ERROR);
        goto out;
    }
    array = JSVAL_TO_OBJECT(argv[0]);
    if (!JS_GetArrayLength(cx, array, &limit)) {
        /* TODO: error */
        goto out;
    }

    for (i = 0; i<limit; i++) {
        if (!JS_GetElement(cx, array, i, &elemval))  return JS_FALSE;
        elem = JSVAL_TO_OBJECT(elemval);
        file_writeln(cx, obj, 1, &elemval, rval);
    }

    *rval = JSVAL_TRUE;
    return JS_TRUE;
out:
    *rval = JSVAL_FALSE;
    return JS_FALSE;
}

static JSBool
file_read(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSFile      *file;
    JSString    *str;
    int32       want, count;
    jschar      *buf;

    file = JS_GetInstancePrivate(cx, obj, &file_class, NULL);

    if(!js_CanRead(file)){
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
            JSFILEMSG_CANNOT_READ, file->path);
        goto out;
    }

    /* SECURITY */

    if (argc!=1){
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
            JSFILEMSG_EXPECTS_ONE_ARG_ERROR, "copyTo", argc);
        goto out;
    }

    if (!file->isOpen) {
        js_FileOpen(cx, obj, file, "readOnly");
    }

    if (!JS_ValueToInt32(cx, argv[0], &want)){
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
            JSFILEMSG_FIRST_ARGUMENT_MUST_BE_A_NUMBER, "read", argv[0]);
        goto out;
    }

    /* want = (want>262144)?262144:want; * arbitrary size limitation */

    buf = JS_malloc(cx, want*sizeof buf[0]);
    if (!buf)  goto out;

    count =  js_FileRead(cx, file, buf, want, file->type);
    if (count>0) {
        /* TODO: do we need unicode here? */
        str = JS_NewUCStringCopyN(cx, buf, count);
        *rval = STRING_TO_JSVAL(str);
        JS_free(cx, buf);
        return JS_TRUE;
    } else {
        JS_free(cx, buf);
        goto out;
    }
out:
    *rval = JSVAL_FALSE;
    return JS_FALSE;
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
    JSBool      endofline;

    file = JS_GetInstancePrivate(cx, obj, &file_class, NULL);

    /* SECURITY */
    if(!js_CanRead(file)){
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
            JSFILEMSG_CANNOT_READ, file->path);
        goto out;
    }

    if (!file->isOpen){
        js_FileOpen(cx, obj, file, "readOnly");
    }

    if (!file->linebuffer) {
        buf = JS_malloc(cx, 128*(sizeof data));
        if (!buf) goto out;
        file->linebuffer = JS_NewUCString(cx, buf, 128);
    }
    room = JS_GetStringLength(file->linebuffer);
    offset = 0;

    /* XXX TEST ME!! TODO: yes, please do */
    for(;;) {
        if (!js_FileRead(cx, file, &data, 1, file->type)) {
            endofline = JS_FALSE;
            goto loop;
        }
        switch (data) {
        case '\n' :
            endofline = JS_TRUE;
            goto loop;
        case '\r' :
            if (!js_FileRead(cx, file, &data2, 1, file->type)) {
                endofline = JS_TRUE;
                goto loop;
            }
            if (data2!='\n') { /* we read one char to far. buffer it. */
                file->charBuffer = data2;
                file->charBufferUsed = JS_TRUE;
            }
            endofline = JS_TRUE;
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
                file->linebuffer->length =  offset+128;
            }
            file->linebuffer->chars[offset++] = data;
            break;
        }
    }
loop:
    file->linebuffer->chars[offset] = 0;
    if ((endofline==JS_TRUE)) {
        str = JS_NewUCStringCopyN(cx, JS_GetStringChars(file->linebuffer),
                                    offset);
        *rval = STRING_TO_JSVAL(str);
        return JS_TRUE;
    } else{
        goto out;
    }
out:
    *rval = JSVAL_NULL;
    return JS_FALSE;
}

static JSBool
file_readAll(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSFile      *file;
    JSObject    *array;
    jsint       len, size;
    jsval       line;
    JSBool      ok = JS_TRUE;

    file = JS_GetInstancePrivate(cx, obj, &file_class, NULL);

    /* SECURITY */
    array = JS_NewArrayObject(cx, 0, NULL);
    len = 0;

    if (!file->isOpen) {
        js_FileOpen(cx, obj, file, "readOnly");
    }

    if(size = js_size(cx, file)==-1) goto out;

    while (ok&&
            (size>(JSUint32)
                ((!file->isNative)?
                    PR_Seek(file->handle, 0, PR_SEEK_CUR):
                    fseek(file->nativehandle, 0, SEEK_CUR)))) {
        if(!file_readln(cx, obj, 0, NULL, &line)) goto out;
        JS_SetElement(cx, array, len, &line);
        len++;
    }

    *rval = OBJECT_TO_JSVAL(array);
    return JS_TRUE;
out:
    *rval = JSVAL_FALSE;
    return JS_FALSE;
}

static JSBool
file_skip(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSFile      *file;
    int32       toskip;

    if (argc!=1){
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
            JSFILEMSG_EXPECTS_ONE_ARG_ERROR, "copyTo", argc);
        goto out;
    }

    if (!JS_ValueToInt32(cx, argv[0], &toskip)){
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
            JSFILEMSG_FIRST_ARGUMENT_MUST_BE_A_NUMBER, "skip", argv[0]);
        goto out;
    }

    file = JS_GetInstancePrivate(cx, obj, &file_class, NULL);

    /* SECURITY */
    if (!file->isOpen) {
        if(!js_FileOpen(cx, obj, file, "readOnly")) goto out;
    }

    if (js_FileSkip(cx, file, toskip, file->type)!=toskip) {
        goto out;
    }else
        return JS_TRUE;
out:
    *rval = JSVAL_FALSE;
    return JS_FALSE;
}

static JSBool
file_list(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    PRDir       *dir;
    PRDirEntry  *entry;
    JSFile      *file;
    JSObject    *array;
    JSObject    *eachFile;
    jsint       len;
    jsval       v;
    JSRegExp    *re = NULL;
    JSFunction  *func = NULL;
    JSString    *str;
    jsval       args[1];
    char        *filePath;

    /* SECURITY */
    file = JS_GetInstancePrivate(cx, obj, &file_class, NULL);

    if (argc=1) {
        if (JSVAL_IS_REGEXP(cx, argv[0])) {
            re = JS_GetPrivate(cx, JSVAL_TO_OBJECT(argv[0]));
        }else
        if (JSVAL_IS_FUNCTION(cx, argv[0])) {
            func = JS_GetPrivate(cx, JSVAL_TO_OBJECT(argv[0]));
        }else{
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                JSFILEMSG_FIRST_ARGUMENT_MUST_BE_A_FUNCTION_OR_REGEX, argv[0]);
            goto out;
        }
    }else{
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
            JSFILEMSG_FIRST_ARGUMENT_MUST_BE_A_NUMBER, "skip", argv[0]);
        goto out;
    }

    if (!js_isDirectory(file)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
            JSFILEMSG_CANNOT_DO_LIST_ON_A_FILE, file->path);
        goto out;
    }

    /* TODO: native support? */
    if(file->isNative){
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
            JSFILEMSG_NATIVE_OPERATION_IS_NOT_SUPPORTED, "list", file->path);
        goto out;
    }

    dir = PR_OpenDir(file->path);
    if(!dir){
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
            JSFILEMSG_OPEN_FAILED, file->path);
        goto out;
    }

    /* create JSArray here... */
    array = JS_NewArrayObject(cx, 0, NULL);
    len = 0;

    while ((entry = PR_ReadDir(dir, PR_SKIP_BOTH))!=NULL) {
        /* first, check if we have a regexp */
        if (re!=NULL) {
            size_t index = 0;

            str = JS_NewStringCopyZ(cx, entry->name);
            if(!js_ExecuteRegExp(cx, re, str, &index, JS_TRUE, &v)){
                /* don't report anything here */
                goto out;
            }
            /* not matched! */
            if (JSVAL_IS_NULL(v)) {
                continue;
            }
        }else
        if (func!=NULL) {
            str = JS_NewStringCopyZ(cx, entry->name);
            args[0] = STRING_TO_JSVAL(str);
            if(!JS_CallFunction(cx, obj, func, 1, args, &v)){
                goto out;
            }

            if (v==JSVAL_FALSE) {
                continue;
            }
        }

        filePath = combinePath(cx, file->path, (char*)entry->name);

        eachFile = js_NewFileObject(cx, filePath);
        JS_free(cx, filePath);
        if (!eachFile){
            /* TODO: error */
            goto out;
        }
        v = OBJECT_TO_JSVAL(eachFile);
        JS_SetElement(cx, array, len, &v);
        JS_SetProperty(cx, array, entry->name, &v); /* accessible by name.. make sense I think.. */
        len++;
    }

    if(!PR_CloseDir(dir)){
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
            JSFILEMSG_CLOSE_FAILED, file->path);
        goto out;
    }
    *rval = OBJECT_TO_JSVAL(array);
    return JS_TRUE;
out:
    *rval = JSVAL_NULL;
    return JS_FALSE;
}

static JSBool
file_mkdir(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSFile      *file;

    if (argc!=1){
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
            JSFILEMSG_EXPECTS_ONE_ARG_ERROR, "mkdir", argc);
        goto out;
    }

    file = JS_GetInstancePrivate(cx, obj, &file_class, NULL);
    /* SECURITY */

    /* if the current file is not a directory, find out the directory name */
    if (!js_isDirectory(file)) {
        char        *dir = fileDirectoryName(cx, file->path);
        JSObject    *dirObj = js_NewFileObject(cx, dir);

        JS_free(cx, dir);

        /* call file_mkdir with the right set of parameters if needed */
        if (!file_mkdir(cx, dirObj, argc, argv, rval)) {
            goto out;
        }
    }else{
        char *dirName = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
        char *fullName;

        fullName = combinePath(cx, file->path, dirName);
        if (PR_MkDir(fullName, 0755)==PR_SUCCESS){
            *rval = JSVAL_TRUE;
            JS_free(cx, fullName);
            return JS_TRUE;
        }else{
            /* TODO: error */
            JS_free(cx, fullName);
            goto out;
        }
    }
out:
    *rval = JSVAL_FALSE;
    return JS_FALSE;
}

static JSBool
file_toString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSFile *file;

    file = JS_GetInstancePrivate(cx, obj, &file_class, NULL);
    /* SECURITY ? */
    *rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, file->path));
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
    FILE_OPEN               = -11,
    FILE_TYPE               = -12,
    FILE_MODE               = -13,
    FILE_CREATED            = -14,
    FILE_MODIFIED           = -15,
    FILE_SIZE               = -16,
    FILE_RANDOMACCESS       = -17,
    FILE_POSITION           = -18,
    FILE_APPEND             = -19,
    FILE_REPLACE            = -20,
    FILE_AUTOFLUSH          = -21,
};

static JSPropertySpec file_props[] = {
    {"length",          FILE_LENGTH,        JSPROP_ENUMERATE | JSPROP_READONLY },
    {"parent",          FILE_PARENT,        JSPROP_ENUMERATE | JSPROP_READONLY },
    {"path",            FILE_PATH,          JSPROP_ENUMERATE | JSPROP_READONLY },
    {"name",            FILE_NAME,          JSPROP_ENUMERATE | JSPROP_READONLY },
    {"isDirectory",     FILE_ISDIR,         JSPROP_ENUMERATE | JSPROP_READONLY },
    {"isFile",          FILE_ISFILE,        JSPROP_ENUMERATE | JSPROP_READONLY },
    {"exists",          FILE_EXISTS,        JSPROP_ENUMERATE | JSPROP_READONLY },
    {"canRead",         FILE_CANREAD,       JSPROP_ENUMERATE | JSPROP_READONLY },
    {"canWrite",        FILE_CANWRITE,      JSPROP_ENUMERATE | JSPROP_READONLY },
    {"canAppend",       FILE_APPEND,        JSPROP_ENUMERATE | JSPROP_READONLY },
    {"canReplace",      FILE_REPLACE,       JSPROP_ENUMERATE | JSPROP_READONLY },
    {"isOpen",          FILE_OPEN,          JSPROP_ENUMERATE | JSPROP_READONLY },
    {"type",            FILE_TYPE,          JSPROP_ENUMERATE | JSPROP_READONLY },
    {"mode",            FILE_MODE,          JSPROP_ENUMERATE | JSPROP_READONLY },
    {"creationTime",    FILE_CREATED,       JSPROP_ENUMERATE | JSPROP_READONLY },
    {"lastModified",    FILE_MODIFIED,      JSPROP_ENUMERATE | JSPROP_READONLY },
    {"size",            FILE_SIZE,          JSPROP_ENUMERATE | JSPROP_READONLY },
    {"hasRandomAccess", FILE_RANDOMACCESS,  JSPROP_ENUMERATE | JSPROP_READONLY },
    {"hasAutoflush",    FILE_AUTOFLUSH,     JSPROP_ENUMERATE | JSPROP_READONLY },
    {"position",        FILE_POSITION,      JSPROP_ENUMERATE },
    {0}
};


static JSBool
file_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSFile      *file;
    char        *str;
    jsint       tiny;
    PRFileInfo  info;
    PRExplodedTime
                expandedTime;
    JSObject    *newobj;

    tiny = JSVAL_TO_INT(id);
    file = JS_GetInstancePrivate(cx, obj, &file_class, NULL);
    if(!file) return JS_TRUE;

    /* SECURITY ??? */
    switch (tiny) {
    case FILE_PARENT:
        str = fileDirectoryName(cx, file->path);
        /* root.parent = null ??? */
        if(!strcmp(file->path, str) ||
                (!strncmp(str, file->path, strlen(str)-1)&&file->path[strlen(file->path)]-1)==FILESEPARATOR){
            *vp = JSVAL_NULL;
        }else{
            newobj = js_NewFileObject(cx, str);
            JS_free(cx, str);
            *vp = OBJECT_TO_JSVAL(newobj);
        }
        break;
    case FILE_PATH:
        *vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, file->path));
        break;
    case FILE_NAME:
        str = fileBaseName(cx, file->path);
        *vp = STRING_TO_JSVAL(JS_NewString(cx, str, strlen(str)));
        break;
    case FILE_ISDIR:
        *vp = BOOLEAN_TO_JSVAL(js_isDirectory(file));
        break;
    case FILE_ISFILE:
        *vp = BOOLEAN_TO_JSVAL(js_isFile(file));
        break;
    case FILE_EXISTS:
        *vp = BOOLEAN_TO_JSVAL(js_exists(file));
        break;
    case FILE_CANREAD:
        *vp = BOOLEAN_TO_JSVAL(js_canRead(file));
        break;
    case FILE_CANWRITE:
        *vp = BOOLEAN_TO_JSVAL(js_canWrite(file));
        break;
    case FILE_OPEN:
        *vp = BOOLEAN_TO_JSVAL(file->isOpen);
        break;
    case FILE_APPEND :
        *vp = BOOLEAN_TO_JSVAL((file->mode&PR_APPEND)==PR_APPEND);
        break;
    case FILE_REPLACE :
        *vp = BOOLEAN_TO_JSVAL((file->mode&PR_TRUNCATE)==PR_TRUNCATE);
        break;
    case FILE_AUTOFLUSH :
        *vp = BOOLEAN_TO_JSVAL(file->hasAutoflush);
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
    /*case FILE_MODE:
        str = (char*)JS_malloc(cx, 200);
        str[0] = '\0';
        flag = JS_FALSE;

        if ((file->mode&PR_RDONLY)==PR_RDONLY) {
            if (flag) strcat(str, ",");
            strcat(str, "readOnly");
            flag = JS_TRUE;
        }
        if ((file->mode&PR_WRONLY)==PR_WRONLY) {
            if (flag) strcat(str, ",");
            strcat(str, "writeOnly");
            flag = JS_TRUE;
        }
        if ((file->mode&PR_RDWR)==PR_RDWR) {
            if (flag) strcat(str, ",");
            strcat(str, "readWrite");
            flag = JS_TRUE;
        }
        if ((file->mode&PR_APPEND)==PR_APPEND) {
            if (flag) strcat(str, ",");
            strcat(str, "append");
            flag = JS_TRUE;
        }
        if ((file->mode&PR_CREATE_FILE)==PR_CREATE_FILE) {
            if (flag) strcat(str, ",");
            strcat(str, "create");
            flag = JS_TRUE;
        }
        if ((file->mode&PR_TRUNCATE)==PR_TRUNCATE) {
            if (flag) strcat(str, ",");
            strcat(str, "replace");
            flag = JS_TRUE;
        }
        if (file->hasAutoflush) {
            if (flag) strcat(str, ",");
            strcat(str, "hasAutoflush");
            flag = JS_TRUE;
        }
        *vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, str));
        JS_free(cx, str);
        break;
    */
    case FILE_CREATED:
        if(!file->isNative){
            if(((file->isOpen)?
                            PR_GetOpenFileInfo(file->handle, &info):
                            PR_GetFileInfo(file->path, &info))!=PR_SUCCESS){
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                    JSFILEMSG_CANNOT_ACCESS_FILE_STATUS, file->path);
                return JS_FALSE;
            }
        }else{
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                JSFILEMSG_NATIVE_OPERATION_IS_NOT_SUPPORTED, "creationTime", file->path);
            return JS_FALSE;
        }

        PR_ExplodeTime(info.creationTime, PR_LocalTimeParameters, &expandedTime);
        *vp = OBJECT_TO_JSVAL(js_NewDateObject(cx,  expandedTime.tm_year,
                                    expandedTime.tm_month,
                                    expandedTime.tm_mday,
                                    expandedTime.tm_hour,
                                    expandedTime.tm_min,
                                    expandedTime.tm_sec));
        break;
    case FILE_MODIFIED:
        if(!file->isNative){
            if(((file->isOpen)?
                            PR_GetOpenFileInfo(file->handle, &info):
                            PR_GetFileInfo(file->path, &info))!=PR_SUCCESS){
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                    JSFILEMSG_CANNOT_ACCESS_FILE_STATUS, file->path);
                return JS_FALSE;
            }
        }else{
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                JSFILEMSG_NATIVE_OPERATION_IS_NOT_SUPPORTED, "lastModified", file->path);
            return JS_FALSE;
        }

        PR_ExplodeTime(info.modifyTime, PR_LocalTimeParameters, &expandedTime);
        *vp = OBJECT_TO_JSVAL(js_NewDateObject(cx, expandedTime.tm_year,
                                    expandedTime.tm_month,
                                    expandedTime.tm_mday,
                                    expandedTime.tm_hour,
                                    expandedTime.tm_min,
                                    expandedTime.tm_sec));
        break;
    case FILE_LENGTH:
        if(!file->isNative){
            if (js_isDirectory(file)) { /* XXX debug me */
                PRDir       *dir;
                PRDirEntry  *entry;
                jsint       count = 0;

                if(!(dir = PR_OpenDir(file->path))){
                    /* TODO: error */
                    return JS_FALSE;
                }

                while ((entry = PR_ReadDir(dir, PR_SKIP_BOTH))) {
                    count++;
                }

                if(!PR_CloseDir(dir)){
                    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                        JSFILEMSG_CLOSE_FAILED, file->path);

                    return JS_FALSE;
                }

                *vp = INT_TO_JSVAL(count);
                break;
            }else{
                /* return file size */
                if(((file->isOpen)?
                        PR_GetOpenFileInfo(file->handle, &info):
                        PR_GetFileInfo(file->path, &info))!=PR_SUCCESS){
                        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                            JSFILEMSG_CANNOT_ACCESS_FILE_STATUS, file->path);
						return JS_FALSE;
					}
					*vp = INT_TO_JSVAL(info.size);
            }
        }else{
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                JSFILEMSG_NATIVE_OPERATION_IS_NOT_SUPPORTED, "length", file->path);
            return JS_FALSE;
        }

        break;
    case FILE_RANDOMACCESS:
            *vp = BOOLEAN_TO_JSVAL(file->hasRandomAccess);
        break;
    case FILE_POSITION:
        if (file->isOpen) {
            *vp = INT_TO_JSVAL(!file->isNative?
                        PR_Seek(file->handle, 0, PR_SEEK_CUR):
                        fseek(file->nativehandle, 0, SEEK_CUR));
        }else {
            JS_ReportWarning(cx, "File %s is closed, can't report the position, proceeding");
            *vp = JSVAL_VOID;
        }
        break;
    default:{
		/* this is some other property -- try to use the dir["file"] syntax */
		if(js_isDirectory(file)){
			PRDir *dir = NULL;
			PRDirEntry *entry = NULL;
            char *prop_name = JS_GetStringBytes(JS_ValueToString(cx, id));

            dir = PR_OpenDir(file->path);
            if(!dir) {
                /* Don't return false, just proceed, this shouldn't happen */
				JS_ReportWarning(cx, "Can't open directory %s", file->path);
                return JS_TRUE;
            }

            while((entry = PR_ReadDir(dir, PR_SKIP_NONE))!=NULL){
				if(!strcmp(entry->name, prop_name)){
					str = combinePath(cx, file->path, prop_name);
                    *vp = OBJECT_TO_JSVAL(js_NewFileObject(cx, str));
					JS_free(cx, str);
					return JS_TRUE;
				}
			}
		}
		}
    }
    return JS_TRUE;
}

static JSBool
file_setProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSFile  *file;
    jsint   slot;
    int32   offset;
    int32   count;

    file = JS_GetInstancePrivate(cx, obj, &file_class, NULL);

    /* SECURITY ? */
    if (JSVAL_IS_STRING(id)){
        return JS_TRUE;
    }

    slot = JSVAL_TO_INT(id);

    switch (slot) {
    /* File.position  = 10 */
    case FILE_POSITION:
        if (file->hasRandomAccess) {
            offset = JSVAL_TO_INT(*vp);
            count = (!file->isNative)?
                        PR_Seek(file->handle, offset, PR_SEEK_SET):
                        fseek(file->nativehandle, offset, SEEK_SET);
            js_ResetBuffers(file);
            *vp = INT_TO_JSVAL(count);
        }
        break;
    default:
        break;
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
            if (file && (!js_exists(file) || !js_isDirectory(file))){
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
        rhsObject = js_NewFileObject(cx, path);
        if (!rhsObject)  return JS_FALSE;

        file = JS_GetInstancePrivate(cx, rhsObject, &file_class, NULL);
        if (!file || !js_exists(file) || !js_isDirectory(file)){
            JS_GetProperty(cx, obj, CURRENTDIR_PROPERTY, vp);
        }else{
            *vp = OBJECT_TO_JSVAL(rhsObject);
            chdir(path);
        }
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

    file = JS_malloc(cx, sizeof *file);
    if (!file) return NULL;
    memset(file, 0 , sizeof *file);

    /* canonize the path */
    /*file->path = canonicalPath(cx, bytes);*/
    file->path = absolutePath(cx, bytes);

    js_ResetAttributes(file);

    if (!JS_SetPrivate(cx, obj, file)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
            JSFILEMSG_CANNOT_SET_PRIVATE_FILE, file->path);
        JS_free(cx, file);
        return NULL;
    }else
        return file;
}

/* Returns a JSObject */
JS_EXPORT_API(JSObject*)
js_NewFileObject(JSContext *cx, char *filename)
{
    JSObject    *obj;
    JSFile      *file;

    obj = JS_NewObject(cx, &file_class, NULL, NULL);
    if (!obj){
        /* TODO: error ? */
        return NULL;
    }
    file = file_init(cx, obj, filename);
    if(!file) return NULL;
    return obj;
}

/* Internal function, used for cases which NSPR file support doesn't cover */
JS_EXPORT_API(JSObject*)
js_NewFileObjectFromFILE(JSContext *cx, FILE *nativehandle, char *filename, JSBool open)
{
    JSObject *obj;
    JSFile   *file;
#ifdef XP_MAC
    JS_ReportWarning(cx, "Native files are not fully supported on the MAC");
#endif

    obj = JS_NewObject(cx, &file_class, NULL, NULL);
    if (!obj){
        /* TODO: error ? */
        return NULL;
    }
    file = file_init(cx, obj, filename);
    if(!file) return NULL;

    file->nativehandle = nativehandle;
    file->path = strdup(filename);
    file->isOpen = open;
    file->isNative = JS_TRUE;
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

    str = (argc==0)?JS_InternString(cx, ""):JS_ValueToString(cx, argv[0]);

    if (!str){
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
            JSFILEMSG_FIRST_ARGUMENT_CONSTRUCTOR_NOT_STRING_ERROR, argv[0]);
        return JS_FALSE;
    }

    /* SECURITY? */
    file = file_init(cx, obj, JS_GetStringBytes(str));
    if (!file)  return JS_FALSE;

    return JS_TRUE;
}

JS_EXPORT_API(JSObject*)
js_InitFileClass(JSContext *cx, JSObject* obj)
{
    JSObject *file, *ctor, *afile;
    JSFile   *fileObj;
    jsval    vp;
    char     *currentdir;

    file = JS_InitClass(cx, obj, NULL, &file_class, File, 1,
        file_props, file_functions, NULL, NULL);
    if (!file) {
        /* TODO: error */
        return NULL;
    }

    /* SECURITY -- need to make sure we want to have default streams */
    ctor = JS_GetConstructor(cx, file);
    if (!ctor)  return NULL;

	/* Define CURRENTDIR property. We are doing this to get a
	slash at the end of the current dir */
    afile = js_NewFileObject(cx, CURRENT_DIR);
    currentdir =  JS_malloc(cx, MAX_PATH_LENGTH);
    currentdir =  getcwd(currentdir, MAX_PATH_LENGTH);
    afile = js_NewFileObject(cx, currentdir);
    JS_free(cx, currentdir);
    vp = OBJECT_TO_JSVAL(afile);
    JS_DefinePropertyWithTinyId(cx, ctor, CURRENTDIR_PROPERTY, 0, vp,
                JS_PropertyStub, file_currentDirSetter, JSPROP_ENUMERATE);

#ifdef JS_FILE_HAS_STANDARD_STREAMS
#	ifdef XP_MAC
#		error "Standard streams are not supported on the MAC, turn JS_FILE_HAS_STANDARD_STREAMS off"
#	else
        /* Code to create stdin, stdout, and stderr. Insert in the appropriate place. */
        /* Define input */
        afile = js_NewFileObjectFromFILE(cx, stdin, SPECIAL_FILE_STRING, JS_TRUE);
        if (!afile) return NULL;
        fileObj = JS_GetInstancePrivate(cx, afile, &file_class, NULL);
        fileObj->hasRandomAccess = JS_FALSE;
        vp = OBJECT_TO_JSVAL(afile);
        JS_SetProperty(cx, ctor, "input", &vp);

        /* Define output */
        afile = js_NewFileObjectFromFILE(cx, stdout, SPECIAL_FILE_STRING, JS_TRUE);
        if (!afile) return NULL;
        fileObj = JS_GetInstancePrivate(cx, afile, &file_class, NULL);
        fileObj->hasRandomAccess = JS_FALSE;
        vp = OBJECT_TO_JSVAL(afile);
        JS_SetProperty(cx, ctor, "output", &vp);

        /* Define error */
        afile = js_NewFileObjectFromFILE(cx, stderr, SPECIAL_FILE_STRING, JS_TRUE);
        if (!afile)
        return NULL;
        fileObj = JS_GetInstancePrivate(cx, afile, &file_class, NULL);
        fileObj->hasRandomAccess = JS_FALSE;
        vp = OBJECT_TO_JSVAL(afile);
        JS_SetProperty(cx, ctor, "error", &vp);
#	endif
#endif
}
#endif /* JS_HAS_FILE_OBJECT */
