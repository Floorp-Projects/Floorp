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
 * JavaScript API.
 */
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "prtypes.h"
#ifndef NSPR20
#include "prarena.h"
#else
#include "plarena.h"
#endif
#include "prlog.h"
#include "prclist.h"
#include "jsapi.h"
#include "jsarray.h"
#include "jsatom.h"
#include "jsbool.h"
#include "jsconfig.h"
#include "jscntxt.h"
#include "jsconfig.h"
#include "jsdate.h"
#include "jsemit.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jsinterp.h"
#include "jslock.h"
#include "jsmath.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsopcode.h"
#include "jsparse.h"
#include "jsregexp.h"
#include "jsscan.h"
#include "jsscope.h"
#include "jsscript.h"
#include "jsstr.h"

PR_IMPLEMENT(jsval)
JS_GetNaNValue(JSContext *cx)
{
    return DOUBLE_TO_JSVAL(cx->runtime->jsNaN);
}

PR_IMPLEMENT(jsval)
JS_GetNegativeInfinityValue(JSContext *cx)
{
    return DOUBLE_TO_JSVAL(cx->runtime->jsNegativeInfinity);
}

PR_IMPLEMENT(jsval)
JS_GetPositiveInfinityValue(JSContext *cx)
{
    return DOUBLE_TO_JSVAL(cx->runtime->jsPositiveInfinity);
}

PR_IMPLEMENT(jsval)
JS_GetEmptyStringValue(JSContext *cx)
{
    return STRING_TO_JSVAL(cx->runtime->emptyString);
}

PR_IMPLEMENT(JSBool)
JS_ConvertValue(JSContext *cx, jsval v, JSType type, jsval *vp)
{
    JSBool ok, b;
    JSObject *obj;
    JSFunction *fun;
    JSString *str;
    jsdouble d, *dp;

    JS_LOCK(cx);
    switch (type) {
      case JSTYPE_VOID:
	*vp = JSVAL_VOID;
	break;
      case JSTYPE_OBJECT:
	ok = js_ValueToObject(cx, v, &obj);
	if (ok)
	    *vp = OBJECT_TO_JSVAL(obj);
	break;
      case JSTYPE_FUNCTION:
	fun = js_ValueToFunction(cx, v);
	ok = (fun != NULL);
	if (ok)
	    *vp = OBJECT_TO_JSVAL(fun->object);
	break;
      case JSTYPE_STRING:
	str = js_ValueToString(cx, v);
	ok = (str != NULL);
	if (ok)
	    *vp = STRING_TO_JSVAL(str);
	break;
      case JSTYPE_NUMBER:
	ok = js_ValueToNumber(cx, v, &d);
	if (ok) {
	    dp = js_NewDouble(cx, d);
	    ok = (dp != NULL);
	    if (ok)
		*vp = DOUBLE_TO_JSVAL(dp);
	}
	break;
      case JSTYPE_BOOLEAN:
	ok = js_ValueToBoolean(cx, v, &b);
	if (ok)
	    *vp = BOOLEAN_TO_JSVAL(b);
	break;
      default:
	JS_ReportError(cx, "unknown type %d", (int)type);
	ok = JS_FALSE;
	break;
    }
    JS_UNLOCK(cx);
    return ok;
}

PR_IMPLEMENT(JSBool)
JS_ValueToObject(JSContext *cx, jsval v, JSObject **objp)
{
    JS_LOCK_AND_RETURN_BOOL(cx, js_ValueToObject(cx, v, objp));
}

PR_IMPLEMENT(JSFunction *)
JS_ValueToFunction(JSContext *cx, jsval v)
{
    JS_LOCK_AND_RETURN_TYPE(cx, JSFunction *, js_ValueToFunction(cx, v));
}

PR_IMPLEMENT(JSString *)
JS_ValueToString(JSContext *cx, jsval v)
{
    JS_LOCK_AND_RETURN_STRING(cx, js_ValueToString(cx, v));
}

PR_IMPLEMENT(JSBool)
JS_ValueToNumber(JSContext *cx, jsval v, jsdouble *dp)
{
    JS_LOCK_AND_RETURN_BOOL(cx, js_ValueToNumber(cx, v, dp));
}

PR_IMPLEMENT(JSBool)
JS_ValueToInt32(JSContext *cx, jsval v, int32 *ip)
{
    JS_LOCK_AND_RETURN_BOOL(cx, js_ValueToInt32(cx, v, ip));
}

PR_IMPLEMENT(JSBool)
JS_ValueToUint16(JSContext *cx, jsval v, uint16 *ip)
{
    JS_LOCK_AND_RETURN_BOOL(cx, js_ValueToUint16(cx, v, ip));
}

PR_IMPLEMENT(JSBool)
JS_ValueToBoolean(JSContext *cx, jsval v, JSBool *bp)
{
    JS_LOCK_AND_RETURN_BOOL(cx, js_ValueToBoolean(cx, v, bp));
}

PR_IMPLEMENT(JSType)
JS_TypeOfValue(JSContext *cx, jsval v)
{
    JSType type;
    JSObject *obj;

    if (JSVAL_IS_VOID(v)) {
	type = JSTYPE_VOID;
    } else if (JSVAL_IS_OBJECT(v)) {
	obj = JSVAL_TO_OBJECT(v);
	if (obj &&
	    (obj->map->clasp == &js_FunctionClass
#if JS_HAS_LEXICAL_CLOSURE
	     || obj->map->clasp == &js_ClosureClass
#endif
	     )) {
	    type = JSTYPE_FUNCTION;
	} else {
	    type = JSTYPE_OBJECT;
	}
    } else if (JSVAL_IS_NUMBER(v)) {
	type = JSTYPE_NUMBER;
    } else if (JSVAL_IS_STRING(v)) {
	type = JSTYPE_STRING;
    } else if (JSVAL_IS_BOOLEAN(v)) {
	type = JSTYPE_BOOLEAN;
    }
    return type;
}

PR_IMPLEMENT(const char *)
JS_GetTypeName(JSContext *cx, JSType type)
{
    if ((uintN)type >= (uintN)JSTYPE_LIMIT)
	return NULL;
    return js_type_str[type];
}

/************************************************************************/

PR_IMPLEMENT(JSRuntime *)
JS_Init(uint32 maxbytes)
{
    JSRuntime *rt;

    rt = malloc(sizeof(JSRuntime));
    if (!rt)
	return NULL;
    memset(rt, 0, sizeof(JSRuntime));
    if (!js_InitGC(rt, maxbytes)) {
	free(rt);
	return NULL;
    }
    rt->propertyCache.empty = JS_TRUE;
    PR_INIT_CLIST(&rt->contextList);
    PR_INIT_CLIST(&rt->trapList);
    PR_INIT_CLIST(&rt->watchPointList);
    return rt;
}

PR_IMPLEMENT(void)
JS_Finish(JSRuntime *rt)
{
    js_FinishGC(rt);
    free(rt);
}

PR_IMPLEMENT(void)
JS_Lock(JSRuntime *rt)
{
    JS_LOCK_RUNTIME(rt);
}

PR_IMPLEMENT(void)
JS_Unlock(JSRuntime *rt)
{
    JS_UNLOCK_RUNTIME(rt);
}

PR_IMPLEMENT(JSContext *)
JS_NewContext(JSRuntime *rt, size_t stacksize)
{
    JSContext *cx;

    JS_LOCK_RUNTIME(rt);
    cx = js_NewContext(rt, stacksize);
    JS_UNLOCK_RUNTIME(rt);
    return cx;
}

