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

/* jband - 11/08/97 -  */

#include "headers.h"

static JSContext* _cx;
static JSObject*  _glob;

#ifdef JSDEBUGGER
static JSDContext*    _jsdc   = NULL;
static JSDSourceText* _jsdsrc = NULL;
static JSDJContext*   _jsdjc  = NULL;
#endif

/********************************/

static char*
_readFile(const char* filename)
{
    int fh;
    int len;
    char* buf;


    fh = _open(filename, _O_RDONLY | _O_BINARY);
    if(-1 ==fh)
        return NULL;
    len = _lseek(fh, 0, SEEK_END);
    if(-1 == len)
    {
        _close(fh);
        return NULL;
    }
    _lseek(fh, 0, SEEK_SET);
    buf = malloc(len+1);
    if(! buf)
    {
        _close(fh);
        return NULL;
    }
    _read(fh, buf, len);
    _close(fh);
    buf[len] = 0;
    return buf;
}    

/********************************/

static void
ignore_ErrorReporter(JSContext *cx, const char *message, JSErrorReport *report)
{
}    

static void
my_ErrorReporter(JSContext *cx, const char *message, JSErrorReport *report)
{
    char buf[1024];
    char bigBuf[2048];

    bigBuf[0] = 0;

    if(report && report->filename)
    {
        sprintf( buf, "FILE: %s\n", report->filename );
        strcat( bigBuf, buf );
    }
    if(report && report->lineno)
    {
        sprintf( buf, "LINE: %d\n", report->lineno );
        strcat( bigBuf, buf );
    }
    strcat( bigBuf, message );
    strcat( bigBuf, "\n\n" );

    if(report && report->linebuf)
        strcat( bigBuf, report->linebuf );

    MessageBox( g_hMainDlg, bigBuf, "JavaScript Error", MB_ICONHAND );
}

/********************************/

static JSBool
_MoveTo(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    int x;
    int y;

    if( argc != 2 ||
        ! JS_ValueToInt32(cx, argv[0], &x) ||
        ! JS_ValueToInt32(cx, argv[1], &y) )
    {
        JS_ReportError(cx, "MoveTo requires 2 numerical args");
        return JS_FALSE;
    }

    DrawMoveTo(x, y);
    return JS_TRUE;
}

static JSBool
_LineTo(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    int x;
    int y;

    if( argc != 2 ||
        ! JS_ValueToInt32(cx, argv[0], &x) ||
        ! JS_ValueToInt32(cx, argv[1], &y) )
    {
        JS_ReportError(cx, "LineTo requires 2 numerical args");
        return JS_FALSE;
    }

    DrawLineTo(x, y);
    return JS_TRUE;
}

static JSBool
_Load(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    char* source;
    const char* filename;
    JSString* s;

    if( argc != 1 )
    {
        JS_ReportError(cx, "requires 1 arg");
        return JS_FALSE;
    }

    if( ! (s = JS_ValueToString(cx, argv[0])) )
    {
        JS_ReportError(cx, "param must be a string");
        return JS_FALSE;
    }
    filename = JS_GetStringBytes(s);

    source = _readFile(filename);
    if(! source)
    {
        JS_ReportError(cx, "failed to load source");
        return JS_FALSE;
    }

#ifdef JSDEBUGGER
    if(_jsdc)
        JSD_AddFullSourceText(_jsdc, source, strlen(source), filename);
#endif
    JS_EvaluateScript(cx, _glob, source, strlen(source), filename, 1, rval);
    free(source);
    return JS_TRUE;
}

static JSBool
_Save(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    const char* filename;
    JSString* s;

    if( DoingRedraw() )
        return JS_TRUE;

    if( argc != 1 )
    {
        JS_ReportError(cx, "requires 1 arg");
        return JS_FALSE;
    }

    if( ! (s = JS_ValueToString(cx, argv[0])) )
    {
        JS_ReportError(cx, "param must be a string");
        return JS_FALSE;
    }
    filename = JS_GetStringBytes(s);

    if( ! SaveRecordedHistory(filename) )
    {
        JS_ReportError(cx, "failed to save recorded history");
        return JS_FALSE;
    }

    return JS_TRUE;
}

static JSBool
_StartRecord(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    /* just the statement in the history is enough to get the action */
    return JS_TRUE;
}

static JSBool
_SetPenWidth(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    jsdouble width;

    if( argc != 1 )
    {
        JS_ReportError(cx, "requires 1 arg");
        return JS_FALSE;
    }

    if( ! JS_ValueToNumber(cx, argv[0], &width) ||
        width < 1 || width > 100 )
    {
        JS_ReportError(cx, "width must be set to a value between 1 and 100");
        return JS_FALSE;
    }
    SetPenWidth( (int) width );
}

typedef struct 
{
    char* name;
    COLORREF color;
    
} COLOR_DEF;    
static COLOR_DEF colors[] = 
{
    {"white", RGB(255,255,255)},
    {"black", RGB(0,0,0)},
    {"red"  , RGB(255,0,0)},
    {"green", RGB(0,255,0)},
    {"blue" , RGB(0,0,255)},
    {NULL, 0}
};

