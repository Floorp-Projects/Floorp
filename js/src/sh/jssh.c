/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

/* Copyright © 1998 Netscape Communications Corporation, All Rights Reserved.*/
/* JavaScript Shell:  jssh. Provides capabilities for calling commans,    */
/* loading libraries, etc. in JavaScript. For a full list of features, see
/* README.html */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "jsapi.h"                          /* The API itself */
#include "jsatom.h"
#ifdef HAS_FILE_SUPPORT                     /* File object */
#   include "jsfile.h"
#endif
#ifdef LIVECONNECT                          /* link w/LiveConnect DLL */
#   include "jsjava.h"
#endif
#ifdef PERLCONNECT                          /* link w/PerlConnect DLL */
#   include "jsperl.h"
#endif

#define STACK_CHUNK_SIZE 8192
/* Platform-dependent stuff */
#if defined(XP_WIN) || defined(XP_OS2)
#   define ENVIRON _environ
#   define PUTENV  _putenv
#   define POPEN   _popen
#   define PCLOSE  _pclose
#   define PLATFORM "PC"
#elif defined(XP_UNIX)
#   define ENVIRON environ
#   define PUTENV  putenv
#   define POPEN   popen
#   define PCLOSE  pclose
#   define PLATFORM "UNIX"
#elif defined(XP_BEOS)
#   define ENVIRON environ
#   define PUTENV  putenv
#   define POPEN   popen
#   define PCLOSE  pclose
#   define PLATFORM "BeOS"
#else
#   error "Platform not supported."
#endif


/* globals */
JSRuntime *rt;
JSContext *cx;
JSObject  *globalObject, *systemObject, *envObject;
/* forward-definitions */
static JSBool Process(JSContext*, JSObject*, char*, jsval *rval);
static JSBool environment_setProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

/* global class */
JSClass globalClass = {
    "Global", 0,
    JS_PropertyStub,  JS_PropertyStub,JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub,  JS_FinalizeStub
};

/* system class */
JSClass systemClass = {
    "System", 0,
    JS_PropertyStub,  JS_PropertyStub,JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub,  JS_FinalizeStub
};

/* environment class */
JSClass envClass = {
    "Environment", 0,
    JS_PropertyStub,  environment_setProperty, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub,  JS_FinalizeStub
};

/* object loaded via use() get a separate namespace of this type */
JSClass libraryClass = {
    "Library", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub,  JS_FinalizeStub
};

/****************************** global methods ******************************/
/* load JS files in argv and execute them */
static JSBool
call(JSContext *cx, JSObject *obj, int argc, jsval *argv, jsval* rval){
    int i;
    char *ch;
    JSBool result = JS_TRUE;
    FILE *file;
    static char* processFile(FILE*);

    for(i=0;i<argc;i++){
        char* source;
        JSObject *anonymous;

        source = JS_GetStringBytes(JS_ValueToString(cx, argv[i]));
        file = fopen(source, "r");
        if(!file){
            JS_ReportError(cx, "Can't open %s", source);
            continue;
        }
        /* read file's contents */
        ch = processFile(file);
        /* define anonyous namespace */
        anonymous = JS_DefineObject(cx, globalObject, "__anonymous__",
            &globalClass, globalObject, JSPROP_ENUMERATE);
        /* evaluate script in that namespace. result gets accumulated */
        result = JS_EvaluateScript(cx, obj, ch, strlen(ch), "jssh", 0, rval) && result;
        free(ch);
        fclose(file);
        /* remove namespace */
        JS_DeleteProperty(cx, globalObject, "__anonymous__");
    }
    return JS_TRUE;
}

/* Foo.eval() evaluates the content of a script preloaded with use('Foo') */
static JSBool
evalScript(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval){
    void *p = JS_GetInstancePrivate(cx, obj, &libraryClass, NULL);
    JSScript *script;

    if(!p)
        return JS_FALSE;

    script = (JSScript*)p;
    return JS_ExecuteScript(cx, obj, script, rval);
}

