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
 * JS object implementation.
 */
#include "jsstddef.h"
#include <stdlib.h>
#include <string.h>
#include "jstypes.h"
#include "jsarena.h" /* Added by JSIFY */
#include "jsutil.h" /* Added by JSIFY */
#include "jshash.h" /* Added by JSIFY */
#include "jsprf.h"
#include "jsapi.h"
#include "jsatom.h"
#include "jsbool.h"
#include "jscntxt.h"
#include "jsconfig.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jsinterp.h"
#include "jslock.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsscope.h"
#include "jsscript.h"
#include "jsstr.h"

#if JS_HAS_OBJ_WATCHPOINT
#include "jsdbgapi.h"
#endif

#ifdef JS_THREADSAFE
#define NATIVE_DROP_PROPERTY js_DropProperty

extern void
js_DropProperty(JSContext *cx, JSObject *obj, JSProperty *prop);
#else
#define NATIVE_DROP_PROPERTY NULL
#endif

#ifdef XP_MAC
#pragma export on
#endif

JS_FRIEND_DATA(JSObjectOps) js_ObjectOps = {
    js_NewObjectMap,    js_DestroyObjectMap,
#if defined JS_THREADSAFE && defined DEBUG
    _js_LookupProperty, js_DefineProperty,
#else
    js_LookupProperty,  js_DefineProperty,
#endif
    js_GetProperty,     js_SetProperty,
    js_GetAttributes,   js_SetAttributes,
    js_DeleteProperty,  js_DefaultValue,
    js_Enumerate,       js_CheckAccess,
    NULL,               NATIVE_DROP_PROPERTY,
    js_Call,            js_Construct,
    NULL,               js_HasInstance
};

#ifdef XP_MAC
#pragma export off
#endif

JSClass js_ObjectClass = {
    js_Object_str,
    0,
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   JS_FinalizeStub
};

#if JS_HAS_OBJ_PROTO_PROP

