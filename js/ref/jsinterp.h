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
    JSObject        *callobj;       /* lazily created Call object */
    JSObject        *argsobj;       /* lazily created arguments object */
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
    JSPackedBool    constructing;   /* true if called via new operator */
    uint8           overrides;      /* bit-set of overridden Call properties */
    JSPackedBool    debugging;      /* true if for JS_EvaluateInStackFrame */
    JSPackedBool    exceptPending;  /* is there an uncaught exception? */
    jsval           exception;      /* most-recently-thrown exception */
};

/*
 * Property cache for quickened get/set property opcodes.
 */
#define PROPERTY_CACHE_LOG2     10
#define PROPERTY_CACHE_SIZE     PR_BIT(PROPERTY_CACHE_LOG2)
#define PROPERTY_CACHE_MASK     PR_BITMASK(PROPERTY_CACHE_LOG2)

#define PROPERTY_CACHE_HASH(obj, id) \
    ((((pruword)(obj) >> JSVAL_TAGBITS) ^ (pruword)(id)) & PROPERTY_CACHE_MASK)

#ifdef JS_THREADSAFE

#if HAVE_ATOMIC_DWORD_ACCESS

#define PCE_LOAD(cache, pce, entry)     PR_ATOMIC_DWORD_LOAD(pce, entry)
#define PCE_STORE(cache, pce, entry)    PR_ATOMIC_DWORD_STORE(pce, entry)

#else  /* !HAVE_ATOMIC_DWORD_ACCESS */

#define PCE_LOAD(cache, pce, entry)                                           \
    PR_BEGIN_MACRO                                                            \
	uint32 _prefills;                                                     \
	uint32 _fills = (cache)->fills;                                       \
	do {                                                                  \
	    /* Load until cache->fills is stable (see FILL macro below). */   \
	    _prefills = _fills;                                               \
	    (entry) = *(pce);                                                 \
	} while ((_fills = (cache)->fills) != _prefills);                     \
    PR_END_MACRO

#define PCE_STORE(cache, pce, entry)                                          \
    PR_BEGIN_MACRO                                                            \
	do {                                                                  \
	    /* Store until no racing collider stores half or all of pce. */   \
	    *(pce) = (entry);                                                 \
	} while (PCE_OBJECT(*pce) != PCE_OBJECT(entry) ||                     \
		 PCE_PROPERTY(*pce) != PCE_PROPERTY(entry));                  \
    PR_END_MACRO

#endif /* !HAVE_ATOMIC_DWORD_ACCESS */

#else  /* !JS_THREADSAFE */

#define PCE_LOAD(cache, pce, entry)     ((entry) = *(pce))
#define PCE_STORE(cache, pce, entry)    (*(pce) = (entry))

#endif /* !JS_THREADSAFE */

typedef union JSPropertyCacheEntry {
    struct {
	JSObject    *object;    /* weak link to object */
	JSProperty  *property;  /* weak link to property, or not-found id */
    } s;
#ifdef HAVE_ATOMIC_DWORD_ACCESS
    prdword align;
#endif
} JSPropertyCacheEntry;

/* These may be called in lvalue or rvalue position. */
#define PCE_OBJECT(entry)       ((entry).s.object)
#define PCE_PROPERTY(entry)     ((entry).s.property)

typedef struct JSPropertyCache {
    JSPropertyCacheEntry table[PROPERTY_CACHE_SIZE];
    JSBool               empty;
    uint32               fills;
    uint32               recycles;
    uint32               tests;
    uint32               misses;
    uint32               flushes;
} JSPropertyCache;

/* Property-not-found lookup results are cached using this invalid pointer. */
#define PROP_NOT_FOUND(obj,id)  ((JSProperty *) ((prword)(id) | 1))
#define PROP_NOT_FOUND_ID(prop) ((jsid) ((prword)(prop) & ~1))
#define PROP_FOUND(prop)        ((prop) && ((prword)(prop) & 1) == 0)

#define PROPERTY_CACHE_FILL(cx, cache, obj, id, prop)                         \
    PR_BEGIN_MACRO                                                            \
	uintN _hashIndex = (uintN)PROPERTY_CACHE_HASH(obj, id);               \
	JSPropertyCache *_cache = (cache);                                    \
	JSPropertyCacheEntry *_pce = &_cache->table[_hashIndex];              \
	JSPropertyCacheEntry _entry;                                          \
	JSProperty *_pce_prop;                                                \
	PCE_LOAD(_cache, _pce, _entry);                                       \
	_pce_prop = PCE_PROPERTY(_entry);                                     \
	if (_pce_prop && _pce_prop != prop)                                   \
	    _cache->recycles++;                                               \
	PCE_OBJECT(_entry) = obj;                                             \
	PCE_PROPERTY(_entry) = prop;                                          \
	_cache->empty = JS_FALSE;                                             \
	_cache->fills++;                                                      \
	PCE_STORE(_cache, _pce, _entry);                                      \
    PR_END_MACRO

#define PROPERTY_CACHE_TEST(cache, obj, id, prop)                             \
    PR_BEGIN_MACRO                                                            \
	uintN _hashIndex = (uintN)PROPERTY_CACHE_HASH(obj, id);               \
	JSPropertyCache *_cache = (cache);                                    \
	JSPropertyCacheEntry *_pce = &_cache->table[_hashIndex];              \
	JSPropertyCacheEntry _entry;                                          \
	JSProperty *_pce_prop;                                                \
	PCE_LOAD(_cache, _pce, _entry);                                       \
	_pce_prop = PCE_PROPERTY(_entry);                                     \
	_cache->tests++;                                                      \
	if (_pce_prop &&                                                      \
	    (((prword)_pce_prop & 1)                                          \
	     ? PROP_NOT_FOUND_ID(_pce_prop)                                   \
	     : sym_id(((JSScopeProperty *)_pce_prop)->symbols)) == id &&      \
	    PCE_OBJECT(_entry) == obj) {                                      \
	    prop = _pce_prop;                                                 \
	} else {                                                              \
	    _cache->misses++;                                                 \
	    prop = NULL;                                                      \
	}                                                                     \
    PR_END_MACRO

extern void
js_FlushPropertyCache(JSContext *cx);

extern void
js_FlushPropertyCacheByProp(JSContext *cx, JSProperty *prop);

extern jsval *
js_AllocStack(JSContext *cx, uintN nslots, void **markp);

extern void
js_FreeStack(JSContext *cx, void *mark);

extern JSBool
js_GetArgument(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

extern JSBool
js_SetArgument(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

extern JSBool
js_GetLocalVariable(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

extern JSBool
js_SetLocalVariable(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

extern JSBool
js_Invoke(JSContext *cx, uintN argc, JSBool constructing);

extern JSBool
js_CallFunctionValue(JSContext *cx, JSObject *obj, jsval fval,
		     uintN argc, jsval *argv, jsval *rval);

extern JSBool
js_Execute(JSContext *cx, JSObject *chain, JSScript *script, JSFunction *fun,
	   JSStackFrame *down, JSBool debugging, jsval *result);

extern JSBool
js_Interpret(JSContext *cx, jsval *result);

#endif /* jsinterp_h___ */
