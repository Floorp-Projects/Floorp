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
 * JS event class.
 */
#include "lm.h"
#include "lo_ele.h"
#include "pa_tags.h"
#include "xp.h"
#include "prtypes.h"

typedef enum JSBarType {
    JSBAR_MENU, JSBAR_TOOL, JSBAR_LOCATION, JSBAR_PERSONAL, JSBAR_STATUS,
    JSBAR_SCROLL
} JSBarType;

const char *barnames[] = {
    "menubar", "toolbar", "locationbar", "personalbar", "statusbar",
    "scrollbars"
};

typedef struct JSBar {
    MochaDecoder    *decoder;
    JSBarType       bartype;
} JSBar;

enum bar_tinyid {
    BAR_VISIBLE		= -1
};

static JSPropertySpec bar_props[] = {
    {"visible"		, BAR_VISIBLE,	    JSPROP_ENUMERATE},
    {0}
};

extern JSClass lm_bar_class;

PR_STATIC_CALLBACK(JSBool)
bar_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSBar * bar;
    MWContext *context;
    Chrome *chrome;

    if (!JSVAL_IS_INT(id))
	return JS_TRUE;

    if (JSVAL_TO_INT(id) != BAR_VISIBLE)
	return JS_TRUE;

    bar = JS_GetInstancePrivate(cx, obj, &lm_bar_class, NULL);
    if (!bar)
	return JS_TRUE;
    context = bar->decoder->window_context;
    if (!context)
	return JS_TRUE;

    chrome = XP_NEW_ZAP(Chrome);
    if (!chrome) {
	JS_ReportOutOfMemory(cx);
	return JS_FALSE;
    }
    ET_PostQueryChrome(context, chrome);

    switch (bar->bartype) {
      case JSBAR_MENU:
	*vp = BOOLEAN_TO_JSVAL(chrome->show_menu);
	break;
      case JSBAR_TOOL:
	*vp = BOOLEAN_TO_JSVAL(chrome->show_button_bar);
	break;
      case JSBAR_LOCATION:
	*vp = BOOLEAN_TO_JSVAL(chrome->show_url_bar);
	break;
      case JSBAR_PERSONAL:
	*vp = BOOLEAN_TO_JSVAL(chrome->show_directory_buttons);
	break;
      case JSBAR_STATUS:
	*vp = BOOLEAN_TO_JSVAL(chrome->show_bottom_status_bar);
	break;
      case JSBAR_SCROLL:
	*vp = BOOLEAN_TO_JSVAL(chrome->show_scrollbar);
	break;
    }
    XP_FREE(chrome);
    return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
bar_setProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSBar * bar;
    MWContext *context;
    JSBool show;
    Chrome *chrome;

    if (!lm_CanAccessTarget(cx, JSTARGET_UNIVERSAL_BROWSER_WRITE))
	return JS_TRUE;
    
    if (!JSVAL_IS_INT(id))
	return JS_TRUE;

    if (JSVAL_TO_INT(id) != BAR_VISIBLE)
	return JS_TRUE;

    bar = JS_GetInstancePrivate(cx, obj, &lm_bar_class, NULL);
    if (!bar)
	return JS_TRUE;
    context = bar->decoder->window_context;
    if (!context)
	return JS_TRUE;

    if (!JS_ValueToBoolean(cx, *vp, &show))
        return JS_FALSE;

    chrome = XP_NEW_ZAP(Chrome);
    if (!chrome) {
	JS_ReportOutOfMemory(cx);
	return JS_FALSE;
    }
    ET_PostQueryChrome(context, chrome);

    switch (bar->bartype) {
      case JSBAR_MENU:
	chrome->show_menu = show;
	if (show)
	    chrome->disable_commands = FALSE;
	break;
      case JSBAR_TOOL:
	chrome->show_button_bar = show;
	break;
      case JSBAR_LOCATION:
	chrome->show_url_bar = show;
	break;
      case JSBAR_PERSONAL:
	chrome->show_directory_buttons = show;
	break;
      case JSBAR_STATUS:
	chrome->show_bottom_status_bar = show;
	break;
      case JSBAR_SCROLL:
	chrome->show_scrollbar = show;
	break;
    }

    ET_PostUpdateChrome(context, chrome);
    XP_FREE(chrome);
    return JS_TRUE;
}

PR_STATIC_CALLBACK(void)
bar_finalize(JSContext *cx, JSObject *obj)
{
    JSBar *bar;

    bar = JS_GetPrivate(cx, obj);
    if (!bar)
	return;
    DROP_BACK_COUNT(bar->decoder);
    JS_free(cx, bar);
}

JSClass lm_bar_class = {
    "Bar", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,  JS_PropertyStub,    bar_getProperty,     bar_setProperty,
    JS_EnumerateStub, JS_ResolveStub,     JS_ConvertStub,      bar_finalize
};

PR_STATIC_CALLBACK(JSBool)
Bar(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval)
{
    return JS_TRUE;
}

static JSBool
define_bar(JSContext *cx, MochaDecoder *decoder, JSBarType type)
{
    JSBar *bar;
    JSObject *obj;

    if (!(bar = JS_malloc(cx, sizeof(JSBar))))
	return JS_FALSE;

    obj = JS_DefineObject(cx, decoder->window_object, barnames[type],
			  &lm_bar_class, decoder->bar_prototype,
			  JSPROP_ENUMERATE|JSPROP_READONLY);
    if (!obj || !JS_SetPrivate(cx, obj, bar)) {
	JS_free(cx, bar);
	return JS_FALSE;
    }

    bar->decoder = HOLD_BACK_COUNT(decoder);
    bar->bartype = type;
    return JS_TRUE;
}

JSBool
lm_DefineBarClasses(MochaDecoder *decoder)
{
    JSContext *cx;
    JSObject *prototype;

    cx = decoder->js_context;
    prototype = JS_InitClass(cx, decoder->window_object, NULL, &lm_bar_class,
			     Bar, 0, bar_props, NULL, NULL, NULL);
    if (!prototype)
	return JS_FALSE;
    decoder->bar_prototype = prototype;
    return JS_TRUE;
}

JSBool
lm_ResolveBar(JSContext *cx, MochaDecoder *decoder, const char *name)
{
#define FROB0(BAR, EXTRA) {                                                   \
    if (XP_STRCMP(name, barnames[BAR]) == 0) {                                \
	if (!define_bar(cx, decoder, BAR))                                    \
	    return JS_FALSE;                                                  \
	EXTRA;                                                                \
	return JS_TRUE;                                                       \
    }                                                                         \
}
#define FROB(BAR)           FROB0(BAR, (void)0)
#define FROB2(BAR, EXTRA)   FROB0(BAR, EXTRA)

    FROB(JSBAR_MENU);
    FROB(JSBAR_TOOL);
    FROB(JSBAR_LOCATION);
    FROB2(JSBAR_PERSONAL,
	if (!JS_AliasProperty(cx, decoder->window_object,
			      barnames[JSBAR_PERSONAL],
			      "directories")) {
	    return JS_FALSE;
	})
    FROB(JSBAR_STATUS);
    FROB(JSBAR_SCROLL);

    return JS_TRUE;

#undef FROB
}