static JSBool
_SetPenColor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    jsdouble dblColor;
    COLORREF color;
    int i;

    if( argc != 1 )
    {
        JS_ReportError(cx, "requires 1 arg");
        return JS_FALSE;
    }

    if( JSVAL_IS_NUMBER(argv[0]) && JS_ValueToNumber(cx, argv[0], &dblColor) )
    {
        color = (COLORREF) dblColor;
        if( color < RGB(0,0,0) || color > RGB(255,255,255) )
        {
            JS_ReportError(cx, "color value is out of range");
            return JS_FALSE;
        }
    }
    else if( JSVAL_IS_STRING(argv[0]) )
    {
        JSString* s = JS_ValueToString(cx, argv[0]);
        const char* colorname;
        if(!s)
        {
            JS_ReportError(cx, "param must be a color string or value");
            return JS_FALSE;
        }
        colorname = JS_GetStringBytes(s);

        for( i = 0; colors[i].name; i++ )
        {
            if( 0 == strcmp(colors[i].name, colorname) )
            {
                color = colors[i].color;
                break;
            }
        }
        if( ! colors[i].name )
        {
            JS_ReportError(cx, "unknown color");
            return JS_FALSE;
        }
    }
    SetPenColor( color );
    return JS_TRUE;
}

/********************************/

static JSFunctionSpec my_functions[] = {
    {"MoveTo",         _MoveTo,        2},
    {"LineTo",         _LineTo,        2},
    {"load",           _Load,          1},
    {"save",           _Save,          1},
    {"start_record",   _StartRecord,   0},
    {"setPenWidth",    _SetPenWidth,   1},
    {"setPenColor",    _SetPenColor,   1},
    {0}
};

static JSClass global_class = {
    "global", 0,
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   JS_FinalizeStub
};

BOOL Eval(char* str, int lineno)
{
    jsval rval;
#ifdef JSDEBUGGER
    if(_jsdc)
    {
        if(1 == lineno)
            _jsdsrc = JSD_NewSourceText(_jsdc, "drawer");
        if(_jsdsrc)
            _jsdsrc = JSD_AppendSourceText(_jsdc, _jsdsrc, str, strlen(str), 
                                           JSD_SOURCE_PARTIAL);
    }
#endif
    return JS_EvaluateScript(_cx, _glob, str, strlen(str), "drawer", lineno, &rval);
}    

static char autoload[] = "load(\"autoload.js\")";


static void 
_startStopCallback(JSDJContext* jsdjc, int event, void *closure)
{
    *((int*)closure) = event;
}    

int WINAPI 
WinMain( HINSTANCE hInstance,
         HINSTANCE hPrevInstance,
         LPSTR lpCmdLine,
         int nCmdShow )
{
    JSRuntime* rt;
    jsval rval;
#ifdef JSDEBUGGER
    int startupFlag;
    int timeoutCounter;
#endif

    rt = JS_Init(8L * 1024L * 1024L);
    if (!rt)
        return 1;

    _cx = JS_NewContext(rt, 8192);
    if (!_cx)
        return 1;

    _glob = JS_NewObject(_cx, &global_class, NULL, NULL);
    if (!_glob)
        return 1;

    if (!JS_InitStandardClasses(_cx, _glob))
        return 1;

    JS_SetErrorReporter(_cx, my_ErrorReporter);

    if (!JS_DefineFunctions(_cx, _glob, my_functions))
        return 1;

#ifdef JSDEBUGGER
    if( strstr(lpCmdLine, "DEBUG") )
    {
        _jsdc = JSD_DebuggerOnForUser(rt, NULL, NULL);
        if(!_jsdc)
            return 1;
        _jsdjc = JSDJ_CreateContext();
        if( ! _jsdjc )
            return 1;
        JSDJ_SetJSDContext(_jsdjc, _jsdc);
        
        JSDJ_SetStartStopCallback(_jsdjc, _startStopCallback, &startupFlag);
        JSD_JSContextInUse(_jsdc, _cx);
        JSDJ_CreateJavaVMAndStartDebugger(_jsdjc);
        
        // timout debugger startup after 30 seconds
        startupFlag = 0;
        for(timeoutCounter = 0; timeoutCounter < 300; timeoutCounter++)
        {
            if(startupFlag)
                break;
            Sleep(100);
        }
        
        /**
        * XXX Hack here because current DebugController as no iterateScripts
        * method and thus the debugger can only see the scripts created
        * after *it* has set it's hooks. The debugger has code to iterate
        * scripts and thus learn about scripts which were in place before
        * the debugger has set its hooks. But, the underlying code does not
        * yet support it.
        * So... we wait for an arbitrary amount of time here to let the
        * debugger get its hooks in before we load any scripts
        */
        Sleep(2000);

        JSDJ_SetStartStopCallback(_jsdjc, NULL, NULL);
    }
#endif

    g_hInstance = hInstance;

    JS_SetErrorReporter(_cx, ignore_ErrorReporter);
    JS_EvaluateScript(_cx, _glob, autoload, strlen(autoload), "autoload", 1, &rval);
    JS_SetErrorReporter(_cx, my_ErrorReporter);

    DialogBox(hInstance, MAKEINTRESOURCE(IDDIALOG_MAIN), NULL, MainDlgProc );

#ifdef JSDEBUGGER
    if(_jsdc)
        JSD_DebuggerOff(_jsdc);
#endif

    JS_DestroyContext(_cx);
    JS_Finish(rt);
    return 0;
} 
