/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * lm_bkgrd.c
 * 
 * Reflects ContinuousUpdate (or background download) methods
 * into JavaScript object.
 *
 */

#include "nspr.h"
#include "plstr.h"
#include "lm.h"
#include "autoupdt.h"
#include "prtypes.h"


typedef struct JSBackground {
    char *id;
    char *url;
    AutoUpdateConnnection bg_connect;
} JSBackground;

enum background_enum {
    BACKGROUND_STATE		= -1,
    BACKGROUND_ID			= -2,
    BACKGROUND_URL			= -3,
    BACKGROUND_FILE_SIZE	= -4,
    BACKGROUND_CUR_SIZE		= -5,
    BACKGROUND_DEST_FILE	= -6,
    BACKGROUND_BYTES_RANGE	= -7,
    BACKGROUND_INTERVAL		= -8,
    BACKGROUND_ETA			= -9,
    BACKGROUND_ERROR_MESSAGE = -10
};

static JSPropertySpec background_props[] = {
    {"state",		BACKGROUND_STATE,		JSPROP_ENUMERATE|JSPROP_READONLY},
    {"id",			BACKGROUND_ID,			JSPROP_ENUMERATE|JSPROP_READONLY},
    {"url",			BACKGROUND_URL,			JSPROP_ENUMERATE|JSPROP_READONLY},
    {"file_size",	BACKGROUND_FILE_SIZE,	JSPROP_ENUMERATE|JSPROP_READONLY},
    {"cur_size",	BACKGROUND_CUR_SIZE,	JSPROP_ENUMERATE|JSPROP_READONLY},
    {"dest_file",	BACKGROUND_DEST_FILE,	JSPROP_ENUMERATE|JSPROP_READONLY},
    {"bytes_range",	BACKGROUND_BYTES_RANGE,	JSPROP_ENUMERATE|JSPROP_READONLY},
    {"interval",	BACKGROUND_INTERVAL,	JSPROP_ENUMERATE|JSPROP_READONLY},
    {"eta",			BACKGROUND_ETA,			JSPROP_ENUMERATE|JSPROP_READONLY},
    {"error",		BACKGROUND_ERROR_MESSAGE,	JSPROP_ENUMERATE|JSPROP_READONLY},
    {0}
};

static JSConstDoubleSpec background_constants[] = {
    {BACKGROUND_NEW             	, "NEW"},
    {BACKGROUND_ERROR         		, "ERROR"},
    {BACKGROUND_COMPLETE        	, "COMPLETE"},
    {BACKGROUND_SUSPEND         	, "SUSPEND"},
    {BACKGROUND_DOWN_LOADING		, "DOWN_LOADING"},
    {BACKGROUND_DOWN_LOADING_NOW	, "DOWN_LOADING_ALL_BYTES_NOW"},
    {BACKGROUND_DONE				, "DONE"},
    {0, 0}
};


extern JSClass lm_background_class;

PR_STATIC_CALLBACK(JSBool)
background_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

PR_STATIC_CALLBACK(void)
background_free(JSBackground *bg);

PR_STATIC_CALLBACK(void)
background_finalize(JSContext *cx, JSObject *obj);

PR_STATIC_CALLBACK(JSBool)
background_stop(JSContext *cx, JSObject *obj, 
                uintN argc, jsval *argv, jsval *rval);

PR_STATIC_CALLBACK(JSBool)
background_start(JSContext *cx, JSObject *obj, 
                uintN argc, jsval *argv, jsval *rval);

PR_STATIC_CALLBACK(JSBool)
background_done(JSContext *cx, JSObject *obj, 
                uintN argc, jsval *argv, jsval *rval);

PR_STATIC_CALLBACK(JSBool)
background_downloadNow(JSContext *cx, JSObject *obj, 
                       uintN argc, jsval *argv, jsval *rval);

PR_STATIC_CALLBACK(JSBool)
background_abort(JSContext *cx, JSObject *obj, 
                 uintN argc, jsval *argv, jsval *rval);

PR_STATIC_CALLBACK(JSBool)
background_toString(JSContext *cx, JSObject *obj,
                    uintN argc, jsval *argv, jsval *rval);