/*
    preload files specified in argv. for each create a namespace and
    make functions available within that namespace
*/
static JSBool
use(JSContext *cx, JSObject *obj, int argc, jsval *argv, jsval* rval){
    int i;
    for(i=0;i<argc;i++){
        char source[256], *ch;
        FILE *file;
        JSObject *scriptObject;
        JSScript *script;
        JSIdArray *ida;
        jsval v;

        strcpy(source, JS_GetStringBytes(JS_ValueToString(cx, argv[i])));
        /* add .js extention if necessary */
        if(!strstr(source, ".js")){
            strcat(source, ".js");
        }
        file = fopen(source, "r");
        if(!file){
            JS_ReportError(cx, "Can't open %s", source);
            return JS_FALSE;
        }
        /* remove '.js' extention */
        *(source + strlen(source)-3) = '\0';
        ch = processFile(file);
        scriptObject = JS_DefineObject(cx, globalObject, source, &libraryClass,
            NULL, JSPROP_ENUMERATE);
        if(!scriptObject){
            JS_ReportError(cx, "Can't create script");
            return JS_FALSE;
        }
        script = JS_CompileScript(cx, scriptObject, ch, strlen(ch), source, 0);
        fclose(file);
        if(!script){
            JS_ReportError(cx, "Can't compile script");
            return JS_FALSE;
        }

        ida = JS_Enumerate(cx, scriptObject);
        if(!ida){
            JS_ReportError(cx, "Can't enumerate");
            return JS_FALSE;
        }

        /* go through the properties and add them to the namespace we created */
        for (i = 0; i < ida->length; i++) {
            jschar *prop;
            JSString *str;

            JS_IdToValue(cx, ida->vector[i], &v);
            str = JS_ValueToString(cx, v);
            prop = JS_GetStringChars(str);
            JS_GetUCProperty(cx, scriptObject, prop, JS_GetStringLength(str), &v);
            /* add v if it's a function */
            if(JS_TypeOfValue(cx, v) == JSTYPE_FUNCTION){
                char const *name = JS_GetFunctionName(JS_ValueToFunction(cx, v));
                JS_DefineProperty(cx, scriptObject, name, v, NULL, NULL,
                    JSPROP_READONLY|JSPROP_PERMANENT);
            }
        }

        /* save script as private data for the object */
        if(!JS_SetPrivate(cx, scriptObject, (void*)script)){
            return JS_FALSE;
        }
        JS_DefineFunction(cx, scriptObject, "eval", evalScript, 0, 0);
    }
    return JS_TRUE;
}

/********************** Define system methods **************************/
/* a replacement for System.out.write */
static JSBool
system_print(JSContext *cx, JSObject *obj, int argc, jsval *argv, jsval* rval){
    int i;
    JSString *str;

    for(i=0;i<argc;i++){
        str = JS_ValueToString(cx, argv[i]);
        if(!str) return JS_FALSE;
        printf("%s%s", i ? " ":"", JS_GetStringBytes(str));
    }

    return JS_TRUE;
}

/* run external commands */
static JSBool
system_system(JSContext *cx, JSObject *obj, int argc, jsval *argv, jsval* rval){
    JSString *str;
    int i;

    for(i=0;i<argc;i++){
        str = JS_ValueToString(cx, argv[i]);
        /* TODO: error processing here */
        system(JS_GetStringBytes(str));
    }

    return JS_TRUE;
}

/* invoke the gc */
static JSBool
system_gc(JSContext *cx, JSObject *obj, int argc, jsval *argv, jsval* rval){
    JS_GC(cx);
    return JS_TRUE;
}

/*
    Runs commands supplied as the first parameter and outputs a special file
    you can read info from.
*/
static JSBool
system_pipe(JSContext *cx, JSObject *obj, int argc, jsval *argv, jsval* rval){
    JSObject *file;
    char     *cmd;
    FILE     *pipe;                     /* pipe we open */

    if(argc!=1) return JS_FALSE;        /* need one parameter -- command */

    cmd = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));

    /* open the pipe */
    pipe = POPEN(cmd, "rt");

    if(!pipe) {
        JS_ReportError(cx, "Attempt to read from a pipe failed, can't run command %s", cmd);
        return JS_FALSE;
    }

    file = js_NewFileObjectFromFILE(cx, pipe, cmd, JS_FALSE);
    *rval = OBJECT_TO_JSVAL(file);

    //PCLOSE(pipe);

    return JS_TRUE;
}

