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

#ifndef jsinterp_h___
#define jsinterp_h___
/*
 * JS interpreter interface.
 */
#include "jsprvtd.h"
#include "jspubtd.h"

/*
 * JS stack frame, allocated on the C stack.
 */
struct JSStackFrame {
    JSObject        *object;        /* object for fun.arguments/fun.caller */
    JSScript        *script;        /* script being interpreted */
    JSFunction      *fun;           /* function being called or null */
    JSObject        *thisp;         /* "this" pointer if in method */
    uintN           argc;           /* actual argument count */
    jsval           *argv;          /* base of argument stack slots */
    jsval           rval;           /* function return value */
    uintN           nvars;          /* local variable count */
    jsval           *vars;          /* base of variable stack slots */
    JSStackFrame    *down;          /* previous frame */
    void            *annotation;    /* used by Java security */
    JSObject        *scopeChain;    /* scope chain */
    jsbytecode      *pc;            /* program counter */
    jsval           *sp;            /* stack pointer */
    uintN           sharpDepth;     /* array/object initializer depth */
    JSObject        *sharpArray;    /* scope for #n= initializer vars */
};

/*
 * Property cache for quickened get/set property opcodes.
 */
#define PROPERTY_CACHE_LOG2     10
#define PROPERTY_CACHE_SIZE     PR_BIT(PROPERTY_CACHE_LOG2)
#define PROPERTY_CACHE_MASK     PR_BITMASK(PROPERTY_CACHE_LOG2)

#define PROPERTY_CACHE_HASH(obj, id) \
    ((((pruword)(obj) >> JSVAL_TAGBITS) ^ (pruword)(id)) & PROPERTY_CACHE_MASK)

typedef struct JSPropertyCacheEntry {
    JSProperty  *property;      /* weak link to property */
    jsval       symbolid;       /* strong link to name atom, or index jsval */
} JSPropertyCacheEntry;

typedef struct JSPropertyCache {
    JSPropertyCacheEntry table[PROPERTY_CACHE_SIZE];
    JSBool               empty;
    uint32               fills;
    uint32               recycles;
    uint32               tests;
    uint32               misses;
    uint32               flushes;
    uint32               pflushes;
} JSPropertyCache;

#define PROP_NOT_FOUND   ((JSProperty *)1)
#define PROP_FOUND(prop) ((prword)(prop) & (prword)-2)

#define PROPERTY_CACHE_FILL(cx, cache, obj, id, prop)                         \
    PR_BEGIN_MACRO                                                            \
	uintN _hashIndex = (uintN)PROPERTY_CACHE_HASH(obj, id);               \
	JSPropertyCacheEntry *_pce = &(cache)->table[_hashIndex];             \
	if (_pce->property && _pce->property != prop) {                       \
	    (cache)->recycles++;                                              \
	    if (!JSVAL_IS_INT(_pce->symbolid))                                \
		js_DropAtom(cx, (JSAtom *)_pce->symbolid);                    \
	}                                                                     \
	_pce->property = prop;                                                \
	_pce->symbolid = id;                                                  \
	if (!JSVAL_IS_INT(id))                                                \
	    js_HoldAtom(cx, (JSAtom *)id);                                    \
	(cache)->empty = JS_FALSE;                                            \
	(cache)->fills++;                                                     \
    PR_END_MACRO

#define PROPERTY_CACHE_TEST(cache, obj, id, prop)                             \
    PR_BEGIN_MACRO                                                            \
	uintN _hashIndex = (uintN)PROPERTY_CACHE_HASH(obj, id);               \
	JSPropertyCacheEntry *_pce = &(cache)->table[_hashIndex];             \
	(cache)->tests++;                                                     \
	if (_pce->property &&                                                 \
	    (_pce->property == PROP_NOT_FOUND ||                              \
	     _pce->property->object == obj) &&                                \
	    _pce->symbolid == id) {                                           \
	    prop = _pce->property;                                            \
	} else {                                                              \
	    (cache)->misses++;                                                \
	    prop = NULL;                                                      \
	}                                                                     \
    PR_END_MACRO

extern void
js_FlushPropertyCache(JSContext *cx);

extern void
js_FlushPropertyCacheByProp(JSContext *cx, JSProperty *prop);

extern JSBool
js_GetArgument(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

extern JSBool
js_SetArgument(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

extern JSBool
js_GetLocalVariable(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

extern JSBool
js_SetLocalVariable(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

extern JSBool
js_DoCall(JSContext *cx, uintN argc);

extern JSBool
js_Call(JSContext *cx, JSObject *obj, jsval fval, uintN argc, jsval *argv,
	jsval *rval);

extern JSBool
js_Execute(JSContext *cx, JSObject *chain, JSScript *script, JSStackFrame *down,
	   jsval *result);

#endif /* jsinterp_h___ */
