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

#ifndef jsinterp_h___
#define jsinterp_h___
/*
 * JS interpreter interface.
 */
#include "jsprvtd.h"
#include "jspubtd.h"

JS_BEGIN_EXTERN_C

/*
 * JS stack frame, allocated on the C stack.
 */
struct JSStackFrame {
    JSObject        *callobj;       /* lazily created Call object */
    JSObject        *argsobj;       /* lazily created arguments object */
    JSObject        *varobj;        /* variables object, where vars go */
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
    jsval           *spbase;        /* operand stack base */
    uintN           sharpDepth;     /* array/object initializer depth */
    JSObject        *sharpArray;    /* scope for #n= initializer vars */
    JSPackedBool    constructing;   /* true if called via new operator */
    uint8           overrides;      /* bit-set of overridden Call properties */
    uint8           special;        /* special frame type flags, see below */
    JSPackedBool    reserved;       /* reserved for future use */
    JSStackFrame    *dormantNext;   /* next dormant frame chain */
};

typedef struct JSInlineFrame {
    JSStackFrame    frame;          /* base struct */
    void            *mark;          /* mark before inline frame */
    void            *hookData;      /* debugger call hook data */
} JSInlineFrame;

/* JS special stack frame flags. */
#define JSFRAME_DEBUGGER    0x1     /* frame for JS_EvaluateInStackFrame */
#define JSFRAME_EVAL        0x2     /* frame for obj_eval */

/*
 * Property cache for quickened get/set property opcodes.
 */
#define PROPERTY_CACHE_LOG2     10
#define PROPERTY_CACHE_SIZE     JS_BIT(PROPERTY_CACHE_LOG2)
#define PROPERTY_CACHE_MASK     JS_BITMASK(PROPERTY_CACHE_LOG2)

#define PROPERTY_CACHE_HASH(obj, id) \
    ((((jsuword)(obj) >> JSVAL_TAGBITS) ^ (jsuword)(id)) & PROPERTY_CACHE_MASK)

#ifdef JS_THREADSAFE

#if HAVE_ATOMIC_DWORD_ACCESS

#define PCE_LOAD(cache, pce, entry)     JS_ATOMIC_DWORD_LOAD(pce, entry)
#define PCE_STORE(cache, pce, entry)    JS_ATOMIC_DWORD_STORE(pce, entry)

#else  /* !HAVE_ATOMIC_DWORD_ACCESS */

#define PCE_LOAD(cache, pce, entry)                                           \
    JS_BEGIN_MACRO                                                            \
	uint32 _prefills;                                                     \
	uint32 _fills = (cache)->fills;                                       \
	do {                                                                  \
	    /* Load until cache->fills is stable (see FILL macro below). */   \
	    _prefills = _fills;                                               \
	    (entry) = *(pce);                                                 \
	} while ((_fills = (cache)->fills) != _prefills);                     \
    JS_END_MACRO

#define PCE_STORE(cache, pce, entry)                                          \
    JS_BEGIN_MACRO                                                            \
	do {                                                                  \
	    /* Store until no racing collider stores half or all of pce. */   \
	    *(pce) = (entry);                                                 \
	} while (PCE_OBJECT(*pce) != PCE_OBJECT(entry) ||                     \
		 PCE_PROPERTY(*pce) != PCE_PROPERTY(entry));                  \
    JS_END_MACRO

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
    JSBool               disabled;
    uint32               fills;
    uint32               recycles;
    uint32               tests;
    uint32               misses;
    uint32               flushes;
} JSPropertyCache;

