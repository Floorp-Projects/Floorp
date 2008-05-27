/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef jsinterp_h___
#define jsinterp_h___
/*
 * JS interpreter interface.
 */
#include "jsprvtd.h"
#include "jspubtd.h"
#include "jsopcode.h"

JS_BEGIN_EXTERN_C

typedef struct JSFrameRegs {
    jsbytecode      *pc;            /* program counter */
    jsval           *sp;            /* stack pointer */
} JSFrameRegs;

/*
 * JS stack frame, may be allocated on the C stack by native callers.  Always
 * allocated on cx->stackPool for calls from the interpreter to an interpreted
 * function.
 *
 * NB: This struct is manually initialized in jsinterp.c and jsiter.c.  If you
 * add new members, update both files.  But first, try to remove members.  The
 * sharp* and xml* members should be moved onto the stack as local variables
 * with well-known slots, if possible.
 */
struct JSStackFrame {
    JSFrameRegs     *regs;
    jsval           *spbase;        /* operand stack base */
    JSObject        *callobj;       /* lazily created Call object */
    JSObject        *argsobj;       /* lazily created arguments object */
    JSObject        *varobj;        /* variables object, where vars go */
    JSObject        *callee;        /* function or script object */
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
    uintN           sharpDepth;     /* array/object initializer depth */
    JSObject        *sharpArray;    /* scope for #n= initializer vars */
    uint32          flags;          /* frame flags -- see below */
    JSStackFrame    *dormantNext;   /* next dormant frame chain */
    JSObject        *xmlNamespace;  /* null or default xml namespace in E4X */
    JSObject        *blockChain;    /* active compile-time block scopes */
#ifdef DEBUG
    jsrefcount      pcDisabledSave; /* for balanced property cache control */
#endif
};

typedef struct JSInlineFrame {
    JSStackFrame    frame;          /* base struct */
    JSFrameRegs     callerRegs;     /* parent's frame registers */
    void            *mark;          /* mark before inline frame */
    void            *hookData;      /* debugger call hook data */
    JSVersion       callerVersion;  /* dynamic version of calling script */
} JSInlineFrame;

/* JS stack frame flags. */
#define JSFRAME_CONSTRUCTING   0x01 /* frame is for a constructor invocation */
#define JSFRAME_COMPUTED_THIS  0x02 /* frame.thisp was computed already */
#define JSFRAME_ASSIGNING      0x04 /* a complex (not simplex JOF_ASSIGNING) op
                                       is currently assigning to a property */
#define JSFRAME_DEBUGGER       0x08 /* frame for JS_EvaluateInStackFrame */
#define JSFRAME_EVAL           0x10 /* frame for obj_eval */
#define JSFRAME_SCRIPT_OBJECT  0x20 /* compiling source for a Script object */
#define JSFRAME_YIELDING       0x40 /* js_Interpret dispatched JSOP_YIELD */
#define JSFRAME_ITERATOR       0x80 /* trying to get an iterator for for-in */
#define JSFRAME_POP_BLOCKS    0x100 /* scope chain contains blocks to pop */
#define JSFRAME_GENERATOR     0x200 /* frame belongs to generator-iterator */
#define JSFRAME_ROOTED_ARGV   0x400 /* frame.argv is rooted by the caller */

#define JSFRAME_OVERRIDE_SHIFT 24   /* override bit-set params; see jsfun.c */
#define JSFRAME_OVERRIDE_BITS  8

#define JSFRAME_SPECIAL       (JSFRAME_DEBUGGER | JSFRAME_EVAL)

/*
 * Property cache with structurally typed capabilities for invalidation, for
 * polymorphic callsite method/get/set speedups.
 *
 * See bug https://bugzilla.mozilla.org/show_bug.cgi?id=365851.
 */
#define PROPERTY_CACHE_LOG2     12
#define PROPERTY_CACHE_SIZE     JS_BIT(PROPERTY_CACHE_LOG2)
#define PROPERTY_CACHE_MASK     JS_BITMASK(PROPERTY_CACHE_LOG2)

#define PROPERTY_CACHE_HASH(pc,kshape)                                        \
    ((((jsuword)(pc) >> PROPERTY_CACHE_LOG2) ^ (jsuword)(pc) ^ (kshape)) &    \
     PROPERTY_CACHE_MASK)