static JSBool
obj_getSlot(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

JS_STATIC_DLL_CALLBACK(JSBool)
obj_setSlot(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

static JSBool
obj_getCount(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

static JSPropertySpec object_props[] = {
    /* These two must come first; see object_props[slot].name usage below. */
    {js_proto_str, JSSLOT_PROTO,  JSPROP_PERMANENT, obj_getSlot,  obj_setSlot},
    {js_parent_str,JSSLOT_PARENT, JSPROP_PERMANENT, obj_getSlot,  obj_setSlot},
    {js_count_str, 0,             JSPROP_PERMANENT, obj_getCount, obj_getCount},
    {0}
};

static JSBool
obj_getSlot(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    jsint slot;
    JSAccessMode mode;
    uintN attrs;

    slot = JSVAL_TO_INT(id);
    if (id == INT_TO_JSVAL(JSSLOT_PROTO)) {
	id = (jsid)cx->runtime->atomState.protoAtom;
	mode = JSACC_PROTO;
    } else {
	id = (jsid)cx->runtime->atomState.parentAtom;
	mode = JSACC_PARENT;
    }
    if (!OBJ_CHECK_ACCESS(cx, obj, id, mode, vp, &attrs))
	return JS_FALSE;
    *vp = OBJ_GET_SLOT(cx, obj, slot);
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
obj_setSlot(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSObject *obj2;
    jsint slot;

    if (!JSVAL_IS_OBJECT(*vp))
	return JS_TRUE;
    obj2 = JSVAL_TO_OBJECT(*vp);
    slot = JSVAL_TO_INT(id);
    while (obj2) {
	if (obj2 == obj) {
	    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
				 JSMSG_CYCLIC_VALUE, object_props[slot].name);
	    return JS_FALSE;
	}
	obj2 = JSVAL_TO_OBJECT(OBJ_GET_SLOT(cx, obj2, slot));
    }
    OBJ_SET_SLOT(cx, obj, slot, *vp);
    return JS_TRUE;
}

static JSBool
obj_getCount(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    jsval iter_state;
    jsid num_properties;

    iter_state = JSVAL_NULL;

    /* Get the number of properties to enumerate. */
    if (!OBJ_ENUMERATE(cx, obj, JSENUMERATE_INIT, &iter_state, &num_properties))
	goto error;
    if (!JSVAL_IS_INT(num_properties)) {
	JS_ASSERT(0);
	goto error;
    }
    *vp = num_properties;
    return JS_TRUE;

error:
    if (iter_state != JSVAL_NULL)
	OBJ_ENUMERATE(cx, obj, JSENUMERATE_DESTROY, &iter_state, 0);
    return JS_FALSE;
}

#else  /* !JS_HAS_OBJ_PROTO_PROP */

#define object_props NULL

#endif /* !JS_HAS_OBJ_PROTO_PROP */

JS_STATIC_DLL_CALLBACK(JSHashNumber)
js_hash_object(const void *key)
{
    return (JSHashNumber)key >> JSVAL_TAGBITS;
}

static JSHashEntry *
MarkSharpObjects(JSContext *cx, JSObject *obj, JSIdArray **idap)
{
    JSSharpObjectMap *map;
    JSHashTable *table;
    JSHashNumber hash;
    JSHashEntry **hep, *he;
    jsatomid sharpid;
    JSIdArray *ida;
    JSBool ok;
    jsint i, length;
    jsval val;

    map = &cx->sharpObjectMap;
    table = map->table;
    hash = js_hash_object(obj);
    hep = JS_HashTableRawLookup(table, hash, obj);
    he = *hep;
    if (!he) {
	sharpid = 0;
	he = JS_HashTableRawAdd(table, hep, hash, obj, (void *)sharpid);
	if (!he) {
	    JS_ReportOutOfMemory(cx);
	    return NULL;
	}
	ida = JS_Enumerate(cx, obj);
	if (!ida)
	    return NULL;
	ok = JS_TRUE;
	for (i = 0, length = ida->length; i < length; i++) {
	    ok = OBJ_GET_PROPERTY(cx, obj, ida->vector[i], &val);
	    if (!ok)
		break;
	    if (!JSVAL_IS_PRIMITIVE(val) &&
		!MarkSharpObjects(cx, JSVAL_TO_OBJECT(val), NULL)) {
		ok = JS_FALSE;
		break;
	    }
	}
	if (!ok || !idap)
	    JS_DestroyIdArray(cx, ida);
	if (!ok)
	    return NULL;
    } else {
	sharpid = (jsatomid) he->value;
	if (sharpid == 0) {
	    sharpid = ++map->sharpgen << 1;
	    he->value = (void *) sharpid;
	}
	ida = NULL;
    }
    if (idap)
	*idap = ida;
    return he;
}

JSHashEntry *
js_EnterSharpObject(JSContext *cx, JSObject *obj, JSIdArray **idap,
		    jschar **sp)
{
    JSSharpObjectMap *map;
    JSHashTable *table;
    JSIdArray *ida;
    JSHashNumber hash;
    JSHashEntry *he, **hep;
    jsatomid sharpid;
    char buf[20];
    size_t len;

    map = &cx->sharpObjectMap;
    table = map->table;
    if (!table) {
	table = JS_NewHashTable(8, js_hash_object, JS_CompareValues,
				JS_CompareValues, NULL, NULL);
	if (!table) {
	    JS_ReportOutOfMemory(cx);
	    return NULL;
	}
	map->table = table;
    }

    ida = NULL;
    if (map->depth == 0) {
	he = MarkSharpObjects(cx, obj, &ida);
	if (!he)
	    return NULL;
	JS_ASSERT((((jsatomid) he->value) & SHARP_BIT) == 0);
	if (!idap) {
	    JS_DestroyIdArray(cx, ida);
	    ida = NULL;
	}
    } else {
	hash = js_hash_object(obj);
	hep = JS_HashTableRawLookup(table, hash, obj);
	he = *hep;

	/*
	 * It's possible that the value of a property has changed from the
	 * first time the object's properties are traversed (when the property
	 * ids are entered into the hash table) to the second (when they are
	 * converted to strings), i.e. the getProperty() call is not
	 * idempotent.
	 */
	if (!he) {
	    he = JS_HashTableRawAdd(table, hep, hash, obj, (void *)0);
	    if (!he)
		JS_ReportOutOfMemory(cx);
	    *sp = NULL;
            sharpid = 0;
	    goto out;
	}
    }

    sharpid = (jsatomid) he->value;
    if (sharpid == 0) {
	*sp = NULL;
    } else {
	len = JS_snprintf(buf, sizeof buf, "#%u%c",
			  sharpid >> 1, (sharpid & SHARP_BIT) ? '#' : '=');
	*sp = js_InflateString(cx, buf, len);
	if (!*sp) {
	    if (ida)
		JS_DestroyIdArray(cx, ida);
	    return NULL;
	}
    }

out:
    if ((sharpid & SHARP_BIT) == 0) {
	if (idap && !ida) {
	    ida = JS_Enumerate(cx, obj);
	    if (!ida) {
		if (*sp) {
		    JS_free(cx, *sp);
		    *sp = NULL;
		}
		return NULL;
	    }
	}
	map->depth++;
    }

    if (idap)
	*idap = ida;
    return he;
}

void
js_LeaveSharpObject(JSContext *cx, JSIdArray **idap)
{
    JSSharpObjectMap *map;
    JSIdArray *ida;

    map = &cx->sharpObjectMap;
    JS_ASSERT(map->depth > 0);
    if (--map->depth == 0) {
	map->sharpgen = 0;
	JS_HashTableDestroy(map->table);
	map->table = NULL;
    }
    if (idap) {
	ida = *idap;
	if (ida) {
	    JS_DestroyIdArray(cx, ida);
	    *idap = NULL;
	}
    }
}

#define OBJ_TOSTRING_EXTRA	2	/* for 2 GC roots */

#if JS_HAS_INITIALIZERS || JS_HAS_TOSOURCE
JSBool
js_obj_toSource(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
    JSHashEntry *he;
    JSIdArray *ida;
    jschar *chars, *ochars, *vchars, *vsharp;
    size_t nchars, vlength, vsharplength;
    JSBool ok;
    char *comma;
    jsint i, length;
    jsid id;
    jsval val;
    JSString *idstr, *valstr, *str;

    he = js_EnterSharpObject(cx, obj, &ida, &chars);
    if (!he)
	return JS_FALSE;
    if (IS_SHARP(he)) {	/* we didn't enter -- obj is already sharp */
	JS_ASSERT(!ida);
#if JS_HAS_SHARP_VARS
	nchars = js_strlen(chars);
#else
	chars[0] = '{';
	chars[1] = '}';
	chars[2] = 0;
	nchars = 2;
#endif
	goto make_string;
    }
    JS_ASSERT(ida);
    ok = JS_TRUE;

    /* Allocate 2 + 1 for "{}" and the terminator. */
    if (!chars) {
	chars = malloc((2 + 1) * sizeof(jschar));
	nchars = 0;
	if (!chars)
	    goto error;
    } else {
	MAKE_SHARP(he);
	nchars = js_strlen(chars);
	chars = realloc((ochars = chars), (nchars + 2 + 1) * sizeof(jschar));
	if (!chars) {
	    free(ochars);
	    goto error;
	}
    }
    chars[nchars++] = '{';

    comma = NULL;

    for (i = 0, length = ida->length; i < length; i++) {
	/* Get strings for id and val and GC-root them via argv. */
	id = ida->vector[i];
	ok = OBJ_GET_PROPERTY(cx, obj, id, &val);
	if (!ok)
	    goto error;

	/* Convert id to a jsval and then to a string. */
	id = js_IdToValue(id);
	idstr = js_ValueToString(cx, id);
	if (!idstr) {
	    ok = JS_FALSE;
	    goto error;
	}
	argv[0] = STRING_TO_JSVAL(idstr);

	/* If id is a non-identifier string, it needs to be quoted. */
	if (JSVAL_IS_STRING(id) && !js_IsIdentifier(idstr)) {
	    idstr = js_QuoteString(cx, idstr, '\'');
	    if (!idstr) {
		ok = JS_FALSE;
		goto error;
	    }
	    argv[0] = STRING_TO_JSVAL(idstr);
	}

	/* Convert val to its canonical source form. */
	valstr = js_ValueToSource(cx, val);
	if (!valstr) {
	    ok = JS_FALSE;
	    goto error;
	}
	argv[1] = STRING_TO_JSVAL(valstr);
	vchars = valstr->chars;
	vlength = valstr->length;

	/* If val is a non-sharp object, consider sharpening it. */
	vsharp = NULL;
	vsharplength = 0;
#if JS_HAS_SHARP_VARS
	if (!JSVAL_IS_PRIMITIVE(val) && vchars[0] != '#') {
	    he = js_EnterSharpObject(cx, JSVAL_TO_OBJECT(val), NULL, &vsharp);
	    if (!he) {
		ok = JS_FALSE;
		goto error;
	    }
	    if (IS_SHARP(he)) {
		vchars = vsharp;
		vlength = js_strlen(vchars);
	    } else {
		if (vsharp) {
		    vsharplength = js_strlen(vsharp);
		    MAKE_SHARP(he);
		}
		js_LeaveSharpObject(cx, NULL);
	    }
	}
#endif

	/* Allocate 1 + 1 at end for closing brace and terminating 0. */
	chars = realloc((ochars = chars),
			(nchars + (comma ? 2 : 0) +
			 idstr->length + 1 + vsharplength + vlength +
			 1 + 1) * sizeof(jschar));
	if (!chars) {
	    /* Save code space on error: let JS_free ignore null vsharp. */
	    JS_free(cx, vsharp);
	    free(ochars);
	    goto error;
	}

	if (comma) {
	    chars[nchars++] = comma[0];
	    chars[nchars++] = comma[1];
	}
	comma = ", ";

	js_strncpy(&chars[nchars], idstr->chars, idstr->length);
	nchars += idstr->length;
	chars[nchars++] = ':';

	if (vsharplength) {
	    js_strncpy(&chars[nchars], vsharp, vsharplength);
	    nchars += vsharplength;
	}
	js_strncpy(&chars[nchars], vchars, vlength);
	nchars += vlength;

	if (vsharp)
	    JS_free(cx, vsharp);
    }

    if (chars) {
	chars[nchars++] = '}';
	chars[nchars] = 0;
    }

error:
    js_LeaveSharpObject(cx, &ida);

    if (!ok) {
	if (chars)
	    free(chars);
	return ok;
    }

    if (!chars) {
	JS_ReportOutOfMemory(cx);
	return JS_FALSE;
    }
  make_string:
    str = js_NewString(cx, chars, nchars, 0);
    if (!str) {
	free(chars);
	return JS_FALSE;
    }
    *rval = STRING_TO_JSVAL(str);
    return JS_TRUE;
}
#endif /* JS_HAS_INITIALIZERS || JS_HAS_TOSOURCE */

JSBool
js_obj_toString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
    jschar *chars;
    size_t nchars;
    char *clazz, *prefix;
    JSString *str;

#if JS_HAS_INITIALIZERS
    if (cx->version == JSVERSION_1_2)
	return js_obj_toSource(cx, obj, argc, argv, rval);
#endif

    clazz = OBJ_GET_CLASS(cx, obj)->name;
    nchars = 9 + strlen(clazz);		/* 9 for "[object ]" */
    chars = JS_malloc(cx, (nchars + 1) * sizeof(jschar));
    if (!chars)
	return JS_FALSE;

    prefix = "[object ";
    nchars = 0;
    while ((chars[nchars] = (jschar)*prefix) != 0)
	nchars++, prefix++;
    while ((chars[nchars] = (jschar)*clazz) != 0)
	nchars++, clazz++;
    chars[nchars++] = ']';
    chars[nchars] = 0;

    str = js_NewString(cx, chars, nchars, 0);
    if (!str) {
	JS_free(cx, chars);
	return JS_FALSE;
    }
    *rval = STRING_TO_JSVAL(str);
    return JS_TRUE;
}

static JSBool
obj_valueOf(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    *rval = OBJECT_TO_JSVAL(obj);
    return JS_TRUE;
}

static JSBool
obj_eval(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSStackFrame *caller;
    JSObject *scopeobj;
    JSString *str;
    const char *file;
    uintN line;
    JSPrincipals *principals;
    JSScript *script;
    JSBool ok;
#if JS_HAS_EVAL_THIS_SCOPE
    JSObject *callerScopeChain;
    JSBool implicitWith;
#endif

    caller = cx->fp->down;
    implicitWith = JS_FALSE; /* Unnecessary init to kill gcc warning */

    if ((cx->version == JSVERSION_DEFAULT || cx->version >= JSVERSION_1_4)
            && (*caller->pc != JSOP_CALLSPECIAL)) {
	JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
			     JSMSG_BAD_INDIRECT_CALL, js_eval_str);
        return JS_FALSE;
    }

    if (!JSVAL_IS_STRING(argv[0])) {
	*rval = argv[0];
	return JS_TRUE;
    }

#if JS_HAS_SCRIPT_OBJECT
    /*
     * Script.prototype.compile/exec and Object.prototype.eval all take an
     * optional trailing argument that overrides the scope object.
     */
    scopeobj = NULL;
    if (argc >= 2) {
	if (!js_ValueToObject(cx, argv[1], &scopeobj))
	    return JS_FALSE;
	argv[1] = OBJECT_TO_JSVAL(scopeobj);
    }
    if (!scopeobj)
#endif
    {
#if JS_HAS_EVAL_THIS_SCOPE
	/* If obj.eval(str), emulate 'with (obj) eval(str)' in the caller. */
	callerScopeChain = caller->scopeChain;
	implicitWith = (callerScopeChain != obj &&
			(OBJ_GET_CLASS(cx, callerScopeChain) != &js_WithClass ||
			 OBJ_GET_PROTO(cx, callerScopeChain) != obj));
	if (implicitWith) {
	    scopeobj = js_NewObject(cx, &js_WithClass, obj, callerScopeChain);
	    if (!scopeobj)
		return JS_FALSE;
	    caller->scopeChain = scopeobj;
	}
	/* From here on, control must exit through label out with ok set. */
#endif

#if JS_BUG_EVAL_THIS_SCOPE
	/* An old version used the object in which eval was found for scope. */
	scopeobj = obj;
#else
	/* Compile using caller's current scope object (might be a function). */
	scopeobj = caller->scopeChain;
#endif
    }

    str = JSVAL_TO_STRING(argv[0]);
    if (caller->script) {
	file = caller->script->filename;
	line = js_PCToLineNumber(caller->script, caller->pc);
	principals = caller->script->principals;
    } else {
	file = NULL;
	line = 0;
	principals = NULL;
    }
    script = JS_CompileUCScriptForPrincipals(cx, scopeobj, principals,
					     str->chars, str->length,
					     file, line);
    if (!script) {
	ok = JS_FALSE;
	goto out;
    }

#if JS_HAS_SCRIPT_OBJECT
    if (argc < 2)
#endif
#if !JS_BUG_EVAL_THIS_SCOPE
    {
	/* Execute using caller's new scope object (might be a Call object). */
	scopeobj = caller->scopeChain;
    }
#endif
    ok = js_Execute(cx, scopeobj, script, caller->fun, caller, JS_FALSE, rval);
    JS_DestroyScript(cx, script);

out:
#if JS_HAS_EVAL_THIS_SCOPE
    /* Restore OBJ_GET_PARENT(scopeobj) not callerScopeChain in case of Call. */
    if (implicitWith)
	caller->scopeChain = OBJ_GET_PARENT(cx, scopeobj);
#endif
    return ok;
}

#if JS_HAS_OBJ_WATCHPOINT

static JSBool
obj_watch_handler(JSContext *cx, JSObject *obj, jsval id, jsval old, jsval *nvp,
		  void *closure)
{
    JSObject *funobj;
    jsval argv[3];

    funobj = closure;
    argv[0] = id;
    argv[1] = old;
    argv[2] = *nvp;
    return js_CallFunctionValue(cx, obj, OBJECT_TO_JSVAL(funobj), 3, argv, nvp);
}

static JSBool
obj_watch(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSFunction *fun;
    jsval userid, value;
    jsid symid;
    JSAtom *atom;
    uintN attrs;

    fun = js_ValueToFunction(cx, &argv[1], JS_FALSE);
    if (!fun)
	return JS_FALSE;
    argv[1] = OBJECT_TO_JSVAL(fun->object);

    /* Compute the unique int/atom symbol id needed by js_LookupProperty. */
    userid = argv[0];
    if (JSVAL_IS_INT(userid)) {
	symid = (jsid)userid;
	atom = NULL;
    } else {
	atom = js_ValueToStringAtom(cx, userid);
	if (!atom)
	    return JS_FALSE;
	symid = (jsid)atom;
    }

    if (!OBJ_CHECK_ACCESS(cx, obj, symid, JSACC_WATCH, &value, &attrs))
	return JS_FALSE;
    if (attrs & JSPROP_READONLY)
	return JS_TRUE;
    return JS_SetWatchPoint(cx, obj, userid, obj_watch_handler, fun->object);
}

static JSBool
obj_unwatch(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JS_ClearWatchPoint(cx, obj, argv[0], NULL, NULL);
    return JS_TRUE;
}

#endif /* JS_HAS_OBJ_WATCHPOINT */

#if JS_HAS_NEW_OBJ_METHODS
/*
 * Prototype and property query methods, to complement the 'in' and
 * 'instanceof' operators.
 */

/* Proposed ECMA 15.2.4.5. */
static JSBool
obj_hasOwnProperty(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                   jsval *rval)
{
    JSObject *obj2;
    JSProperty *prop;
    JSAtom *atom;
    
    atom = js_ValueToStringAtom(cx, *argv);
    if (atom == NULL || !OBJ_LOOKUP_PROPERTY(cx, obj, (jsid)atom, &obj2, &prop))
        return JS_FALSE;
    *rval = (!prop || obj2 != obj) ? JSVAL_FALSE : JSVAL_TRUE;
    if (prop)
        OBJ_DROP_PROPERTY(cx, obj2, prop);
    return JS_TRUE;
}

/* Proposed ECMA 15.2.4.6. */
static JSBool
obj_isPrototypeOf(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                  jsval *rval)
{
    JSBool b;

    if (!js_IsDelegate(cx, obj, *argv, &b))
        return JS_FALSE;
    *rval = b ? JSVAL_TRUE : JSVAL_FALSE;
    return JS_TRUE;
}

/* Proposed ECMA 15.2.4.7. */
static JSBool
obj_propertyIsEnumerable(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                         jsval *rval)
{
    JSObject *obj2;
    JSProperty *prop;
    JSAtom *atom;
    uintN attrs;
    JSBool ok;
    
    atom = js_ValueToStringAtom(cx, *argv);
    if (atom == NULL || !OBJ_LOOKUP_PROPERTY(cx, obj, (jsid)atom, &obj2, &prop))
        return JS_FALSE;
    if (!prop) {
        *rval = JSVAL_FALSE;
        return JS_TRUE;
    }
    ok = OBJ_GET_ATTRIBUTES(cx, obj2, (jsid)atom, prop, &attrs);
    if (ok)
        *rval = (attrs & JSPROP_ENUMERATE) != 0 ? JSVAL_TRUE : JSVAL_FALSE;
    OBJ_DROP_PROPERTY(cx, obj2, prop);
    return ok;
}
#endif /* JS_HAS_NEW_OBJ_METHODS */

static JSFunctionSpec object_methods[] = {
#if JS_HAS_TOSOURCE
    {js_toSource_str,   js_obj_toSource,        0, 0, OBJ_TOSTRING_EXTRA},
#endif
    {js_toString_str,	js_obj_toString,	0, 0, OBJ_TOSTRING_EXTRA},
    {js_valueOf_str,	obj_valueOf,		0},
    {js_eval_str,	obj_eval,		1, 0, 1},
#if JS_HAS_OBJ_WATCHPOINT
    {"watch",           obj_watch,              2},
    {"unwatch",         obj_unwatch,            1},
#endif
#if JS_HAS_NEW_OBJ_METHODS
    {"hasOwnProperty",  obj_hasOwnProperty,     1},
    {"isPrototypeOf",   obj_isPrototypeOf,      1},
    {"propertyIsEnumerable", obj_propertyIsEnumerable, 1},
#endif
    {0}
};

static JSBool
Object(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    if (argc == 0) {
	/* Trigger logic below to construct a blank object. */
	obj = NULL;
    } else {
	/* If argv[0] is null or undefined, obj comes back null. */
	if (!js_ValueToObject(cx, argv[0], &obj))
	    return JS_FALSE;
    }
    if (!obj) {
	JS_ASSERT(!argc || JSVAL_IS_NULL(argv[0]) || JSVAL_IS_VOID(argv[0]));
	if (cx->fp->constructing)
	    return JS_TRUE;
	obj = js_NewObject(cx, &js_ObjectClass, NULL, NULL);
	if (!obj)
	    return JS_FALSE;
    }
    *rval = OBJECT_TO_JSVAL(obj);
    return JS_TRUE;
}

/*
 * ObjectOps and Class for with-statement stack objects.
 */
static JSBool
with_LookupProperty(JSContext *cx, JSObject *obj, jsid id, JSObject **objp,
		    JSProperty **propp
#if defined JS_THREADSAFE && defined DEBUG
		    , const char *file, uintN line
#endif
		    )
{
    JSObject *proto = OBJ_GET_PROTO(cx, obj);
    if (!proto)
	return js_LookupProperty(cx, obj, id, objp, propp);
    return OBJ_LOOKUP_PROPERTY(cx, proto, id, objp, propp);
}

static JSBool
with_GetProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    JSObject *proto = OBJ_GET_PROTO(cx, obj);
    if (!proto)
	return js_GetProperty(cx, obj, id, vp);
    return OBJ_GET_PROPERTY(cx, proto, id, vp);
}

