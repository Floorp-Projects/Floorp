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
#include "prbit.h"
#include "prtypes.h"
#include "mkhelp.h"		/* For onhelp support */
#include "jsapi.h"		/* For onhelp support */

static JSEventNames event_names[] = {
    /* ordered by log2(event_bit) */
    {"mousedown",	"MouseDown"},
    {"mouseup",		"MouseUp"},
    {"mouseover",	"MouseOver"},
    {"mouseout",	"MouseOut"},
    {"mousemove",	"MouseMove"},
    {"mousedrag",	"MouseDrag"},
    {"click",		"Click"},
    {"dblclick",	"DblClick"},
    {"keydown",		"KeyDown"},
    {"keyup",		"KeyUp"},
    {"keypress",	"KeyPress"},
    {"dragdrop",	"DragDrop"},
    {"focus",		"Focus"},
    {"blur",		"Blur"},
    {"select",		"Select"},
    {"change",		"Change"},
    {"reset",		"Reset"},
    {"submit",		"Submit"},
    {"scroll",		"Scroll"},
    {"load",		"Load"},
    {"unload",		"Unload"},
    {"xferdone",	"XferDone"},
    {"abort",		"Abort"},
    {"error",		"Error"},
    {"locate",		"Locate"},
    {"move",		"Move"},
    {"resize",		"Resize"},
    {"forward",		"Forward"},
    {"help",		"Help"},
    {"back",		"Back"},
};

#define NUM_EVENTS      (sizeof event_names / sizeof event_names[0])

static JSConstDoubleSpec event_constants[] = {
    {EVENT_ALT_MASK             , "ALT_MASK"},
    {EVENT_CONTROL_MASK         , "CONTROL_MASK"},
    {EVENT_SHIFT_MASK           , "SHIFT_MASK"},
    {EVENT_META_MASK            , "META_MASK"},
    {EVENT_MOUSEDOWN		, "MOUSEDOWN"},
    {EVENT_MOUSEUP		, "MOUSEUP"},
    {EVENT_MOUSEOVER		, "MOUSEOVER"},
    {EVENT_MOUSEOUT		, "MOUSEOUT"},
    {EVENT_MOUSEMOVE		, "MOUSEMOVE"},
    {EVENT_MOUSEDRAG		, "MOUSEDRAG"},
    {EVENT_CLICK		, "CLICK"},
    {EVENT_DBLCLICK		, "DBLCLICK"},
    {EVENT_KEYDOWN		, "KEYDOWN"},
    {EVENT_KEYUP		, "KEYUP"},
    {EVENT_KEYPRESS		, "KEYPRESS"},
    {EVENT_DRAGDROP		, "DRAGDROP"},
    {EVENT_FOCUS		, "FOCUS"},
    {EVENT_BLUR			, "BLUR"},
    {EVENT_SELECT		, "SELECT"},
    {EVENT_CHANGE		, "CHANGE"},
    {EVENT_RESET		, "RESET"},
    {EVENT_SUBMIT		, "SUBMIT"},
    {EVENT_SCROLL		, "SCROLL"},
    {EVENT_LOAD			, "LOAD"},
    {EVENT_UNLOAD		, "UNLOAD"},
    {EVENT_XFER_DONE		, "XFER_DONE"},
    {EVENT_ABORT		, "ABORT"},
    {EVENT_ERROR		, "ERROR"},
    {EVENT_LOCATE		, "LOCATE"},
    {EVENT_MOVE			, "MOVE"},
    {EVENT_RESIZE		, "RESIZE"},
    {EVENT_FORWARD		, "FORWARD"},
    {EVENT_HELP			, "HELP"},
    {EVENT_BACK			, "BACK"},
    {0, 0}
};

const char *
lm_EventName(uint32 event_bit)
{
    uint index = (uint)PR_CeilingLog2(event_bit);

    if (index >= NUM_EVENTS)
	return "unknown event";
    return event_names[index].lowerName;
}