#define PROPERTY_CACHE_HASH_PC(pc,kshape)                                     \
    PROPERTY_CACHE_HASH(pc, kshape)

#define PROPERTY_CACHE_HASH_ATOM(atom,obj,pobj)                               \
    PROPERTY_CACHE_HASH((jsuword)(atom) >> 2, OBJ_SCOPE(obj)->shape)

/*
 * Property cache value capability macros.
 */
#define PCVCAP_PROTOBITS        4
#define PCVCAP_PROTOSIZE        JS_BIT(PCVCAP_PROTOBITS)
#define PCVCAP_PROTOMASK        JS_BITMASK(PCVCAP_PROTOBITS)

#define PCVCAP_SCOPEBITS        4
#define PCVCAP_SCOPESIZE        JS_BIT(PCVCAP_SCOPEBITS)
#define PCVCAP_SCOPEMASK        JS_BITMASK(PCVCAP_SCOPEBITS)

#define PCVCAP_TAGBITS          (PCVCAP_PROTOBITS + PCVCAP_SCOPEBITS)
#define PCVCAP_TAGMASK          JS_BITMASK(PCVCAP_TAGBITS)
#define PCVCAP_TAG(t)           ((t) & PCVCAP_TAGMASK)

#define PCVCAP_MAKE(t,s,p)      (((t) << PCVCAP_TAGBITS) |                    \
                                 ((s) << PCVCAP_PROTOBITS) |                  \
                                 (p))
#define PCVCAP_SHAPE(t)         ((t) >> PCVCAP_TAGBITS)

#define SHAPE_OVERFLOW_BIT      JS_BIT(32 - PCVCAP_TAGBITS)

extern uint32
js_GenerateShape(JSContext *cx, JSBool gcLocked);

struct JSPropCacheEntry {
    jsbytecode          *kpc;           /* pc if vcap tag is <= 1, else atom */
    jsuword             kshape;         /* key shape if pc, else obj for atom */
    jsuword             vcap;           /* value capability, see above */
    jsuword             vword;          /* value word, see PCVAL_* below */
};

#if defined DEBUG_brendan || defined DEBUG_brendaneich
#define JS_PROPERTY_CACHE_METERING 1
#endif

typedef struct JSPropertyCache {
    JSPropCacheEntry    table[PROPERTY_CACHE_SIZE];
    JSBool              empty;
    jsrefcount          disabled;       /* signed for anti-underflow asserts */
#ifdef JS_PROPERTY_CACHE_METERING
    uint32              fills;          /* number of cache entry fills */
    uint32              nofills;        /* couldn't fill (e.g. default get) */
    uint32              rofills;        /* set on read-only prop can't fill */
    uint32              disfills;       /* fill attempts on disabled cache */
    uint32              oddfills;       /* fill attempt after setter deleted */
    uint32              modfills;       /* fill that rehashed to a new entry */
    uint32              brandfills;     /* scope brandings to type structural
                                           method fills */
    uint32              noprotos;       /* resolve-returned non-proto pobj */
    uint32              longchains;     /* overlong scope and/or proto chain */
    uint32              recycles;       /* cache entries recycled by fills */
    uint32              pcrecycles;     /* pc-keyed entries recycled by atom-
                                           keyed fills */
    uint32              tests;          /* cache probes */
    uint32              pchits;         /* fast-path polymorphic op hits */
    uint32              protopchits;    /* pchits hitting immediate prototype */
    uint32              initests;       /* cache probes from JSOP_INITPROP */
    uint32              inipchits;      /* init'ing next property pchit case */
    uint32              inipcmisses;    /* init'ing next property pc misses */
    uint32              settests;       /* cache probes from JOF_SET opcodes */
    uint32              addpchits;      /* adding next property pchit case */
    uint32              setpchits;      /* setting existing property pchit */
    uint32              setpcmisses;    /* setting/adding property pc misses */
    uint32              slotchanges;    /* clasp->reserveSlots result variance-
                                           induced slot changes */
    uint32              setmisses;      /* JSOP_SET{NAME,PROP} total misses */
    uint32              idmisses;       /* slow-path key id == atom misses */
    uint32              komisses;       /* slow-path key object misses */
    uint32              vcmisses;       /* value capability misses */
    uint32              misses;         /* cache misses */
    uint32              flushes;        /* cache flushes */
# define PCMETER(x)     x
#else
# define PCMETER(x)     ((void)0)
#endif
} JSPropertyCache;