static JSBool
with_SetProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    JSObject *proto = OBJ_GET_PROTO(cx, obj);
    if (!proto)
	return js_SetProperty(cx, obj, id, vp);
    return OBJ_SET_PROPERTY(cx, proto, id, vp);
}

static JSBool
with_GetAttributes(JSContext *cx, JSObject *obj, jsid id, JSProperty *prop,
		   uintN *attrsp)
{
    JSObject *proto = OBJ_GET_PROTO(cx, obj);
    if (!proto)
	return js_GetAttributes(cx, obj, id, prop, attrsp);
    return OBJ_GET_ATTRIBUTES(cx, proto, id, prop, attrsp);
}

static JSBool
with_SetAttributes(JSContext *cx, JSObject *obj, jsid id, JSProperty *prop,
		   uintN *attrsp)
{
    JSObject *proto = OBJ_GET_PROTO(cx, obj);
    if (!proto)
	return js_SetAttributes(cx, obj, id, prop, attrsp);
    return OBJ_SET_ATTRIBUTES(cx, proto, id, prop, attrsp);
}

static JSBool
with_DeleteProperty(JSContext *cx, JSObject *obj, jsid id, jsval *rval)
{
    JSObject *proto = OBJ_GET_PROTO(cx, obj);
    if (!proto)
	return js_DeleteProperty(cx, obj, id, rval);
    return OBJ_DELETE_PROPERTY(cx, proto, id, rval);
}

static JSBool
with_DefaultValue(JSContext *cx, JSObject *obj, JSType hint, jsval *vp)
{
    JSObject *proto = OBJ_GET_PROTO(cx, obj);
    if (!proto)
	return js_DefaultValue(cx, obj, hint, vp);
    return OBJ_DEFAULT_VALUE(cx, proto, hint, vp);
}

static JSBool
with_Enumerate(JSContext *cx, JSObject *obj, JSIterateOp enum_op,
	       jsval *statep, jsid *idp)
{
    JSObject *proto = OBJ_GET_PROTO(cx, obj);
    if (!proto)
	return js_Enumerate(cx, obj, enum_op, statep, idp);
    return OBJ_ENUMERATE(cx, proto, enum_op, statep, idp);
}

static JSBool
with_CheckAccess(JSContext *cx, JSObject *obj, jsid id, JSAccessMode mode,
		 jsval *vp, uintN *attrsp)
{
    JSObject *proto = OBJ_GET_PROTO(cx, obj);
    if (!proto)
	return js_CheckAccess(cx, obj, id, mode, vp, attrsp);
    return OBJ_CHECK_ACCESS(cx, proto, id, mode, vp, attrsp);
}

static JSObject *
with_ThisObject(JSContext *cx, JSObject *obj)
{
    JSObject *proto = OBJ_GET_PROTO(cx, obj);
    if (!proto)
	return obj;
    return OBJ_THIS_OBJECT(cx, proto);
}

JS_FRIEND_DATA(JSObjectOps) js_WithObjectOps = {
    js_NewObjectMap,        js_DestroyObjectMap,
    with_LookupProperty,    js_DefineProperty,
    with_GetProperty,       with_SetProperty,
    with_GetAttributes,     with_SetAttributes,
    with_DeleteProperty,    with_DefaultValue,
    with_Enumerate,         with_CheckAccess,
    with_ThisObject,        NATIVE_DROP_PROPERTY
};

static JSObjectOps *
with_getObjectOps(JSContext *cx, JSClass *clasp)
{
    return &js_WithObjectOps;
}

JSClass js_WithClass = {
    "With",
    0,
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   JS_FinalizeStub,
    with_getObjectOps
};

#if JS_HAS_OBJ_PROTO_PROP
static JSBool
With(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSObject *parent, *proto;
    jsval v;

    if (!cx->fp->constructing) {
	obj = js_NewObject(cx, &js_WithClass, NULL, NULL);
	if (!obj)
	    return JS_FALSE;
	*rval = OBJECT_TO_JSVAL(obj);
    }

    parent = cx->fp->scopeChain;
    if (argc > 0) {
	if (!js_ValueToObject(cx, argv[0], &proto))
	    return JS_FALSE;
	v = OBJECT_TO_JSVAL(proto);
	if (!obj_setSlot(cx, obj, INT_TO_JSVAL(JSSLOT_PROTO), &v))
	    return JS_FALSE;
	if (argc > 1) {
	    if (!js_ValueToObject(cx, argv[1], &parent))
		return JS_FALSE;
	}
    }
    v = OBJECT_TO_JSVAL(parent);
    return obj_setSlot(cx, obj, INT_TO_JSVAL(JSSLOT_PARENT), &v);
}
#endif

JSObject *
js_InitObjectClass(JSContext *cx, JSObject *obj)
{
    JSObject *proto;

#if JS_HAS_SHARP_VARS
    JS_ASSERT(sizeof(jsatomid) * JS_BITS_PER_BYTE >= ATOM_INDEX_LIMIT_LOG2 + 1);
#endif

    proto = JS_InitClass(cx, obj, NULL, &js_ObjectClass, Object, 1,
			 object_props, object_methods, NULL, NULL);
#if JS_HAS_OBJ_PROTO_PROP
    if (!JS_InitClass(cx, obj, NULL, &js_WithClass, With, 0,
		      NULL, NULL, NULL, NULL)) {
	return NULL;
    }
#endif
    return proto;
}

void
js_InitObjectMap(JSObjectMap *map, jsrefcount nrefs, JSObjectOps *ops,
		 JSClass *clasp)
{
    map->nrefs = nrefs;
    map->ops = ops;
    map->nslots = 0;
    map->freeslot = JSSLOT_FREE(clasp);
}

JSObjectMap *
js_NewObjectMap(JSContext *cx, jsrefcount nrefs, JSObjectOps *ops,
		JSClass *clasp, JSObject *obj)
{
    return (JSObjectMap *) js_NewScope(cx, nrefs, ops, clasp, obj);
}

void
js_DestroyObjectMap(JSContext *cx, JSObjectMap *map)
{
    js_DestroyScope(cx, (JSScope *)map);
}

JSObjectMap *
js_HoldObjectMap(JSContext *cx, JSObjectMap *map)
{
    JS_ASSERT(map->nrefs >= 0);
    JS_ATOMIC_ADDREF(&map->nrefs, 1);
    return map;
}

JSObjectMap *
js_DropObjectMap(JSContext *cx, JSObjectMap *map, JSObject *obj)
{
    JS_ASSERT(map->nrefs > 0);
    JS_ATOMIC_ADDREF(&map->nrefs, -1);
    if (map->nrefs == 0) {
	map->ops->destroyObjectMap(cx, map);
	return NULL;
    }
    if (MAP_IS_NATIVE(map) && ((JSScope *)map)->object == obj)
	((JSScope *)map)->object = NULL;
    return map;
}