JSEventNames *
lm_GetEventNames(uint32 event_bit)
{
    uint index = (uint)PR_CeilingLog2(event_bit);

    if (index >= NUM_EVENTS)
	return NULL;
    return &event_names[index];
}

uint32
lm_FindEventInMWContext(MWContext *context)
{
    int i;
    XP_List *kids;
    MWContext *kid;
    uint32 events;


    if (!context->grid_children)
        return context->event_bit;

    events = 0;

    if ((kids = context->grid_children) != NULL) {
	for (i = 1; ((kid = XP_ListGetObjectNum(kids, i)) != NULL); i++) {
	    events |= lm_FindEventInMWContext(kid);
        }
    }
    return events;
}

XP_Bool
LM_EventCaptureCheck(MWContext * context, uint32 current_event) {

    if (context->event_bit & current_event)
	return TRUE;
    if (context->grid_parent)
	return LM_EventCaptureCheck(context->grid_parent, current_event);
    return FALSE;
}

enum event_tinyid {
    EVENT_TYPE		= -1,
    EVENT_X		= -2,
    EVENT_Y		= -3,
    EVENT_LAYERX	= -4,
    EVENT_LAYERY	= -5,
    EVENT_WHICH		= -6,
    EVENT_MODIFIERS	= -7,
    EVENT_DATA		= -8,
    EVENT_DOCX		= -9,
    EVENT_DOCY		= -10,
    EVENT_SCREENX	= -11,
    EVENT_SCREENY	= -12,
    EVENT_OBJECT	= -13
};

static JSPropertySpec event_props[] = {
    {"type"		, EVENT_TYPE,		JSPROP_ENUMERATE},
    {"x"		, EVENT_X,		JSPROP_ENUMERATE},
    {"y"		, EVENT_Y,		JSPROP_ENUMERATE},
    {"width"		, EVENT_X,		JSPROP_ENUMERATE},
    {"height"		, EVENT_Y,		JSPROP_ENUMERATE},
    {"layerX"		, EVENT_LAYERX,		JSPROP_ENUMERATE},
    {"layerY"		, EVENT_LAYERY,		JSPROP_ENUMERATE},
    {"which"		, EVENT_WHICH,		JSPROP_ENUMERATE},
    {"modifiers"	, EVENT_MODIFIERS,	JSPROP_ENUMERATE},
    {"data"		, EVENT_DATA,		JSPROP_ENUMERATE},
    {"pageX"		, EVENT_DOCX,		JSPROP_ENUMERATE},
    {"pageY"		, EVENT_DOCY,		JSPROP_ENUMERATE},
    {"screenX"		, EVENT_SCREENX,	JSPROP_ENUMERATE},
    {"screenY"		, EVENT_SCREENY,	JSPROP_ENUMERATE},
    {"target"		, EVENT_OBJECT,		JSPROP_ENUMERATE},
    {0}
};

extern JSClass lm_event_class;

