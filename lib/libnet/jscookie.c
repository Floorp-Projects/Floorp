/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
   jscookie.c -- javascript reflection of cookies for filters.
   Created: Frederick G.M. Roeber <roeber@netscape.com>, 12-Jul-97.
   Adopted: Judson Valeski, 1997.
   Large chunks of this were stolen from jsmsg.c.
 */

#include "mkutils.h"
#include "mkutils.h"
#include "mkparse.h"
#include "mkaccess.h"
#include "prefapi.h"
#include "jsapi.h"
#include "xp_core.h"
#include "xp_mcom.h"
#include "jscookie.h"
#include "ds.h"
#include "htmldlgs.h"
#include "xpgetstr.h"

extern int MK_ACCESS_JAVASCRIPT_COOKIE_FILTER;

static JSObject *filter_obj = NULL;
static JSContext *filter_context = NULL;

static JSBool error_reporter_installed = JS_FALSE;
static JSErrorReporter previous_error_reporter;

/* tells us when we should recompile the file. */
static JSBool need_compile = JS_TRUE;

/* This is the private instance data associated with a cookie */
typedef struct JSCookieData {
    JSContext *js_context;
    JSObject *js_object;
    JSCFCookieData *data;
    PRPackedBool property_changed, rejected, accepted, ask, decision_made;
} JSCookieData;

/* The properties of a cookie that we reflect */
enum cookie_slot {
    COOKIE_PATH			= -1,
    COOKIE_DOMAIN		= -2,
    COOKIE_NAME			= -3,
    COOKIE_VALUE		= -4,
    COOKIE_EXPIRES		= -5,
    COOKIE_URL			= -6,
    COOKIE_IS_SECURE		= -7,
    COOKIE_IS_DOMAIN		= -8,
    COOKIE_PROMPT_PREF		= -9,
    COOKIE_PREF			= -10
};

/* 
 * Should more of these be readonly? What does it mean for a cookie
 *   to de secure?  -chouck
 */
static JSPropertySpec cookie_props[] = {
    { "path",       COOKIE_PATH,        JSPROP_ENUMERATE },
    { "domain",     COOKIE_DOMAIN,      JSPROP_ENUMERATE },
    { "name",       COOKIE_NAME,        JSPROP_ENUMERATE },
    { "value",      COOKIE_VALUE,       JSPROP_ENUMERATE },
    { "expires",    COOKIE_EXPIRES,     JSPROP_ENUMERATE },
    { "url",        COOKIE_URL,         JSPROP_ENUMERATE|JSPROP_READONLY },
    { "isSecure",   COOKIE_IS_SECURE,   JSPROP_ENUMERATE },
    { "isDomain",   COOKIE_IS_DOMAIN,   JSPROP_ENUMERATE },
    { "prompt",     COOKIE_PROMPT_PREF, JSPROP_ENUMERATE|JSPROP_READONLY },
    { "preference", COOKIE_PREF,        JSPROP_ENUMERATE|JSPROP_READONLY },
    { 0 }
};

PR_STATIC_CALLBACK(JSBool)
cookie_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSCookieData *data;
    JSString *str;
    jsint slot;

    data = (JSCookieData *)JS_GetPrivate(cx, obj);
    if (!data)
	return JS_TRUE;

    if (!JSVAL_IS_INT(id))
        return JS_TRUE;

    slot = JSVAL_TO_INT(id);

    switch (slot) {
    case COOKIE_PATH:
        str = JS_NewStringCopyZ(cx, (const char *)data->data->path_from_header);
        if( (JSString *)0 == str )
            return JS_FALSE;
        *vp = STRING_TO_JSVAL(str);
        break;
    case COOKIE_DOMAIN:
        str = JS_NewStringCopyZ(cx, (const char *)data->data->host_from_header);
        if( (JSString *)0 == str )
            return JS_FALSE;
        *vp = STRING_TO_JSVAL(str);
        break;
    case COOKIE_NAME:
        str = JS_NewStringCopyZ(cx, (const char *)data->data->name_from_header);
        if( (JSString *)0 == str )
            return JS_FALSE;
        *vp = STRING_TO_JSVAL(str);
        break;
    case COOKIE_VALUE:
        str = JS_NewStringCopyZ(cx, (const char *)data->data->cookie_from_header);
        if( (JSString *)0 == str )
            return JS_FALSE;
        *vp = STRING_TO_JSVAL(str);
        break;
    case COOKIE_EXPIRES:
        *vp = INT_TO_JSVAL(data->data->expires);
        break;
    case COOKIE_URL:
        str = JS_NewStringCopyZ(cx, (const char *)data->data->url);
        if( (JSString *)0 == str )
            return JS_FALSE;
        *vp = STRING_TO_JSVAL(str);
        break;
    case COOKIE_IS_SECURE:
        *vp = BOOLEAN_TO_JSVAL(data->data->secure);
        break;
    case COOKIE_IS_DOMAIN:
        *vp = BOOLEAN_TO_JSVAL(data->data->domain);
        break;
    case COOKIE_PROMPT_PREF:
        *vp = BOOLEAN_TO_JSVAL(data->data->prompt);
        break;
    case COOKIE_PREF:
        *vp = INT_TO_JSVAL(data->data->preference);
        break;
    }

    return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