JSObject *
js_NewObject(JSContext *cx, JSClass *clasp, JSObject *proto, JSObject *parent)
{
    JSObject *obj, *ctor;
    JSObjectOps *ops;
    JSObjectMap *map;
    jsval cval;
    uint32 i;

    /* Allocate an object from the GC heap and zero it. */
    obj = js_AllocGCThing(cx, GCX_OBJECT);
    if (!obj)
	return NULL;

    /* Bootstrap the ur-object, and make it the default prototype object. */
    if (!proto) {
	if (!js_GetClassPrototype(cx, clasp->name, &proto))
	    goto bad;
	if (!proto && !js_GetClassPrototype(cx, js_ObjectClass.name, &proto))
	    goto bad;
    }

    /* Always call the class's getObjectOps hook if it has one. */
    ops = clasp->getObjectOps
	  ? clasp->getObjectOps(cx, clasp)
	  : &js_ObjectOps;

    if (proto && (map = proto->map)->ops == ops) {
	/* Default parent to the parent of the prototype's constructor. */
	if (!parent) {
	    if (!OBJ_GET_PROPERTY(cx, proto,
				  (jsid)cx->runtime->atomState.constructorAtom,
				  &cval)) {
		goto bad;
	    }
	    if (JSVAL_IS_OBJECT(cval) && (ctor = JSVAL_TO_OBJECT(cval)) != NULL)
		parent = OBJ_GET_PARENT(cx, ctor);
	}

	/* Share the given prototype's map. */
	obj->map = js_HoldObjectMap(cx, map);
    } else {
	/* Leave parent alone.  Allocate a new map for obj. */
	map = ops->newObjectMap(cx, 1, ops, clasp, obj);
	if (!map)
	    goto bad;
	if (map->nslots == 0)
	    map->nslots = JS_INITIAL_NSLOTS;
	obj->map = map;
    }

    /* Set the proto, parent, and class properties. */
    obj->slots = JS_malloc(cx, JS_INITIAL_NSLOTS * sizeof(jsval));
    if (!obj->slots)
	goto bad;
    obj->slots[JSSLOT_PROTO] = OBJECT_TO_JSVAL(proto);
    obj->slots[JSSLOT_PARENT] = OBJECT_TO_JSVAL(parent);
    obj->slots[JSSLOT_CLASS] = PRIVATE_TO_JSVAL(clasp);
    for (i = JSSLOT_CLASS+1; i < JS_INITIAL_NSLOTS; i++)
	obj->slots[i] = JSVAL_VOID;

    if (cx->runtime->objectHook) {
        cx->runtime->objectHook(cx, obj, JS_TRUE, cx->runtime->objectHookData);
    }

    return obj;

bad:
    cx->newborn[GCX_OBJECT] = NULL;
    return NULL;
}

static JSBool
FindConstructor(JSContext *cx, const char *name, jsval *vp)
{
    JSAtom *atom;
    JSObject *obj, *tmp;
    JSObject *pobj;
    JSScopeProperty *sprop;

    atom = js_Atomize(cx, name, strlen(name), 0);
    if (!atom)
	return JS_FALSE;

    if (cx->fp && (tmp = cx->fp->scopeChain) != NULL) {
	/* Find the topmost object in the scope chain. */
	do {
	    obj = tmp;
	    tmp = OBJ_GET_PARENT(cx, obj);
	} while (tmp);
    } else {
	obj = cx->globalObject;
	if (!obj) {
	    *vp = JSVAL_VOID;
	    return JS_TRUE;
	}
    }

    if (!OBJ_LOOKUP_PROPERTY(cx, obj, (jsid)atom, &pobj, (JSProperty**)&sprop))
	return JS_FALSE;
    if (!sprop)  {
	*vp = JSVAL_VOID;
	return JS_TRUE;
    }

    JS_ASSERT(OBJ_IS_NATIVE(pobj));
    *vp = OBJ_GET_SLOT(cx, pobj, sprop->slot);
    OBJ_DROP_PROPERTY(cx, pobj, (JSProperty *)sprop);
    return JS_TRUE;
}

JSObject *
js_ConstructObject(JSContext *cx, JSClass *clasp, JSObject *proto,
		   JSObject *parent)
{
    JSObject *obj, *ctor;
    JSBool ok;
    jsval cval, *sp, *oldsp, rval;
    void *mark;
    JSStackFrame *fp;

    ok = FindConstructor(cx, clasp->name, &cval);
    if (!ok)
	return NULL;

    /*
     * If proto or parent are NULL, set them to Constructor.prototype and/or
     * Constructor.__parent__, just like JSOP_NEW does.
     */
    ctor = JSVAL_TO_OBJECT(cval);
    if (!parent)
	parent = OBJ_GET_PARENT(cx, ctor);
    if (!proto) {
	if (!OBJ_GET_PROPERTY(cx, ctor,
			      (jsid)cx->runtime->atomState.classPrototypeAtom,
			      &rval)) {
	    return NULL;
	}
	if (JSVAL_IS_OBJECT(rval))
	    proto = JSVAL_TO_OBJECT(rval);
    }

    obj = js_NewObject(cx, clasp, proto, parent);
    if (!obj)
	return NULL;

    /* Allocate stack space for cval and obj, then push them. */
    sp = js_AllocStack(cx, 2, &mark);
    if (!sp)
	goto bad;
    *sp++ = cval;
    *sp++ = OBJECT_TO_JSVAL(obj);

    /* Lift current frame to include the args and do the call. */
    fp = cx->fp;
    oldsp = fp->sp;
    fp->sp = sp;
    ok = js_Invoke(cx, 0, JS_TRUE);
    rval = sp[-1];
    fp->sp = oldsp;
    js_FreeStack(cx, mark);

    if (!ok)
	goto bad;
    return JSVAL_IS_OBJECT(rval) ? JSVAL_TO_OBJECT(rval) : obj;
bad:
    cx->newborn[GCX_OBJECT] = NULL;
    return NULL;
}

void
js_FinalizeObject(JSContext *cx, JSObject *obj)
{
    JSObjectMap *map;

    /* Cope with stillborn objects that have no map. */
    map = obj->map;
    if (!map)
	return;

    if (cx->runtime->objectHook) {
        cx->runtime->objectHook(cx, obj, JS_FALSE, cx->runtime->objectHookData);
    }

#if JS_HAS_OBJ_WATCHPOINT
    /* Remove all watchpoints with weak links to obj. */
    JS_ClearWatchPointsForObject(cx, obj);
#endif

    /* Finalize obj first, in case it needs map and slots. */
    OBJ_GET_CLASS(cx, obj)->finalize(cx, obj);

    /* Drop map and free slots. */
    js_DropObjectMap(cx, map, obj);
    obj->map = NULL;
    JS_free(cx, obj->slots);
    obj->slots = NULL;
}

JSBool
js_AllocSlot(JSContext *cx, JSObject *obj, uint32 *slotp)
{
    JSObjectMap *map;
    uint32 nslots;
    size_t nbytes;
    jsval *newslots;

    map = obj->map;
    nslots = map->nslots;
    if (map->freeslot >= nslots) {
	nslots = JS_MAX(map->freeslot, nslots);
	if (nslots < JS_INITIAL_NSLOTS)
	    nslots = JS_INITIAL_NSLOTS;
	else
	    nslots += (nslots + 1) / 2;

	nbytes = (size_t)nslots * sizeof(jsval);
#if defined(XP_PC) && defined _MSC_VER && _MSC_VER <= 800
	if (nbytes > 60000U) {
	    JS_ReportOutOfMemory(cx);
	    return JS_FALSE;
	}
#endif

	if (obj->slots) {
	    newslots = JS_realloc(cx, obj->slots, nbytes);
	} else {
	    /* obj must be newborn and unshared at this point. */
	    newslots = JS_malloc(cx, nbytes);
	}
	if (!newslots)
	    return JS_FALSE;
	obj->slots = newslots;
	map->nslots = nslots;
    }

#ifdef TOO_MUCH_GC
    obj->slots[map->freeslot] = JSVAL_VOID;
#endif
    *slotp = map->freeslot++;
    return JS_TRUE;
}

void
js_FreeSlot(JSContext *cx, JSObject *obj, uint32 slot)
{
    JSObjectMap *map;
    uint32 nslots;
    size_t nbytes;
    jsval *newslots;

    obj->slots[slot] = JSVAL_VOID;
    map = obj->map;
    if (map->freeslot == slot + 1)
	map->freeslot = slot;
    nslots = map->nslots;
    if (nslots > JS_INITIAL_NSLOTS && map->freeslot < nslots / 2) {
	nslots = map->freeslot;
	nslots += nslots / 2;
	nbytes = (size_t)nslots * sizeof(jsval);
	newslots = JS_realloc(cx, obj->slots, nbytes);
	if (!newslots)
	    return;
	obj->slots = newslots;
	map->nslots = nslots;
    }
}

#if JS_BUG_EMPTY_INDEX_ZERO
#define CHECK_FOR_EMPTY_INDEX(id)                                             \
    JS_BEGIN_MACRO                                                            \
	if (_str->length == 0)                                                \
	    id = JSVAL_ZERO;                                                  \
    JS_END_MACRO
#else
#define CHECK_FOR_EMPTY_INDEX(id) /* nothing */
#endif

/* JSVAL_INT_MAX as a string */
#define JSVAL_INT_MAX_STRING "1073741823"

#define CHECK_FOR_FUNNY_INDEX(id)                                             \
    JS_BEGIN_MACRO                                                            \
	if (!JSVAL_IS_INT(id)) {                                              \
	    JSAtom *_atom = (JSAtom *)id;                                     \
	    JSString *_str = ATOM_TO_STRING(_atom);                           \
	    const jschar *_cp = _str->chars;                                  \
	    if (JS7_ISDEC(*_cp) &&                                            \
		_str->length <= sizeof(JSVAL_INT_MAX_STRING)-1)               \
	    {                                                                 \
		jsuint _index = JS7_UNDEC(*_cp++);                            \
		jsuint _oldIndex = 0;                                         \
		jsuint _c = 0;                                                \
		if (_index != 0) {                                            \
		    while (JS7_ISDEC(*_cp)) {                                 \
			_oldIndex = _index;                                   \
			_c = JS7_UNDEC(*_cp);                                 \
			_index = 10*_index + _c;                              \
			_cp++;                                                \
		    }                                                         \
		}                                                             \
		if (*_cp == 0 &&                                              \
		     (_oldIndex < (JSVAL_INT_MAX / 10) ||                     \
		      (_oldIndex == (JSVAL_INT_MAX / 10) &&                   \
		       _c < (JSVAL_INT_MAX % 10))))                           \
		    id = INT_TO_JSVAL(_index);                                \
	    } else {                                                          \
		CHECK_FOR_EMPTY_INDEX(id);                                    \
	    }                                                                 \
	}                                                                     \
    JS_END_MACRO


JSBool
js_DefineProperty(JSContext *cx, JSObject *obj, jsid id, jsval value,
		  JSPropertyOp getter, JSPropertyOp setter, uintN attrs,
		  JSProperty **propp)
{
    JSClass *clasp;
    JSScope *scope;
    JSScopeProperty *sprop;

    /* Handle old bug that treated empty string as zero index.
     * Also convert string indices to numbers if applicable. */
    CHECK_FOR_FUNNY_INDEX(id);

    /* Lock if object locking is required by this implementation. */
    JS_LOCK_OBJ(cx, obj);

    /* Use the object's class getter and setter by default. */
    clasp = LOCKED_OBJ_GET_CLASS(obj);
    if (!getter)
	getter = clasp->getProperty;
    if (!setter)
	setter = clasp->setProperty;

    /* Find a sharable scope, or get a new one for obj. */
    scope = js_MutateScope(cx, obj, id, getter, setter, attrs, &sprop);
    if (!scope)
	goto bad;

    /* Add the property only if MutateScope didn't find a shared scope. */
    if (!sprop) {
	sprop = js_NewScopeProperty(cx, scope, id, getter, setter, attrs);
	if (!sprop)
	    goto bad;
	/* XXXbe called with lock held */
	if (!clasp->addProperty(cx, obj, sprop->id, &value) ||
	    !scope->ops->add(cx, scope, id, sprop)) {
	    js_DestroyScopeProperty(cx, scope, sprop);
	    goto bad;
	}
	PROPERTY_CACHE_FILL(cx, &cx->runtime->propertyCache, obj, id,
			    (JSProperty *)sprop);
    }

    LOCKED_OBJ_SET_SLOT(obj, sprop->slot, value);
    if (propp) {
#ifdef JS_THREADSAFE
	js_HoldScopeProperty(cx, scope, sprop);
#endif
	*propp = (JSProperty *) sprop;
    } else {
	JS_UNLOCK_OBJ(cx, obj);
    }
    return JS_TRUE;

bad:
    JS_UNLOCK_OBJ(cx, obj);
    return JS_FALSE;
}