PR_STATIC_CALLBACK(JSBool)
event_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSEvent *event;
    JSString *str;
    char ** urlArray;
    jsval urlVal;
    JSObject *array;
    jsint slot;

    if (!JSVAL_IS_INT(id))
	return JS_TRUE;

    event = JS_GetInstancePrivate(cx, obj, &lm_event_class, NULL);
    if (!event)
	return JS_TRUE;

    if (!JSVAL_IS_INT(id))
	return JS_TRUE;

    /*
     * You might think the following would make a real nice
     *   switch() statement.  But if you make it into one be
     *   ready to battle the Win16 internal compiler error
     *   demons.
     */
    if (JSVAL_TO_INT(id) == EVENT_TYPE) {
	str = JS_NewStringCopyZ(cx, lm_EventName(event->type));
	if (!str)
	    return JS_FALSE;
	*vp = STRING_TO_JSVAL(str);
    }
    else if ((JSVAL_TO_INT(id) == EVENT_X) || (JSVAL_TO_INT(id) == EVENT_LAYERX)) {
	* vp = INT_TO_JSVAL(event->x);
    }
    else if ((JSVAL_TO_INT(id) == EVENT_Y) || (JSVAL_TO_INT(id) == EVENT_LAYERY)){
	* vp = INT_TO_JSVAL(event->y);
    }
    else if (JSVAL_TO_INT(id) == EVENT_DOCX) {
	* vp = INT_TO_JSVAL(event->docx);
    }
    else if (JSVAL_TO_INT(id) == EVENT_DOCY) {
	* vp = INT_TO_JSVAL(event->docy);
    }
    else if (JSVAL_TO_INT(id) == EVENT_SCREENX) {
	* vp = INT_TO_JSVAL(event->screenx);
    }
    else if (JSVAL_TO_INT(id) == EVENT_SCREENY) {
	* vp = INT_TO_JSVAL(event->screeny);
    }
    else if (JSVAL_TO_INT(id) == EVENT_WHICH) {
	if (event->type == EVENT_HELP) {
	    /* For onHelp events, the which parameter holds the
	       URL of the help topic, as given in the pHelpInfo
	       field of the MWContext associated with this window. */
	    MWContext	*context;

	    context = event->ce.context;
	    if (context->pHelpInfo) {
		char 		*topicID;
		JSString	*topicStr;

		topicID = ((HelpInfoStruct *) (context->pHelpInfo))->topicURL;
		topicStr = JS_NewStringCopyZ(cx, topicID);

		* vp = STRING_TO_JSVAL(topicStr);

	    }
	    else {
		JSString	*topicStr;

		topicStr = JS_NewStringCopyZ(cx, "about:blank"); /* Do not localize */
		* vp = STRING_TO_JSVAL(topicStr);
	    }

	}
	else {
	    * vp = INT_TO_JSVAL(event->which);
	}
    }
    else if (JSVAL_TO_INT(id) == EVENT_MODIFIERS) {
	* vp = INT_TO_JSVAL(event->modifiers);
    }

    else if (JSVAL_TO_INT(id) == EVENT_OBJECT) {
	if (event->object)
	    * vp = OBJECT_TO_JSVAL(event->object);
    }

    else if (JSVAL_TO_INT(id) == EVENT_DATA) {
	if (event->type == EVENT_DRAGDROP) {
	    if (lm_CanAccessTarget(cx, JSTARGET_UNIVERSAL_BROWSER_READ)) {
		array = JS_NewArrayObject(cx, 0, NULL);
		if (!array)
		    return JS_FALSE;

		urlArray = (char**)event->data;
		if (!urlArray)
		    return JS_TRUE;
		for (slot=0; slot<(jsint)event->dataSize; slot++) {
		    str = JS_NewStringCopyZ(cx, urlArray[slot]);
		    if (!str)
			return JS_FALSE;
		    urlVal = STRING_TO_JSVAL(str);
		    if (!JS_SetElement(cx, array, slot, &urlVal))
			return JS_FALSE;
		}

		*vp = OBJECT_TO_JSVAL(array);				     
	    }
	}
    }

    return JS_TRUE;

}