PR_IMPLEMENT(void)
JS_DestroyContext(JSContext *cx)
{
    JS_LOCK_VOID(cx, js_DestroyContext(cx));
}

PR_IMPLEMENT(JSRuntime *)
JS_GetRuntime(JSContext *cx)
{
    return cx->runtime;
}

PR_IMPLEMENT(JSContext *)
JS_ContextIterator(JSRuntime *rt, JSContext **iterp)
{
    JSContext *cx;

    JS_LOCK_RUNTIME(rt);
    cx = js_ContextIterator(rt, iterp);
    JS_UNLOCK_RUNTIME(rt);
    return cx;
}

PR_IMPLEMENT(JSVersion)
JS_GetVersion(JSContext *cx)
{
    return cx->version;
}

PR_IMPLEMENT(JSVersion)
JS_SetVersion(JSContext *cx, JSVersion version)
{
    JSVersion oldVersion;

    oldVersion = cx->version;
    cx->version = version;

#if !JS_BUG_FALLIBLE_EQOPS
    if (cx->version == JSVERSION_1_2) {
	cx->jsop_eq = JSOP_NEW_EQ;
	cx->jsop_ne = JSOP_NEW_NE;
    } else {
	cx->jsop_eq = JSOP_EQ;
	cx->jsop_ne = JSOP_NE;
    }
#endif /* !JS_BUG_FALLIBLE_EQOPS */

#if JS_HAS_EXPORT_IMPORT
    /* XXX this might fail due to low memory */
    JS_LOCK_VOID(cx, js_InitScanner(cx));
#endif /* JS_HAS_EXPORT_IMPORT */

    return oldVersion;
}

PR_IMPLEMENT(JSObject *)
JS_GetGlobalObject(JSContext *cx)
{
    return cx->globalObject;
}

PR_IMPLEMENT(void)
JS_SetGlobalObject(JSContext *cx, JSObject *obj)
{
    cx->globalObject = obj;
}

PR_IMPLEMENT(JSBool)
JS_InitStandardClasses(JSContext *cx, JSObject *obj)
{
    JSBool ok;
    JSObject *proto, *fun_proto, *obj_proto, *array_proto;

    ok = JS_TRUE;
    JS_LOCK(cx);

    /* If cx has no global object, use obj so prototypes can be found. */
    if (!cx->globalObject)
	cx->globalObject = obj;

    /* Initialize the function class first so constructors can be made. */
    fun_proto = js_InitFunctionClass(cx, obj);
    if (!fun_proto) {
	ok = JS_FALSE;
	goto out;
    }

    /* Initialize the object class next so Object.prototype works. */
    obj_proto = js_InitObjectClass(cx, obj);
    if (!obj_proto) {
	ok = JS_FALSE;
	goto out;
    }

    /* Link the global object and Function.prototype to Object.prototype. */
    proto = OBJ_GET_PROTO(obj);
    if (!proto)
	OBJ_SET_PROTO(obj, obj_proto);
    proto = OBJ_GET_PROTO(fun_proto);
    if (!proto)
	OBJ_SET_PROTO(fun_proto, obj_proto);

    /* Initialize the rest of the standard objects and functions. */
    ok = (array_proto = js_InitArrayClass(cx, obj)) &&
	 js_InitCallAndClosureClasses(cx, obj, array_proto) &&
	 js_InitBooleanClass(cx, obj) &&
	 js_InitMathClass(cx, obj) &&
	 js_InitNumberClass(cx, obj) &&
	 js_InitStringClass(cx, obj) &&
#if JS_HAS_REGEXPS
	 js_InitRegExpClass(cx, obj) &&
#endif
	 js_InitDateClass(cx, obj);

out:
    JS_UNLOCK(cx);
    return ok;
}

PR_IMPLEMENT(JSObject *)
JS_GetScopeChain(JSContext *cx)
{
    return cx->fp ? cx->fp->scopeChain : NULL;
}

PR_IMPLEMENT(void *)
JS_malloc(JSContext *cx, size_t nbytes)
{
    void *p;

#ifdef XP_OS2
    if (nbytes == 0) /*DSR072897 - this sucks, but Microsoft allows it!*/
	nbytes = 1;
#endif
#ifdef TOO_MUCH_GC
    p = NULL;
#else
    p = malloc(nbytes);
#endif
    if (!p) {
	JS_LOCK_VOID(cx, js_GC(cx));
	p = malloc(nbytes);
	if (!p)
	    JS_ReportOutOfMemory(cx);
    }
    return p;
}

PR_IMPLEMENT(void *)
JS_realloc(JSContext *cx, void *p, size_t nbytes)
{
#ifndef TOO_MUCH_GC
    p = realloc(p, nbytes);
    if (!p) {
#endif
	JS_LOCK_VOID(cx, js_GC(cx));
	p = realloc(p, nbytes);
	if (!p)
	    JS_ReportOutOfMemory(cx);
#ifndef TOO_MUCH_GC
    }
#endif
    return p;
}

PR_IMPLEMENT(void)
JS_free(JSContext *cx, void *p)
{
    if (p)
	free(p);
}

PR_IMPLEMENT(char *)
JS_strdup(JSContext *cx, const char *s)
{
    char *p = JS_malloc(cx, strlen(s) + 1);
    if (!p)
	return NULL;
    return strcpy(p, s);
}

PR_IMPLEMENT(jsdouble *)
JS_NewDouble(JSContext *cx, jsdouble d)
{
    JS_LOCK_AND_RETURN_TYPE(cx, jsdouble *, js_NewDouble(cx, d));
}

PR_IMPLEMENT(JSBool)
JS_NewDoubleValue(JSContext *cx, jsdouble d, jsval *rval)
{
    JS_LOCK_AND_RETURN_BOOL(cx, js_NewDoubleValue(cx, d, rval));
}

PR_IMPLEMENT(JSBool)
JS_NewNumberValue(JSContext *cx, jsdouble d, jsval *rval)
{
    JS_LOCK_AND_RETURN_BOOL(cx, js_NewNumberValue(cx, d, rval));
}

PR_IMPLEMENT(JSBool)
JS_AddRoot(JSContext *cx, void *rp)
{
    JS_LOCK_AND_RETURN_BOOL(cx, js_AddRoot(cx, rp));
}

PR_IMPLEMENT(JSBool)
JS_RemoveRoot(JSContext *cx, void *rp)
{
    JS_LOCK_AND_RETURN_BOOL(cx, js_RemoveRoot(cx, rp));
}

PR_IMPLEMENT(JSBool)
JS_AddNamedRoot(JSContext *cx, void *rp, const char *name)
{
    JS_LOCK_AND_RETURN_BOOL(cx, js_AddNamedRoot(cx, rp, name));
}

#ifdef DEBUG

#ifndef NSPR20
#include "prhash.h"
#else
#include "plhash.h"
#endif

typedef struct NamedRootDumpArgs {
    void (*dump)(const char *name, void *rp, void *data);
    void *data;
} NamedRootDumpArgs;

PR_STATIC_CALLBACK(intN)
js_named_root_dumper(PRHashEntry *he, intN i, void *arg)
{
    NamedRootDumpArgs *args = arg;

    args->dump(he->value, (void *)he->key, args->data);
    return HT_ENUMERATE_NEXT;
}