#if defined JS_THREADSAFE && defined DEBUG
JS_FRIEND_API(JSBool)
_js_LookupProperty(JSContext *cx, JSObject *obj, jsid id, JSObject **objp,
		   JSProperty **propp, const char *file, uintN line)
#else
JS_FRIEND_API(JSBool)
js_LookupProperty(JSContext *cx, JSObject *obj, jsid id, JSObject **objp,
		  JSProperty **propp)
#endif
{
    JSHashNumber hash;
    JSScope *prevscope, *scope;
    JSSymbol *sym;
    JSClass *clasp;
    JSResolveOp resolve;
    JSNewResolveOp newresolve;
    uintN flags;
    uint32 format;
    JSObject *obj2, *proto;
    JSScopeProperty *sprop;

    /* Handle old bug that treated empty string as zero index.
     * Also convert string indices to numbers if applicable. */
    CHECK_FOR_FUNNY_INDEX(id);

    /* Search scopes starting with obj and following the prototype link. */
    hash = js_HashValue(id);
    prevscope = NULL;
    for (;;) {
	JS_LOCK_OBJ(cx, obj);
	_SET_OBJ_INFO(obj, file, line);
	scope = (JSScope *)obj->map;
	if (scope == prevscope)
	    goto skip;
	sym = scope->ops->lookup(cx, scope, id, hash);
	if (!sym) {
	    clasp = LOCKED_OBJ_GET_CLASS(obj);
	    resolve = clasp->resolve;
	    if (resolve != JS_ResolveStub) {
		if (clasp->flags & JSCLASS_NEW_RESOLVE) {
		    newresolve = (JSNewResolveOp)resolve;
		    flags = 0;
		    if (cx->fp && cx->fp->pc) {
			format = js_CodeSpec[*cx->fp->pc].format;
			if ((format & JOF_MODEMASK) != JOF_NAME)
			    flags |= JSRESOLVE_QUALIFIED;
			if (format & JOF_SET)
			    flags |= JSRESOLVE_ASSIGNING;
		    }
		    obj2 = NULL;
		    JS_UNLOCK_OBJ(cx, obj);
		    if (!newresolve(cx, obj, js_IdToValue(id), flags, &obj2))
			return JS_FALSE;
		    JS_LOCK_OBJ(cx, obj);
		    _SET_OBJ_INFO(obj, file, line);
		    if (obj2) {
			scope = (JSScope *)obj2->map;
			if (MAP_IS_NATIVE(&scope->map))
			    sym = scope->ops->lookup(cx, scope, id, hash);
		    }
		} else {
		    JS_UNLOCK_OBJ(cx, obj);
		    if (!resolve(cx, obj, js_IdToValue(id)))
			return JS_FALSE;
		    JS_LOCK_OBJ(cx, obj);
		    _SET_OBJ_INFO(obj, file, line);
		    scope = (JSScope *)obj->map;
		    if (MAP_IS_NATIVE(&scope->map))
			sym = scope->ops->lookup(cx, scope, id, hash);
		}
	    }
	}
	if (sym && (sprop = sym_property(sym)) != NULL) {
	    JS_ASSERT((JSScope *)obj->map == scope);
	    *objp = scope->object;	/* XXXbe hide in jsscope.[ch] */
#ifdef JS_THREADSAFE
	    js_HoldScopeProperty(cx, scope, sprop);
#endif
	    *propp = (JSProperty *) sprop;
	    return JS_TRUE;
	}
	prevscope = scope;
      skip:
	proto = LOCKED_OBJ_GET_PROTO(obj);
	JS_UNLOCK_OBJ(cx, obj);
	if (!proto)
	    break;
	if (!OBJ_IS_NATIVE(proto))
	    return OBJ_LOOKUP_PROPERTY(cx, proto, id, objp, propp);
	obj = proto;
    }
    *objp = NULL;
    *propp = NULL;
    return JS_TRUE;
}

JS_FRIEND_API(JSBool)
js_FindProperty(JSContext *cx, jsid id, JSObject **objp, JSObject **pobjp,
		JSProperty **propp)
{
    JSRuntime *rt;
    JSObject *obj, *pobj, *parent, *lastobj;
    JSProperty *prop;

    rt = cx->runtime;

    lastobj = NULL;             /* Suppress gcc warning */
    for (obj = cx->fp->scopeChain; obj; obj = parent) {
	/* Try the property cache and return immediately on cache hit. */
	PROPERTY_CACHE_TEST(&rt->propertyCache, obj, id, prop);
	if (PROP_FOUND(prop)) {
#ifdef JS_THREADSAFE
	    JS_ASSERT(OBJ_IS_NATIVE(obj));
	    JS_LOCK_OBJ(cx, obj);
	    JS_ATOMIC_ADDREF(&((JSScopeProperty *)prop)->nrefs, 1);
#endif
	    *objp = obj;
	    *pobjp = obj;
	    *propp = prop;
	    return JS_TRUE;
	}

	/* If cache miss (not cached-as-not-found), take the slow path. */
	if (!prop) {
	    if (!OBJ_LOOKUP_PROPERTY(cx, obj, id, &pobj, &prop))
		return JS_FALSE;
	    if (prop) {
		PROPERTY_CACHE_FILL(cx, &rt->propertyCache, pobj, id, prop);
		*objp = obj;
		*pobjp = pobj;
		*propp = prop;
		return JS_TRUE;
	    }

	    /* No such property -- cache obj[id] as not-found. */
	    PROPERTY_CACHE_FILL(cx, &rt->propertyCache, obj, id,
				PROP_NOT_FOUND(obj, id));
	}
	parent = OBJ_GET_PARENT(cx, obj);
	lastobj = obj;
    }
    *objp = lastobj;
    *pobjp = NULL;
    *propp = NULL;
    return JS_TRUE;
}

JSBool
js_FindVariable(JSContext *cx, jsid id, JSObject **objp, JSObject **pobjp,
		JSProperty **propp)
{
    JSObject *obj;
    JSProperty *prop;

    /*
     * First look for id's property along the "with" statement and the
     * statically-linked scope chains.
     */
    if (!js_FindProperty(cx, id, objp, pobjp, propp))
	return JS_FALSE;
    if (*propp)
	return JS_TRUE;

    /*
     * Use the top-level scope from the scope chain, which won't end in the
     * same scope as cx->globalObject for cross-context function calls.
     */
    obj = *objp;
    JS_ASSERT(obj);

    /*
     * Make a top-level variable.
     */
    if (!OBJ_DEFINE_PROPERTY(cx, obj, id, JSVAL_VOID, NULL, NULL,
			     JSPROP_ENUMERATE, &prop)) {
	return JS_FALSE;
    }
    *pobjp = obj;
    *propp = prop;
    return JS_TRUE;
}

JSObject *
js_FindVariableScope(JSContext *cx, JSFunction **funp)
{
    JSStackFrame *fp;
    JSObject *obj, *parent, *withobj;
    JSClass *clasp;
    JSFunction *fun;

    fp = cx->fp;
    withobj = NULL;
    for (obj = fp->scopeChain; ; obj = parent) {
	parent = OBJ_GET_PARENT(cx, obj);
	clasp = OBJ_GET_CLASS(cx, obj);
	if (!parent || clasp != &js_WithClass)
	    break;
	withobj = obj;
    }

    fun = (clasp == &js_FunctionClass) ? JS_GetPrivate(cx, obj) : NULL;
#if JS_HAS_CALL_OBJECT
    if (fun && fun->script) {
	for (; fp && fp->fun != fun; fp = fp->down)
	    ;
	if (fp)
	    obj = js_GetCallObject(cx, fp, parent, withobj);
    } else if (clasp == &js_CallClass) {
	fp = (JSStackFrame *) JS_GetPrivate(cx, obj);
	fun = fp->fun;
    }
#endif

    *funp = fun;
    return obj;
}

JSBool
js_GetProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    JSObject *obj2;
    JSScopeProperty *sprop;
    jsint slot;

    if (!js_LookupProperty(cx, obj, id, &obj2, (JSProperty **)&sprop))
	return JS_FALSE;
    if (!sprop) {
	/* Handle old bug that treated empty string as zero index.
	 * Also convert string indices to numbers if applicable. */
	CHECK_FOR_FUNNY_INDEX(id);

#if JS_BUG_NULL_INDEX_PROPS
	/* Indexed properties defaulted to null in old versions. */
	*vp = (JSVAL_IS_INT(id) && JSVAL_TO_INT(id) >= 0)
	      ? JSVAL_NULL
	      : JSVAL_VOID;
#else
	*vp = JSVAL_VOID;
#endif

	return OBJ_GET_CLASS(cx, obj)->getProperty(cx, obj, js_IdToValue(id),
			     vp);
    }

    if (!OBJ_IS_NATIVE(obj2)) {
	OBJ_DROP_PROPERTY(cx, obj2, (JSProperty *)sprop);
	return OBJ_GET_PROPERTY(cx, obj2, id, vp);
    }

    /* Unlock obj2 before calling getter, relock after to avoid deadlock. */
    slot = sprop->slot;
    *vp = LOCKED_OBJ_GET_SLOT(obj2, slot);
    JS_UNLOCK_OBJ(cx, obj2);
    if (!SPROP_GET(cx, sprop, obj, obj2, vp)) {
	JS_LOCK_OBJ(cx, obj2);
	OBJ_DROP_PROPERTY(cx, obj2, (JSProperty *)sprop);
	return JS_FALSE;
    }
    JS_LOCK_OBJ(cx, obj2);
    LOCKED_OBJ_SET_SLOT(obj2, slot, *vp);
    PROPERTY_CACHE_FILL(cx, &cx->runtime->propertyCache, obj2, id,
			(JSProperty *)sprop);
    OBJ_DROP_PROPERTY(cx, obj2, (JSProperty *)sprop);
    return JS_TRUE;
}

