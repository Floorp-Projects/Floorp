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
 * JS reflection of the Screen info top-level object.
 *
 * Tom Pixley, 3/1/97
 */
#include "lm.h"

typedef struct JSScreen {
    MochaDecoder    *decoder;
} JSScreen;

enum screen_tinyid {
    SCREEN_WIDTH	    = -1,
    SCREEN_HEIGHT   	    = -2,
    SCREEN_PIXEL_DEPTH	    = -3,
    SCREEN_COLOR_DEPTH	    = -4,
    SCREEN_AVAIL_WIDTH	    = -5,
    SCREEN_AVAIL_HEIGHT	    = -6,
    SCREEN_AVAIL_LEFT	    = -7,
    SCREEN_AVAIL_TOP	    = -8
};

static JSPropertySpec screen_props[] = {
    {"width",		SCREEN_WIDTH,		JSPROP_ENUMERATE | JSPROP_READONLY},
    {"height",		SCREEN_HEIGHT,		JSPROP_ENUMERATE | JSPROP_READONLY},
    {"pixelDepth",      SCREEN_PIXEL_DEPTH,     JSPROP_ENUMERATE | JSPROP_READONLY},
    {"colorDepth",      SCREEN_COLOR_DEPTH,     JSPROP_ENUMERATE | JSPROP_READONLY},
    {"availWidth",      SCREEN_AVAIL_WIDTH,     JSPROP_ENUMERATE | JSPROP_READONLY},
    {"availHeight",     SCREEN_AVAIL_HEIGHT,    JSPROP_ENUMERATE | JSPROP_READONLY},
    {"availLeft",       SCREEN_AVAIL_LEFT,      JSPROP_ENUMERATE | JSPROP_READONLY},
    {"availTop",        SCREEN_AVAIL_TOP,       JSPROP_ENUMERATE | JSPROP_READONLY},
    {0}
};

extern JSClass lm_screen_class;

PR_STATIC_CALLBACK(JSBool)
screen_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    MWContext *context;
    JSScreen *screen;
    jsint slot;
    int32 width, height, left, top, pixel, pallette;

    if (!JSVAL_IS_INT(id))
	return JS_TRUE;

    slot = JSVAL_TO_INT(id);

    screen = JS_GetInstancePrivate(cx, obj, &lm_screen_class, NULL);
    if (!screen)
	return JS_TRUE;
    context = screen->decoder->window_context;
    if (!context)
	return JS_TRUE;

    switch (slot) {
	case SCREEN_WIDTH:
	    ET_PostGetScreenSize(context, &width, &height);
            *vp = INT_TO_JSVAL(width);
	    break;
	case SCREEN_HEIGHT:
	    ET_PostGetScreenSize(context, &width, &height);
            *vp = INT_TO_JSVAL(height);
	    break;
	case SCREEN_PIXEL_DEPTH:
	    ET_PostGetColorDepth(context, &pixel, &pallette);
            *vp = INT_TO_JSVAL(pixel);
	    break;
	case SCREEN_COLOR_DEPTH:
	    ET_PostGetColorDepth(context, &pixel, &pallette);
            *vp = INT_TO_JSVAL(pallette);
	    break;
	case SCREEN_AVAIL_WIDTH:
	    ET_PostGetAvailScreenRect(context, &width, &height, &left, &top);
            *vp = INT_TO_JSVAL(width);
	    break;
	case SCREEN_AVAIL_HEIGHT:
	    ET_PostGetAvailScreenRect(context, &width, &height, &left, &top);
            *vp = INT_TO_JSVAL(height);
	    break;
	case SCREEN_AVAIL_LEFT:
	    ET_PostGetAvailScreenRect(context, &width, &height, &left, &top);
            *vp = INT_TO_JSVAL(left);
	    break;
	case SCREEN_AVAIL_TOP:
	    ET_PostGetAvailScreenRect(context, &width, &height, &left, &top);
            *vp = INT_TO_JSVAL(top);
	    break;
	default:
	    return JS_TRUE;
    }
    return JS_TRUE;
}


PR_STATIC_CALLBACK(void)
screen_finalize(JSContext *cx, JSObject *obj)
{
    JSScreen *screen;

    screen = JS_GetPrivate(cx, obj);
    if (!screen)
	return;
    DROP_BACK_COUNT(screen->decoder);
    JS_free(cx, screen);
}

JSClass lm_screen_class = {
    "Screen", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, screen_getProperty, screen_getProperty,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, screen_finalize
};

PR_STATIC_CALLBACK(JSBool)
Screen(JSContext *cx, JSObject *obj,
          uint argc, jsval *argv, jsval *rval)
{
    return JS_TRUE;
}

JSObject *
lm_DefineScreen(MochaDecoder *decoder, JSObject *parent)
{
    JSObject *obj;
    JSContext *cx;
    JSScreen *screen;

    if (parent == decoder->window_object) {
        obj = decoder->screen;
        if (obj)
            return obj;
    }

    cx = decoder->js_context;
    if (!(screen = JS_malloc(cx, sizeof(JSScreen))))
	return NULL;
    screen->decoder = NULL; /* in case of error below */

    obj = JS_InitClass(cx, decoder->window_object, NULL, &lm_screen_class,
                       Screen, 0, screen_props, NULL, NULL, NULL);

    if (!obj || !JS_SetPrivate(cx, obj, screen)) {
        JS_free(cx, screen);
        return NULL;
    }
    
    if (!JS_DefineProperty(cx, parent, "screen",
			   OBJECT_TO_JSVAL(obj), NULL, NULL,
                           JSPROP_ENUMERATE | JSPROP_READONLY)) {
        return NULL;
    }

    screen->decoder = HOLD_BACK_COUNT(decoder);
    if (parent == decoder->window_object)
        decoder->screen = obj;
    return obj;
}