PR_IMPLEMENT(void)
JS_DumpNamedRoots(JSRuntime *rt,
		  void (*dump)(const char *name, void *rp, void *data),
		  void *data)
{
    NamedRootDumpArgs args;

    args.dump = dump;
    args.data = data;
    JS_LOCK_RUNTIME(rt);
    PR_HashTableEnumerateEntries(rt->gcRootsHash, js_named_root_dumper, &args);
    JS_UNLOCK_RUNTIME(rt);
}

#endif /* DEBUG */

PR_IMPLEMENT(JSBool)
JS_LockGCThing(JSContext *cx, void *thing)
{
    JSBool ok;

    JS_LOCK_VOID(cx, ok = js_LockGCThing(cx, thing));
    if (!ok)
	JS_ReportError(cx, "can't lock memory");
    return ok;
}

PR_IMPLEMENT(JSBool)
JS_UnlockGCThing(JSContext *cx, void *thing)
{
    JSBool ok;

    JS_LOCK_VOID(cx, ok = js_UnlockGCThing(cx, thing));
    if (!ok)
	JS_ReportError(cx, "can't unlock memory");
    return ok;
}

PR_IMPLEMENT(void)
JS_GC(JSContext *cx)
{
    JS_LOCK(cx);
    if (!cx->fp)
	PR_FinishArenaPool(&cx->stackPool);
    PR_FinishArenaPool(&cx->codePool);
    PR_FinishArenaPool(&cx->tempPool);
    js_ForceGC(cx);
    JS_UNLOCK(cx);
}

PR_IMPLEMENT(void)
JS_MaybeGC(JSContext *cx)
{
    JSRuntime *rt;
    uint32 bytes, lastBytes;

    rt = cx->runtime;
    JS_LOCK_RUNTIME(rt);
    bytes = rt->gcBytes;
    lastBytes = rt->gcLastBytes;
    if (bytes > 8192 && bytes > lastBytes + lastBytes / 2)
	js_GC(cx);
    JS_UNLOCK_RUNTIME(rt);
}

/************************************************************************/

PR_IMPLEMENT(JSBool)
JS_PropertyStub(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    return JS_TRUE;
}

PR_IMPLEMENT(JSBool)
JS_EnumerateStub(JSContext *cx, JSObject *obj)
{
    return JS_TRUE;
}

PR_IMPLEMENT(JSBool)
JS_ResolveStub(JSContext *cx, JSObject *obj, jsval id)
{
    return JS_TRUE;
}

PR_IMPLEMENT(JSBool)
JS_ConvertStub(JSContext *cx, JSObject *obj, JSType type, jsval *vp)
{
#if JS_BUG_EAGER_TOSTRING
    if (type == JSTYPE_STRING)
	return JS_TRUE;
#endif
    js_TryValueOf(cx, obj, type, vp);
    return JS_TRUE;
}

PR_IMPLEMENT(void)
JS_FinalizeStub(JSContext *cx, JSObject *obj)
{
}

PR_IMPLEMENT(JSObject *)
JS_InitClass(JSContext *cx, JSObject *obj, JSObject *parent_proto,
	     JSClass *clasp, JSNative constructor, uintN nargs,
	     JSPropertySpec *ps, JSFunctionSpec *fs,
	     JSPropertySpec *static_ps, JSFunctionSpec *static_fs)
{
    JSAtom *atom;
    JSObject *proto, *fun_proto;
    JSFunction *fun;
    jsval junk;

    JS_LOCK(cx);
    atom = js_Atomize(cx, clasp->name, strlen(clasp->name), 0);
    if (!atom) {
	proto = NULL;
	goto out;
    }

    /* Define the constructor function in obj's scope. */
    fun = js_DefineFunction(cx, obj, atom, constructor, nargs, 0);
    if (!fun) {
	proto = NULL;
	goto out;
    }

    /* Construct a proto object for this class. */
    proto = js_NewObject(cx, clasp, parent_proto, fun->object);
    if (!proto)
	goto out;

    /* Bootstrap Function.prototype (see also JS_InitStandardClasses). */
    if (fun->object->map->clasp == clasp) {
	fun_proto = OBJ_GET_PROTO(fun->object);
	if (!fun_proto)
	    OBJ_SET_PROTO(fun->object, proto);
    }

    /* Add properties and methods to the prototype and the constructor. */
    if (!js_SetClassPrototype(cx, fun, proto) ||
	(ps && !JS_DefineProperties(cx, proto, ps)) ||
	(fs && !JS_DefineFunctions(cx, proto, fs)) ||
	(static_ps && !JS_DefineProperties(cx, fun->object, static_ps)) ||
	(static_fs && !JS_DefineFunctions(cx, fun->object, static_fs))) {
	cx->newborn[GCX_OBJECT] = NULL;
	proto = NULL;
	goto out;
    }

out:
    if (!proto)
	(void) js_DeleteProperty(cx, obj, (jsval)atom, &junk);
    if (atom)
	js_DropAtom(cx, atom);
    JS_UNLOCK(cx);
    return proto;
}

PR_IMPLEMENT(JSClass *)
JS_GetClass(JSObject *obj)
{
    return obj->map->clasp;
}

PR_IMPLEMENT(JSBool)
JS_InstanceOf(JSContext *cx, JSObject *obj, JSClass *clasp, jsval *argv)
{
    JSFunction *fun;

    if (obj->map->clasp == clasp)
	return JS_TRUE;
    if (argv) {
	JS_LOCK_VOID(cx, fun = js_ValueToFunction(cx, argv[-2]));
	if (fun) {
	    JS_ReportError(cx, "method %s.%s called on incompatible %s",
			   clasp->name, JS_GetFunctionName(fun),
			   obj->map->clasp->name);
	}
    }
    return JS_FALSE;
}

PR_IMPLEMENT(void *)
JS_GetPrivate(JSContext *cx, JSObject *obj)
{
    jsval v;

    PR_ASSERT(obj->map->clasp->flags & JSCLASS_HAS_PRIVATE);
    JS_LOCK_VOID(cx, v = js_GetSlot(cx, obj, JSSLOT_PRIVATE));
    if (!JSVAL_IS_INT(v))
	return NULL;
    return JSVAL_TO_PRIVATE(v);
}

PR_IMPLEMENT(JSBool)
JS_SetPrivate(JSContext *cx, JSObject *obj, void *data)
{
    PR_ASSERT(obj->map->clasp->flags & JSCLASS_HAS_PRIVATE);
    JS_LOCK_AND_RETURN_BOOL(cx, js_SetSlot(cx, obj, JSSLOT_PRIVATE,
					   PRIVATE_TO_JSVAL(data)));
}

PR_IMPLEMENT(void *)
JS_GetInstancePrivate(JSContext *cx, JSObject *obj, JSClass *clasp,
		      jsval *argv)
{
    if (!JS_InstanceOf(cx, obj, clasp, argv))
	return NULL;
    return JS_GetPrivate(cx, obj);
}

PR_IMPLEMENT(JSObject *)
JS_GetPrototype(JSContext *cx, JSObject *obj)
{
    JSObject *proto;

    JS_LOCK_VOID(cx,
		 proto = JSVAL_TO_OBJECT(js_GetSlot(cx, obj, JSSLOT_PROTO)));

    /* Beware ref to dead object (we may be called from obj's finalizer). */
    return proto && proto->map ? proto : NULL;
}

PR_IMPLEMENT(JSBool)
JS_SetPrototype(JSContext *cx, JSObject *obj, JSObject *proto)
{
    JS_LOCK_AND_RETURN_BOOL(cx, js_SetSlot(cx, obj, JSSLOT_PROTO,
					   OBJECT_TO_JSVAL(proto)));
}