static JSBool
GetUint32(JSContext *cx, jsval v, uint32 *uip)
{
    jsdouble d;

    if (!JS_ValueToNumber(cx, v, &d))
	return JS_FALSE;
    *uip = (uint32)d;
    return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
event_setProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSEvent *event;
    jsint slot, i;
    JSString *str;
    const char *name;
    uint32 temp;

    if (!JSVAL_IS_INT(id))
	return JS_TRUE;

    event = JS_GetInstancePrivate(cx, obj, &lm_event_class, NULL);
    if (!event)
	return JS_TRUE;

    slot = JSVAL_TO_INT(id);

    if (slot==EVENT_TYPE) {
	if (!JSVAL_IS_STRING(*vp) || !(str = JS_ValueToString(cx, *vp)))
	    return JS_FALSE;
	name = JS_GetStringBytes(str);
	for (i = 0; i < NUM_EVENTS; i++) {
	    if (!XP_STRCASECMP(event_names[i].lowerName, name))
		event->type = PR_BIT(i);
	}
    }
    else if ((slot==EVENT_X) || (slot==EVENT_LAYERX))
	return JS_ValueToInt32(cx, *vp, &event->x);
    else if ((slot==EVENT_Y) ||	(slot==EVENT_LAYERY))
	return JS_ValueToInt32(cx, *vp, &event->y);
    else if (slot==EVENT_DOCX)
	return JS_ValueToInt32(cx, *vp, &event->docx);
    else if (slot==EVENT_DOCY)
	return JS_ValueToInt32(cx, *vp, &event->docy);
    else if (slot==EVENT_SCREENX)
	return JS_ValueToInt32(cx, *vp, &event->screenx);
    else if (slot==EVENT_SCREENY)
	return JS_ValueToInt32(cx, *vp, &event->screeny);
    else if (slot==EVENT_WHICH)
	return GetUint32(cx, *vp, &event->which);
    else if (slot==EVENT_MODIFIERS)
	GetUint32(cx, *vp, &temp);
	event->modifiers = temp;
	return JS_TRUE;
    /* Win16 hack */
    /*else if (slot==EVENT_DATA) {
	GetUint32(cx, *vp, &temp);
	event->data = temp;
	return JS_TRUE;
    } */

    return JS_TRUE;
}

PR_STATIC_CALLBACK(void)
event_finalize(JSContext *cx, JSObject *obj)
{
    JSEvent *event;

    event = JS_GetPrivate(cx, obj);
    if (!event)
	return;
    DROP_BACK_COUNT(event->decoder);
    JS_free(cx, event);
}

JSClass lm_event_class = {
    "Event", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,  JS_PropertyStub,  event_getProperty,  event_setProperty,
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,     event_finalize
};


static JSBool
InitEventObject(JSContext *cx, JSObject *obj, JSEvent *pEvent)

