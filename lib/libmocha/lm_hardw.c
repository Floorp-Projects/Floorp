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
 * JS reflection of the Hardware info top-level object.
 *
 * Tom Pixley, 1/2/98
 */
#include "lm.h"

typedef struct JSHardware {
    MochaDecoder    *decoder;
} JSHardware;

enum hardware_tinyid {
    HARDWARE_ARCHITECTURE   = -1,
    HARDWARE_CLOCK_SPEED    = -2,
    HARDWARE_RAM	    = -3
};

static JSPropertySpec hardware_props[] = {
    {"architecture",	HARDWARE_ARCHITECTURE,	JSPROP_ENUMERATE | JSPROP_READONLY},
    {"clockSpeed",	HARDWARE_CLOCK_SPEED,	JSPROP_ENUMERATE | JSPROP_READONLY},
    {"ram",		HARDWARE_RAM	,	JSPROP_ENUMERATE | JSPROP_READONLY},
    {0}
};

extern JSClass lm_hardware_class;

PR_STATIC_CALLBACK(JSBool)
hardware_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    MWContext *context;
    JSHardware *hardware;
    jsint slot;
    char * arch;

    if (!JSVAL_IS_INT(id))
	return JS_TRUE;

    slot = JSVAL_TO_INT(id);

    hardware = JS_GetInstancePrivate(cx, obj, &lm_hardware_class, NULL);
    if (!hardware)
	return JS_TRUE;
    context = hardware->decoder->window_context;
    if (!context)
	return JS_TRUE;

    switch (slot) {
	case HARDWARE_ARCHITECTURE:
#ifdef XP_WIN
	    arch = ET_moz_CallFunctionString((ETStringPtrFunc)FE_SystemCPUInfo, NULL);
            *vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, arch));
	    XP_FREE(arch);
#endif
	    break;
	case HARDWARE_CLOCK_SPEED:
#ifdef XP_WIN
            *vp = INT_TO_JSVAL(
		ET_moz_CallFunctionInt((ETIntPtrFunc)FE_SystemClockSpeed, NULL));
#endif
	    break;
	case HARDWARE_RAM:
#ifdef XP_WIN
            *vp = INT_TO_JSVAL(
		ET_moz_CallFunctionInt((ETIntPtrFunc)FE_SystemRAM, NULL));
#endif
	    break;
	default:
	    return JS_TRUE;
    }
    return JS_TRUE;
}


PR_STATIC_CALLBACK(void)
hardware_finalize(JSContext *cx, JSObject *obj)
{
    JSHardware *hardware;

    hardware = JS_GetPrivate(cx, obj);
    if (!hardware)
	return;
    DROP_BACK_COUNT(hardware->decoder);
    JS_free(cx, hardware);
}

JSClass lm_hardware_class = {
    "Hardware", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, hardware_getProperty, hardware_getProperty,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, hardware_finalize
};

PR_STATIC_CALLBACK(JSBool)
Hardware(JSContext *cx, JSObject *obj,
          uint argc, jsval *argv, jsval *rval)
{
    return JS_TRUE;
}

JSObject *
lm_DefineHardware(MochaDecoder *decoder, JSObject *parent)
{
    JSObject *obj;
    JSContext *cx;
    JSHardware *hardware;

    if (parent == decoder->window_object) {
        obj = decoder->hardware;
        if (obj)
            return obj;
    }

    cx = decoder->js_context;
    if (!(hardware = JS_malloc(cx, sizeof(JSHardware))))
	return 0;
    hardware->decoder = NULL; /* in case of error below */

    obj = JS_InitClass(cx, decoder->window_object, NULL, &lm_hardware_class,
                       Hardware, 0, hardware_props, NULL, NULL, NULL);

    if (!obj || !JS_SetPrivate(cx, obj, hardware)) {
        JS_free(cx, hardware);
        return 0;
    }
    
    if (!JS_DefineProperty(cx, parent, "hardware",
			   OBJECT_TO_JSVAL(obj), NULL, NULL,
                           JSPROP_ENUMERATE | JSPROP_READONLY)) {
        return 0;
    }

    hardware->decoder = HOLD_BACK_COUNT(decoder);
    if (parent == decoder->window_object)
        decoder->hardware = obj;
    return obj;
}