#define PROPERTY_CACHE_FILL(cx, cache, obj, id, prop)                         \
    JS_BEGIN_MACRO                                                            \
        JSPropertyCache *_cache = (cache);                                    \
        if (!_cache->disabled) {                                              \
            uintN _hashIndex = (uintN)PROPERTY_CACHE_HASH(obj, id);           \
            JSPropertyCacheEntry *_pce = &_cache->table[_hashIndex];          \
            JSPropertyCacheEntry _entry;                                      \
            JSProperty *_pce_prop;                                            \
            PCE_LOAD(_cache, _pce, _entry);                                   \
            _pce_prop = PCE_PROPERTY(_entry);                                 \
            if (_pce_prop && _pce_prop != prop)                               \
                _cache->recycles++;                                           \
            PCE_OBJECT(_entry) = obj;                                         \
            PCE_PROPERTY(_entry) = prop;                                      \
            _cache->empty = JS_FALSE;                                         \
            _cache->fills++;                                                  \
            PCE_STORE(_cache, _pce, _entry);                                  \
        }                                                                     \
    JS_END_MACRO

#define PROPERTY_CACHE_TEST(cache, obj, id, prop)                             \
    JS_BEGIN_MACRO                                                            \
	uintN _hashIndex = (uintN)PROPERTY_CACHE_HASH(obj, id);               \
	JSPropertyCache *_cache = (cache);                                    \
	JSPropertyCacheEntry *_pce = &_cache->table[_hashIndex];              \
	JSPropertyCacheEntry _entry;                                          \
	JSProperty *_pce_prop;                                                \
	PCE_LOAD(_cache, _pce, _entry);                                       \
	_pce_prop = PCE_PROPERTY(_entry);                                     \
	_cache->tests++;                                                      \
	if (_pce_prop &&                                                      \
	    sym_id(((JSScopeProperty *)_pce_prop)->symbols) == id &&          \
	    PCE_OBJECT(_entry) == obj) {                                      \
	    prop = _pce_prop;                                                 \
	} else {                                                              \
	    _cache->misses++;                                                 \
	    prop = NULL;                                                      \
	}                                                                     \
    JS_END_MACRO

extern void
js_FlushPropertyCache(JSContext *cx);

extern void
js_FlushPropertyCacheByProp(JSContext *cx, JSProperty *prop);

extern void
js_DisablePropertyCache(JSContext *cx);

extern void
js_EnablePropertyCache(JSContext *cx);

extern JS_FRIEND_API(jsval *)
js_AllocStack(JSContext *cx, uintN nslots, void **markp);

extern JS_FRIEND_API(void)
js_FreeStack(JSContext *cx, void *mark);

extern JSBool
js_GetArgument(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

extern JSBool
js_SetArgument(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

extern JSBool
js_GetLocalVariable(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

extern JSBool
js_SetLocalVariable(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

/*
 * NB: js_Invoke requires that cx is currently running JS (i.e., that cx->fp
 * is non-null).
 */
extern JS_FRIEND_API(JSBool)
js_Invoke(JSContext *cx, uintN argc, uintN flags);

/*
 * Consolidated js_Invoke flags.  NB: JSINVOKE_CONSTRUCT must be 1 or JS_TRUE
 * (see js_Invoke's initialization of frame.constructing).
 */
#define JSINVOKE_CONSTRUCT      0x1     /* construct object rather than call */
#define JSINVOKE_INTERNAL       0x2     /* internal call, not from a script */

/*
 * "Internal" calls may come from C or C++ code using a JSContext on which no
 * JS is running (!cx->fp), so they may need to push a dummy JSStackFrame.
 */
#define js_InternalCall(cx,obj,fval,argc,argv,rval)                           \
    js_InternalInvoke(cx, obj, fval, 0, argc, argv, rval)

#define js_InternalConstruct(cx,obj,fval,argc,argv,rval)                      \
    js_InternalInvoke(cx, obj, fval, JSINVOKE_CONSTRUCT, argc, argv, rval)

extern JSBool
js_InternalInvoke(JSContext *cx, JSObject *obj, jsval fval, uintN flags,
                  uintN argc, jsval *argv, jsval *rval);

extern JSBool
js_Execute(JSContext *cx, JSObject *chain, JSScript *script,
           JSStackFrame *down, uintN flags, jsval *result);

extern JSBool
js_CheckRedeclaration(JSContext *cx, JSObject *obj, jsid id, uintN attrs,
                      JSBool *foundp);

extern JSBool
js_Interpret(JSContext *cx, jsval *result);

JS_END_EXTERN_C

#endif /* jsinterp_h___ */