{
    MochaDecoder *decoder;

    XP_ASSERT(JS_InstanceOf(cx, obj, &lm_event_class, NULL));

    if (!JS_SetPrivate(cx, obj, pEvent)) {
	JS_free(cx, pEvent);
	return JS_FALSE;
    }

    decoder = JS_GetPrivate(cx, JS_GetGlobalObject(cx));
    pEvent->decoder = HOLD_BACK_COUNT(decoder);
    pEvent->saved = JS_TRUE;
    return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
lm_Event(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval)
{
    JSEvent *pEvent;

    if (!JS_InstanceOf(cx, obj, &lm_event_class, argv))
	return JS_FALSE;

    pEvent = JS_malloc(cx, sizeof(JSEvent));
    if (!pEvent)
	return JS_FALSE;
    XP_BZERO(pEvent, sizeof(JSEvent));

    /*  Need to decide what arguments can be used */

    if (!InitEventObject(cx, obj, pEvent))
	return JS_FALSE;
    *rval = OBJECT_TO_JSVAL(obj);
    return JS_TRUE;
}

static JSClass event_receiver_class = {
    "EventReceiver", 0,
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   JS_FinalizeStub
};

PR_STATIC_CALLBACK(JSBool)
EventReceiver(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval)
{
    return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
event_receiver_handle_event(JSContext *cx, JSObject *obj,
			    uint argc, jsval *argv, jsval *rval)
{
    JSObject *eventObj;

    if (argc != 1)
	return JS_TRUE;

    if (!JSVAL_IS_OBJECT(argv[0]) &&
	!JS_ConvertValue(cx, argv[0], JSTYPE_OBJECT, &argv[0]))
	return JS_FALSE;

    eventObj = JSVAL_TO_OBJECT(argv[0]);
    if (!JS_InstanceOf(cx, eventObj, &lm_event_class, argv))
	return JS_FALSE;

    return lm_HandleEvent(cx, obj, eventObj, JSVAL_NULL, rval);
}

static JSFunctionSpec event_receiver_methods[] = {
    {"handleEvent",     event_receiver_handle_event,       1},
    {0}
};

static JSClass event_capturer_class = {
    "EventCapturer", 0,
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   JS_FinalizeStub
};

PR_STATIC_CALLBACK(JSBool)
EventCapturer(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval)
{
    return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
event_capturer_route_event(JSContext *cx, JSObject *obj,
			   uint argc, jsval *argv, jsval *rval)
{
    JSObject *eventObj;
    JSEvent *event;

    if (argc != 1)
	return JS_TRUE;

    if (!JSVAL_IS_OBJECT(argv[0]) &&
	!JS_ConvertValue(cx, argv[0], JSTYPE_OBJECT, &argv[0]))
	return JS_FALSE;

    eventObj = JSVAL_TO_OBJECT(argv[0]);
    if (!(event = JS_GetInstancePrivate (cx, eventObj, &lm_event_class, argv)))
	return JS_FALSE;

    /* Routing objects to themselves causes infinite recursion.
     * And that's a big no-no.
     */
    if (event->object == obj)
	return JS_TRUE;

    if (!event->object || !event->decoder || !event->decoder->window_context)
	return JS_TRUE;
    return lm_FindEventHandler(event->decoder->window_context,
			       event->object, eventObj, JSVAL_NULL,
			       rval);
}

PR_STATIC_CALLBACK(JSBool)
event_capturer_capture_events(JSContext *cx, JSObject *obj,
	     uint argc, jsval *argv, jsval *rval)
{
    return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
event_capturer_release_events(JSContext *cx, JSObject *obj,
	     uint argc, jsval *argv, jsval *rval)
{
    return JS_TRUE;
}

static JSFunctionSpec event_capturer_methods[] = {
    {"routeEvent",	event_capturer_route_event,        1},
    {"captureEvents",   event_capturer_capture_events,	   1},
    {"releaseEvents",   event_capturer_release_events,     1},
    {0}
};

JSBool
lm_InitEventClasses(MochaDecoder *decoder)
{
    JSContext *cx;
    JSObject *prototype, *ctor;

    cx = decoder->js_context;

    prototype = JS_InitClass(cx, decoder->window_object, NULL, &lm_event_class,
			     lm_Event, 0, event_props, NULL, NULL, NULL);

    if (!prototype|| !(ctor = JS_GetConstructor(cx, prototype)))
	return JS_FALSE;

    if (!JS_DefineConstDoubles(cx, ctor, event_constants))
	return JS_FALSE;

    decoder->event_prototype = prototype;

    prototype = JS_InitClass(cx, decoder->window_object, NULL,
			     &event_receiver_class, EventReceiver, 0,
			     NULL, event_receiver_methods, NULL, NULL);

    if (!prototype)
	return JS_FALSE;
    decoder->event_receiver_prototype = prototype;

    prototype = JS_InitClass(cx, decoder->window_object,
			     decoder->event_receiver_prototype,
			     &event_capturer_class, EventCapturer, 0,
			     NULL, event_capturer_methods, NULL, NULL);


    if (!prototype)
	return JS_FALSE;
    decoder->event_capturer_prototype = prototype;
    return JS_TRUE;
}


JSObject *
lm_NewEventObject(MochaDecoder * decoder, JSEvent *pEvent)
{
    JSContext * cx;
    JSObject *obj;

    cx = decoder->js_context;
    obj = JS_NewObject(cx, &lm_event_class, decoder->event_prototype, 0);
    if (!obj)
	return NULL;
    if (!InitEventObject(cx, obj, pEvent))
	return NULL;
    return obj;
}