/*
 * Property cache value tagging/untagging macros.
 */
#define PCVAL_OBJECT            0
#define PCVAL_SLOT              1
#define PCVAL_SPROP             2

#define PCVAL_TAGBITS           2
#define PCVAL_TAGMASK           JS_BITMASK(PCVAL_TAGBITS)
#define PCVAL_TAG(v)            ((v) & PCVAL_TAGMASK)
#define PCVAL_CLRTAG(v)         ((v) & ~(jsuword)PCVAL_TAGMASK)
#define PCVAL_SETTAG(v,t)       ((jsuword)(v) | (t))

#define PCVAL_NULL              0
#define PCVAL_IS_NULL(v)        ((v) == PCVAL_NULL)

#define PCVAL_IS_OBJECT(v)      (PCVAL_TAG(v) == PCVAL_OBJECT)
#define PCVAL_TO_OBJECT(v)      ((JSObject *) (v))
#define OBJECT_TO_PCVAL(obj)    ((jsuword) (obj))

#define PCVAL_OBJECT_TO_JSVAL(v) OBJECT_TO_JSVAL(PCVAL_TO_OBJECT(v))
#define JSVAL_OBJECT_TO_PCVAL(v) OBJECT_TO_PCVAL(JSVAL_TO_OBJECT(v))

#define PCVAL_IS_SLOT(v)        ((v) & PCVAL_SLOT)
#define PCVAL_TO_SLOT(v)        ((jsuint)(v) >> 1)
#define SLOT_TO_PCVAL(i)        (((jsuword)(i) << 1) | PCVAL_SLOT)

#define PCVAL_IS_SPROP(v)       (PCVAL_TAG(v) == PCVAL_SPROP)
#define PCVAL_TO_SPROP(v)       ((JSScopeProperty *) PCVAL_CLRTAG(v))
#define SPROP_TO_PCVAL(sprop)   PCVAL_SETTAG(sprop, PCVAL_SPROP)

/*
 * Fill property cache entry for key cx->fp->pc, optimized value word computed
 * from obj and sprop, and entry capability forged from OBJ_SCOPE(obj)->shape,
 * scopeIndex, and protoIndex.
 */
extern void
js_FillPropertyCache(JSContext *cx, JSObject *obj, jsuword kshape,
                     uintN scopeIndex, uintN protoIndex,
                     JSObject *pobj, JSScopeProperty *sprop,
                     JSPropCacheEntry **entryp);

/*
 * Property cache lookup macros. PROPERTY_CACHE_TEST is designed to inline the
 * fast path in js_Interpret, so it makes "just-so" restrictions on parameters,
 * e.g. pobj and obj should not be the same variable, since for JOF_PROP-mode
 * opcodes, obj must not be changed because of a cache miss.
 *
 * On return from PROPERTY_CACHE_TEST, if atom is null then obj points to the
 * scope chain element in which the property was found, pobj is locked, and
 * entry is valid. If atom is non-null then no object is locked but entry is
 * still set correctly for use, e.g., by js_FillPropertyCache and atom should
 * be used as the id to find.
 *
 * We must lock pobj on a hit in order to close races with threads that might
 * be deleting a property from its scope, or otherwise invalidating property
 * caches (on all threads) by re-generating scope->shape.
 */