PR_IMPLEMENT(JSObject *)
JS_GetParent(JSContext *cx, JSObject *obj)
{
    JSObject *parent;

    JS_LOCK_VOID(cx,
		 parent = JSVAL_TO_OBJECT(js_GetSlot(cx, obj, JSSLOT_PARENT)));

    /* Beware ref to dead object (we may be called from obj's finalizer). */
    return parent && parent->map ? parent : NULL;
}

PR_IMPLEMENT(JSBool)
JS_SetParent(JSContext *cx, JSObject *obj, JSObject *parent)
{
    JS_LOCK_AND_RETURN_BOOL(cx, js_SetSlot(cx, obj, JSSLOT_PARENT,
					   OBJECT_TO_JSVAL(parent)));
}

PR_IMPLEMENT(JSObject *)
JS_GetConstructor(JSContext *cx, JSObject *proto)
{
    JSProperty *prop;
    jsval cval;

    JS_LOCK(cx);
    prop = js_GetProperty(cx, proto,
			  (jsval)cx->runtime->atomState.constructorAtom,
			  &cval);
    JS_UNLOCK(cx);
    if (!prop)
	return NULL;
    if (!JSVAL_IS_FUNCTION(cval)) {
	JS_ReportError(cx, "%s has no constructor", proto->map->clasp->name);
	return NULL;
    }
    return JSVAL_TO_OBJECT(cval);
}

PR_IMPLEMENT(JSObject *)
JS_NewObject(JSContext *cx, JSClass *clasp, JSObject *proto, JSObject *parent)
{
    JS_LOCK_AND_RETURN_TYPE(cx, JSObject *,
			    js_NewObject(cx, clasp, proto, parent));
}

PR_IMPLEMENT(JSObject *)
JS_ConstructObject(JSContext *cx, JSClass *clasp, JSObject *proto,
		   JSObject *parent)
{
    JS_LOCK_AND_RETURN_TYPE(cx, JSObject *,
			    js_ConstructObject(cx, clasp, proto, parent));
}

static JSProperty *
DefineProperty(JSContext *cx, JSObject *obj, const char *name, jsval value,
	       JSPropertyOp getter, JSPropertyOp setter, uintN flags)
{
    jsval id;
    JSAtom *atom;
    JSProperty *prop;

    if (flags & JSPROP_INDEX) {
	id = INT_TO_JSVAL((jsint)name);
	atom = NULL;
    } else {
	atom = js_Atomize(cx, name, strlen(name), 0);
	if (!atom)
	    return NULL;
	id = (jsval)atom;
    }
    prop = js_DefineProperty(cx, obj, id, value, getter, setter, flags);
    if (atom)
	js_DropAtom(cx, atom);
    return prop;
}

PR_IMPLEMENT(JSObject *)
JS_DefineObject(JSContext *cx, JSObject *obj, const char *name, JSClass *clasp,
		JSObject *proto, uintN flags)
{
    JSObject *nobj;

    JS_LOCK(cx);
    nobj = js_NewObject(cx, clasp, proto, obj);
    if (nobj &&
	!DefineProperty(cx, obj, name, OBJECT_TO_JSVAL(nobj), NULL, NULL, flags)) {
	cx->newborn[GCX_OBJECT] = NULL;
	nobj = NULL;
    }
    JS_UNLOCK(cx);
    return nobj;
}

PR_IMPLEMENT(JSBool)
JS_DefineConstDoubles(JSContext *cx, JSObject *obj, JSConstDoubleSpec *cds)
{
    JSBool ok;
    jsval value;

    JS_LOCK(cx);
    for (ok = JS_TRUE; cds->name; cds++) {
#if PR_ALIGN_OF_DOUBLE == 8
	/*
	 * The GC ignores references outside its pool such as &cds->dval,
	 * so we don't need to GC-alloc constant doubles.
	 */
	jsdouble d = cds->dval;
	jsint i = (jsint)d;

	value = (JSDOUBLE_IS_INT(d, i) && INT_FITS_IN_JSVAL(i))
		? INT_TO_JSVAL(i)
		: DOUBLE_TO_JSVAL(&cds->dval);
#else
	ok = js_NewNumberValue(cx, cds->dval, &value);
	if (!ok)
	    break;
#endif
	ok = (DefineProperty(cx, obj, cds->name, value,
			     NULL, NULL,
			     /* XXX these should come from an API parameter */
			     JSPROP_READONLY | JSPROP_PERMANENT)
	      != NULL);
	if (!ok)
	    break;
    }
    JS_UNLOCK(cx);
    return ok;
}

PR_IMPLEMENT(JSBool)
JS_DefineProperties(JSContext *cx, JSObject *obj, JSPropertySpec *ps)
{
    JSBool ok;
    JSProperty *prop;

    JS_LOCK(cx);
    for (ok = JS_TRUE; ps->name; ps++) {
	prop = DefineProperty(cx, obj, ps->name, JSVAL_VOID,
			      ps->getter, ps->setter, ps->flags);
	if (!prop) {
	    ok = JS_FALSE;
	    break;
	}
	prop->id = INT_TO_JSVAL(ps->tinyid);
	prop->flags |= JSPROP_TINYIDHACK;
    }
    JS_UNLOCK(cx);
    return ok;
}

PR_IMPLEMENT(JSBool)
JS_DefineProperty(JSContext *cx, JSObject *obj, const char *name, jsval value,
		  JSPropertyOp getter, JSPropertyOp setter, uintN flags)
{
    JS_LOCK_AND_RETURN_BOOL(cx, DefineProperty(cx, obj, name, value,
					       getter, setter, flags) != NULL);
}

PR_IMPLEMENT(JSBool)
JS_DefinePropertyWithTinyId(JSContext *cx, JSObject *obj, const char *name,
			    int8 tinyid, jsval value,
			    JSPropertyOp getter, JSPropertyOp setter,
			    uintN flags)
{
    JSProperty *prop;

    JS_LOCK(cx);
    prop = DefineProperty(cx, obj, name, value, getter, setter, flags);
    if (prop) {
	prop->id = INT_TO_JSVAL(tinyid);
	prop->flags |= JSPROP_TINYIDHACK;
    }
    JS_UNLOCK(cx);
    return prop != 0;
}

static JSBool
LookupProperty(JSContext *cx, JSObject *obj, const char *name,
	       JSProperty **propp)
{
    JSAtom *atom;
    JSBool ok;

    atom = js_Atomize(cx, name, strlen(name), 0);
    if (!atom)
	return JS_FALSE;
    ok = js_LookupProperty(cx, obj, (jsval)atom, NULL, propp);
    js_DropAtom(cx, atom);
    return ok;
}

PR_IMPLEMENT(JSBool)
JS_AliasProperty(JSContext *cx, JSObject *obj, const char *name,
		 const char *alias)
{
    JSBool ok;
    JSProperty *prop;
    JSScope *scope;
    JSAtom *atom;

    JS_LOCK(cx);
    ok = LookupProperty(cx, obj, name, &prop);
    if (!ok)
	goto out;
    if (!prop) {
	js_ReportIsNotDefined(cx, name);
	ok = JS_FALSE;
	goto out;
    }
    scope = js_GetMutableScope(cx, obj);
    if (!scope) {
	ok = JS_FALSE;
	goto out;
    }
    atom = js_Atomize(cx, alias, strlen(alias), 0);
    if (!atom) {
	ok = JS_FALSE;
    } else {
	ok = (scope->ops->add(cx, scope, (jsval)atom, prop) != NULL);
	js_DropAtom(cx, atom);
    }
out:
    JS_UNLOCK(cx);
    return ok;
}

