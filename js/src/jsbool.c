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
 * JS boolean implementation.
 */
#include "prtypes.h"
#include "jsapi.h"
#include "jsatom.h"
#include "jsbool.h"
#include "jscntxt.h"
#include "jslock.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsstr.h"

static JSClass boolean_class = {
    "Boolean",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   JS_FinalizeStub
};

static JSBool
bool_toString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
	      jsval *rval)
{
    jsval v;
    JSAtom *atom;
    JSString *str;

    if (!JS_InstanceOf(cx, obj, &boolean_class, argv))
	return JS_FALSE;
    JS_LOCK_VOID(cx, v = js_GetSlot(cx, obj, JSSLOT_PRIVATE));
    atom = cx->runtime->atomState.booleanAtoms[JSVAL_TO_BOOLEAN(v) ? 1 : 0];
    str = ATOM_TO_STRING(atom);
    if (!str)
	return JS_FALSE;
    *rval = STRING_TO_JSVAL(str);
    return JS_TRUE;
}

static JSBool
bool_valueOf(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    if (!JS_InstanceOf(cx, obj, &boolean_class, argv))
	return JS_FALSE;
    JS_LOCK_VOID(cx, *rval = js_GetSlot(cx, obj, JSSLOT_PRIVATE));
    return JS_TRUE;
}

static JSFunctionSpec boolean_methods[] = {
    {js_toString_str,	bool_toString,		0},
    {js_valueOf_str,	bool_valueOf,		0},
    {0}
};

#ifdef XP_MAC
#undef Boolean
#define Boolean js_Boolean
#endif

static JSBool
Boolean(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSBool b;
    jsval bval;

    if (argc != 0) {
	if (!JS_ValueToBoolean(cx, argv[0], &b))
	    return JS_FALSE;
	bval = BOOLEAN_TO_JSVAL(b);
    } else {
	bval = JSVAL_FALSE;
    }
    if (obj->map->clasp != &boolean_class) {
	*rval = bval;
	return JS_TRUE;
    }
    if (!js_SetSlot(cx, obj, JSSLOT_PRIVATE, bval))
	return JS_FALSE;
    *rval = OBJECT_TO_JSVAL(obj);
    return JS_TRUE;
}

JSObject *
js_InitBooleanClass(JSContext *cx, JSObject *obj)
{
    JSObject *proto;

    proto = JS_InitClass(cx, obj, NULL, &boolean_class, Boolean, 1,
			NULL, boolean_methods, NULL, NULL);
    if (!proto || !js_SetSlot(cx, proto, JSSLOT_PRIVATE, JSVAL_FALSE))
	return NULL;
    return proto;
}

JSObject *
js_BooleanToObject(JSContext *cx, JSBool b)
{
    JSObject *obj;

    obj = js_NewObject(cx, &boolean_class, NULL, NULL);
    if (!obj)
	return NULL;
    if (!js_SetSlot(cx, obj, JSSLOT_PRIVATE, BOOLEAN_TO_JSVAL(b))) {
	cx->newborn[GCX_OBJECT] = NULL;
	return NULL;
    }
    return obj;
}

JSString *
js_BooleanToString(JSContext *cx, JSBool b)
{
    return ATOM_TO_STRING(cx->runtime->atomState.booleanAtoms[b ? 1 : 0]);
}

JSBool
js_ValueToBoolean(JSContext *cx, jsval v, JSBool *bp)
{
    JSBool b;
    JSObject *obj;
    jsdouble d;

#if defined XP_PC && defined _MSC_VER &&_MSC_VER <= 800
    /* MSVC1.5 coredumps */
    if (!bp)
	return JS_TRUE;
#endif

    /* XXX this should be an if-else chain, but MSVC1.5 really sucks. */
    if (JSVAL_IS_NULL(v) || JSVAL_IS_VOID(v)) {
	/* Must return early to avoid falling thru to JSVAL_IS_OBJECT case. */
	*bp = JS_FALSE;
	return JS_TRUE;
    }
    if (JSVAL_IS_OBJECT(v)) {
	obj = JSVAL_TO_OBJECT(v);
	if (!obj->map->clasp->convert(cx, obj, JSTYPE_BOOLEAN, &v))
	    return JS_FALSE;
	if (!JSVAL_IS_BOOLEAN(v))
	    v = JSVAL_TRUE;		/* non-null object is true */
	b = JSVAL_TO_BOOLEAN(v);
    }
    if (JSVAL_IS_STRING(v)) {
	b = JSVAL_TO_STRING(v)->length ? JS_TRUE : JS_FALSE;
    }
    if (JSVAL_IS_INT(v)) {
	b = JSVAL_TO_INT(v) ? JS_TRUE : JS_FALSE;
    }
    if (JSVAL_IS_DOUBLE(v)) {
	d = *JSVAL_TO_DOUBLE(v);
	b = (!JSDOUBLE_IS_NaN(d) && d != 0) ? JS_TRUE : JS_FALSE;
    }
    if (JSVAL_IS_BOOLEAN(v)) {
	b = JSVAL_TO_BOOLEAN(v);
    }
    *bp = b;
    return JS_TRUE;
}