/*
    Runs command supplied as the first parameter and creates a special
    file you can write to
*/
static JSBool
system_script(JSContext *cx, JSObject *obj, int argc, jsval *argv, jsval* rval){
    FILE     *pipe;                     /* pipe we open */
    char     *cmd;
    JSObject *file;

    /* basic parameter checking */
    if(argc!=1){
        JS_ReportError(cx, "Expected one argument, i.e. file = script('ls')");
        return JS_FALSE;
    }

    cmd = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
    /* create pipe */
    pipe = POPEN(cmd, "wt");

    if(!pipe){
        JS_ReportError(cx, "Attempt to read from a pipe failed, can't run command %s", cmd);
        return JS_FALSE;
    }

    file = js_NewFileObjectFromFILE(cx, pipe, cmd, JS_FALSE);
    *rval = OBJECT_TO_JSVAL(file);
    //PCLOSE(pipe);
    return JS_TRUE;
}

/* get/set version of the language */
static JSBool
system_version(JSContext *cx, JSObject *obj, int argc, jsval *argv, jsval* rval){
    if(argc){
        JS_SetVersion(cx, JSVAL_TO_INT(argv[0]));
        *rval = INT_TO_JSVAL(argv[0]);
    }else
        *rval = INT_TO_JSVAL(JS_GetVersion(cx));
    return JS_TRUE;
}

/* exit the interpreter. takes a return code */
static JSBool
system_exit(JSContext *cx, JSObject *obj, int argc, jsval *argv, jsval* rval){
    int32 ip;
    static void finilizeShell();

    finilizeShell();

    if(!argc)
        exit(0);
    else{
        exit(JS_ValueToInt32(cx, argv[0], &ip));
    }
    return JS_TRUE;
}

/* setter for Environment. updates the environment itself */
static JSBool
environment_setProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp){
    char envString[256];

    printf("trying to set %s = %s\n", JS_GetStringBytes(JS_ValueToString(cx, id)),
        JS_GetStringBytes(JS_ValueToString(cx, *vp)));
    strcpy(envString, JS_GetStringBytes(JS_ValueToString(cx, id)));
    strcat(envString, "=");
    /* This is not preserved after the script terminates */
    strcat(envString, JS_GetStringBytes(JS_ValueToString(cx, *vp)));
    printf("%s\n", envString);
    PUTENV(envString);
    return JS_TRUE;
}

/* custom error reporter */
static void
errorReporter(JSContext *cx, const char *message, JSErrorReport *report){
    if(report == NULL)
        fprintf(stderr, "jssh: %s\n", message);
    else
        fprintf(stderr, "%s: line %i: error: %s\n",
            report->filename, report->lineno, message);
}

/* add properties to Env */
static void
readEnvironment(JSContext *cx, JSObject *env){
    char **currEnvString;
    char currEnvName [256];
    char currEnvValue[1024];            /* this must be big enough */
    JSString *str;
    JSBool ok;

    for(currEnvString = ENVIRON; *currEnvString!=NULL; currEnvString++){
        char *equal = strchr(*currEnvString, '=');
        strncpy(currEnvName, *currEnvString, equal-*currEnvString);
        currEnvName[equal-*currEnvString] = 0;      /* append null char at the end */
        equal++;
        strncpy(currEnvValue, equal, *currEnvString+strlen(*currEnvString)-equal+1);
        str=JS_NewStringCopyZ(cx, equal/*, *currEnvString+strlen(*currEnvString)-equal*/);
        if(!str) {
            JS_ReportError(cx, "Error in readEnvironment, str==NULL");
            return;
        }
        /* add a new property to env object */
        ok = JS_DefineProperty(cx, env, currEnvName,
            STRING_TO_JSVAL(str), JS_PropertyStub, environment_setProperty,
            JSPROP_ENUMERATE|JSPROP_PERMANENT);
        if(!ok) {
            JS_ReportError(cx, "Error in readEnvironment, can't define a property");
            return;
        }
    }
}

/* formatted output of the environment */
static JSBool
env_toString(JSContext *cx, JSObject *obj, int argc, jsval *argv, jsval* rval){
    char **currEnvString;
    char currEnvName [256];
    char currEnvValue[1024];            /* this must be big enough */

    char *result;
    int  len=0;

    result = JS_malloc(cx, 1);
    strcpy(result, "");

    for(currEnvString = ENVIRON; *currEnvString!=NULL; currEnvString++){
        char *equal = strchr(*currEnvString, '='), *p;
        char line[1024];

        strncpy(currEnvName, *currEnvString, equal-*currEnvString);
        currEnvName[equal-*currEnvString] = 0;      /* append null char at the end */
        equal++;
        strncpy(currEnvValue, equal, *currEnvString+strlen(*currEnvString)-equal+1);
        sprintf(line, "%25s = %s\n", currEnvName, currEnvValue);
        len+=strlen(line)+1;
        p = (char*)JS_realloc(cx, result, len*sizeof(char));
        if(!p) {
            JS_ReportOutOfMemory(cx);
            return JS_FALSE;
        }
        strcpy(p, result);
        result=p;
        strcat(result, line);
    }

    //JS_free(cx, result);

    *rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, result));
    return JS_TRUE;
}