PR_IMPLEMENT(JSBool)
JS_LookupProperty(JSContext *cx, JSObject *obj, const char *name, jsval *vp)
{
    JSBool ok;
    JSProperty *prop;

    JS_LOCK(cx);
    ok = LookupProperty(cx, obj, name, &prop);
    if (ok) {
	if (prop)
	    *vp = prop->object->slots[prop->slot];
	else
	    *vp = JSVAL_VOID;
    }
    JS_UNLOCK(cx);
    return ok;
}

PR_IMPLEMENT(JSBool)
JS_GetProperty(JSContext *cx, JSObject *obj, const char *name, jsval *vp)
{
    JSAtom *atom;
    JSBool ok;

    JS_LOCK(cx);
    atom = js_Atomize(cx, name, strlen(name), 0);
    if (!atom) {
	ok = JS_FALSE;
    } else {
	ok = (js_GetProperty(cx, obj, (jsval)atom, vp) != NULL);
	js_DropAtom(cx, atom);
    }
    JS_UNLOCK(cx);
    return ok;
}

PR_IMPLEMENT(JSBool)
JS_SetProperty(JSContext *cx, JSObject *obj, const char *name, jsval *vp)
{
    JSAtom *atom;
    JSBool ok;

    JS_LOCK(cx);
    atom = js_Atomize(cx, name, strlen(name), 0);
    if (!atom) {
	ok = JS_FALSE;
    } else {
	ok = (js_SetProperty(cx, obj, (jsval)atom, vp) != NULL);
	js_DropAtom(cx, atom);
    }
    JS_UNLOCK(cx);
    return ok;
}

PR_IMPLEMENT(JSBool)
JS_DeleteProperty(JSContext *cx, JSObject *obj, const char *name)
{
    JSAtom *atom;
    JSBool ok;
    jsval rval;	/* XXX not in API */

    JS_LOCK(cx);
    atom = js_Atomize(cx, name, strlen(name), 0);
    if (!atom) {
	ok = JS_FALSE;
    } else {
	ok = js_DeleteProperty(cx, obj, (jsval)atom, &rval);
	js_DropAtom(cx, atom);
    }
    JS_UNLOCK(cx);
    return ok;
}

PR_IMPLEMENT(JSObject *)
JS_NewArrayObject(JSContext *cx, jsint length, jsval *vector)
{
    JS_LOCK_AND_RETURN_TYPE(cx, JSObject *,
			    js_NewArrayObject(cx, length, vector));
}

PR_IMPLEMENT(JSBool)
JS_IsArrayObject(JSContext *cx, JSObject *obj)
{
    return obj->map->clasp == &js_ArrayClass;
}

PR_IMPLEMENT(JSBool)
JS_GetArrayLength(JSContext *cx, JSObject *obj, jsint *lengthp)
{
    JS_LOCK_AND_RETURN_BOOL(cx, js_GetArrayLength(cx, obj, lengthp));
}

PR_IMPLEMENT(JSBool)
JS_SetArrayLength(JSContext *cx, JSObject *obj, jsint length)
{
    JS_LOCK_AND_RETURN_BOOL(cx, js_SetArrayLength(cx, obj, length));
}

PR_IMPLEMENT(JSBool)
JS_HasLengthProperty(JSContext *cx, JSObject *obj)
{
    JS_LOCK_AND_RETURN_BOOL(cx, js_HasLengthProperty(cx, obj) != NULL);
}

PR_IMPLEMENT(JSBool)
JS_DefineElement(JSContext *cx, JSObject *obj, jsint index, jsval value,
		 JSPropertyOp getter, JSPropertyOp setter, uintN flags)
{
    JS_LOCK_AND_RETURN_BOOL(cx, js_DefineProperty(cx, obj, INT_TO_JSVAL(index),
						  value, getter, setter, flags)
				!= NULL);
}

PR_IMPLEMENT(JSBool)
JS_AliasElement(JSContext *cx, JSObject *obj, const char *name, jsint alias)
{
    JSBool ok;
    JSProperty *prop;
    JSScope *scope;

    JS_LOCK(cx);
    ok = LookupProperty(cx, obj, name, &prop);
    if (!ok)
	goto out;
    if (!prop) {
	js_ReportIsNotDefined(cx, name);
	ok = JS_FALSE;
	goto out;
    }
    scope = js_GetMutableScope(cx, obj);
    if (!scope)
	ok = JS_FALSE;
    else
	ok = (scope->ops->add(cx, scope, INT_TO_JSVAL(alias), prop) != NULL);
out:
    JS_UNLOCK(cx);
    return ok;
}

PR_IMPLEMENT(JSBool)
JS_LookupElement(JSContext *cx, JSObject *obj, jsint index, jsval *vp)
{
    JSBool ok;
    JSProperty *prop;

    JS_LOCK(cx);
    ok = js_LookupProperty(cx, obj, INT_TO_JSVAL(index), NULL, &prop);
    if (ok) {
	if (prop)
	    *vp = prop->object->slots[prop->slot];
	else
	    *vp = JSVAL_VOID;
    }
    JS_UNLOCK(cx);
    return ok;
}

PR_IMPLEMENT(JSBool)
JS_GetElement(JSContext *cx, JSObject *obj, jsint index, jsval *vp)
{
    JS_LOCK_AND_RETURN_BOOL(cx,
			    js_GetProperty(cx, obj, INT_TO_JSVAL(index), vp)
			    != NULL);
}

PR_IMPLEMENT(JSBool)
JS_SetElement(JSContext *cx, JSObject *obj, jsint index, jsval *vp)
{
    JS_LOCK_AND_RETURN_BOOL(cx,
			    js_SetProperty(cx, obj, INT_TO_JSVAL(index), vp)
			    != NULL);
}

PR_IMPLEMENT(JSBool)
JS_DeleteElement(JSContext *cx, JSObject *obj, jsint index)
{
    jsval rval;	/* XXX not in API */

    JS_LOCK_AND_RETURN_BOOL(cx,
			    js_DeleteProperty(cx, obj, INT_TO_JSVAL(index),
					      &rval));
}

PR_IMPLEMENT(void)
JS_ClearScope(JSContext *cx, JSObject *obj)
{
    JSScope *scope;

    JS_LOCK(cx);

    /* Avoid lots of js_FlushPropertyCacheByProp activity. */
    js_FlushPropertyCache(cx);

    scope = (JSScope *)obj->map;
    scope->ops->clear(cx, scope);

    /* Reset freeslot so we're consistent. */
    if (scope->map.clasp->flags & JSCLASS_HAS_PRIVATE)
	scope->map.freeslot = JSSLOT_PRIVATE + 1;
    else
	scope->map.freeslot = JSSLOT_START;

    JS_UNLOCK(cx);
}

PR_IMPLEMENT(JSFunction *)
JS_NewFunction(JSContext *cx, JSNative call, uintN nargs, uintN flags,
	       JSObject *parent, const char *name)
{
    JSAtom *atom;
    JSFunction *fun;

    JS_LOCK(cx);
    atom = js_Atomize(cx, name, strlen(name), 0);
    if (!atom) {
	fun = NULL;
    } else {
	fun = js_NewFunction(cx, call, nargs, flags, parent, atom);
	js_DropAtom(cx, atom);
    }
    JS_UNLOCK(cx);
    return fun;
}

PR_IMPLEMENT(JSObject *)
JS_GetFunctionObject(JSFunction *fun)
{
    return fun->object;
}