JSBool
js_SetProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    JSRuntime *rt;
    JSScope *scope, *protoscope;
    JSScopeProperty *sprop, *protosprop;
    JSHashNumber hash;
    JSSymbol *sym, *protosym;
    JSObject *proto, *tmp, *assignobj;
    jsval protoid;
    JSPropertyOp protogetter, protosetter;
    uintN protoattrs;
    JSClass *clasp;
    jsval pval, aval, rval;
    jsint slot;
    JSErrorReporter older;
    JSString *str;

    rt = cx->runtime;
    JS_LOCK_OBJ(cx, obj);
    protoid = protoattrs = 0;   /* Suppress use-before-set gcc warning */
    protogetter = protosetter = NULL; /* Suppress use-before-set gcc warning */

    scope = js_GetMutableScope(cx, obj);
    if (!scope) {
	JS_UNLOCK_OBJ(cx, obj);
	return JS_FALSE;
    }

    /* Handle old bug that treated empty string as zero index.
     * Also convert string indices to numbers if applicable. */
    CHECK_FOR_FUNNY_INDEX(id);

    hash = js_HashValue(id);
    sym = scope->ops->lookup(cx, scope, id, hash);
    if (sym) {
	sprop = sym_property(sym);
#if JS_HAS_OBJ_WATCHPOINT
	if (!sprop) {
	    uint32 slot, nslots;
	    jsval *slots;

	    /*
	     * Deleted property place-holder, could have a watchpoint that
	     * holds the deleted-but-watched property.  If so, slots may have
	     * shrunk, or at least freeslot may have shrunk due to the delete
	     * operation destroying the property.
	     */
	    sprop = js_FindWatchPoint(rt, obj, js_IdToValue(id));
	    if (sprop && (slot = sprop->slot) >= scope->map.freeslot) {
		if (slot >= scope->map.nslots) {
		    nslots = slot + slot / 2;
		    slots = JS_realloc(cx, obj->slots, nslots * sizeof(jsval));
		    if (!slots) {
			JS_UNLOCK_OBJ(cx, obj);
			return JS_FALSE;
		    }
		    scope->map.nslots = nslots;
		    obj->slots = slots;
		}
		scope->map.freeslot = slot + 1;
	    }
	}
#endif
    } else {
	sprop = NULL;
    }

    if (!sprop) {
	/* Find a prototype property with the same id. */
	proto = LOCKED_OBJ_GET_PROTO(obj);
	protosprop = NULL;

	JS_UNLOCK_OBJ(cx, obj);
	while (proto) {
	    JS_LOCK_OBJ(cx, proto);
	    protoscope = (JSScope *)proto->map;
	    if (MAP_IS_NATIVE(&protoscope->map)) {
		protosym = protoscope->ops->lookup(cx, protoscope, id, hash);
		if (protosym) {
		    protosprop = sym_property(protosym);
		    if (protosprop) {
			protoid = protosprop->id;
			protogetter = protosprop->getter;
			protosetter = protosprop->setter;
			protoattrs = protosprop->attrs;
			JS_UNLOCK_OBJ(cx, proto);
			break;
		    }
		}
	    }
	    tmp = LOCKED_OBJ_GET_PROTO(proto);
	    JS_UNLOCK_OBJ(cx, proto);
	    proto = tmp;
	}
	JS_LOCK_OBJ(cx, obj);

	/* Make a new property descriptor with the right heritage. */
	clasp = LOCKED_OBJ_GET_CLASS(obj);
	if (protosprop) {
	    if (protoattrs & JSPROP_READONLY)
		goto _readonly;
	    sprop = js_NewScopeProperty(cx, scope, id,
					protogetter, protosetter,
					protoattrs);
	    sprop->id = protoid;
	} else {
	    sprop = js_NewScopeProperty(cx, scope, id,
					clasp->getProperty, clasp->setProperty,
					JSPROP_ENUMERATE);
	}
	if (!sprop) {
	    JS_UNLOCK_OBJ(cx, obj);
	    return JS_FALSE;
	}

	/* XXXbe called with obj locked */
	if (!clasp->addProperty(cx, obj, sprop->id, vp)) {
	    js_DestroyScopeProperty(cx, scope, sprop);
	    JS_UNLOCK_OBJ(cx, obj);
	    return JS_FALSE;
	}

	/* Initialize new properties to undefined. */
	LOCKED_OBJ_SET_SLOT(obj, sprop->slot, JSVAL_VOID);

	if (sym) {
	    /* Null-valued symbol left behind from a delete operation. */
	    sym->entry.value = js_HoldScopeProperty(cx, scope, sprop);
	}
    }

    if (!sym) {
	/* Need a new symbol as well as a new property. */
	sym = scope->ops->add(cx, scope, id, sprop);
	if (!sym) {
	    js_DestroyScopeProperty(cx, scope, sprop);
	    JS_UNLOCK_OBJ(cx, obj);
	    return JS_FALSE;
	}
#if JS_BUG_AUTO_INDEX_PROPS
	{
	    jsid id2 = (jsid) INT_TO_JSVAL(sprop->slot - JSSLOT_START);
	    if (!scope->ops->add(cx, scope, id2, sprop)) {
		scope->ops->remove(cx, scope, id);
		JS_UNLOCK_OBJ(cx, obj);
		return JS_FALSE;
	    }
	    PROPERTY_CACHE_FILL(cx, &rt->propertyCache, obj, id2,
				(JSProperty *)sprop);
	}
#endif
	PROPERTY_CACHE_FILL(cx, &rt->propertyCache, obj, id,
			    (JSProperty *)sprop);
    }

    /* Get the current property value from its slot. */
    JS_ASSERT(sprop->slot < obj->map->freeslot);
    slot = sprop->slot;
    pval = LOCKED_OBJ_GET_SLOT(obj, slot);

    /* Evil overloaded operator assign() hack. */
    if ((!JSVERSION_IS_ECMA(cx->version)) && (JSVAL_IS_OBJECT(pval))) {
	assignobj = JSVAL_TO_OBJECT(pval);
	if (assignobj) {
	    older = JS_SetErrorReporter(cx, NULL);
	    JS_UNLOCK_OBJ(cx, obj);
	    if (OBJ_GET_PROPERTY(cx, assignobj, (jsid)rt->atomState.assignAtom,
				 &aval) &&
		JSVAL_IS_FUNCTION(cx, aval) &&
		js_CallFunctionValue(cx, assignobj, aval, 1, vp, &rval))
	    {
		*vp = rval;
		JS_SetErrorReporter(cx, older);
		sprop->attrs |= JSPROP_ASSIGNHACK;
		return JS_TRUE;
	    }
	    JS_SetErrorReporter(cx, older);
	    JS_LOCK_OBJ(cx, obj);
	}
    }

    /* Check for readonly *after* the assign() hack. */
    if (sprop->attrs & JSPROP_READONLY) {

/* "readonly" can be a language extension on OSF */
_readonly:
	JS_UNLOCK_OBJ(cx, obj);
	if (JSVERSION_IS_ECMA(cx->version))
	    return JS_TRUE;
	str = js_DecompileValueGenerator(cx, js_IdToValue(id), NULL);
	if (str)
	    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
				 JSMSG_READ_ONLY, JS_GetStringBytes(str));
	return JS_FALSE;
    }

#ifdef JS_THREADSAFE
    /* Hold sprop across setter callout, and drop after, in case of delete. */
    sprop->nrefs++;
#endif

    /* Avoid deadlock by unlocking obj while calling sprop's setter. */
    JS_UNLOCK_OBJ(cx, obj);

    /* Let the setter modify vp before copying from it to obj->slots[slot]. */
    if (!SPROP_SET(cx, sprop, obj, obj, vp)) {
#ifdef JS_THREADSAFE
	JS_LOCK_OBJ_VOID(cx, obj, js_DropScopeProperty(cx, scope, sprop));
#endif
	return JS_FALSE;
    }

    /* Relock obj until we are done with sprop. */
    JS_LOCK_OBJ(cx, obj);

#ifdef JS_THREADSAFE
    sprop = js_DropScopeProperty(cx, scope, sprop);
    if (!sprop) {
	/* Lost a race with someone who deleted sprop. */
	JS_UNLOCK_OBJ(cx, obj);
	return JS_TRUE;
    }
#endif
    GC_POKE(cx, pval);
    LOCKED_OBJ_SET_SLOT(obj, slot, *vp);

#if JS_BUG_SET_ENUMERATE
    /* Setting a property makes it enumerable. */
    sprop->attrs |= JSPROP_ENUMERATE;
#endif
    JS_UNLOCK_OBJ(cx, obj);
    return JS_TRUE;
}

JSBool
js_GetAttributes(JSContext *cx, JSObject *obj, jsid id, JSProperty *prop,
		 uintN *attrsp)
{
    JSBool noprop, ok;
    JSScopeProperty *sprop;

    noprop = !prop;
    if (noprop) {
	if (!js_LookupProperty(cx, obj, id, &obj, &prop))
	    return JS_FALSE;
	if (!prop)
	    return JS_TRUE;
	if (!OBJ_IS_NATIVE(obj)) {
	    ok = OBJ_GET_ATTRIBUTES(cx, obj, id, prop, attrsp);
	    OBJ_DROP_PROPERTY(cx, obj, prop);
	    return ok;
	}
    }
    sprop = (JSScopeProperty *)prop;
    *attrsp = sprop->attrs;
    if (noprop)
	OBJ_DROP_PROPERTY(cx, obj, prop);
    return JS_TRUE;
}

JSBool
js_SetAttributes(JSContext *cx, JSObject *obj, jsid id, JSProperty *prop,
		 uintN *attrsp)
{
    JSBool noprop, ok;
    JSScopeProperty *sprop;

    noprop = !prop;
    if (noprop) {
	if (!js_LookupProperty(cx, obj, id, &obj, &prop))
	    return JS_FALSE;
	if (!prop)
	    return JS_TRUE;
	if (!OBJ_IS_NATIVE(obj)) {
	    ok = OBJ_SET_ATTRIBUTES(cx, obj, id, prop, attrsp);
	    OBJ_DROP_PROPERTY(cx, obj, prop);
	    return ok;
	}
    }
    sprop = (JSScopeProperty *)prop;
    sprop->attrs = *attrsp;
    if (noprop)
	OBJ_DROP_PROPERTY(cx, obj, prop);
    return JS_TRUE;
}

JSBool
js_DeleteProperty(JSContext *cx, JSObject *obj, jsid id, jsval *rval)
{
#if JS_HAS_PROP_DELETE
    JSRuntime *rt;
    JSObject *proto;
    JSProperty *prop;
    JSScopeProperty *sprop;
    JSString *str;
    JSScope *scope;
    JSSymbol *sym;

    rt = cx->runtime;

    *rval = JSVERSION_IS_ECMA(cx->version) ? JSVAL_TRUE : JSVAL_VOID;

    /* Handle old bug that treated empty string as zero index.
     * Also convert string indices to numbers if applicable. */
    CHECK_FOR_FUNNY_INDEX(id);

    if (!js_LookupProperty(cx, obj, id, &proto, &prop))
	return JS_FALSE;
    if (!prop || proto != obj) {
	if (prop)
	    OBJ_DROP_PROPERTY(cx, proto, prop);
	/*
	 * If no property, or the property comes from a prototype, call the
	 * class's delProperty hook with rval as the result parameter.
	 */
	return OBJ_GET_CLASS(cx, obj)->delProperty(cx, obj, js_IdToValue(id),
						   rval);
    }

    sprop = (JSScopeProperty *)prop;
    if (sprop->attrs & JSPROP_PERMANENT) {
	OBJ_DROP_PROPERTY(cx, obj, prop);
	if (JSVERSION_IS_ECMA(cx->version)) {
	    *rval = JSVAL_FALSE;
	    return JS_TRUE;
	}
	str = js_DecompileValueGenerator(cx, js_IdToValue(id), NULL);
	if (str) {
	    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
				 JSMSG_PERMANENT, JS_GetStringBytes(str));
	}
	return JS_FALSE;
    }

    /* XXXbe called with obj locked */
    if (!LOCKED_OBJ_GET_CLASS(obj)->delProperty(cx, obj, sprop->id, rval)) {
	OBJ_DROP_PROPERTY(cx, obj, prop);
	return JS_FALSE;
    }

    GC_POKE(cx, LOCKED_OBJ_GET_SLOT(obj, sprop->slot));
    scope = (JSScope *)obj->map;

    /*
     * Purge cache only if prop is not about to be destroyed (since
     * js_DestroyScopeProperty purges for us).
     */
    if (sprop->nrefs != 1) {
	PROPERTY_CACHE_FILL(cx, &rt->propertyCache, obj, id,
			    PROP_NOT_FOUND(obj, id));
    }