/*************************************************************************/
/* methods available */
JSFunctionSpec globalMethods[] = {
    {"call", call,  0, 0},
    {"use",  use,   0, 0},
    { NULL,  NULL,  0, 0}
};

JSFunctionSpec systemMethods[] = {
    {"print",   system_print,   0, 0},
    {"version", system_version, 0, 0},
    {"system",  system_system,  0, 0},
    {"gc",      system_gc,      0, 0},
    {"pipe",    system_pipe,    0, 0},
    {"script",  system_script,  0, 0},
    //{"help",  system_help,    0, 0},
    {"exit",    system_exit,    0, 0},
    { NULL,     NULL,           0, 0}
};

JSFunctionSpec envMethods[] = {
    {"toString",  env_toString, 0},
    { NULL, NULL,0 }
};

/*
    Read a file. Return it as a char pointer that
    should be deallocated. Ignore #-comments.
*/
static char*
processFile(FILE *file){
    char *buf, *bufEnd, *bufUpto, ch;
    int bufLen, len;

    /* read the file into memory */
    bufLen  = 4096;
    buf     = malloc(bufLen);                       /* allocate character buffer */
    bufEnd  = buf+bufLen;                           /* end of the buffer */
    bufUpto = buf;                                  /* current buffer pointer */

    ch=fgetc(file);                                 /* ignore first line starting with # */
    if(ch=='#')
        while((ch=fgetc(file))!=EOF) { if(ch=='\n') break;}
    else
        ungetc(ch,file);

    while((len=fread(bufUpto, sizeof(char), bufEnd-bufUpto, file))>0){
        bufUpto+=len;
        if(bufUpto>=bufEnd) {                       /* expand the buffer if needed */
            bufLen+=bufLen;
            bufEnd=buf+bufLen;
            buf=realloc(buf,bufLen);
        }
    }
    *bufUpto = '\0';
    //puts(buf);
    return buf;
}

/*
    Read JavaScript source from file or stdin if !filename
    Evaluate it.
*/
static JSBool
Process(JSContext *cx, JSObject *obj, char *filename, jsval *rval){
    FILE *file;
    char *ch;

    /* Process the input */
    if(filename){
        file = fopen(filename, "r");
        if(file==NULL){
            JS_ReportError(cx, "Can't open %s\n", filename);
            return JS_FALSE;
        }

        ch = processFile(file);
        JS_EvaluateScript(cx, obj, ch, strlen(ch), "jssh", 0, rval);
        free(ch);
        fclose(file);
    }else{
        /* process input line-by-line */
        char line[256];
        int  currLine = 0;
        JSString *str;
        do{
            printf("js> ");
            gets(line);
            *rval = JSVAL_VOID;
            JS_EvaluateScript(cx, globalObject, line, strlen(line), "jss", ++currLine, rval);
            if(!JSVAL_IS_VOID(*rval)){
                str=JS_ValueToString(cx, *rval);
                if(!str){
                    JS_ReportError(cx, "Can't convert to string");
                }
                printf("%s\n", JS_GetStringBytes(str));
            }
        }while(1);                                      /* need to type 'exit' to exit */
    }
}

static int
usage(){
    fprintf(stderr, "%s", JS_GetImplementationVersion());
    fputs("usage: js [ [scriptfile] [scriptarg...] | - ]", stderr);
    return 2;
}

