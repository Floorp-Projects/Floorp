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
#include <stdlib.h>
#include <string.h>
#include "prtypes.h"
#ifndef NSPR20
#include "prarena.h"
#else
#include "plarena.h"
#endif
#include "prlog.h"
#ifndef NSPR20
#include "prhash.h"
#else
#include "plhash.h"
#endif
#include "prprf.h"
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
#include "jsopcode.h"
#include "jsscope.h"
#include "jsscript.h"
#include "jsstr.h"

#if JS_HAS_OBJ_WATCHPOINT
#include "jsdbgapi.h"

extern JSProperty *
js_FindWatchPoint(JSRuntime *rt, JSObject *obj, jsval userid);

extern JSBool PR_CALLBACK
js_watch_set(JSContext *cx, JSObject *obj, jsval id, jsval *vp);
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

PR_STATIC_CALLBACK(JSBool)
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
    JS_LOCK_VOID(cx, *vp = js_GetSlot(cx, obj, JSVAL_TO_INT(id)));
    return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
obj_setSlot(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSObject *obj2;
    jsint slot;
    JSBool ok;

    if (!JSVAL_IS_OBJECT(*vp))
	return JS_TRUE;
    obj2 = JSVAL_TO_OBJECT(*vp);
    slot = JSVAL_TO_INT(id);
    JS_LOCK(cx);
    while (obj2) {
	if (obj2 == obj) {
	    JS_ReportError(cx, "cyclic %s value", object_props[slot].name);
	    ok = JS_FALSE;
	    goto out;
	}
	obj2 = JSVAL_TO_OBJECT(js_GetSlot(cx, obj2, slot));
    }
    ok = js_SetSlot(cx, obj, slot, *vp);
out:
    JS_UNLOCK(cx);
    return ok;
}

static JSBool
obj_getCount(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    jsint count;
    JSProperty *prop;

    count = 0;
    JS_LOCK(cx);
    for (prop = obj->map->props; prop; prop = prop->next)
	if (prop->flags & JSPROP_ENUMERATE)
	    count++;
    JS_UNLOCK(cx);
    *vp = INT_TO_JSVAL(count);
    return JS_TRUE;
}

#else  /* !JS_HAS_OBJ_PROTO_PROP */

#define object_props NULL

#endif /* !JS_HAS_OBJ_PROTO_PROP */

#if JS_HAS_SHARP_VARS

PR_STATIC_CALLBACK(PRHashNumber)
js_hash_object(const void *key)
{
    return (PRHashNumber)key >> JSVAL_TAGBITS;
}

static PRHashEntry *
MarkSharpObjects(JSContext *cx, JSObject *obj)
{
    JSSharpObjectMap *map;
    PRHashTable *table;
    PRHashNumber hash;
    PRHashEntry **hep, *he;
    jsatomid sharpid;
    JSProperty *prop;
    jsval val;

    map = &cx->sharpObjectMap;
    table = map->table;
    hash = js_hash_object(obj);
    hep = PR_HashTableRawLookup(table, hash, obj);
    he = *hep;
    if (!he) {
	sharpid = 0;
	he = PR_HashTableRawAdd(table, hep, hash, obj, (void *)sharpid);
	if (!he) {
	    JS_ReportOutOfMemory(cx);
	    return NULL;
	}
	for (prop = obj->map->props; prop; prop = prop->next) {
	    if (!prop->symbols || !(prop->flags & JSPROP_ENUMERATE))
		continue;
	    val = prop->object->slots[prop->slot];
	    if (JSVAL_IS_OBJECT(val) && val != JSVAL_NULL)
		(void) MarkSharpObjects(cx, JSVAL_TO_OBJECT(val));
	}
    } else {
	sharpid = (jsatomid) he->value;
	if (sharpid == 0) {
	    sharpid = ++map->sharpgen << 1;
	    he->value = (void *) sharpid;
	}
    }
    return he;
}

PRHashEntry *
js_EnterSharpObject(JSContext *cx, JSObject *obj, jschar **sp)
{
    JSSharpObjectMap *map;
    PRHashTable *table;
    PRHashNumber hash;
    PRHashEntry *he;
    jsatomid sharpid;
    char buf[20];
    size_t len;

    map = &cx->sharpObjectMap;
    table = map->table;
    if (!table) {
	table = PR_NewHashTable(8, js_hash_object, PR_CompareValues,
				PR_CompareValues, NULL, NULL);
	if (!table) {
	    JS_ReportOutOfMemory(cx);
	    return NULL;
	}
	map->table = table;
    }

    if (map->depth == 0) {
	he = MarkSharpObjects(cx, obj);
	if (!he)
	    return NULL;
    } else {
	hash = js_hash_object(obj);
	he = *PR_HashTableRawLookup(table, hash, obj);
	PR_ASSERT(he);
    }

    sharpid = (jsatomid) he->value;
    if (sharpid == 0) {
	*sp = NULL;
    } else {
	len = PR_snprintf(buf, sizeof buf, "#%u%c",
			  sharpid >> 1, (sharpid & SHARP_BIT) ? '#' : '=');
	*sp = js_InflateString(cx, buf, len);
	if (!*sp)
	    return NULL;
    }

    if ((sharpid & SHARP_BIT) == 0)
	map->depth++;
    return he;
}

void
js_LeaveSharpObject(JSContext *cx)
{
    JSSharpObjectMap *map;

    map = &cx->sharpObjectMap;
    PR_ASSERT(map->depth > 0);
    if (--map->depth == 0) {
	map->sharpgen = 0;
	PR_HashTableDestroy(map->table);
	map->table = NULL;
    }
}

#endif /* JS_HAS_SHARP_VARS */

#define OBJ_TOSTRING_NARGS	2	/* for 2 GC roots */

JSBool
js_obj_toString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
    jschar *chars;
    size_t nchars;
    char *clazz, *prefix;
    JSString *str;