cookie_setProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSCookieData *data;
    jsint slot;
    PRInt32 i;
    JSBool b;

    data = (JSCookieData *)JS_GetPrivate(cx, obj);
    if (!data)
		return JS_TRUE;

    if (!JSVAL_IS_INT(id))
        return JS_TRUE;

    slot = JSVAL_TO_INT(id);

	if (slot == COOKIE_PATH) {
		data->data->path_from_header = JS_GetStringBytes(JSVAL_TO_STRING(*vp));
	} 
	else if (slot == COOKIE_DOMAIN) {
		data->data->host_from_header = JS_GetStringBytes(JSVAL_TO_STRING(*vp));
	}
	else if (slot == COOKIE_NAME) {
		data->data->name_from_header = JS_GetStringBytes(JSVAL_TO_STRING(*vp));
	}
	else if (slot == COOKIE_VALUE) {
		data->data->cookie_from_header = JS_GetStringBytes(JSVAL_TO_STRING(*vp));
	}
	else if (slot == COOKIE_EXPIRES) {
		if( !JS_ValueToInt32(cx, *vp, (long *)&i) )
            return JS_FALSE;
        data->data->expires = i;
	}
	else if (slot == COOKIE_IS_SECURE) {
        if( !JS_ValueToBoolean(cx, *vp, &b) )
            return JS_FALSE;
        data->data->secure = b;		
	}
	else if (slot == COOKIE_IS_DOMAIN) {
        if( !JS_ValueToBoolean(cx, *vp, &b) )
            return JS_FALSE;
        data->data->domain = b;		
	}

    data->property_changed = TRUE;
    return JS_TRUE;
}

PR_STATIC_CALLBACK(void)
cookie_finalize(JSContext *cx, JSObject *obj)
{
    JSCookieData *cookie;
    cookie = JS_GetPrivate(cx, obj);
    FREEIF(cookie);
}