PR_IMPLEMENT(const char *)
JS_GetFunctionName(JSFunction *fun)
{
    return fun->atom
	   ? JS_GetStringBytes(ATOM_TO_STRING(fun->atom))
	   : js_anonymous_str;
}

PR_IMPLEMENT(JSBool)
JS_DefineFunctions(JSContext *cx, JSObject *obj, JSFunctionSpec *fs)
{
    JSFunction *fun;

    for (; fs->name; fs++) {
	fun = JS_DefineFunction(cx, obj, fs->name, fs->call, fs->nargs,
				fs->flags);
	if (!fun)
	    return JS_FALSE;
    }
    return JS_TRUE;
}

PR_IMPLEMENT(JSFunction *)
JS_DefineFunction(JSContext *cx, JSObject *obj, const char *name, JSNative call,
		  uintN nargs, uintN flags)
{
    JSAtom *atom;
    JSFunction *fun;

    JS_LOCK(cx);
    atom = js_Atomize(cx, name, strlen(name), 0);
    if (!atom) {
	fun = NULL;
    } else {
	fun = js_DefineFunction(cx, obj, atom, call, nargs, flags);
	js_DropAtom(cx, atom);
    }
    JS_UNLOCK(cx);
    return fun;
}

static JSScript *
CompileTokenStream(JSContext *cx, JSObject *obj, JSTokenStream *ts,
		   void *tempMark)
{
    void *codeMark;
    JSCodeGenerator cg;
    uintN lineno;
    JSScript *script;

    PR_ASSERT(JS_IS_LOCKED(cx));
    codeMark = PR_ARENA_MARK(&cx->codePool);
    if (!js_InitCodeGenerator(cx, &cg, &cx->codePool))
	return NULL;
    lineno = ts->lineno;
    if (js_Parse(cx, obj, ts, &cg))
	script = js_NewScript(cx, &cg, ts->filename, lineno,
			      ts->principals, NULL);
    else
	script = NULL;
    if (!js_CloseTokenStream(cx, ts) && script) {
	js_DestroyScript(cx, script);
	script = NULL;
    }
    PR_ARENA_RELEASE(&cx->codePool, codeMark);
    PR_ARENA_RELEASE(&cx->tempPool, tempMark);
    return script;
}

PR_IMPLEMENT(JSScript *)
JS_CompileScript(JSContext *cx, JSObject *obj,
		 const char *bytes, size_t length,
		 const char *filename, uintN lineno)
{
    jschar *chars;
    JSScript *script;

    chars = js_InflateString(cx, bytes, length);
    if (!chars)
	return NULL;
    script = JS_CompileUCScript(cx, obj, chars, length, filename, lineno);
    JS_free(cx, chars);
    return script;
}

PR_IMPLEMENT(JSScript *)
JS_CompileScriptForPrincipals(JSContext *cx, JSObject *obj,
			      JSPrincipals *principals,
			      const char *bytes, size_t length,
			      const char *filename, uintN lineno)
{
    jschar *chars;
    JSScript *script;

    chars = js_InflateString(cx, bytes, length);
    if (!chars)
	return NULL;
    script = JS_CompileUCScriptForPrincipals(cx, obj, principals,
					     chars, length, filename, lineno);
    JS_free(cx, chars);
    return script;
}

PR_IMPLEMENT(JSScript *)
JS_CompileUCScript(JSContext *cx, JSObject *obj,
		   const jschar *chars, size_t length,
		   const char *filename, uintN lineno)
{
    return JS_CompileUCScriptForPrincipals(cx, obj, NULL, chars, length,
					   filename, lineno);
}

PR_IMPLEMENT(JSScript *)
JS_CompileUCScriptForPrincipals(JSContext *cx, JSObject *obj,
				JSPrincipals *principals,
				const jschar *chars, size_t length,
				const char *filename, uintN lineno)
{
    void *mark;
    JSTokenStream *ts;
    JSScript *script;

    JS_LOCK(cx);
    mark = PR_ARENA_MARK(&cx->tempPool);
    ts = js_NewTokenStream(cx, chars, length, filename, lineno, principals);
    if (ts)
	script = CompileTokenStream(cx, obj, ts, mark);
    else
	script = NULL;
    JS_UNLOCK(cx);
    return script;
}

#ifdef JSFILE
PR_IMPLEMENT(JSScript *)
JS_CompileFile(JSContext *cx, JSObject *obj, const char *filename)
{
    void *mark;
    JSTokenStream *ts;
    JSScript *script;

    JS_LOCK(cx);
    mark = PR_ARENA_MARK(&cx->tempPool);
    if (filename && strcmp(filename, "-") != 0) {
	ts = js_NewFileTokenStream(cx, filename);
	if (!ts) {
	    script = NULL;
	    goto out;
	}
    } else {
	ts = js_NewBufferTokenStream(cx, NULL, 0);
	if (!ts) {
	    script = NULL;
	    goto out;
	}
	ts->file = stdin;
    }
    script = CompileTokenStream(cx, obj, ts, mark);
out:
    JS_UNLOCK(cx);
    return script;
}
#endif

PR_IMPLEMENT(void)
JS_DestroyScript(JSContext *cx, JSScript *script)
{
    JS_LOCK_VOID(cx, js_DestroyScript(cx, script));
}

PR_IMPLEMENT(JSFunction *)
JS_CompileFunction(JSContext *cx, JSObject *obj, const char *name,
		   uintN nargs, const char **argnames,
		   const char *bytes, size_t length,
		   const char *filename, uintN lineno)
{
    jschar *chars;
    JSFunction *fun;

    chars = js_InflateString(cx, bytes, length);
    if (!chars)
	return NULL;
    fun = JS_CompileUCFunction(cx, obj, name, nargs, argnames, chars, length,
			       filename, lineno);
    JS_free(cx, chars);
    return fun;
}

PR_IMPLEMENT(JSFunction *)
JS_CompileFunctionForPrincipals(JSContext *cx, JSObject *obj,
				JSPrincipals *principals, const char *name,
				uintN nargs, const char **argnames,
				const char *bytes, size_t length,
				const char *filename, uintN lineno)
{
    jschar *chars;
    JSFunction *fun;

    chars = js_InflateString(cx, bytes, length);
    if (!chars)
	return NULL;
    fun = JS_CompileUCFunctionForPrincipals(cx, obj, principals, name,
					    nargs, argnames, chars, length,
					    filename, lineno);
    JS_free(cx, chars);
    return fun;
}

PR_IMPLEMENT(JSFunction *)
JS_CompileUCFunction(JSContext *cx, JSObject *obj, const char *name,
		     uintN nargs, const char **argnames,
		     const jschar *chars, size_t length,
		     const char *filename, uintN lineno)
{
    return JS_CompileUCFunctionForPrincipals(cx, obj, NULL, name,
					     nargs, argnames,
					     chars, length,
					     filename, lineno);
}