static JSBool
background_constructor(JSContext *cx, JSObject *obj, 
                       char *id, char *url, int32 file_size,
                       char* script);

PR_STATIC_CALLBACK(JSBool)
Background(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval);

static JSObject *
lm_NewBackgroundObject(JSContext* cx, char* id, char* url, int32 file_size, char* script);

static JSObject *
lm_GetCurrentBackgroundObject(JSContext* cx, char* id);


PR_STATIC_CALLBACK(JSBool)
background_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSBackground *bg;
    JSString *str;
    jsint slot;
    AutoUpdateConnnection bg_connect;

    if (!JSVAL_IS_INT(id))
	    return JS_TRUE;

    slot = JSVAL_TO_INT(id);

    bg = JS_GetInstancePrivate(cx, obj, &lm_background_class, NULL);
    if (!bg)
	    return JS_TRUE;

    /*
	if (!lm_CanAccessTarget(cx, JSTARGET_UNIVERSAL_BROWSER_READ))
	    return JS_FALSE;
        */

    bg_connect = bg->bg_connect;
    if (!bg_connect)
	    return JS_TRUE;

    str = JSVAL_TO_STRING(JS_GetEmptyStringValue(cx));

    switch (slot) {
      case BACKGROUND_STATE:
        {
            BackgroundState state = AutoUpdate_GetState(bg_connect);
            *vp = INT_TO_JSVAL(state);
        }
        return JS_TRUE;
          

      case BACKGROUND_ID:
        if (bg->id) {
            str = JS_NewStringCopyZ(cx, bg->id);
        }
        break;

      case BACKGROUND_URL:
        if (bg->url) {
            str = JS_NewStringCopyZ(cx, bg->url);
        }
        break;

      case BACKGROUND_FILE_SIZE:
        {
            int32 file_size = AutoUpdate_GetFileSize(bg_connect);
            *vp = INT_TO_JSVAL(file_size);
        }
	    return JS_TRUE;

      case BACKGROUND_CUR_SIZE:
        {
            int32 downloaded = AutoUpdate_GetCurrentFileSize(bg_connect);
            *vp = INT_TO_JSVAL(downloaded);
        }
	    return JS_TRUE;

      case BACKGROUND_DEST_FILE:
        {
            const char* dest_file = AutoUpdate_GetDestFile(bg_connect);
            if (dest_file)
                str = JS_NewStringCopyZ(cx, dest_file);
        }
        break;

      case BACKGROUND_BYTES_RANGE:
        {
            int32 range = AutoUpdate_GetBytesRange(bg_connect);
            *vp = INT_TO_JSVAL(range);
        }
	    return JS_TRUE;

      case BACKGROUND_INTERVAL:
        {
            uint32 interval = AutoUpdate_GetInterval(bg_connect);
            *vp = INT_TO_JSVAL(interval);
        }
	    return JS_TRUE;

      case BACKGROUND_ETA:
        {
            int32 cur_size = AutoUpdate_GetCurrentFileSize(bg_connect);
            int32 file_size = AutoUpdate_GetFileSize(bg_connect);
            int32 range = AutoUpdate_GetBytesRange(bg_connect);
            int32 interval = AutoUpdate_GetInterval(bg_connect);
            int32 remaining_bytes = file_size - cur_size;
            int32 eta = (interval/range) * remaining_bytes;
            *vp = INT_TO_JSVAL(eta);
        }
	    return JS_TRUE;

      case BACKGROUND_ERROR_MESSAGE:
        {
            const char* msg = AutoUpdate_GetErrorMessage(bg_connect);
            if (msg)
                str = JS_NewStringCopyZ(cx, msg);
        }
        break;

      default:
        if (slot < 0) {
            /* Don't mess with user-defined or method properties. */
            return JS_TRUE;
        }
        break;
	}

    /* Common tail code for string-type properties. */
    if (!str)
      return JS_FALSE;
    *vp = STRING_TO_JSVAL(str);
    return JS_TRUE;
}