#define PROPERTY_CACHE_TEST(cx, pc, obj, pobj, entry, atom)                   \
    do {                                                                      \
        JSPropertyCache *cache_ = &JS_PROPERTY_CACHE(cx);                     \
        uint32 kshape_ = (JS_ASSERT(OBJ_IS_NATIVE(obj)),                      \
                          OBJ_SCOPE(obj)->shape);                             \
        entry = &cache_->table[PROPERTY_CACHE_HASH_PC(pc, kshape_)];          \
        PCMETER(cache_->tests++);                                             \
        JS_ASSERT(&obj != &pobj);                                             \
        if (entry->kpc == pc && entry->kshape == kshape_) {                   \
            JSObject *tmp_;                                                   \
            pobj = obj;                                                       \
            JS_LOCK_OBJ(cx, pobj);                                            \
            JS_ASSERT(PCVCAP_TAG(entry->vcap) <= 1);                          \
            if (PCVCAP_TAG(entry->vcap) == 1 &&                               \
                (tmp_ = LOCKED_OBJ_GET_PROTO(pobj)) != NULL &&                \
                OBJ_IS_NATIVE(tmp_)) {                                        \
                JS_UNLOCK_OBJ(cx, pobj);                                      \
                pobj = tmp_;                                                  \
                JS_LOCK_OBJ(cx, pobj);                                        \
            }                                                                 \
            if (PCVCAP_SHAPE(entry->vcap) == OBJ_SCOPE(pobj)->shape) {        \
                PCMETER(cache_->pchits++);                                    \
                PCMETER(!PCVCAP_TAG(entry->vcap) || cache_->protopchits++);   \
                pobj = OBJ_SCOPE(pobj)->object;                               \
                atom = NULL;                                                  \
                break;                                                        \
            }                                                                 \
            JS_UNLOCK_OBJ(cx, pobj);                                          \
        }                                                                     \
        atom = js_FullTestPropertyCache(cx, pc, &obj, &pobj, &entry);         \
        if (atom)                                                             \
            PCMETER(cache_->misses++);                                        \
    } while (0)

extern JSAtom *
js_FullTestPropertyCache(JSContext *cx, jsbytecode *pc,
                         JSObject **objp, JSObject **pobjp,
                         JSPropCacheEntry **entryp);

extern void
js_FlushPropertyCache(JSContext *cx);

extern void
js_FlushPropertyCacheForScript(JSContext *cx, JSScript *script);

extern void
js_DisablePropertyCache(JSContext *cx);

extern void
js_EnablePropertyCache(JSContext *cx);

/*
 * Interpreter stack arena-pool alloc and free functions.
 */
extern JS_FRIEND_API(jsval *)
js_AllocStack(JSContext *cx, uintN nslots, void **markp);

extern JS_FRIEND_API(void)
js_FreeStack(JSContext *cx, void *mark);

extern jsval *
js_AllocRawStack(JSContext *cx, uintN nslots, void **markp);

extern void
js_FreeRawStack(JSContext *cx, void *mark);

/*
 * Refresh and return fp->scopeChain.  It may be stale if block scopes are
 * active but not yet reflected by objects in the scope chain.  If a block
 * scope contains a with, eval, XML filtering predicate, or similar such
 * dynamically scoped construct, then compile-time block scope at fp->blocks
 * must reflect at runtime.
 */
extern JSObject *
js_GetScopeChain(JSContext *cx, JSStackFrame *fp);

/*
 * Given a context and a vector of [callee, this, args...] for a function that
 * was specified with a JSFUN_THISP_PRIMITIVE flag, get the primitive value of
 * |this| into *thisvp. In doing so, if |this| is an object, insist it is an
 * instance of clasp and extract its private slot value to return via *thisvp.
 *
 * NB: this function loads and uses *vp before storing *thisvp, so the two may
 * alias the same jsval.
 */
extern JSBool
js_GetPrimitiveThis(JSContext *cx, jsval *vp, JSClass *clasp, jsval *thisvp);

/*
 * For a call with arguments argv including argv[-1] (nominal |this|) and
 * argv[-2] (callee) replace null |this| with callee's parent, replace
 * primitive values with the equivalent wrapper objects and censor activation
 * objects as, per ECMA-262, they may not be referred to by |this|. argv[-1]
 * must not be a JSVAL_VOID.
 */
extern JSObject *
js_ComputeThis(JSContext *cx, JSBool lazy, jsval *argv);

/*
 * ECMA requires "the global object", but in embeddings such as the browser,
 * which have multiple top-level objects (windows, frames, etc. in the DOM),
 * we prefer fun's parent.  An example that causes this code to run:
 *
 *   // in window w1
 *   function f() { return this }
 *   function g() { return f }
 *
 *   // in window w2
 *   var h = w1.g()
 *   alert(h() == w1)
 *
 * The alert should display "true".
 */
JSObject *
js_ComputeGlobalThis(JSContext *cx, JSBool lazy, jsval *argv);

extern const uint16 js_PrimitiveTestFlags[];