PR_IMPLEMENT(JSFunction *)
JS_CompileUCFunctionForPrincipals(JSContext *cx, JSObject *obj,
				  JSPrincipals *principals, const char *name,
				  uintN nargs, const char **argnames,
				  const jschar *chars, size_t length,
				  const char *filename, uintN lineno)
{
    void *mark;
    JSTokenStream *ts;
    JSFunction *fun;
    JSAtom *funAtom, *argAtom;
    JSSymbol *arg, *args, **argp;
    uintN i;
    JSProperty *prop;
    jsval junk;

    JS_LOCK(cx);
    mark = PR_ARENA_MARK(&cx->tempPool);
    ts = js_NewTokenStream(cx, chars, length, filename, lineno, principals);
    if (!ts) {
	fun = NULL;
	funAtom = NULL;
	goto out;
    }
    funAtom = js_Atomize(cx, name, strlen(name), 0);
    if (!funAtom) {
	fun = NULL;
	goto out;
    }
    fun = js_DefineFunction(cx, obj, funAtom, NULL, nargs, 0);
    if (!fun)
	goto out;
    args = NULL;
    if (nargs) {
	argp = &args;
	for (i = 0; i < nargs; i++) {
	    argAtom = js_Atomize(cx, argnames[i], strlen(argnames[i]), 0);
	    if (!argAtom)
		break;
	    prop = js_DefineProperty(cx, fun->object, (jsval)argAtom,
				     JSVAL_VOID, js_GetArgument, js_SetArgument,
				     JSPROP_ENUMERATE|JSPROP_PERMANENT);
	    js_DropAtom(cx, argAtom);
	    if (!prop)
		break;
	    prop->id = INT_TO_JSVAL(i);
	    arg = prop->symbols;
	    *argp = arg;
	    argp = &arg->next;
	}
	if (i < nargs) {
	    (void) js_DeleteProperty(cx, obj, (jsval)funAtom, &junk);
	    fun = NULL;
	    goto out;
	}
    }
    if (!js_ParseFunctionBody(cx, ts, fun, args)) {
	(void) js_DeleteProperty(cx, obj, (jsval)funAtom, &junk);
	fun = NULL;
    }
out:
    if (funAtom)
	js_DropAtom(cx, funAtom);
    if (ts)
        js_CloseTokenStream(cx, ts);
    PR_ARENA_RELEASE(&cx->tempPool, mark);
    JS_UNLOCK(cx);
    return fun;
}

PR_IMPLEMENT(JSString *)
JS_DecompileScript(JSContext *cx, JSScript *script, const char *name,
		   uintN indent)
{
    JSPrinter *jp;
    JSString *str;

    JS_LOCK(cx);
    jp = js_NewPrinter(cx, name, indent);
    if (!jp) {
	str = NULL;
    } else {
	if (js_DecompileScript(jp, script))
	    str = js_GetPrinterOutput(jp);
	else
	    str = NULL;
	js_DestroyPrinter(jp);
    }
    JS_UNLOCK(cx);
    return str;
}

PR_IMPLEMENT(JSString *)
JS_DecompileFunction(JSContext *cx, JSFunction *fun, uintN indent)
{
    JSPrinter *jp;
    JSString *str;

    JS_LOCK(cx);
    jp = js_NewPrinter(cx, JS_GetFunctionName(fun), indent);
    if (!jp) {
	str = NULL;
    } else {
	if (js_DecompileFunction(jp, fun, JS_TRUE))
	    str = js_GetPrinterOutput(jp);
	else
	    str = NULL;
	js_DestroyPrinter(jp);
    }
    JS_UNLOCK(cx);
    return str;
}

PR_IMPLEMENT(JSString *)
JS_DecompileFunctionBody(JSContext *cx, JSFunction *fun, uintN indent)
{
    return JS_DecompileScript(cx, fun->script, JS_GetFunctionName(fun), indent);
}

PR_IMPLEMENT(JSBool)
JS_ExecuteScript(JSContext *cx, JSObject *obj, JSScript *script, jsval *rval)
{
    return js_Execute(cx, obj, script, NULL, rval);
}

PR_IMPLEMENT(JSBool)
JS_EvaluateScript(JSContext *cx, JSObject *obj,
		  const char *bytes, uintN length,
		  const char *filename, uintN lineno,
		  jsval *rval)
{
    jschar *chars;
    JSBool ok;

    chars = js_InflateString(cx, bytes, length);
    if (!chars)
	return JS_FALSE;
    ok = JS_EvaluateUCScript(cx, obj, chars, length, filename, lineno, rval);
    JS_free(cx, chars);
    return ok;
}

PR_IMPLEMENT(JSBool)
JS_EvaluateScriptForPrincipals(JSContext *cx, JSObject *obj,
			       JSPrincipals *principals,
			       const char *bytes, uintN length,
			       const char *filename, uintN lineno,
			       jsval *rval)
{
    jschar *chars;
    JSBool ok;

    chars = js_InflateString(cx, bytes, length);
    if (!chars)
	return JS_FALSE;
    ok = JS_EvaluateUCScriptForPrincipals(cx, obj, principals, chars, length,
					  filename, lineno, rval);
    JS_free(cx, chars);
    return ok;
}

PR_IMPLEMENT(JSBool)
JS_EvaluateUCScript(JSContext *cx, JSObject *obj,
		    const jschar *chars, uintN length,
		    const char *filename, uintN lineno,
		    jsval *rval)
{
    return JS_EvaluateUCScriptForPrincipals(cx, obj, NULL, chars, length,
					    filename, lineno, rval);
}

PR_IMPLEMENT(JSBool)
JS_EvaluateUCScriptForPrincipals(JSContext *cx, JSObject *obj,
				 JSPrincipals *principals,
				 const jschar *chars, uintN length,
				 const char *filename, uintN lineno,
				 jsval *rval)
{
    JSScript *script;
    JSBool ok;

    script = JS_CompileUCScriptForPrincipals(cx, obj, principals, chars, length,
					     filename, lineno);
    if (!script)
	return JS_FALSE;
    ok = js_Execute(cx, obj, script, NULL, rval);
    JS_DestroyScript(cx, script);
    return ok;
}

PR_IMPLEMENT(JSBool)
JS_CallFunction(JSContext *cx, JSObject *obj, JSFunction *fun, uintN argc,
		jsval *argv, jsval *rval)
{
    JS_LOCK_AND_RETURN_BOOL(cx,
			    js_Call(cx, obj, OBJECT_TO_JSVAL(fun->object),
				    argc, argv, rval));
}

PR_IMPLEMENT(JSBool)
JS_CallFunctionName(JSContext *cx, JSObject *obj, const char *name, uintN argc,
		    jsval *argv, jsval *rval)
{
    jsval fval;

    if (!JS_GetProperty(cx, obj, name, &fval))
	return JS_FALSE;
    JS_LOCK_AND_RETURN_BOOL(cx, js_Call(cx, obj, fval, argc, argv, rval));
}

PR_IMPLEMENT(JSBool)
JS_CallFunctionValue(JSContext *cx, JSObject *obj, jsval fval, uintN argc,
		     jsval *argv, jsval *rval)
{
    JS_LOCK_AND_RETURN_BOOL(cx, js_Call(cx, obj, fval, argc, argv, rval));
}

PR_IMPLEMENT(JSBranchCallback)
JS_SetBranchCallback(JSContext *cx, JSBranchCallback cb)
{
    JSBranchCallback oldcb;

    oldcb = cx->branchCallback;
    cx->branchCallback = cb;
    return oldcb;
}

PR_IMPLEMENT(JSBool)
JS_IsRunning(JSContext *cx)
{
    return cx->fp != NULL;
}

/************************************************************************/

PR_IMPLEMENT(JSString *)
JS_NewString(JSContext *cx, char *bytes, size_t length)
{
    jschar *chars;
    JSString *str;

    /* Make a Unicode vector from the 8-bit char codes in bytes. */
    chars = js_InflateString(cx, bytes, length);
    if (!chars)
	return NULL;

    /* Free chars (but not bytes, which caller frees on error) if we fail. */
    str = js_NewString(cx, chars, length, 0);
    if (!str) {
	JS_free(cx, chars);
	return NULL;
    }

    /* Hand off bytes to the deflated string cache, if possible. */
    if (!js_SetStringBytes(str, bytes, length))
	JS_free(cx, bytes);
    return str;
}