#if JS_HAS_OBJ_WATCHPOINT
    if (sprop->setter == js_watch_set) {
	/*
	 * Keep the symbol around with null value in case of re-set.
	 * The watchpoint will hold the "deleted" property until it
	 * is removed by obj_unwatch or a native JS_ClearWatchPoint.
	 * See js_SetProperty for the re-set logic.
	 */
	for (sym = sprop->symbols; sym; sym = sym->next) {
	    if (sym_id(sym) == id) {
		sym->entry.value = NULL;
		sprop = js_DropScopeProperty(cx, scope, sprop);
		JS_ASSERT(sprop);
		goto out;
	    }
	}
    }
#endif /* JS_HAS_OBJ_WATCHPOINT */

    scope->ops->remove(cx, scope, id);
out:
    OBJ_DROP_PROPERTY(cx, obj, prop);
    return JS_TRUE;
#else  /* !JS_HAS_PROP_DELETE */
    jsval null = JSVAL_NULL;

    *rval = JSVAL_VOID;
    return js_SetProperty(cx, obj, id, &null);
#endif /* !JS_HAS_PROP_DELETE */
}

JSBool
js_DefaultValue(JSContext *cx, JSObject *obj, JSType hint, jsval *vp)
{
    jsval v;
    JSString *str;

    v = OBJECT_TO_JSVAL(obj);
    switch (hint) {
      case JSTYPE_STRING:
        /*
         * Propagate the exception if js_TryMethod finds an appropriate
         * method, and calling that method returned failure.
         */
        if (!js_TryMethod(cx, obj, cx->runtime->atomState.toStringAtom, 0, NULL,
                          &v))
            return JS_FALSE;

	if (!JSVAL_IS_PRIMITIVE(v)) {
	    if (!OBJ_GET_CLASS(cx, obj)->convert(cx, obj, hint, &v))
		return JS_FALSE;

	    /*
	     * JS1.2 never failed (except for malloc failure) to convert an
	     * object to a string.  ECMA requires an error if both toString
	     * and valueOf fail to produce a primitive value.
	     */
	    if (!JSVAL_IS_PRIMITIVE(v) && cx->version == JSVERSION_1_2) {
		char *bytes = JS_smprintf("[object %s]",
					  OBJ_GET_CLASS(cx, obj)->name);
		if (!bytes)
		    return JS_FALSE;
		str = JS_NewString(cx, bytes, strlen(bytes));
		if (!str) {
		    free(bytes);
		    return JS_FALSE;
		}
		v = STRING_TO_JSVAL(str);
		goto out;
	    }
	}
	break;

      default:
	if (!OBJ_GET_CLASS(cx, obj)->convert(cx, obj, hint, &v))
	    return JS_FALSE;
	if (!JSVAL_IS_PRIMITIVE(v)) {
	    JSType type = JS_TypeOfValue(cx, v);
	    if (type == hint || 
	        (type == JSTYPE_FUNCTION && hint == JSTYPE_OBJECT)) {
		goto out;
	    }
	    /* Don't convert to string (source object literal) for JS1.2. */
	    if (cx->version == JSVERSION_1_2 && hint == JSTYPE_BOOLEAN)
		goto out;
	    if (!js_TryMethod(cx, obj, cx->runtime->atomState.toStringAtom, 0,
                              NULL, &v))
                return JS_FALSE;
	}
	break;
    }
    if (!JSVAL_IS_PRIMITIVE(v)) {
	/* Avoid recursive death through js_DecompileValueGenerator. */
	if (hint == JSTYPE_STRING) {
	    str = JS_InternString(cx, OBJ_GET_CLASS(cx, obj)->name);
	    if (!str)
		return JS_FALSE;
	} else {
	    str = NULL;
	}
	*vp = OBJECT_TO_JSVAL(obj);
	str = js_DecompileValueGenerator(cx, v, str);
	if (str) {
	    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
				 JSMSG_CANT_CONVERT_TO,
				 JS_GetStringBytes(str),
				 (hint == JSTYPE_VOID)
				 ? "primitive type"
				 : js_type_str[hint]);
	}
	return JS_FALSE;
    }
out:
    *vp = v;
    return JS_TRUE;
}

extern JSIdArray *
js_NewIdArray(JSContext *cx, jsint length)
{
    JSIdArray *ida;

    ida = JS_malloc(cx, sizeof(JSIdArray) + (length - 1) * sizeof(jsval));
    if (ida)
	ida->length = length;
    return ida;
}

/* Private type used to iterate over all properties of a native JS object */
typedef struct JSNativeIteratorState {
    jsint next_index;   /* index into jsid array */
    JSIdArray *ida;     /* All property ids in enumeration */
} JSNativeIteratorState;

/*
 * This function is used to enumerate the properties of native JSObjects
 * and those host objects that do not define a JSNewEnumerateOp-style iterator
 * function.
 */
JSBool
js_Enumerate(JSContext *cx, JSObject *obj, JSIterateOp enum_op,
	     jsval *statep, jsid *idp)
{
    JSObject *proto_obj;
    JSClass *clasp;
    JSEnumerateOp enumerate;
    JSScopeProperty *sprop;
    jsint i, length;
    JSScope *scope;
    JSIdArray *ida;
    JSNativeIteratorState *state;

    clasp = OBJ_GET_CLASS(cx, obj);
    enumerate = clasp->enumerate;
    if (clasp->flags & JSCLASS_NEW_ENUMERATE)
	return ((JSNewEnumerateOp) enumerate)(cx, obj, enum_op, statep, idp);

    switch (enum_op) {

    case JSENUMERATE_INIT:
	if (!enumerate(cx, obj))
	    goto init_error;
	length = 0;

	/*
	 * The set of all property ids is pre-computed when the iterator
	 * is initialized so as to avoid problems with properties being
	 * deleted during the iteration.
	 */
	JS_LOCK_OBJ(cx, obj);
	scope = (JSScope *) obj->map;

	/*
	 * If this object shares a scope with its prototype, don't enumerate
	 * its properties.  Otherwise they will be enumerated a second time
	 * when the prototype object is enumerated.
	 */
	proto_obj = OBJ_GET_PROTO(cx, obj);
	if (proto_obj && (scope == (JSScope *)proto_obj->map)) {
	    ida = js_NewIdArray(cx, 0);
	    if (!ida) {
	      JS_UNLOCK_OBJ(cx, obj);
	      goto init_error;
	    }
	} else {
	    /* Object has a private scope; Enumerate all props in scope. */
	    for (sprop = scope->props; sprop; sprop = sprop->next) {
		if ((sprop->attrs & JSPROP_ENUMERATE) && sprop->symbols)
		    length++;
	    }
	    ida = js_NewIdArray(cx, length);
	    if (!ida) {
		JS_UNLOCK_OBJ(cx, obj);
		goto init_error;
	    }
	    i = 0;
	    for (sprop = scope->props; sprop; sprop = sprop->next) {
		if ((sprop->attrs & JSPROP_ENUMERATE) && sprop->symbols) {
		    JS_ASSERT(i < length);
		    ida->vector[i++] = sym_id(sprop->symbols);
		}
	    }
	}
	JS_UNLOCK_OBJ(cx, obj);

	state = JS_malloc(cx, sizeof(JSNativeIteratorState));
	if (!state) {
	    JS_DestroyIdArray(cx, ida);
	    goto init_error;
	}
	state->ida = ida;
	state->next_index = 0;
	*statep = PRIVATE_TO_JSVAL(state);
	if (idp)
	    *idp = INT_TO_JSVAL(length);
	return JS_TRUE;

    case JSENUMERATE_NEXT:
	state = JSVAL_TO_PRIVATE(*statep);
	ida = state->ida;
	length = ida->length;
	if (state->next_index != length) {
	    *idp = ida->vector[state->next_index++];
	    return JS_TRUE;
	}

	/* Fall through ... */

    case JSENUMERATE_DESTROY:
	state = JSVAL_TO_PRIVATE(*statep);
	JS_DestroyIdArray(cx, state->ida);
	JS_free(cx, state);
	*statep = JSVAL_NULL;
	return JS_TRUE;

    default:
	JS_ASSERT(0);
	return JS_FALSE;
    }

init_error:
    *statep = JSVAL_NULL;
    return JS_FALSE;
}

JSBool
js_CheckAccess(JSContext *cx, JSObject *obj, jsid id, JSAccessMode mode,
	       jsval *vp, uintN *attrsp)
{
    JSObject *pobj;
    JSProperty *prop;
    JSScopeProperty *sprop;
    JSClass *clasp;
    JSBool ok;

    if (!js_LookupProperty(cx, obj, id, &pobj, &prop))
	return JS_FALSE;
    if (!prop) {
	*vp = JSVAL_VOID;
	*attrsp = 0;
	return JS_TRUE;
    }
    if (!OBJ_IS_NATIVE(pobj)) {
	OBJ_DROP_PROPERTY(cx, pobj, prop);
	return OBJ_CHECK_ACCESS(cx, pobj, id, mode, vp, attrsp);
    }
    sprop = (JSScopeProperty *)prop;
    *vp = LOCKED_OBJ_GET_SLOT(pobj, sprop->slot);
    *attrsp = sprop->attrs;
    clasp = LOCKED_OBJ_GET_CLASS(obj);
    if (clasp->checkAccess) {
	JS_UNLOCK_OBJ(cx, pobj);
	ok = clasp->checkAccess(cx, obj, sprop->id, mode, vp);
	JS_LOCK_OBJ(cx, pobj);
    } else {
	ok = JS_TRUE;
    }
    OBJ_DROP_PROPERTY(cx, pobj, prop);
    return ok;
}

JSBool
js_Call(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSClass *clasp;

    clasp = OBJ_GET_CLASS(cx, JSVAL_TO_OBJECT(argv[-2]));
    if (!clasp->call) {
	js_ReportIsNotFunction(cx, &argv[-2], JS_FALSE);
	return JS_FALSE;
    }
    return clasp->call(cx, obj, argc, argv, rval);
}

