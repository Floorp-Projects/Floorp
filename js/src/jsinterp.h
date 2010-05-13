/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=78:
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
#include "jsfun.h"
#include "jsopcode.h"
#include "jsscript.h"

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
    jsbytecode      *imacpc;        /* null or interpreter macro call pc */
    jsval           *slots;         /* variables, locals and operand stack */
    JSObject        *callobj;       /* lazily created Call object */
    jsval           argsobj;        /* lazily created arguments object, must be
                                       JSVAL_OBJECT */
    JSScript        *script;        /* script being interpreted */
    JSFunction      *fun;           /* function being called or null */
    jsval           thisv;          /* "this" pointer if in method */
    uintN           argc;           /* actual argument count */
    jsval           *argv;          /* base of argument stack slots */
    jsval           rval;           /* function return value */
    JSStackFrame    *down;          /* previous frame */
    void            *annotation;    /* used by Java security */

    /*
     * We can't determine in advance which local variables can live on
     * the stack and be freed when their dynamic scope ends, and which
     * will be closed over and need to live in the heap.  So we place
     * variables on the stack initially, note when they are closed
     * over, and copy those that are out to the heap when we leave
     * their dynamic scope.
     *
     * The bytecode compiler produces a tree of block objects
     * accompanying each JSScript representing those lexical blocks in
     * the script that have let-bound variables associated with them.
     * These block objects are never modified, and never become part
     * of any function's scope chain.  Their parent slots point to the
     * innermost block that encloses them, or are NULL in the
     * outermost blocks within a function or in eval or global code.
     *
     * When we are in the static scope of such a block, blockChain
     * points to its compiler-allocated block object; otherwise, it is
     * NULL.
     *
     * scopeChain is the current scope chain, including 'call' and
     * 'block' objects for those function calls and lexical blocks
     * whose static scope we are currently executing in, and 'with'
     * objects for with statements; the chain is typically terminated
     * by a global object.  However, as an optimization, the young end
     * of the chain omits block objects we have not yet cloned.  To
     * create a closure, we clone the missing blocks from blockChain
     * (which is always current), place them at the head of
     * scopeChain, and use that for the closure's scope chain.  If we
     * never close over a lexical block, we never place a mutable
     * clone of it on scopeChain.
     *
     * This lazy cloning is implemented in js_GetScopeChain, which is
     * also used in some other cases --- entering 'with' blocks, for
     * example.
     */
    union {
        JSObject    *scopeChain;
        jsval       scopeChainVal;
    };
    JSObject        *blockChain;

    uint32          flags;          /* frame flags -- see below */
    JSStackFrame    *displaySave;   /* previous value of display entry for
                                       script->staticLevel */

    inline void assertValidStackDepth(uintN depth);

    void putActivationObjects(JSContext *cx) {
        /*
         * The order of calls here is important as js_PutCallObject needs to
         * access argsobj.
         */
        if (callobj) {
            js_PutCallObject(cx, this);
            JS_ASSERT(!argsobj);
        } else if (argsobj) {
            js_PutArgsObject(cx, this);
        }
    }

    jsval calleeValue() {
        JS_ASSERT(argv);
        return argv[-2];
    }

    JSObject *calleeObject() {
        JS_ASSERT(argv);
        return JSVAL_TO_OBJECT(argv[-2]);
    }

    JSObject *callee() {
        return argv ? JSVAL_TO_OBJECT(argv[-2]) : NULL;
    }

    /*
     * Get the object associated with the Execution Context's
     * VariableEnvironment (ES5 10.3). The given CallStack must contain this
     * stack frame.
     */
    JSObject *varobj(js::CallStack *cs);

    /* Short for: varobj(cx->activeCallStack()). */
    JSObject *varobj(JSContext *cx);

    inline JSObject *getThisObject(JSContext *cx);
};

#ifdef __cplusplus
static JS_INLINE uintN
FramePCOffset(JSStackFrame* fp)
{
    return uintN((fp->imacpc ? fp->imacpc : fp->regs->pc) - fp->script->code);
}
#endif

static JS_INLINE jsval *
StackBase(JSStackFrame *fp)
{
    return fp->slots + fp->script->nfixed;
}

#ifdef DEBUG
void
JSStackFrame::assertValidStackDepth(uintN depth)
{
    JS_ASSERT(0 <= regs->sp - StackBase(this));
    JS_ASSERT(depth <= uintptr_t(regs->sp - StackBase(this)));
}
#else
void
JSStackFrame::assertValidStackDepth(uintN /*depth*/){}
#endif

static JS_INLINE uintN
GlobalVarCount(JSStackFrame *fp)
{
    JS_ASSERT(!fp->fun);
    return fp->script->nfixed;
}

typedef struct JSInlineFrame {
    JSStackFrame    frame;          /* base struct */
    JSFrameRegs     callerRegs;     /* parent's frame registers */
    void            *mark;          /* mark before inline frame */
    void            *hookData;      /* debugger call hook data */
    JSVersion       callerVersion;  /* dynamic version of calling script */
} JSInlineFrame;

/* JS stack frame flags. */
#define JSFRAME_CONSTRUCTING   0x01 /* frame is for a constructor invocation */
#define JSFRAME_COMPUTED_THIS  0x02 /* frame.thisv was computed already and
                                       JSVAL_IS_OBJECT(thisv) */
#define JSFRAME_ASSIGNING      0x04 /* a complex (not simplex JOF_ASSIGNING) op
                                       is currently assigning to a property */