PR_IMPLEMENT(JSString *)
JS_NewStringCopyN(JSContext *cx, const char *s, size_t n)
{
    jschar *js;
    JSString *str;

    js = js_InflateString(cx, s, n);
    if (!js)
	return NULL;
    str = js_NewString(cx, js, n, 0);
    if (!str)
	JS_free(cx, js);
    return str;
}

PR_IMPLEMENT(JSString *)
JS_NewStringCopyZ(JSContext *cx, const char *s)
{
    size_t n;
    jschar *js;
    JSString *str;

    if (!s)
	return cx->runtime->emptyString;
    n = strlen(s);
    js = js_InflateString(cx, s, n);
    if (!js)
	return NULL;
    str = js_NewString(cx, js, n, 0);
    if (!str)
	JS_free(cx, js);
    return str;
}

PR_IMPLEMENT(JSString *)
JS_InternString(JSContext *cx, const char *s)
{
    JSAtom *atom;

    JS_LOCK_VOID(cx, atom = js_Atomize(cx, s, strlen(s), 0));
    if (!atom)
	return NULL;
    return ATOM_TO_STRING(atom);
}

PR_IMPLEMENT(JSString *)
JS_NewUCString(JSContext *cx, jschar *chars, size_t length)
{
    return js_NewString(cx, chars, length, 0);
}

PR_IMPLEMENT(JSString *)
JS_NewUCStringCopyN(JSContext *cx, const jschar *s, size_t n)
{
    return js_NewStringCopyN(cx, s, n, 0);
}

PR_IMPLEMENT(JSString *)
JS_NewUCStringCopyZ(JSContext *cx, const jschar *s)
{
    if (!s)
	return cx->runtime->emptyString;
    return js_NewStringCopyZ(cx, s, 0);
}

PR_IMPLEMENT(JSString *)
JS_InternUCString(JSContext *cx, const jschar *s)
{
    JSAtom *atom;

    JS_LOCK_VOID(cx, atom = js_AtomizeChars(cx, s, js_strlen(s), 0));
    if (!atom)
	return NULL;
    return ATOM_TO_STRING(atom);
}

PR_IMPLEMENT(char *)
JS_GetStringBytes(JSString *str)
{
    char *bytes = js_GetStringBytes(str);
    return bytes ? bytes : "";
}

PR_IMPLEMENT(jschar *)
JS_GetStringChars(JSString *str)
{
    return str->chars;
}

PR_IMPLEMENT(size_t)
JS_GetStringLength(JSString *str)
{
    return str->length;
}

PR_IMPLEMENT(intN)
JS_CompareStrings(JSString *str1, JSString *str2)
{
    return js_CompareStrings(str1, str2);
}

/************************************************************************/

PR_IMPLEMENT(void)
JS_ReportError(JSContext *cx, const char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    js_ReportErrorVA(cx, format, ap);
    va_end(ap);
}

PR_IMPLEMENT(void)
JS_ReportOutOfMemory(JSContext *cx)
{
    JS_ReportError(cx, "out of memory");
}

PR_IMPLEMENT(JSErrorReporter)
JS_SetErrorReporter(JSContext *cx, JSErrorReporter er)
{
    JSErrorReporter older;

    older = cx->errorReporter;
    cx->errorReporter = er;
    return older;
}

/************************************************************************/

/*
 * Regular Expressions.
 */
PR_IMPLEMENT(JSObject *)
JS_NewRegExpObject(JSContext *cx, char *bytes, size_t length, uintN flags)
{
#if JS_HAS_REGEXPS
    jschar *chars;
    JSObject *obj;

    chars = js_InflateString(cx, bytes, length);
    if (!chars)
	return NULL;
    JS_LOCK_VOID(cx, obj = js_NewRegExpObject(cx, chars, length, flags));
    JS_free(cx, chars);
    return obj;
#else
    JS_ReportError(cx, "sorry, regular expression are not supported");
    return JS_FALSE;
#endif
}

PR_IMPLEMENT(JSObject *)
JS_NewUCRegExpObject(JSContext *cx, jschar *chars, size_t length, uintN flags)
{
#if JS_HAS_REGEXPS
    JS_LOCK_AND_RETURN_TYPE(cx, JSObject *,
			    js_NewRegExpObject(cx, chars, length, flags));
#else
    JS_ReportError(cx, "sorry, regular expression are not supported");
    return JS_FALSE;
#endif
}

PR_IMPLEMENT(void)
JS_SetRegExpInput(JSContext *cx, JSString *input, JSBool multiline)
{
    JSRegExpStatics *res;

    /* No locking required, cx is thread-private and input must be live. */
    res = &cx->regExpStatics;
    res->input = input;
    res->multiline = multiline;
}

PR_IMPLEMENT(void)
JS_ClearRegExpStatics(JSContext *cx)
{
    JSRegExpStatics *res;

    /* No locking required, cx is thread-private and input must be live. */
    res = &cx->regExpStatics;
    res->input = NULL;
    res->multiline = JS_FALSE;
    res->parenCount = 0;
    res->lastMatch = res->lastParen = js_EmptySubString;
    res->leftContext = res->rightContext = js_EmptySubString;
}

PR_IMPLEMENT(void)
JS_ClearRegExpRoots(JSContext *cx)
{
    JSRegExpStatics *res;

    /* No locking required, cx is thread-private and input must be live. */
    res = &cx->regExpStatics;
    res->input = NULL;
    res->execWrapper = NULL;
}

/* TODO: compile, execute, get/set other statics... */

/************************************************************************/

#ifdef NETSCAPE_INTERNAL
PR_IMPLEMENT(void)
JS_SetCharSetInfo(JSContext *cx, const char *csName, int csId,
		  JSCharScanner scanner)
{
    cx->charSetName       = csName;
    cx->charSetNameLength = csName ? strlen(csName) : 0;
    cx->charSetId         = csId;
    cx->charScanner       = scanner;
}

PR_IMPLEMENT(JSBool)
JS_IsAssigning(JSContext *cx)
{
    JSStackFrame *fp;
    jsbytecode *pc;

    if (!(fp = cx->fp) || !(pc = fp->pc))
	return JS_FALSE;
    return (js_CodeSpec[*pc].format & JOF_SET) != 0;
}
#endif

/************************************************************************/

#ifdef XP_PC
#if defined(XP_OS2_HACK)
/*DSR031297 - the OS/2 equiv is dll_InitTerm, but I don't see the need for it*/
#else
/*
 * Initialization routine for the JS DLL...
 */

/*
 * Global Instance handle...
 * In Win32 this is the module handle of the DLL.
 *
 * In Win16 this is the instance handle of the application
 * which loaded the DLL.
 */

#ifdef _WIN32
BOOL WINAPI DllMain (HINSTANCE hDLL, DWORD dwReason, LPVOID lpReserved)
{
    return TRUE;
}

#else  /* !_WIN32 */

int CALLBACK LibMain( HINSTANCE hInst, WORD wDataSeg,
		      WORD cbHeapSize, LPSTR lpszCmdLine )
{
    return TRUE;
}

BOOL CALLBACK __loadds WEP(BOOL fSystemExit)
{
    return TRUE;
}

#endif /* !_WIN32 */
#endif /* XP_OS2 */
#endif /* XP_PC */