#define PRIMITIVE_THIS_TEST(fun,thisv)                                        \
    (JS_ASSERT(thisv != JSVAL_VOID),                                          \
     JSFUN_THISP_TEST(JSFUN_THISP_FLAGS((fun)->flags),                        \
                      js_PrimitiveTestFlags[JSVAL_TAG(thisv) - 1]))

/*
 * NB: js_Invoke requires that cx is currently running JS (i.e., that cx->fp
 * is non-null), and that vp points to the callee, |this| parameter, and
 * actual arguments of the call. [vp .. vp + 2 + argc) must belong to the last
 * JS stack segment that js_AllocStack allocated. The function may use the
 * space available after vp + 2 + argc in the stack segment for temporaries,
 * so the caller should not use that space for values that must be preserved
 * across the call.
 */
extern JS_FRIEND_API(JSBool)
js_Invoke(JSContext *cx, uintN argc, jsval *vp, uintN flags);

/*
 * Consolidated js_Invoke flags simply rename certain JSFRAME_* flags, so that
 * we can share bits stored in JSStackFrame.flags and passed to:
 *
 *   js_Invoke
 *   js_InternalInvoke
 *   js_ValueToFunction
 *   js_ValueToFunctionObject
 *   js_ValueToCallableObject
 *   js_ReportIsNotFunction
 *
 * See jsfun.h for the latter four and flag renaming macros.
 */
#define JSINVOKE_CONSTRUCT      JSFRAME_CONSTRUCTING
#define JSINVOKE_ITERATOR       JSFRAME_ITERATOR

/*
 * Mask to isolate construct and iterator flags for use with jsfun.h functions.
 */
#define JSINVOKE_FUNFLAGS       (JSINVOKE_CONSTRUCT | JSINVOKE_ITERATOR)

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
js_InternalGetOrSet(JSContext *cx, JSObject *obj, jsid id, jsval fval,
                    JSAccessMode mode, uintN argc, jsval *argv, jsval *rval);

extern JSBool
js_Execute(JSContext *cx, JSObject *chain, JSScript *script,
           JSStackFrame *down, uintN flags, jsval *result);

extern JSBool
js_InvokeConstructor(JSContext *cx, uintN argc, jsval *vp);

extern JSBool
js_Interpret(JSContext *cx);

#define JSPROP_INITIALIZER 0x100   /* NB: Not a valid property attribute. */

extern JSBool
js_CheckRedeclaration(JSContext *cx, JSObject *obj, jsid id, uintN attrs,
                      JSObject **objp, JSProperty **propp);

extern JSBool
js_StrictlyEqual(JSContext *cx, jsval lval, jsval rval);

extern JSBool
js_EnterWith(JSContext *cx, jsint stackIndex);

extern void
js_LeaveWith(JSContext *cx);

extern JSClass *
js_IsActiveWithOrBlock(JSContext *cx, JSObject *obj, int stackDepth);

extern jsint
js_CountWithBlocks(JSContext *cx, JSStackFrame *fp);

/*
 * Unwind block and scope chains to match the given depth. The function sets
 * fp->sp on return to stackDepth.
 */
extern JSBool
js_UnwindScope(JSContext *cx, JSStackFrame *fp, jsint stackDepth,
               JSBool normalUnwind);

extern JSBool
js_InternNonIntElementId(JSContext *cx, JSObject *obj, jsval idval, jsid *idp);

extern JSBool
js_ImportProperty(JSContext *cx, JSObject *obj, jsid id);

extern JSBool
js_OnUnknownMethod(JSContext *cx, jsval *vp);

/*
 * Find the results of incrementing or decrementing *vp. For pre-increments,
 * both *vp and *vp2 will contain the result on return. For post-increments,
 * vp will contain the original value converted to a number and vp2 will get
 * the result. Both vp and vp2 must be roots.
 */
extern JSBool
js_DoIncDec(JSContext *cx, const JSCodeSpec *cs, jsval *vp, jsval *vp2);

/*
 * JS_OPMETER helper functions.
 */
extern void
js_MeterOpcodePair(JSOp op1, JSOp op2);

extern void
js_MeterSlotOpcode(JSOp op, uint32 slot);

JS_END_EXTERN_C

#endif /* jsinterp_h___ */