/*
    This is taken from JSShell.
*/
static int
ProcessArgs(JSContext *cx, JSObject *obj, char **argv, int argc){
    int i;
    char *filename;
    jsval *vector;
    jsval *p;
    JSObject *argsObj;
    jsval rval;
    JSBool ok;

    /* prompt */
    if(argc<2 || !strcmp(argv[1], "-"))
        filename = NULL;
    else if(argc==2)
        filename = argv[1];
    else
        return usage();

    vector = JS_malloc(cx, (argc-1)* sizeof(jsval));
    p = vector;
    if(!p){
        JS_ReportError(cx, "Error in ProcessArgs, p==NULL!");
    }

    for(i=1;i<argc;i++){
        JSString *str = JS_NewStringCopyZ(cx, argv[i]);
        if (str == NULL) return 1;
        *p++ = STRING_TO_JSVAL(str);
    }

    argsObj = JS_NewArrayObject(cx, argc-1, vector);
    if(!argsObj){
        JS_ReportError(cx, "Error in ProcessArgs, argsObj==NULL!");
    }

    ok = JS_DefineProperty(cx, systemObject, "ARGV",
                           OBJECT_TO_JSVAL(argsObj), NULL, NULL,
                           JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT);
    if(!ok){
        JS_ReportError(cx, "Can't define property ARGV");
    }

    JS_free(cx, vector);

    Process(cx, obj, filename, &rval);

    return 0;
}

/* perform basic initialization */
static void
initShell(){
    /* Initialization */
    rt = JS_Init(1000000L);                     /* runtime */

    cx = JS_NewContext(rt, STACK_CHUNK_SIZE);   /* context */

    JS_SetErrorReporter(cx, errorReporter);     /* error reporter */

    globalObject =                               /* global object */
        JS_NewObject(cx, &globalClass, NULL, NULL);

    JS_InitStandardClasses(cx, globalObject);   /* basic classes */

#ifdef HAS_FILE_SUPPORT
    if(!js_InitFileClass(cx, globalObject)){    /* File */
        JS_ReportError(cx, "Failed to initialize File support");
        return;
    }
#endif

#ifdef LIVECONNECT
    if(!JSJ_SimpleInit(cx, globalObject, NULL, getenv("CLASSPATH"))){
            JS_ReportError(cx, "Failed to initialize LiveConnect");
            return;
    }
#endif
}

/* destroy everything */
static void
finilizeShell(){
#ifdef LIVECONNECT
    JSJ_SimpleShutdown();
#endif
    JS_GC(cx);
    JS_DestroyContext(cx);
    JS_Finish(rt);
}

/* standard shell objects and properties */
static void
initShellObjects(){
    JSObject *stream;
    jsval    v;
    extern JSObject* JS_InitPerlClass(JSContext*, JSObject*);

    /* predefine System and its properties */
    JS_DefineFunctions(cx, globalObject, globalMethods);
    systemObject =                               /* System */
        JS_DefineObject(cx, globalObject, "System", &systemClass, NULL, JSPROP_ENUMERATE);
    JS_DefineFunctions(cx, systemObject, systemMethods);
    JS_DefineProperty(cx, systemObject, "platform",
        STRING_TO_JSVAL(JS_NewStringCopyZ(cx, PLATFORM)), NULL, NULL,
        JSPROP_ENUMERATE|JSPROP_PERMANENT|JSPROP_READONLY);
    /* define standard stream */
    stream = js_NewFileObjectFromFILE(cx, stdout, "Special file System.out", JS_TRUE);
    if (!stream){
        JS_ReportError(cx, "Can't create System.out");
        exit(1);
    }
    v=OBJECT_TO_JSVAL(stream);
    JS_SetProperty(cx, systemObject, "stdout", &v);

    stream = js_NewFileObjectFromFILE(cx, stderr, "Special file System.err", JS_TRUE);
    if (!stream){
        JS_ReportError(cx, "Can't create System.err");
        exit(1);
    }
    v=OBJECT_TO_JSVAL(stream);
    JS_SetProperty(cx, systemObject, "stderr", &v);

    stream = js_NewFileObjectFromFILE(cx, stdin, "Special file System.in", JS_TRUE);
    if (!stream){
        JS_ReportError(cx, "Can't create System.in");
        exit(1);
    }
    v=OBJECT_TO_JSVAL(stream);
    JS_SetProperty(cx, systemObject, "stdin", &v);

    /* define Environment and populate it */
    envObject =                                  /* Environment */
        JS_DefineObject(cx, globalObject, "Environment", &envClass, NULL, JSPROP_ENUMERATE);
    JS_DefineFunctions(cx, envObject, envMethods);
    readEnvironment(cx, envObject);                     /* initialize Environment's properties */
#ifdef PERLCONNECT
    JS_InitPerlClass(cx, globalObject);
#endif
}

int
main(int argc, char **argv){
    int result;

    initShell();
    initShellObjects();
    result = ProcessArgs(cx, globalObject, argv, argc);
    finilizeShell();
    return result;
}