PR_STATIC_CALLBACK(void)
background_free(JSBackground *bg)
{
    if (!bg)
        return;

    /* Autoupdate_Done(bg->bg_connect); */
    bg->bg_connect = NULL;
    PR_FREEIF(bg->id);
    bg->id = NULL;
    PR_FREEIF(bg->url);
    bg->url = NULL;
}

PR_STATIC_CALLBACK(void)
background_finalize(JSContext *cx, JSObject *obj)
{
    JSBackground *bg;

    bg = JS_GetPrivate(cx, obj);
    if (!bg)
        return;
    background_free(bg);

    JS_free(cx, bg);
}

PR_STATIC_CALLBACK(JSBool)
background_stop(JSContext *cx, JSObject *obj, 
                uintN argc, jsval *argv, jsval *rval)
{
    JSBackground *bg;

    bg = JS_GetInstancePrivate(cx, obj, &lm_background_class, argv);
    if (!bg)
	    return JS_FALSE;

    /*
	if (!lm_CanAccessTarget(cx, JSTARGET_UNIVERSAL_BROWSER_READ))
	    return JS_FALSE;
        */

    Autoupdate_Suspend(bg->bg_connect);

    return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
background_start(JSContext *cx, JSObject *obj, 
                uintN argc, jsval *argv, jsval *rval)
{
    JSBackground *bg;

    bg = JS_GetInstancePrivate(cx, obj, &lm_background_class, argv);
    if (!bg)
	    return JS_FALSE;

    /*
	if (!lm_CanAccessTarget(cx, JSTARGET_UNIVERSAL_BROWSER_READ))
	    return JS_FALSE;
        */

    Autoupdate_Resume(bg->bg_connect);

    return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
background_done(JSContext *cx, JSObject *obj, 
                uintN argc, jsval *argv, jsval *rval)
{
    JSBackground *bg;

    bg = JS_GetInstancePrivate(cx, obj, &lm_background_class, argv);
    if (!bg)
	    return JS_FALSE;

    /*
	if (!lm_CanAccessTarget(cx, JSTARGET_UNIVERSAL_BROWSER_READ))
	    return JS_FALSE;
        */

    Autoupdate_Done(bg->bg_connect);

    return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
background_downloadNow(JSContext *cx, JSObject *obj, 
                       uintN argc, jsval *argv, jsval *rval)
{
    JSBackground *bg;

    bg = JS_GetInstancePrivate(cx, obj, &lm_background_class, argv);
    if (!bg)
	    return JS_FALSE;

    /*
	if (!lm_CanAccessTarget(cx, JSTARGET_UNIVERSAL_BROWSER_READ))
	    return JS_FALSE;
        */

    Autoupdate_DownloadNow(bg->bg_connect);

    return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
background_abort(JSContext *cx, JSObject *obj, 
                 uintN argc, jsval *argv, jsval *rval)
{
    JSBackground *bg;

    bg = JS_GetInstancePrivate(cx, obj, &lm_background_class, argv);
    if (!bg)
	    return JS_FALSE;

    /*
	if (!lm_CanAccessTarget(cx, JSTARGET_UNIVERSAL_BROWSER_READ))
	    return JS_FALSE;
        */

    Autoupdate_Abort(bg->bg_connect);

    return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
background_toString(JSContext *cx, JSObject *obj,
                    uintN argc, jsval *argv, jsval *rval)
{
    JSBackground *bg;
    char *bytes;
    JSString *str;

    bg = JS_GetInstancePrivate(cx, obj, &lm_background_class, argv);
    if (!bg)
	    return JS_FALSE;

    /*
	if (!lm_CanAccessTarget(cx, JSTARGET_UNIVERSAL_BROWSER_READ))
	    return JS_FALSE;
        */

    bytes = 0;
    StrAllocCopy(bytes, "<TITLE>Background download details</TITLE>"
		      "<TABLE BORDER=0 ALIGN=center VALIGN=top HSPACE=8>");
	StrAllocCat(bytes, "<TR><TD VALIGN=top><STRONG>");
	/* i18n problem */
    /* I believe this part is broken for I18N. Particular after we change the he->title to UTF8 */
    /* force the output HTML to display as in UTF8 will fix it. */
	StrAllocCat(bytes, bg->id);
	StrAllocCat(bytes, "</STRONG></TD><TD>&nbsp;</TD>"
			 "<TD VALIGN=top><A HREF=\"");
	StrAllocCat(bytes, bg->url);
	StrAllocCat(bytes, "\">");
	StrAllocCat(bytes, bg->url);
	StrAllocCat(bytes, "</A></TD></TR>");
    StrAllocCat(bytes, "</TABLE>");
    if (!bytes) {
    	JS_ReportOutOfMemory(cx);
	    return JS_FALSE;
    }

    str = JS_NewString(cx, bytes, XP_STRLEN(bytes));
    if (!str)
    	return JS_FALSE;
    *rval = STRING_TO_JSVAL(str);
    return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
background_getInfo(JSContext *cx, JSObject *obj, uintN argc,
                   jsval *argv, jsval *rval)
{
    JSObject *bg_obj;
    JSString *str;
    char *id;

    if (argc == 0 || !JSVAL_IS_STRING(argv[0])) {
        JS_ReportError(cx, "String argument expected for getInfo.");
        return JS_FALSE;
    }

	str = JS_ValueToString(cx, argv[0]);
	if (!str)
	    return JS_FALSE;

    id = JS_GetStringBytes(str);
    if (id == NULL)
        return JS_FALSE;

    bg_obj = lm_GetCurrentBackgroundObject(cx, id);
    if (!bg_obj)
		return JS_TRUE;
    *rval = OBJECT_TO_JSVAL(bg_obj);  
    return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
background_addPending(JSContext *cx, JSObject *obj, uintN argc,
                      jsval *argv, jsval *rval)
{
    JSString *str;
    char *id;
    char* url;
    char* script;
    int32 file_size;
    JSObject *bg_obj;

    if (argc == 0 || !JSVAL_IS_STRING(argv[0])) {
        JS_ReportError(cx, "String argument expected for addPending.");
        return JS_FALSE;
    }

	str = JS_ValueToString(cx, argv[0]);
	if (!str)
	    return JS_FALSE;

    id = JS_GetStringBytes(str);
    if (id == NULL)
        return JS_FALSE;

	str = JS_ValueToString(cx, argv[1]);
	if (!str)
	    return JS_FALSE;
    url = JS_GetStringBytes(str);

    if (!JS_ValueToInt32(cx, argv[2], &file_size))
		return JS_FALSE;

	str = JS_ValueToString(cx, argv[3]);
	if (!str)
	    return JS_FALSE;
    script = JS_GetStringBytes(str);

    bg_obj = lm_NewBackgroundObject(cx, id, url, file_size, script);
    if (!bg_obj)
		return JS_FALSE;
    *rval = OBJECT_TO_JSVAL(bg_obj);  
    return JS_TRUE;
}


PR_STATIC_CALLBACK(JSBool)
background_getPending(JSContext *cx, JSObject *obj, uintN argc,
                      jsval *argv, jsval *rval)
{
    JSObject *bg_obj;

    /* XXX: We need to get a list of bg objects. Or we need to
     * get a list of id's and build an array of objects from
     * that list
     */
    bg_obj = lm_GetCurrentBackgroundObject(cx, NULL);
    if (!bg_obj)
		return JS_TRUE;
    *rval = OBJECT_TO_JSVAL(bg_obj);  
    return JS_TRUE;
}


static JSBool
background_constructor(JSContext *cx, JSObject *obj, 
                       char *id, char *url, int32 file_size,
                       char* script)
{
    JSBackground *bg;
    MWContext* someRandomContext = XP_FindSomeContext();

    if (!(bg = JS_malloc(cx, sizeof(JSBackground))))
    	return JS_FALSE;
    XP_BZERO(bg, sizeof *bg);

    bg->bg_connect = AutoUpdate_Setup(someRandomContext, id, url, file_size, script);
    bg->id = PL_strdup(id);
    bg->url = PL_strdup(url);

    if (!JS_SetPrivate(cx, obj, bg)) {
        background_free(bg);
        JS_free(cx, bg);
        return JS_FALSE;
    }
    return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
Background(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval)
{
    JSString *str;
    char *id;
    char *url;
    char *script;
    int32 file_size;

    if ((!obj) || (argc != 4))
    	return JS_FALSE;

	str = JS_ValueToString(cx, argv[0]);
	if (!str)
	    return JS_FALSE;
    id = JS_GetStringBytes(str);

	str = JS_ValueToString(cx, argv[1]);
	if (!str)
	    return JS_FALSE;
    url = JS_GetStringBytes(str);

    if (!JS_ValueToInt32(cx, argv[2], &file_size))
		return JS_FALSE;

	str = JS_ValueToString(cx, argv[3]);
	if (!str)
	    return JS_FALSE;
    script = JS_GetStringBytes(str);

    return background_constructor(cx, obj, id, url, file_size, script);
}

/* 
 * Initialize the class
 */
JSClass lm_background_class = {
    "Background", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,  JS_PropertyStub,    background_getProperty,     JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,     JS_ConvertStub,             background_finalize
};

static JSFunctionSpec background_methods[] = {
    {"stop",         	background_stop,           	  0},
    {"start",           background_start,             0},
    {"done",            background_done,              0},
    {"downloadNow",     background_downloadNow,       0},
    {"abort",           background_abort,             0},
    {lm_toString_str,   background_toString,          1},
    {0}
};

static JSFunctionSpec background_static_methods[] = {
    {"getInfo",       background_getInfo,          1},
    {"addPending",    background_addPending,       1},
    {"getPending",    background_getPending,       0},
    {0}
};

/* XXX: The following is a hack. We shouldn't have to store stuff in globals */
/* They are used by the static methods */
static JSObject *gPrototype = NULL;	/* Using this as a class prototype */
									/* and a signal if class has been loaded */

static MochaDecoder *gDecoder=NULL;

JSBool
lm_InitBackgroundClass(MochaDecoder *decoder)
{
    JSObject *ctor;
    JSObject *obj;
    JSContext *cx;
    JSObject *prototype;

    obj = decoder->background_update;
    if (obj) {
        /* XXX: The following is not thread safe */
        gDecoder = decoder;
        gPrototype = obj;
		return JS_TRUE;
    }
    cx = decoder->js_context;

    prototype = JS_InitClass(cx, decoder->window_object, NULL, 
                             &lm_background_class, Background, 3, 
                             background_props, background_methods, 
                             NULL, background_static_methods);
    if (!prototype || !(ctor = JS_GetConstructor(cx, prototype)))
        return JS_FALSE;

    if (!JS_DefineConstDoubles(cx, ctor, background_constants))
	    return JS_FALSE;

    /* XXX: Should we hold a reference to prototype and decoder objects?? */
    decoder->background_update = gPrototype = prototype;
    gDecoder =  decoder;
    return JS_TRUE;
}

static JSObject *
lm_NewBackgroundObject(JSContext* cx, char* id, char* url, int32 file_size, char* script)
{
    JSObject *obj;

    obj = JS_NewObject(cx, &lm_background_class, gPrototype, 
                       gDecoder->window_object);
    if (!obj)
    	return NULL;

    if (!background_constructor(cx, obj, id, url, file_size, script))
	    return NULL;

    return obj;
}

static JSObject *
lm_GetCurrentBackgroundObject(JSContext* cx, char* id) 
{
    JSObject *obj;
    JSBackground *bg;
    AutoUpdateConnnection bg_connect;

    bg_connect = AutoUpdate_GetPending(id);
    if (!bg_connect) 
        return NULL;
    obj = JS_NewObject(cx, &lm_background_class, gPrototype, 
                       gDecoder->window_object);
    if (!obj)
    	return NULL;

    if (!(bg = JS_malloc(cx, sizeof(JSBackground))))
    	return NULL;
    XP_BZERO(bg, sizeof *bg);

    bg->bg_connect = bg_connect;
    bg->id = PL_strdup(AutoUpdate_GetID(bg->bg_connect));
    bg->url = PL_strdup(AutoUpdate_GetURL(bg->bg_connect));

    if (!JS_SetPrivate(cx, obj, bg)) {
        background_free(bg);
        JS_free(cx, bg);
        return NULL;
    }
    return obj;
}