#if JS_HAS_OBJECT_LITERAL
    if (cx->version >= JSVERSION_1_2) {
	JSProperty *list, *prop;
	JSBool ok;
	char *comma, *idquote, *valquote;
	jsval id, val;
	JSString *idstr, *valstr;
#if JS_HAS_SHARP_VARS
	PRHashEntry *he;
#endif

	/* Early returns after this must unlock, or goto done with ok set. */
	JS_LOCK(cx);
	list = obj->map->props;

#if JS_HAS_SHARP_VARS
	he = js_EnterSharpObject(cx, obj, &chars);
	if (!he) {
	    JS_UNLOCK(cx);
	    return JS_FALSE;
	}
	if (IS_SHARP(he)) {	/* we didn't enter -- obj is already sharp */
	    JS_UNLOCK(cx);
	    nchars = js_strlen(chars);
	    goto make_string;
	}
	MAKE_SHARP(he);
#else
	/* Shut off recursion by temporarily clearing obj's property list. */
	obj->map->props = NULL;
	chars = NULL;
#endif
	ok = JS_TRUE;

	/* Allocate 2 + 1 for "{}" and the terminator. */
	if (!chars) {
	    chars = malloc((2 + 1) * sizeof(jschar));
	    nchars = 0;
	} else {
	    nchars = js_strlen(chars);
	    chars = realloc(chars, (nchars + 2 + 1) * sizeof(jschar));
	}
	if (!chars)
	    goto done;
	chars[nchars++] = '{';

	comma = NULL;

	for (prop = list; prop; prop = prop->next) {
	    if (!prop->symbols || !(prop->flags & JSPROP_ENUMERATE))
		continue;

	    /* Get strings for id and val and GC-root them via argv. */
	    id = js_IdToValue(sym_id(prop->symbols));
	    idstr = js_ValueToString(cx, id);
	    if (idstr)
		argv[0] = STRING_TO_JSVAL(idstr);
	    val = prop->object->slots[prop->slot];
	    valstr = js_ValueToString(cx, val);
	    if (!idstr || !valstr) {
		ok = JS_FALSE;
		goto done;
	    }
	    argv[1] = STRING_TO_JSVAL(valstr);

	    /* If id is a non-identifier string, it needs to be quoted. */
	    if (JSVAL_IS_STRING(id) && !js_IsIdentifier(idstr)) {
		idquote = "'";
		idstr = js_EscapeString(cx, idstr, *idquote);
		if (!idstr) {
		    ok = JS_FALSE;
		    goto done;
		}
		argv[0] = STRING_TO_JSVAL(idstr);
	    } else {
		idquote = NULL;
	    }

	    /* Same for val, except it can't be an identifier. */
	    if (JSVAL_IS_STRING(val)) {
		valquote = "\"";
		valstr = js_EscapeString(cx, valstr, *valquote);
		if (!valstr) {
		    ok = JS_FALSE;
		    goto done;
		}
		argv[1] = STRING_TO_JSVAL(valstr);
	    } else {
		valquote = NULL;
	    }

	    /* Allocate 1 + 1 at end for closing brace and terminating 0. */
	    chars = realloc(chars,
			    (nchars + (comma ? 2 : 0) +
			     (idquote ? 2 : 0) + idstr->length + 1 +
			     (valquote ? 2 : 0) + valstr->length +
			     1 + 1) * sizeof(jschar));
	    if (!chars)
		goto done;

	    if (comma) {
		chars[nchars++] = comma[0];
		chars[nchars++] = comma[1];
	    }
	    comma = ", ";

	    if (idquote)
		chars[nchars++] = *idquote;
	    js_strncpy(&chars[nchars], idstr->chars, idstr->length);
	    nchars += idstr->length;
	    if (idquote)
		chars[nchars++] = *idquote;

	    chars[nchars++] = ':';

	    if (valquote)
		chars[nchars++] = *valquote;
	    js_strncpy(&chars[nchars], valstr->chars, valstr->length);
	    nchars += valstr->length;
	    if (valquote)
		chars[nchars++] = *valquote;
	}

      done:
	if (chars) {
	    chars[nchars++] = '}';
	    chars[nchars] = 0;
	}
#if JS_HAS_SHARP_VARS
	js_LeaveSharpObject(cx);
#else
	/* Restore obj's property list before bailing on error. */
	obj->map->props = list;
#endif

	JS_UNLOCK(cx);
	if (!ok) {
	    if (chars)
		free(chars);
	    return ok;
	}
    } else
#endif /* JS_HAS_OBJECT_LITERAL */
    {
	clazz = obj->map->clasp->name;
	nchars = 9 + strlen(clazz);		/* 9 for "[object ]" */
	chars = malloc((nchars + 1) * sizeof(jschar));
	if (chars) {
	    prefix = "[object ";
	    nchars = 0;
	    while ((chars[nchars] = (jschar)*prefix) != 0)
		nchars++, prefix++;
	    while ((chars[nchars] = (jschar)*clazz) != 0)
		nchars++, clazz++;
	    chars[nchars++] = ']';
	    chars[nchars] = 0;
	}
    }

#if JS_HAS_SHARP_VARS
  make_string:
#endif
    if (!chars) {
	JS_ReportOutOfMemory(cx);
	return JS_FALSE;
    }
    str = js_NewString(cx, chars, nchars, 0);
    if (!str) {
	free(chars);
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
    JSStackFrame *fp, *caller;
    JSBool ok;
    JSString *str;
    const char *file;
    uintN line;
    JSPrincipals *principals;
    JSScript *script;
#if JS_HAS_EVAL_THIS_SCOPE
    JSObject *callerScopeChain;
    JSBool implicitWith;
#endif

    if (!JSVAL_IS_STRING(argv[0])) {
	*rval = argv[0];
	return JS_TRUE;
    }

    fp = cx->fp;
    caller = fp->down;
#if !JS_BUG_EVAL_THIS_FUN
    /* Ensure that this flows into eval from the calling function, if any. */
    fp->thisp = caller->thisp;
#endif
#if JS_HAS_SHARP_VARS
    fp->sharpArray = caller->sharpArray;
#endif

#if JS_HAS_EVAL_THIS_SCOPE
    /* If obj.eval(str), emulate 'with (obj) eval(str)' in the calling frame. */
    callerScopeChain = caller->scopeChain;
    implicitWith = (callerScopeChain != obj &&
		    (callerScopeChain->map->clasp != &js_WithClass ||
		     OBJ_GET_PROTO(callerScopeChain) != obj));
    if (implicitWith) {
	obj = js_NewObject(cx, &js_WithClass, obj, callerScopeChain);
	if (!obj)
	    return JS_FALSE;
	caller->scopeChain = obj;
    }
#endif

#if !JS_BUG_EVAL_THIS_SCOPE
    /* Compile using caller's current scope object (might be a function). */
    obj = caller->scopeChain;
#endif

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
    script = JS_CompileUCScriptForPrincipals(cx, obj, principals,
					     str->chars, str->length,
					     file, line);
    if (!script) {
	ok = JS_FALSE;
	goto out;
    }

#if !JS_BUG_EVAL_THIS_SCOPE
    /* Interpret using caller's new scope object (might be a Call object). */
    obj = caller->scopeChain;
#endif
    ok = js_Execute(cx, obj, script, fp, rval);
    JS_DestroyScript(cx, script);

out:
#if JS_HAS_EVAL_THIS_SCOPE
    if (implicitWith) {
	/* Restore OBJ_GET_PARENT(obj) not callerScopeChain in case of Call. */
	caller->scopeChain = OBJ_GET_PARENT(obj);
    }
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

    PR_ASSERT(JS_IS_LOCKED(cx));
    funobj = closure;
    argv[0] = id;
    argv[1] = old;
    argv[2] = *nvp;
    return js_Call(cx, obj, OBJECT_TO_JSVAL(funobj), 3, argv, nvp);
}

static JSBool
obj_watch(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSFunction *fun;
    jsval userid, symid, propid, value;
    JSAtom *atom;
    JSRuntime *rt;
    JSBool ok;
    JSProperty *prop;
    JSPropertyOp getter, setter;
    JSClass *clasp;

    fun = JS_ValueToFunction(cx, argv[1]);
    if (!fun)
	return JS_FALSE;
    argv[1] = OBJECT_TO_JSVAL(fun->object);

    /*
     * Lock the world while we look in obj.  Be sure to return via goto out
     * on error, so we unlock.
     */
    rt = cx->runtime;
    JS_LOCK_RUNTIME(rt);

    /* Compute the unique int/atom symbol id needed by js_LookupProperty. */
    userid = argv[0];
    if (JSVAL_IS_INT(userid)) {
	symid = userid;
	atom = NULL;
    } else {
	atom = js_ValueToStringAtom(cx, userid);
	if (!atom) {
	    ok = JS_FALSE;
	    goto out;
	}
	symid = (jsval)atom;
    }

    ok = js_LookupProperty(cx, obj, symid, &obj, &prop);
    if (atom)
	js_DropAtom(cx, atom);
    if (!ok)
	goto out;

    /* Set propid from the property, in case it has a tinyid. */
    if (prop) {
	propid = prop->id;
	getter = prop->getter;
	setter = prop->setter;
	value = prop->object->slots[prop->slot];
    } else {
	propid = userid;
	clasp = obj->map->clasp;
	getter = clasp->getProperty;
	setter = clasp->setProperty;
	value = JSVAL_VOID;
    }

    /*
     * Security policy is implemented in getters and setters, so we have to
     * call get and set here to let the JS API client check for a watchpoint
     * that crosses a trust boundary.
     * XXX assumes get and set are idempotent, should use a clasp->watch hook
     */
    ok = getter(cx, obj, propid, &value) && setter(cx, obj, propid, &value);
    if (!ok)
	goto out;

    /* Finally, call into jsdbgapi.c to set the watchpoint on userid. */
    ok = JS_SetWatchPoint(cx, obj, userid, obj_watch_handler, fun->object);

out:
    JS_UNLOCK_RUNTIME(rt);
    return ok;
}

static JSBool
obj_unwatch(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JS_LOCK_VOID(cx, JS_ClearWatchPoint(cx, obj, argv[0], NULL, NULL));
    return JS_TRUE;
}

#endif /* JS_HAS_OBJ_WATCHPOINT */

static JSFunctionSpec object_methods[] = {
    {js_toString_str,	js_obj_toString,	OBJ_TOSTRING_NARGS},
    {js_valueOf_str,	obj_valueOf,		0},
    {js_eval_str,	obj_eval,		1},
#if JS_HAS_OBJ_WATCHPOINT
    {"watch",           obj_watch,              2},
    {"unwatch",         obj_unwatch,            1},
#endif
    {0}
};

static JSBool
Object(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    if (argc != 0) {
	if (!JS_ValueToObject(cx, argv[0], &obj))
	    return JS_FALSE;
	if (!obj)
	    return JS_TRUE;
    }
    *rval = OBJECT_TO_JSVAL(obj);
    return JS_TRUE;
}

/*
 * Class for with-statement stack objects.
 */
static JSBool
with_resolve(JSContext *cx, JSObject *obj, jsval id, JSObject **objp)
{
    *objp = OBJ_GET_PROTO(obj);
    return JS_TRUE;
}

JSClass js_WithClass = {
    "With",
    JSCLASS_NEW_RESOLVE,
    JS_PropertyStub,  JS_PropertyStub,
    JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, (JSResolveOp)with_resolve,
    JS_ConvertStub,   JS_FinalizeStub
};