/* So we can possibly add functions "global" to filters... */
static JSClass global_class = {
    "CookieFilters", 0 /* no private data */,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

static JSClass js_cookie_class = {
    "Cookie", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, cookie_getProperty, cookie_setProperty,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, cookie_finalize
};

/* cookie.accept() -- mark it okay */
PR_STATIC_CALLBACK(JSBool)
cookie_accept(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval * rval)
{
    JSCookieData *data;

    if (!(data = (JSCookieData*)JS_GetInstancePrivate(cx, obj, &js_cookie_class, argv)))
        return JS_FALSE;

    data->accepted = TRUE;
    data->rejected = FALSE;
    data->ask = FALSE;
    data->decision_made = TRUE;

    return JS_TRUE;
}

/* cookie.reject() -- reject it out of hand */
PR_STATIC_CALLBACK(JSBool)
cookie_reject(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval * rval)
{
    JSCookieData *data;

    if (!(data = (JSCookieData*)JS_GetInstancePrivate(cx, obj, &js_cookie_class, argv)))
        return JS_FALSE;

    data->accepted = FALSE;
    data->rejected = TRUE;
    data->ask = FALSE;
    data->decision_made = TRUE;

    return JS_TRUE;
}

/* cookie.ask() -- ask the luser, even if that pref is off */
PR_STATIC_CALLBACK(JSBool)
cookie_ask(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval * rval)
{
    JSCookieData *data;

    if (!(data = (JSCookieData*)JS_GetInstancePrivate(cx, obj, &js_cookie_class, argv)))
        return JS_FALSE;

    data->accepted = FALSE;
    data->rejected = FALSE;
    data->ask = TRUE;
    data->decision_made = TRUE;

    return JS_TRUE;
}

/* cookie.confirm() -- pop up a confirmation box */
PR_STATIC_CALLBACK(JSBool)
cookie_confirm(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval * rval)
{
    JSCookieData *data;
    JSString *str;
    char *msg = (char *)0;
    Bool result;
    MWContext * context = XP_FindSomeContext();

    if (argc < 1 || !context)
        return JS_FALSE;

    if (!(data = (JSCookieData*)JS_GetInstancePrivate(cx, obj, &js_cookie_class, argv)))
        return JS_FALSE;

    str = JS_ValueToString(cx, argv[0]);
    if (!str)
        return JS_FALSE;

    StrAllocCopy(msg, XP_GetString(MK_ACCESS_JAVASCRIPT_COOKIE_FILTER));
    StrAllocCat(msg, JS_GetStringBytes(str));
    if (!msg)
        return JS_FALSE;

    result = FE_Confirm(context, msg);
    FREEIF(msg);

    *rval = BOOLEAN_TO_JSVAL(result);

    return JS_TRUE;
}

#if DEBUG
/* trace() -- outputs spew to stderr.  Actually, this (or something like it
   that perhaps outputs to the same file that the rest of the filter logging code
   writes to) would probably be very useful in the normal course of writing filters. */
PR_STATIC_CALLBACK(JSBool)
cookie_filter_trace(JSContext *cx, JSObject * obj, uint argc, jsval *argv, jsval * rval)
{

        if (argc > 0)
                {
                        JSString *str;
                        const char *trace_str;
                        if (!(str = JS_ValueToString(cx, argv[0])))
                                return JS_FALSE;

                        trace_str = JS_GetStringBytes(str);
                        if (*trace_str != '\0')
                                {
                                        fprintf (stderr, "cookie filter trace: %s\n", trace_str);
                                }
                        return JS_TRUE;
                }

        return JS_FALSE;
}
#endif

static JSFunctionSpec cookie_methods[] = {
    { "accept", cookie_accept, 0 },
    { "reject", cookie_reject, 0 },
    { "ask",    cookie_ask, 0 },
    { "confirm", cookie_confirm, 1 },
    { 0 }
};

static JSFunctionSpec filter_methods[] = {
#ifdef DEBUG
    { "trace", cookie_filter_trace, 1 },
#endif
    { 0 }
};

PRIVATE void
destroyJSCookieFilterStuff(void)
{
        filter_obj = NULL;
}

/*
 * This function used to take an MWContext and store it as the private data
 *   of the filter object.  That is a no-no since the filter_obj is going to
 *   be around until the end of time but there is no guarentee that the
 *   context won't get free'd out from under us.  The solution is to not
 *   hold onto any particular context but just call XP_FindSomeContext() or
 *   some derivative of it when we need to.
 */
PRIVATE JSContext *
initializeJSCookieFilterStuff()
{

    /* Only bother initializing once */
    if (filter_obj)
        return filter_context;

    /* If we can't get the mozilla-thread global context just bail */
    PREF_GetConfigContext(&filter_context);
    if (!filter_context)
        return NULL;

    /* create our "global" object.  We make the message object a child of this */
    filter_obj = JS_NewObject(filter_context, &global_class, NULL, NULL);
    
    /* MLM - don't do JS_InitStandardClasses() twice */
    if (!filter_obj
            || !JS_DefineFunctions(filter_context, filter_obj, filter_methods))
            {
                destroyJSCookieFilterStuff();
                return NULL;
            }

    return filter_context;
}

PRIVATE JSObject *
newCookieObject(void)
{
    JSObject *rv;
    JSCookieData *cookie_data;

    rv = JS_DefineObject(filter_context, filter_obj,
                         "cookie", &js_cookie_class,
                         NULL, JSPROP_ENUMERATE);

    if( (JSObject *)0 == rv )
        return (JSObject *)0;

    cookie_data = XP_NEW_ZAP(JSCookieData);

    if( (JSCookieData *)0 == cookie_data )
        return (JSObject *)0;

    if( !JS_SetPrivate(filter_context, rv, cookie_data)
        || !JS_DefineProperties(filter_context, rv, cookie_props)
        || !JS_DefineFunctions(filter_context, rv, cookie_methods)) {
        return (JSObject *)0;
    }

    return rv;
}

PR_STATIC_CALLBACK(void)
jscookie_ErrorReporter(JSContext *cx, const char *message, JSErrorReport *report)
{
    char *msg = NULL;
    MWContext *context;
    
    context = XP_FindSomeContext();
    if(!context || !report)
        return;

    /*XXX: i18n-ise this */
    msg = PR_sprintf_append(NULL,
                            "JavaScript Cookie Filter Error:\n"
                            "You will be prompted manually to accept or reject this cookie.\n"
                            "Filename: %s\n"
                            "Line #: %u\n"
                            "%s\n"
                            "%.*s\n",
                            report->filename, 
                            report->lineno, 
                            report->linebuf,
                            (int)(report->tokenptr - report->linebuf) + 1,
                            "^"
                            );

    if (!msg)
        return;

    FE_Alert(context, msg);
    XP_FREE(msg);

    return;
}

PRIVATE JSBool
compileJSCookieFilters(void)
{
    static time_t m_time = 0; /* the last modification time of filters.js */
    static JSBool ret_val = JS_FALSE;
    char *filename;
    XP_File fp;
    XP_StatStruct stats;

    if (!need_compile)
        return ret_val;

    filename = WH_FileName("", xpJSCookieFilters);

    XP_Trace("+Filename for script filter is %s\n", filename);

	/* If we can't get to the file, get the hell outa dodge. */
    if(XP_Stat(filename, &stats, xpJSCookieFilters))
		return ret_val;

    if (stats.st_mtime > m_time || need_compile)
        {
            long fileLength;
            char *buffer;
            jsval rval;
            
            m_time = stats.st_mtime;
            
            fileLength = stats.st_size;
            if (fileLength <= 1)
                    {
                            ret_val = JS_FALSE;
                            return ret_val;
                    }
            
            if( !(fp = XP_FileOpen(filename, xpJSCookieFilters, "r")) ) {
					    ret_val = JS_FALSE;
					    return ret_val;
				    }
            
            buffer = (char*)malloc(fileLength);
            if (!buffer) {
                XP_FileClose(fp);
                ret_val = JS_FALSE;
                return ret_val;
            }
            
            fileLength = XP_FileRead(buffer, fileLength, fp);

            XP_FileClose(fp);

            XP_Trace("+Compiling filters.js...\n");

            ret_val = JS_EvaluateScript(filter_context, filter_obj, buffer, fileLength,
                                                                    filename, 1, &rval);

            XP_Trace("+Done.\n");
            
            XP_FREE(buffer);

            need_compile = JS_FALSE;
        }

    return ret_val;
}

PUBLIC JSCFResult 
JSCF_Execute(
                               MWContext *mwcontext,
                               const char *script_name,
                               JSCFCookieData *data,
                               Bool *data_changed
                               )
{
    jsval result;
    jsval filter_arg; /* we will this in with the message object we create. */
    JSObject *cookie_obj;
    JSCookieData *cookie_data;

    if (!script_name)
        return JSCF_error;

	/* initialize the filter stuff, and bomb out early if it fails */
    if (!initializeJSCookieFilterStuff())
        return JSCF_error;

    /* 
	 * try loading (reloading if necessary) the filter file before bothering 
	 *   to create any JS-objects
	 */
    if (!compileJSCookieFilters())
        return JSCF_error;

    if (!error_reporter_installed)
        {
            error_reporter_installed = JS_TRUE;
            previous_error_reporter = JS_SetErrorReporter(filter_context,
                                                          jscookie_ErrorReporter);
        }


    cookie_obj = newCookieObject();
    if( (JSObject *)0 == cookie_obj )
        return JSCF_error;

    cookie_data = (JSCookieData *)JS_GetPrivate(filter_context, cookie_obj);
    cookie_data->js_context = filter_context;
    cookie_data->js_object = cookie_obj;
    cookie_data->data = data;
    cookie_data->property_changed = FALSE;
    cookie_data->rejected = FALSE;
    cookie_data->accepted = FALSE;
    cookie_data->decision_made = FALSE;

    filter_arg = OBJECT_TO_JSVAL(cookie_obj);
    JS_CallFunctionName(filter_context, filter_obj, script_name, 1, 
                        &filter_arg, &result);

    *data_changed = cookie_data->property_changed;
    if( cookie_data->decision_made ) {
        if( cookie_data->rejected )
            return JSCF_reject;
        else if( cookie_data->accepted )
            return JSCF_accept;
        else if( cookie_data->ask )
            return JSCF_ask;
    }

    return JSCF_whatever;
}

PUBLIC void
JSCF_Cleanup(void)
{
    TRACEMSG(("+Cleaning up JS Cookie Filters"));

    need_compile = JS_TRUE;

    if (filter_context)
    {
        if (error_reporter_installed)
            {
                error_reporter_installed = JS_FALSE;
                JS_SetErrorReporter(filter_context, previous_error_reporter);
            }

        JS_GC(filter_context);

        destroyJSCookieFilterStuff();
	}
}