JSBool
js_Construct(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
	     jsval *rval)
{
    JSClass *clasp;

    clasp = OBJ_GET_CLASS(cx, JSVAL_TO_OBJECT(argv[-2]));
    if (!clasp->construct) {
	js_ReportIsNotFunction(cx, &argv[-2], JS_TRUE);
	return JS_FALSE;
    }
    return clasp->construct(cx, obj, argc, argv, rval);
}

JSBool
js_HasInstance(JSContext *cx, JSObject *obj, jsval v, JSBool *bp)
{
    JSClass *clasp;

    clasp = OBJ_GET_CLASS(cx, obj);
    if (clasp->hasInstance)
	return clasp->hasInstance(cx, obj, v, bp);
    *bp = JS_FALSE;
    return JS_TRUE;
}

JSBool
js_IsDelegate(JSContext *cx, JSObject *obj, jsval v, JSBool *bp)
{
    JSObject *obj2;

    *bp = JS_FALSE;
    if (JSVAL_IS_PRIMITIVE(v))
	return JS_TRUE;
    obj2 = JSVAL_TO_OBJECT(v);
    do {
	if (obj2 == obj) {
	    *bp = JS_TRUE;
	    break;
	}
    } while ((obj2 = OBJ_GET_PROTO(cx, obj2)) != NULL);
    return JS_TRUE;
}

#ifdef JS_THREADSAFE
void
js_DropProperty(JSContext *cx, JSObject *obj, JSProperty *prop)
{
    js_DropScopeProperty(cx, (JSScope *)obj->map, (JSScopeProperty *)prop);
    JS_UNLOCK_OBJ(cx, obj);
}
#endif

JSBool
js_GetClassPrototype(JSContext *cx, const char *name, JSObject **protop)
{
    jsval v;
    JSObject *ctor;

    if (!FindConstructor(cx, name, &v))
	return JS_FALSE;
    if (JSVAL_IS_FUNCTION(cx, v)) {
	ctor = JSVAL_TO_OBJECT(v);
	if (!OBJ_GET_PROPERTY(cx, ctor,
			      (jsid)cx->runtime->atomState.classPrototypeAtom,
			      &v)) {
	    return JS_FALSE;
	}
    }
    *protop = JSVAL_IS_OBJECT(v) ? JSVAL_TO_OBJECT(v) : NULL;
    return JS_TRUE;
}

JSBool
js_SetClassPrototype(JSContext *cx, JSObject *ctor, JSObject *proto,
		     uintN attrs)
{
    /*
     * Use the given attributes for the prototype property of the constructor,
     * as user-defined constructors have a DontEnum prototype (it can be reset
     * or even deleted), while native "system" constructors require DontEnum |
     * ReadOnly | DontDelete.
     */
    if (!OBJ_DEFINE_PROPERTY(cx, ctor,
			     (jsid)cx->runtime->atomState.classPrototypeAtom,
			     OBJECT_TO_JSVAL(proto), NULL, NULL,
			     attrs, NULL)) {
	return JS_FALSE;
    }

    /*
     * ECMA says that Object.prototype.constructor, or f.prototype.constructor
     * for a user-defined function f, is DontEnum.
     */
    return OBJ_DEFINE_PROPERTY(cx, proto,
			       (jsid)cx->runtime->atomState.constructorAtom,
			       OBJECT_TO_JSVAL(ctor), NULL, NULL,
			       0, NULL);
}

JSBool
js_ValueToObject(JSContext *cx, jsval v, JSObject **objp)
{
    JSObject *obj;

    if (JSVAL_IS_NULL(v) || JSVAL_IS_VOID(v)) {
	obj = NULL;
    } else if (JSVAL_IS_OBJECT(v)) {
	obj = JSVAL_TO_OBJECT(v);
	if (!OBJ_DEFAULT_VALUE(cx, obj, JSTYPE_OBJECT, &v))
	    return JS_FALSE;
	if (JSVAL_IS_OBJECT(v))
	    obj = JSVAL_TO_OBJECT(v);
    } else {
	if (JSVAL_IS_STRING(v)) {
	    obj = js_StringToObject(cx, JSVAL_TO_STRING(v));
	} else if (JSVAL_IS_INT(v)) {
	    obj = js_NumberToObject(cx, (jsdouble)JSVAL_TO_INT(v));
	} else if (JSVAL_IS_DOUBLE(v)) {
	    obj = js_NumberToObject(cx, *JSVAL_TO_DOUBLE(v));
	} else {
	    JS_ASSERT(JSVAL_IS_BOOLEAN(v));
	    obj = js_BooleanToObject(cx, JSVAL_TO_BOOLEAN(v));
	}
	if (!obj)
	    return JS_FALSE;
    }
    *objp = obj;
    return JS_TRUE;
}

JSObject *
js_ValueToNonNullObject(JSContext *cx, jsval v)
{
    JSObject *obj;
    JSString *str;

    if (!js_ValueToObject(cx, v, &obj))
	return NULL;
    if (!obj) {
	str = js_DecompileValueGenerator(cx, v, NULL);
	if (str) {
	    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
				 JSMSG_NO_PROPERTIES, JS_GetStringBytes(str));
	}
    }
    return obj;
}

JSBool
js_TryValueOf(JSContext *cx, JSObject *obj, JSType type, jsval *rval)
{
#if JS_HAS_VALUEOF_HINT
    jsval argv[1];

    argv[0] = ATOM_KEY(cx->runtime->atomState.typeAtoms[type]);
    return js_TryMethod(cx, obj, cx->runtime->atomState.valueOfAtom, 1, argv,
                        rval);
#else
    return js_TryMethod(cx, obj, cx->runtime->atomState.valueOfAtom, 0, NULL,
                        rval);
#endif
}

JSBool
js_TryMethod(JSContext *cx, JSObject *obj, JSAtom *atom,
	     uintN argc, jsval *argv, jsval *rval)
{
    JSErrorReporter older;
    jsval fval;
    JSBool ok;

    /*
     * Report failure only if an appropriate method was found, and calling it
     * returned failure.  We propagate failure in this case to make exceptions
     * behave properly.
     */

    older = JS_SetErrorReporter(cx, NULL);
    if (OBJ_GET_PROPERTY(cx, obj, (jsid)atom, &fval) &&
	JSVAL_IS_OBJECT(fval) &&
	fval != JSVAL_NULL) {
	ok = js_CallFunctionValue(cx, obj, fval, argc, argv, rval);
    } else {
        ok = JS_TRUE;
    }
    JS_SetErrorReporter(cx, older);
    return ok;
}

#if JS_HAS_XDR

#include "jsxdrapi.h"

JSBool
js_XDRObject(JSXDRState *xdr, JSObject **objp)
{
    JSContext *cx;
    JSClass *clasp;
    char *className;
    uint32 classId, classDef;
    JSBool ok;
    JSObject *proto;

    cx = xdr->cx;
    if (xdr->mode == JSXDR_ENCODE) {
	clasp = OBJ_GET_CLASS(cx, *objp);
	className = clasp->name;
	classId = JS_FindClassIdByName(xdr, className);
	classDef = !classId;
	if (classDef && !JS_RegisterClass(xdr, clasp, &classId))
	    return JS_FALSE;
    } else {
	classDef = 0;
	className = NULL;
        clasp = NULL;           /* quell GCC overwarning */
    }

    /* XDR a flag word followed (if true) by the class name. */
    if (!JS_XDRUint32(xdr, &classDef))
	return JS_FALSE;
    if (classDef && !JS_XDRCString(xdr, &className))
	return JS_FALSE;

    /* From here on, return through out: to free className if it was set. */
    ok = JS_XDRUint32(xdr, &classId);
    if (!ok)
	goto out;

    if (xdr->mode != JSXDR_ENCODE) {
	if (classDef) {
	    ok = js_GetClassPrototype(cx, className, &proto);
	    if (!ok)
		goto out;
	    clasp = OBJ_GET_CLASS(cx, proto);
	    ok = JS_RegisterClass(xdr, clasp, &classId);
	    if (!ok)
		goto out;
	} else {
	    clasp = JS_FindClassById(xdr, classId);
	    if (!clasp) {
		char numBuf[12];
		JS_snprintf(numBuf, sizeof numBuf, "%ld", (long)classId);
		JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
				     JSMSG_CANT_FIND_CLASS, numBuf);
		ok = JS_FALSE;
		goto out;
	    }
	}
    }

    if (!clasp->xdrObject) {
	JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
			     JSMSG_CANT_XDR_CLASS, clasp->name);
	ok = JS_FALSE;
    } else {
	ok = clasp->xdrObject(xdr, objp);
    }
out:
    if (xdr->mode != JSXDR_ENCODE && className)
	JS_free(cx, className);
    return ok;
}

#endif /* JS_HAS_XDR */


#ifdef DEBUG

/* Routines to print out values during debugging. */

void printChar(jschar *cp) {
    fprintf(stderr, "jschar* (0x%p) \"", cp);
    while (*cp)
	fputc(*cp++, stderr);
    fputc('"', stderr);
    fputc('\n', stderr);
}

void printString(JSString *str) {
    jsuint i;
    fprintf(stderr, "string (0x%p) \"", str);
    for (i=0; i < str->length; i++)
	fputc(str->chars[i], stderr);
    fputc('"', stderr);
    fputc('\n', stderr);
}

void printVal(jsval val);

void printObj(JSObject *jsobj) { 
    jsuint i;
    jsval val;
    JSClass *clasp;

    fprintf(stderr, "object 0x%p\n", jsobj);
    clasp = OBJ_GET_CLASS(NULL, jsobj);
    fprintf(stderr, "class 0x%p %s\n", clasp, clasp->name);
    for (i=0; i < jsobj->map->nslots; i++) {
        fprintf(stderr, "slot %3d ", i);
        val = jsobj->slots[i];
        if (JSVAL_IS_OBJECT(val))
	    fprintf(stderr, "object 0x%p\n", JSVAL_TO_OBJECT(val));
        else
            printVal(val);
    }
}

void printVal(jsval val) {
    fprintf(stderr, "val %d (0x%p) = ", val, val);
    if (JSVAL_IS_NULL(val)) {
	fprintf(stderr, "null\n");
    } else if (JSVAL_IS_VOID(val)) {
	fprintf(stderr, "undefined\n");
    } else if (JSVAL_IS_OBJECT(val)) {
        printObj(JSVAL_TO_OBJECT(val));
    } else if (JSVAL_IS_INT(val)) {
	fprintf(stderr, "(int) %d\n", JSVAL_TO_INT(val));
    } else if (JSVAL_IS_STRING(val)) {
	printString(JSVAL_TO_STRING(val));
    } else if (JSVAL_IS_DOUBLE(val)) {
	fprintf(stderr, "(double) %g\n", *JSVAL_TO_DOUBLE(val));
    } else {
	JS_ASSERT(JSVAL_IS_BOOLEAN(val));
	fprintf(stderr, "(boolean) %s\n",
		JSVAL_TO_BOOLEAN(val) ? "true" : "false");
    }
    fflush(stderr);
}

void printId(jsid id) {
    fprintf(stderr, "id %d (0x%p) is ", id, id);
    printVal(js_IdToValue(id));
}

void printAtom(JSAtom *atom) {
    printString(ATOM_TO_STRING(atom));
}

#endif