#define JSFRAME_DEBUGGER       0x08 /* frame for JS_EvaluateInStackFrame */
#define JSFRAME_EVAL           0x10 /* frame for obj_eval */
#define JSFRAME_ROOTED_ARGV    0x20 /* frame.argv is rooted by the caller */
#define JSFRAME_YIELDING       0x40 /* js_Interpret dispatched JSOP_YIELD */
#define JSFRAME_ITERATOR       0x80 /* trying to get an iterator for for-in */
#define JSFRAME_GENERATOR     0x200 /* frame belongs to generator-iterator */
#define JSFRAME_OVERRIDE_ARGS 0x400 /* overridden arguments local variable */

#define JSFRAME_SPECIAL       (JSFRAME_DEBUGGER | JSFRAME_EVAL)

/*
 * Interpreter stack arena-pool alloc and free functions.
 */
extern JS_REQUIRES_STACK JS_FRIEND_API(jsval *)
js_AllocStack(JSContext *cx, uintN nslots, void **markp);

extern JS_REQUIRES_STACK JS_FRIEND_API(void)
js_FreeStack(JSContext *cx, void *mark);

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
js_ComputeThis(JSContext *cx, jsval *argv);

extern const uint16 js_PrimitiveTestFlags[];

#define PRIMITIVE_THIS_TEST(fun,thisv)                                        \
    (JS_ASSERT(!JSVAL_IS_VOID(thisv)),                                        \
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
extern JS_REQUIRES_STACK JS_FRIEND_API(JSBool)
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

extern JS_FORCES_STACK JSBool
js_Execute(JSContext *cx, JSObject *chain, JSScript *script,
           JSStackFrame *down, uintN flags, jsval *result);

extern JS_REQUIRES_STACK JSBool
js_InvokeConstructor(JSContext *cx, uintN argc, JSBool clampReturn, jsval *vp);

extern JS_REQUIRES_STACK JSBool
js_Interpret(JSContext *cx);

#define JSPROP_INITIALIZER 0x100   /* NB: Not a valid property attribute. */

extern JSBool
js_CheckRedeclaration(JSContext *cx, JSObject *obj, jsid id, uintN attrs,
                      JSObject **objp, JSProperty **propp);

extern JSBool
js_StrictlyEqual(JSContext *cx, jsval lval, jsval rval);

/* === except that NaN is the same as NaN and -0 is not the same as +0. */
extern JSBool
js_SameValue(jsval v1, jsval v2, JSContext *cx);

extern JSBool
js_InternNonIntElementId(JSContext *cx, JSObject *obj, jsval idval, jsid *idp);

/*
 * Given an active context, a static scope level, and an upvar cookie, return
 * the value of the upvar.
 */
extern jsval&
js_GetUpvar(JSContext *cx, uintN level, uintN cookie);

/*
 * JS_LONE_INTERPRET indicates that the compiler should see just the code for
 * the js_Interpret function when compiling jsinterp.cpp. The rest of the code
 * from the file should be visible only when compiling jsinvoke.cpp. It allows
 * platform builds to optimize selectively js_Interpret when the granularity
 * of the optimizations with the given compiler is a compilation unit.
 *
 * JS_STATIC_INTERPRET is the modifier for functions defined in jsinterp.cpp
 * that only js_Interpret calls. When JS_LONE_INTERPRET is true all such
 * functions are declared below.
 */
#ifndef JS_LONE_INTERPRET
# ifdef _MSC_VER
#  define JS_LONE_INTERPRET 0
# else
#  define JS_LONE_INTERPRET 1
# endif
#endif

#define JS_MAX_INLINE_CALL_COUNT 3000

#if !JS_LONE_INTERPRET
# define JS_STATIC_INTERPRET    static
#else
# define JS_STATIC_INTERPRET

extern JS_REQUIRES_STACK jsval *
js_AllocRawStack(JSContext *cx, uintN nslots, void **markp);

extern JS_REQUIRES_STACK void
js_FreeRawStack(JSContext *cx, void *mark);

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
extern JSObject *
js_ComputeGlobalThis(JSContext *cx, jsval *argv);

extern JS_REQUIRES_STACK JSBool
js_EnterWith(JSContext *cx, jsint stackIndex);

extern JS_REQUIRES_STACK void
js_LeaveWith(JSContext *cx);

extern JS_REQUIRES_STACK JSClass *
js_IsActiveWithOrBlock(JSContext *cx, JSObject *obj, int stackDepth);

/*
 * Unwind block and scope chains to match the given depth. The function sets
 * fp->sp on return to stackDepth.
 */
extern JS_REQUIRES_STACK JSBool
js_UnwindScope(JSContext *cx, JSStackFrame *fp, jsint stackDepth,
               JSBool normalUnwind);

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
 * Opcode tracing helper. When len is not 0, cx->fp->regs->pc[-len] gives the
 * previous opcode.
 */
extern JS_REQUIRES_STACK void
js_TraceOpcode(JSContext *cx);

/*
 * JS_OPMETER helper functions.
 */
extern void
js_MeterOpcodePair(JSOp op1, JSOp op2);

extern void
js_MeterSlotOpcode(JSOp op, uint32 slot);

#endif /* JS_LONE_INTERPRET */

JS_END_EXTERN_C

inline JSObject *
JSStackFrame::getThisObject(JSContext *cx)
{
    if (flags & JSFRAME_COMPUTED_THIS)
        return JSVAL_TO_OBJECT(thisv);  /* JSVAL_COMPUTED_THIS invariant */
    JSObject* obj = js_ComputeThis(cx, argv);
    if (!obj)
        return NULL;
    thisv = OBJECT_TO_JSVAL(obj);
    flags |= JSFRAME_COMPUTED_THIS;
    return obj;
}

#endif /* jsinterp_h___ */
