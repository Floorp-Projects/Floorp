/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
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

/*
 * JS boolean implementation.
 */
#include "jsstddef.h"
#include "jstypes.h"
#include "jsutil.h" /* Added by JSIFY */
#include "jsapi.h"
#include "jsatom.h"
#include "jsbool.h"
#include "jscntxt.h"
#include "jsconfig.h"
#include "jsinterp.h"
#include "jslock.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsstr.h"

static JSClass boolean_class = {
    "Boolean",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   JS_FinalizeStub,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

#if JS_HAS_TOSOURCE
#include "jsprf.h"

static JSBool
bool_toSource(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
	      jsval *rval)
{
    jsval v;
    char buf[32];
    JSString *str;

    if (!JS_InstanceOf(cx, obj, &boolean_class, argv))
	return JS_FALSE;
    v = OBJ_GET_SLOT(cx, obj, JSSLOT_PRIVATE);
    if (!JSVAL_IS_BOOLEAN(v))
	return js_obj_toSource(cx, obj, argc, argv, rval);
    JS_snprintf(buf, sizeof buf, "(new %s(%s))",
		boolean_class.name,
		js_boolean_str[JSVAL_TO_BOOLEAN(v) ? 1 : 0]);
    str = JS_NewStringCopyZ(cx, buf);
    if (!str)
	return JS_FALSE;
    *rval = STRING_TO_JSVAL(str);
    return JS_TRUE;
}
#endif

static JSBool
bool_toString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
	      jsval *rval)
{
    jsval v;
    JSAtom *atom;
    JSString *str;

    if (!JS_InstanceOf(cx, obj, &boolean_class, argv))
	return JS_FALSE;
    v = OBJ_GET_SLOT(cx, obj, JSSLOT_PRIVATE);
    if (!JSVAL_IS_BOOLEAN(v))
	return js_obj_toString(cx, obj, argc, argv, rval);
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
    *rval = OBJ_GET_SLOT(cx, obj, JSSLOT_PRIVATE);
    return JS_TRUE;
}

static JSFunctionSpec boolean_methods[] = {
#if JS_HAS_TOSOURCE
    {js_toSource_str,   bool_toSource,          0,0,0},
#endif
    {js_toString_str,	bool_toString,		0,0,0},
    {js_valueOf_str,	bool_valueOf,		0,0,0},
    {0,0,0,0,0}
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
	if (!js_ValueToBoolean(cx, argv[0], &b))
	    return JS_FALSE;
	bval = BOOLEAN_TO_JSVAL(b);
    } else {
	bval = JSVAL_FALSE;
    }
    if (!(cx->fp->flags & JSFRAME_CONSTRUCTING)) {
	*rval = bval;
	return JS_TRUE;
    }
    OBJ_SET_SLOT(cx, obj, JSSLOT_PRIVATE, bval);
    return JS_TRUE;
}

JSObject *
js_InitBooleanClass(JSContext *cx, JSObject *obj)
{
    JSObject *proto;

    proto = JS_InitClass(cx, obj, NULL, &boolean_class, Boolean, 1,
			NULL, boolean_methods, NULL, NULL);
    if (!proto)
	return NULL;
    OBJ_SET_SLOT(cx, proto, JSSLOT_PRIVATE, JSVAL_FALSE);
    return proto;
}

JSObject *
js_BooleanToObject(JSContext *cx, JSBool b)
{
    JSObject *obj;

    obj = js_NewObject(cx, &boolean_class, NULL, NULL);
    if (!obj)
	return NULL;
    OBJ_SET_SLOT(cx, obj, JSSLOT_PRIVATE, BOOLEAN_TO_JSVAL(b));
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
    jsdouble d;

#if defined XP_PC && defined _MSC_VER && _MSC_VER <= 800
    /* MSVC1.5 coredumps */
    if (!bp)
	return JS_TRUE;
    /* This should be an if-else chain, but MSVC1.5 crashes if it is. */
#define ELSE
#else
#define ELSE else
#endif

    if (JSVAL_IS_NULL(v) || JSVAL_IS_VOID(v)) {
	/* Must return early to avoid falling thru to JSVAL_IS_OBJECT case. */
	*bp = JS_FALSE;
	return JS_TRUE;
    }
    if (JSVAL_IS_OBJECT(v)) {
	if (!JSVERSION_IS_ECMA(cx->version)) {
	    if (!OBJ_DEFAULT_VALUE(cx, JSVAL_TO_OBJECT(v), JSTYPE_BOOLEAN, &v))
		return JS_FALSE;
	    if (!JSVAL_IS_BOOLEAN(v))
		v = JSVAL_TRUE;		/* non-null object is true */
	    b = JSVAL_TO_BOOLEAN(v);
	} else {
	    b = JS_TRUE;
	}
    } ELSE
    if (JSVAL_IS_STRING(v)) {
	b = JSVAL_TO_STRING(v)->length ? JS_TRUE : JS_FALSE;
    } ELSE
    if (JSVAL_IS_INT(v)) {
	b = JSVAL_TO_INT(v) ? JS_TRUE : JS_FALSE;
    } ELSE
    if (JSVAL_IS_DOUBLE(v)) {
	d = *JSVAL_TO_DOUBLE(v);
	b = (!JSDOUBLE_IS_NaN(d) && d != 0) ? JS_TRUE : JS_FALSE;
    } ELSE
#if defined XP_PC && defined _MSC_VER && _MSC_VER <= 800
    if (JSVAL_IS_BOOLEAN(v)) {
	b = JSVAL_TO_BOOLEAN(v);
    }
#else
    {
        JS_ASSERT(JSVAL_IS_BOOLEAN(v));
	b = JSVAL_TO_BOOLEAN(v);
    }
#endif

#undef ELSE
    *bp = b;
    return JS_TRUE;
}