#if JS_HAS_OBJ_PROTO_PROP
static JSBool
With(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSObject *parent, *proto;
    jsval v;

    if (!JS_InstanceOf(cx, obj, &js_WithClass, argv))
	return JS_FALSE;

    parent = cx->fp->scopeChain;
    if (argc > 0) {
	if (!JS_ValueToObject(cx, argv[0], &proto))
	    return JS_FALSE;
	v = OBJECT_TO_JSVAL(proto);
	if (!obj_setSlot(cx, obj, INT_TO_JSVAL(JSSLOT_PROTO), &v))
	    return JS_FALSE;
	if (argc > 1) {
	    if (!JS_ValueToObject(cx, argv[1], &parent))
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
    PR_ASSERT(sizeof(jsatomid) * PR_BITS_PER_BYTE >= ATOM_INDEX_LIMIT_LOG2 + 1);
#endif

    proto = JS_InitClass(cx, obj, NULL, &js_ObjectClass, Object, 0,
			 object_props, object_methods, NULL, NULL);
#if JS_HAS_OBJ_PROTO_PROP
    if (!JS_InitClass(cx, obj, NULL, &js_WithClass, With, 0,
		      NULL, NULL, NULL, NULL)) {
	return NULL;
    }
#endif
    return proto;
}

JSObject *
js_NewObject(JSContext *cx, JSClass *clasp, JSObject *proto, JSObject *parent)
{
    JSObject *obj;
    JSScope *scope;

    /* Allocate an object from the GC heap and zero it. */
    obj = js_AllocGCThing(cx, GCX_OBJECT);
    if (!obj)
	return NULL;

    /* Bootstrap the ur-object, and make it the default prototype object. */
    if (!proto) {
	if (!js_GetClassPrototype(cx, clasp, &proto) ||
	    (!proto && !js_GetClassPrototype(cx, &js_ObjectClass, &proto))) {
	    cx->newborn[GCX_OBJECT] = NULL;
	    return NULL;
	}
    }

    /* Share the given prototype's scope (create one if necessary). */
    if (proto && proto->map->clasp == clasp) {
	scope = (JSScope *)proto->map;
	JS_LOCK_VOID(cx, obj->map = (JSObjectMap *)js_HoldScope(cx, scope));
    } else {
	scope = js_NewScope(cx, clasp, obj);
	if (!scope) {
	    cx->newborn[GCX_OBJECT] = NULL;
	    return NULL;
	}
	scope->map.nrefs = 1;
	obj->map = &scope->map;
    }

    /* Set the prototype and parent properties. */
    if (!js_SetSlot(cx, obj, JSSLOT_PROTO, OBJECT_TO_JSVAL(proto)) ||
	!js_SetSlot(cx, obj, JSSLOT_PARENT, OBJECT_TO_JSVAL(parent))) {
	cx->newborn[GCX_OBJECT] = NULL;
	return NULL;
    }
    return obj;
}

static JSBool
FindConstructor(JSContext *cx, JSClass *clasp, jsval *vp)
{
    JSAtom *atom;
    JSObject *obj, *tmp;
    JSBool ok;

    PR_ASSERT(JS_IS_LOCKED(cx));

    /* XXX pre-atomize in JS_InitClass! */
    atom = js_Atomize(cx, clasp->name, strlen(clasp->name), 0);
    if (!atom)
	return JS_FALSE;

    if (cx->fp && (tmp = cx->fp->scopeChain)) {
	/* Find the topmost object in the scope chain. */
	do {
	    obj = tmp;
	    tmp = OBJ_GET_PARENT(obj);
	} while (tmp);
    } else {
	obj = cx->globalObject;
	if (!obj) {
	    *vp = JSVAL_VOID;
	    return JS_TRUE;
	}
    }

    ok = (js_GetProperty(cx, obj, (jsval)atom, vp) != NULL);
    js_DropAtom(cx, atom);
    return JS_TRUE;
}

JSObject *
js_ConstructObject(JSContext *cx, JSClass *clasp, JSObject *proto,
		   JSObject *parent)
{
    JSObject *obj;
    JSBool ok;
    jsval ctor, rval;

    obj = js_NewObject(cx, clasp, proto, parent);
    if (!obj)
	return NULL;
    JS_LOCK_VOID(cx, ok = FindConstructor(cx, clasp, &ctor));
    if (!ok || !js_Call(cx, obj, ctor, 0, NULL, &rval)) {
	cx->newborn[GCX_OBJECT] = NULL;
	return NULL;
    }
    return JSVAL_IS_OBJECT(rval) ? JSVAL_TO_OBJECT(rval) : obj;
}

void
js_FinalizeObject(JSContext *cx, JSObject *obj)
{
    JSScope *scope;

    PR_ASSERT(JS_IS_LOCKED(cx));

    /* Cope with stillborn objects that have no scope. */
    scope = (JSScope *)obj->map;
    if (!scope)
	return;

#if JS_HAS_OBJ_WATCHPOINT
    /* Remove all watchpoints with weak links to obj. */
    JS_ClearWatchPointsForObject(cx, obj);
#endif

    /* Finalize obj first, in case it needs map and slots. */
    scope->map.clasp->finalize(cx, obj);
    if (scope->object == obj)
	scope->object = NULL;

    /* Drop scope and free slots. */
    js_DropScope(cx, scope);
    obj->map = NULL;
    JS_free(cx, obj->slots);
    obj->slots = NULL;
}

jsval
js_GetSlot(JSContext *cx, JSObject *obj, uint32 slot)
{
    if (!obj->slots || slot >= obj->map->freeslot)
	return JSVAL_NULL;
    return obj->slots[slot];
}

JSBool
js_SetSlot(JSContext *cx, JSObject *obj, uint32 slot, jsval value)
{
    uint32 i;

    PR_ASSERT(slot < JS_INITIAL_NSLOTS);
    if (!obj->slots) {
	obj->slots = JS_malloc(cx, JS_INITIAL_NSLOTS * sizeof(jsval));
	if (!obj->slots)
	    return JS_FALSE;
	for (i = 0; i < (uint32)JS_INITIAL_NSLOTS; i++) {
	    if (i == slot)
		continue;
	    obj->slots[i] = JSVAL_VOID;
	}
	if (obj->map->nslots == 0)
	    obj->map->nslots = JS_INITIAL_NSLOTS;
    }
    if (slot >= obj->map->freeslot)
	obj->map->freeslot = slot + 1;
    obj->slots[slot] = value;
    return JS_TRUE;
}

JSBool
js_AllocSlot(JSContext *cx, JSObject *obj, uint32 *slotp)
{
    JSObjectMap *map;
    uint32 nslots;
    size_t nbytes;
    jsval *newslots;

    PR_ASSERT(JS_IS_LOCKED(cx));
    map = obj->map;
    PR_ASSERT(((JSScope *)map)->object == obj);

    nslots = map->nslots;
    if (map->freeslot >= nslots) {
	nslots = PR_MAX(map->freeslot, nslots);
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

	if (obj->slots)
	    newslots = JS_realloc(cx, obj->slots, nbytes);
	else
	    newslots = JS_malloc(cx, nbytes);
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

    PR_ASSERT(JS_IS_LOCKED(cx));
    map = obj->map;
    PR_ASSERT(((JSScope *)map)->object == obj);
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
    PR_BEGIN_MACRO                                                            \
	if (_str->length == 0)                                                \
	    id = JSVAL_ZERO;                                                  \
    PR_END_MACRO
#else
#define CHECK_FOR_EMPTY_INDEX(id) /* nothing */
#endif

#define CHECK_FOR_FUNNY_INDEX(id)                                             \
    PR_BEGIN_MACRO                                                            \
	if (!JSVAL_IS_INT(id)) {                                              \
	    JSAtom *_atom = (JSAtom *)id;                                     \
	    JSString *_str = ATOM_TO_STRING(_atom);                           \
	    const jschar *_cp = _str->chars;                                  \
	    if (JS7_ISDEC(*_cp)) {                                            \
		jsint _index = JS7_UNDEC(*_cp);                               \
		_cp++;                                                        \
		if (_index != 0) {                                            \
		    while (JS7_ISDEC(*_cp)) {                                 \
			_index = 10 * _index + JS7_UNDEC(*_cp);               \
			_cp++;                                                \
		    }                                                         \
		}                                                             \
		if (*_cp == 0 && INT_FITS_IN_JSVAL(_index))                   \
		    id = INT_TO_JSVAL(_index);                                \
	    } else {                                                          \
		CHECK_FOR_EMPTY_INDEX(id);                                    \
	    }                                                                 \
	}                                                                     \
    PR_END_MACRO

JSProperty *
js_DefineProperty(JSContext *cx, JSObject *obj, jsval id, jsval value,
		  JSPropertyOp getter, JSPropertyOp setter, uintN flags)
{
    JSRuntime *rt;
    JSScope *scope;
    JSProperty *prop;

    rt = cx->runtime;
    PR_ASSERT(JS_IS_RUNTIME_LOCKED(rt));

    /* Handle old bug that treated empty string as zero index. */
    CHECK_FOR_FUNNY_INDEX(id);

    /* Use the object's class getter and setter by default. */
    if (!getter)
	getter = obj->map->clasp->getProperty;
    if (!setter)
	setter = obj->map->clasp->setProperty;

    /* Find a sharable scope, or get a new one for obj. */
    scope = js_MutateScope(cx, obj, id, getter, setter, flags, &prop);
    if (!scope)
	return NULL;

    /* Add the property only if MutateScope didn't find a shared scope. */
    if (!prop) {
	prop = js_NewProperty(cx, scope, id, getter, setter, flags);
	if (!prop)
	    return NULL;
	if (!obj->map->clasp->addProperty(cx, obj, prop->id, &value) ||
	    !scope->ops->add(cx, scope, id, prop)) {
	    js_DestroyProperty(cx, prop);
	    return NULL;
	}
	PROPERTY_CACHE_FILL(cx, &rt->propertyCache, obj, id, prop);
    }

    PR_ASSERT(prop->slot < obj->map->freeslot);
    obj->slots[prop->slot] = value;
    return prop;
}

JS_FRIEND_API(JSBool)
js_LookupProperty(JSContext *cx, JSObject *obj, jsval id, JSObject **objp,
		  JSProperty **propp)
{
    JSObject *pobj;
    PRHashNumber hash;
    JSScope *prevscope, *scope;
    JSSymbol *sym;
    JSClass *clasp;
    JSResolveOp resolve;
    JSNewResolveOp newresolve;

    PR_ASSERT(JS_IS_LOCKED(cx));

    /* Handle old bug that treated empty string as zero index. */
    CHECK_FOR_FUNNY_INDEX(id);

    /* Search scopes starting with obj and following the prototype link. */
    hash = js_HashValue(id);
    prevscope = NULL;
    do {
	scope = (JSScope *)obj->map;
	if (scope == prevscope)
	    continue;
	sym = scope->ops->lookup(cx, scope, id, hash);
	if (!sym && objp) {
	    clasp = scope->map.clasp;
	    resolve = clasp->resolve;
	    if (resolve != JS_ResolveStub) {
		if (clasp->flags & JSCLASS_NEW_RESOLVE) {
		    newresolve = (JSNewResolveOp)resolve;
		    pobj = NULL;
		    if (!newresolve(cx, obj, js_IdToValue(id), &pobj))
			return JS_FALSE;
		    if (pobj) {
			*objp = pobj;
			scope = (JSScope *)pobj->map;
			sym = scope->ops->lookup(cx, scope, id, hash);
		    }
		} else {
		    if (!resolve(cx, obj, js_IdToValue(id)))
			return JS_FALSE;
		    scope = (JSScope *)obj->map;
		    sym = scope->ops->lookup(cx, scope, id, hash);
		}
	    }
	}
	if (sym) {
	    *propp = sym_property(sym);
	    return JS_TRUE;
	}
	prevscope = scope;
    } while ((obj = OBJ_GET_PROTO(obj)) != NULL);
    *propp = NULL;
    return JS_TRUE;
}

JSBool
js_FindProperty(JSContext *cx, jsval id, JSObject **objp, JSProperty **propp)
{
    JSRuntime *rt;
    JSObject *obj, *parent, *lastobj;
    JSProperty *prop;

    rt = cx->runtime;
    PR_ASSERT(JS_IS_RUNTIME_LOCKED(rt));

    for (obj = cx->fp->scopeChain; obj; obj = parent) {
	/* Try the property cache and return immediately on cache hit. */
	PROPERTY_CACHE_TEST(&rt->propertyCache, obj, id, prop);
	if (PROP_FOUND(prop)) {
	    *objp = obj;
	    *propp = prop;
	    return JS_TRUE;
	}

	/*
	 * Set parent here, after cache hit to minimize cycles in that case,
	 * but before js_LookupProperty, which might change obj.
	 */
	parent = OBJ_GET_PARENT(obj);

	/* If cache miss (not cached-as-not-found), take the slow path. */
	if (!prop) {
	    if (!js_LookupProperty(cx, obj, id, &obj, &prop))
		return JS_FALSE;
	    if (prop) {
		PROPERTY_CACHE_FILL(cx, &rt->propertyCache, obj, id, prop);
		*objp = obj;
		*propp = prop;
		return JS_TRUE;
	    }

	    /* No such property -- cache obj[id] as not-found. */
	    PROPERTY_CACHE_FILL(cx, &rt->propertyCache, obj, id,
				PROP_NOT_FOUND);
	}
	lastobj = obj;
    }
    *objp = lastobj;
    *propp = NULL;
    return JS_TRUE;
}

JSBool
js_FindVariable(JSContext *cx, jsval id, JSObject **objp, JSProperty **propp)
{
    JSRuntime *rt;
    JSObject *obj;
    JSProperty *prop;

    rt = cx->runtime;
    PR_ASSERT(JS_IS_RUNTIME_LOCKED(rt));

    /*
     * First look for id's property along the "with" statement and the
     * statically-linked scope chains.
     */
    if (!js_FindProperty(cx, id, objp, propp))
	return JS_FALSE;
    if (*propp)
	return JS_TRUE;

    /*
     * Use the top-level scope from the scope chain, which won't end in the
     * same scope as cx->globalObject's for cross-context function calls.
     */
    obj = *objp;
    PR_ASSERT(obj);

    /*
     * Make a top-level variable.
     */
    prop = js_DefineProperty(cx, obj, id, JSVAL_VOID, NULL, NULL,
			     JSPROP_ENUMERATE);
    if (!prop)
	return JS_FALSE;
    PROPERTY_CACHE_FILL(cx, &rt->propertyCache, obj, id, prop);
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

    PR_ASSERT(JS_IS_LOCKED(cx));

    fp = cx->fp;
    for (obj = fp->scopeChain, withobj = NULL; ; obj = parent) {
	parent = OBJ_GET_PARENT(obj);
	clasp = obj->map->clasp;
	if (!parent || clasp != &js_WithClass)
	    break;
	withobj = obj;
    }

    fun = (clasp == &js_FunctionClass) ? JS_GetPrivate(cx, obj) : NULL;
#if JS_HAS_CALL_OBJECT
    if (fun && fun->script) {
	for (; fp && fp->fun != fun; fp = fp->down)
	    ;
	if (fp) {
	    obj = js_GetCallObject(cx, fp, parent);
	    if (withobj)
		OBJ_SET_PARENT(withobj, obj);
	}
    }
#endif

    *funp = fun;
    return obj;
}

JSProperty *
js_GetProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSProperty *prop;
    JSObject *obj2;
    jsint slot;

    PR_ASSERT(JS_IS_LOCKED(cx));

    if (!js_LookupProperty(cx, obj, id, &obj, &prop))
	return NULL;
    if (!prop) {
	prop = js_DefineProperty(cx, obj, id,
#if JS_BUG_NULL_INDEX_PROPS
				 (JSVAL_IS_INT(id) && JSVAL_TO_INT(id) >= 0)
				 ? JSVAL_NULL
				 : JSVAL_VOID,
#else
				 JSVAL_VOID,
#endif
				 NULL, NULL, 0);
	if (!prop)
	    return NULL;
    }
    obj2 = prop->object;
    slot = prop->slot;
    *vp = obj2->slots[slot];
    if (!prop->getter(cx, obj, prop->id, vp))
	return NULL;
    obj2->slots[slot] = *vp;
    return prop;
}

JSProperty *
js_SetProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSRuntime *rt;
    JSScope *scope, *protoScope;
    JSProperty *prop, *protoProp;
    PRHashNumber hash;
    JSSymbol *sym, *protoSym;
    JSObject *proto, *assignobj;
    jsval pval, aval, rval;
    jsint slot;
    JSErrorReporter older;
    JSString *str;

    rt = cx->runtime;
    PR_ASSERT(JS_IS_RUNTIME_LOCKED(rt));

#ifdef SCOPE_TABLE_NOTYET
    /* XXX hash recompute */
    /* XXX protoProp->getter, protoProp->setter, protoProp->flags */
    scope = js_MutateScope(cx, obj, id, getter, setter, flags, &prop);
#endif
    scope = js_GetMutableScope(cx, obj);
    if (!scope)
	return NULL;

    /* Handle old bug that treated empty string as zero index. */
    CHECK_FOR_FUNNY_INDEX(id);

    hash = js_HashValue(id);
    sym = scope->ops->lookup(cx, scope, id, hash);
    if (sym) {
	prop = sym_property(sym);
#if JS_HAS_OBJ_WATCHPOINT
	if (!prop) {
	    /*
	     * Deleted property place-holder, could have a watchpoint that
	     * holds the deleted-but-watched property.
	     */
	    prop = js_FindWatchPoint(rt, obj, js_IdToValue(id));
	}
#endif
    } else {
	prop = NULL;
    }

    if (!prop) {
	/* Find a prototype property with the same id. */
	proto = OBJ_GET_PROTO(obj);
	protoProp = NULL;
	while (proto) {
	    protoScope = (JSScope *)proto->map;
	    protoSym = protoScope->ops->lookup(cx, protoScope, id, hash);
	    if (protoSym) {
		protoProp = sym_property(protoSym);
		if (protoProp)
		    break;
	    }
	    proto = OBJ_GET_PROTO(proto);
	}

	/* Make a new property descriptor with the right heritage. */
	if (protoProp) {
	    prop = js_NewProperty(cx, scope, id,
				  protoProp->getter, protoProp->setter,
				  protoProp->flags | JSPROP_ENUMERATE);
	    prop->id = protoProp->id;
	} else {
	    prop = js_NewProperty(cx, scope, id,
				  scope->map.clasp->getProperty,
				  scope->map.clasp->setProperty,
				  JSPROP_ENUMERATE);
	}
	if (!prop)
	    return NULL;

	if (!obj->map->clasp->addProperty(cx, obj, prop->id, vp)) {
	    js_DestroyProperty(cx, prop);
	    return NULL;
	}

	/* Initialize new properties to undefined. */
	obj->slots[prop->slot] = JSVAL_VOID;

	if (sym) {
	    /* Null-valued symbol left behind from a delete operation. */
	    sym->entry.value = js_HoldProperty(cx, prop);
	}
    }

    if (!sym) {
	/* Need a new symbol as well as a new property. */
	sym = scope->ops->add(cx, scope, id, prop);
	if (!sym)
	    return NULL;
#if JS_BUG_AUTO_INDEX_PROPS
	{
	    jsval id2 = INT_TO_JSVAL(prop->slot - JSSLOT_START);
	    if (!scope->ops->add(cx, scope, id2, prop)) {
		scope->ops->remove(cx, scope, id);
		return NULL;
	    }
	    PROPERTY_CACHE_FILL(cx, &rt->propertyCache, obj, id2, prop);
	}
#endif
	PROPERTY_CACHE_FILL(cx, &rt->propertyCache, obj, id, prop);
    }

    /* Get the current property value from its slot. */
    PR_ASSERT(prop->slot < obj->map->freeslot);
    slot = prop->slot;
    pval = obj->slots[slot];

    /* Evil overloaded operator assign() hack. */
    if (JSVAL_IS_OBJECT(pval)) {
	assignobj = JSVAL_TO_OBJECT(pval);
	if (assignobj) {
	    older = JS_SetErrorReporter(cx, NULL);
	    if (js_GetProperty(cx, assignobj, (jsval)rt->atomState.assignAtom,
			       &aval) &&
		JSVAL_IS_FUNCTION(aval) &&
		js_Call(cx, assignobj, aval, 1, vp, &rval)) {
		*vp = rval;
		JS_SetErrorReporter(cx, older);
		prop->flags |= JSPROP_ASSIGNHACK;
		return prop;
	    }
	    JS_SetErrorReporter(cx, older);
	}
    }

    /* Check for readonly *after* the assign() hack. */
    if (prop->flags & JSPROP_READONLY) {
	if (!JSVERSION_IS_ECMA(cx->version)) {
	    str = js_ValueToSource(cx, js_IdToValue(id));
	    if (str) {
		JS_ReportError(cx, "%s is read-only",
			       JS_GetStringBytes(str));
	    }
	}
	return NULL;
    }

    /* Let the setter modify vp before copying from it to obj->slots[slot]. */
    if (!prop->setter(cx, obj, prop->id, vp))
	return NULL;
    GC_POKE(cx, pval);
    obj->slots[slot] = *vp;

    /* Setting a property makes it enumerable. */
    prop->flags |= JSPROP_ENUMERATE;
    return prop;
}

JSBool
js_DeleteProperty(JSContext *cx, JSObject *obj, jsval id, jsval *rval)
{
#if JS_HAS_PROP_DELETE
    JSProperty *prop;

    PR_ASSERT(JS_IS_LOCKED(cx));

    if (!js_LookupProperty(cx, obj, id, NULL, &prop))
	return JS_FALSE;
    if (!prop)
	return JS_TRUE;
    return js_DeleteProperty2(cx, obj, prop, id, rval);
#else
    jsval null = JSVAL_NULL;

    *rval = JSVAL_VOID;
    return (js_SetProperty(cx, obj, id, &null) != NULL);
#endif
}

JSBool
js_DeleteProperty2(JSContext *cx, JSObject *obj, JSProperty *prop, jsval id,
		   jsval *rval)
{
#if JS_HAS_PROP_DELETE
    JSRuntime *rt;
    JSString *str;
    JSScope *scope;
    JSObject *proto;
    PRHashNumber hash;
    JSSymbol *sym;

    rt = cx->runtime;
    PR_ASSERT(JS_IS_RUNTIME_LOCKED(rt));

    *rval = JSVERSION_IS_ECMA(cx->version) ? JSVAL_TRUE : JSVAL_VOID;

    if (prop->flags & JSPROP_PERMANENT) {
	if (JSVERSION_IS_ECMA(cx->version)) {
	    *rval = JSVAL_FALSE;
	    return JS_TRUE;
	}
	str = js_ValueToSource(cx, js_IdToValue(id));
	if (str)
	    JS_ReportError(cx, "%s is permanent", JS_GetStringBytes(str));
	return JS_FALSE;
    }

    if (!obj->map->clasp->delProperty(cx, obj, prop->id,
				      &prop->object->slots[prop->slot])) {
	return JS_FALSE;
    }

    /* Handle old bug that treated empty string as zero index. */
    CHECK_FOR_FUNNY_INDEX(id);

    GC_POKE(cx, prop->object->slots[prop->slot]);
    scope = (JSScope *)obj->map;
    proto = scope->object;
    if (proto == obj) {
	/* The object has its own scope, so remove id if it was found here. */
	if (prop->object == obj) {
	    /* Purge cache only if prop is not about to be destroyed. */
	    if (prop->nrefs != 1) {
		PROPERTY_CACHE_FILL(cx, &rt->propertyCache, obj, id,
				    PROP_NOT_FOUND);
	    }
#if JS_HAS_OBJ_WATCHPOINT
	    if (prop->setter == js_watch_set) {
		/*
		 * Keep the symbol around with null value in case of re-set.
		 * The watchpoint will hold the "deleted" property until it
		 * is removed by obj_unwatch or a native JS_ClearWatchPoint.
		 * See js_SetProperty for the re-set logic.
		 */
		for (sym = prop->symbols; sym; sym = sym->next) {
		    if (sym_id(sym) == id) {
			sym->entry.value = NULL;
			prop = js_DropProperty(cx, prop);
			PR_ASSERT(prop);
			return JS_TRUE;
		    }
		}
	    }
#endif
	    scope->ops->remove(cx, scope, id);
	}
	proto = OBJ_GET_PROTO(obj);
	if (!proto)
	    return JS_TRUE;
    }

    /* Search shared prototype scopes for an inherited property to hide. */
    hash = js_HashValue(id);
    do {
	scope = (JSScope *)proto->map;
	sym = scope->ops->lookup(cx, scope, id, hash);
	if (sym) {
	    /* Add a null-valued symbol to hide the prototype property. */
	    scope = js_GetMutableScope(cx, obj);
	    if (!scope)
		return JS_FALSE;
	    if (!scope->ops->add(cx, scope, id, NULL))
		return JS_FALSE;
	    PROPERTY_CACHE_FILL(cx, &rt->propertyCache, obj, id,
				PROP_NOT_FOUND);
	    return JS_TRUE;
	}
	proto = OBJ_GET_PROTO(proto);
    } while (proto);
    return JS_TRUE;
#else
    jsval null = JSVAL_NULL;
    return (js_SetProperty(cx, obj, id, &null) != NULL);
#endif
}

JSBool
js_GetClassPrototype(JSContext *cx, JSClass *clasp, JSObject **protop)
{
    JSBool ok;
    JSObject *ctor;
    jsval pval;
    JSProperty *prop;

    *protop = NULL;
    JS_LOCK(cx);
    ok = FindConstructor(cx, clasp, &pval);
    if (!ok || !JSVAL_IS_FUNCTION(pval))
	goto out;
    ctor = JSVAL_TO_OBJECT(pval);
    if (!js_LookupProperty(cx, ctor,
			   (jsval)cx->runtime->atomState.classPrototypeAtom,
			   NULL, &prop)) {
	ok = JS_FALSE;
	goto out;
    }
    if (prop) {
	pval = prop->object->slots[prop->slot];
	if (JSVAL_IS_OBJECT(pval))
	    *protop = JSVAL_TO_OBJECT(pval);
    }
out:
    JS_UNLOCK(cx);
    return ok;
}

JSBool
js_SetClassPrototype(JSContext *cx, JSFunction *fun, JSObject *obj)
{
    JSBool ok;

    ok = JS_TRUE;
    JS_LOCK(cx);
    if (!js_DefineProperty(cx, fun->object,
			   (jsval)cx->runtime->atomState.classPrototypeAtom,
			   OBJECT_TO_JSVAL(obj), NULL, NULL,
			   JSPROP_READONLY | JSPROP_PERMANENT)) {
	ok = JS_FALSE;
	goto out;
    }
    if (!js_DefineProperty(cx, obj,
			   (jsval)cx->runtime->atomState.constructorAtom,
			   OBJECT_TO_JSVAL(fun->object), NULL, NULL,
			   JSPROP_READONLY | JSPROP_PERMANENT)) {
	ok = JS_FALSE;
	goto out;
    }
out:
    JS_UNLOCK(cx);
    return ok;
}

JSString *
js_ObjectToString(JSContext *cx, JSObject *obj)
{
    jsval v, *mark, *argv;
    JSString *str;

    if (!obj)
	return ATOM_TO_STRING(cx->runtime->atomState.nullAtom);
    v = JSVAL_VOID;
    if (!obj->map->clasp->convert(cx, obj, JSTYPE_STRING, &v))
	return NULL;
    if (JSVAL_IS_STRING(v))
	return JSVAL_TO_STRING(v);

    /* Try the toString method if it's defined. */
    js_TryMethod(cx, obj, cx->runtime->atomState.toStringAtom, 0, NULL, &v);
    if (JSVAL_IS_STRING(v))
	return JSVAL_TO_STRING(v);
#if JS_BUG_EAGER_TOSTRING
    js_TryValueOf(cx, obj, JSTYPE_STRING, &v);
    if (JSVAL_IS_STRING(v))
	return JSVAL_TO_STRING(v);
#endif
    mark = PR_ARENA_MARK(&cx->stackPool);
    PR_ARENA_ALLOCATE(argv, &cx->stackPool, OBJ_TOSTRING_NARGS * sizeof(jsval));
    if (!argv || !js_obj_toString(cx, obj, OBJ_TOSTRING_NARGS, argv, &v))
	str = NULL;
    else
	str = JSVAL_TO_STRING(v);
    PR_ARENA_RELEASE(&cx->stackPool, mark);
    return str;
}

JSBool
js_ValueToObject(JSContext *cx, jsval v, JSObject **objp)
{
    JSObject *obj;

    if (JSVAL_IS_NULL(v) || JSVAL_IS_VOID(v)) {
	obj = NULL;
    } else if (JSVAL_IS_OBJECT(v)) {
	obj = JSVAL_TO_OBJECT(v);
	if (!obj->map->clasp->convert(cx, obj, JSTYPE_OBJECT, &v))
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
	    PR_ASSERT(JSVAL_IS_BOOLEAN(v));
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
    JSString *name;

    if (!js_ValueToObject(cx, v, &obj))
	return NULL;
    if (!obj) {
	name = js_ValueToSource(cx, v);
	if (name) {
	    JS_ReportError(cx, "%s has no properties",
			   JS_GetStringBytes(name));
	}
    }
    return obj;
}

void
js_TryValueOf(JSContext *cx, JSObject *obj, JSType type, jsval *rval)
{
#if JS_HAS_VALUEOF_HINT
    jsval argv[1];

    argv[0] = ATOM_KEY(cx->runtime->atomState.typeAtoms[type]);
    js_TryMethod(cx, obj, cx->runtime->atomState.valueOfAtom, 1, argv, rval);
#else
    js_TryMethod(cx, obj, cx->runtime->atomState.valueOfAtom, 0, NULL, rval);
#endif
}

void
js_TryMethod(JSContext *cx, JSObject *obj, JSAtom *atom,
	     uintN argc, jsval *argv, jsval *rval)
{
    JSErrorReporter older;
    JSProperty *prop;
    jsval fval;

    older = JS_SetErrorReporter(cx, NULL);
    JS_LOCK_VOID(cx, prop = js_GetProperty(cx, obj, (jsval)atom, &fval));
    if (prop &&
	JSVAL_IS_OBJECT(fval) &&
	fval != JSVAL_NULL) {
	(void) js_Call(cx, obj, fval, argc, argv, rval);
    }
    JS_SetErrorReporter(cx, older);
}
