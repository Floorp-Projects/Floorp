/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

JS_BEGIN_EXTERN_C

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
    jsbytecode      *pc;            /* program counter */
    jsval           *sp;            /* stack pointer */
    jsval           *spbase;        /* operand stack base */
    uintN           sharpDepth;     /* array/object initializer depth */
    JSObject        *sharpArray;    /* scope for #n= initializer vars */
    uint32          flags;          /* frame flags -- see below */
    JSStackFrame    *dormantNext;   /* next dormant frame chain */
    JSObject        *xmlNamespace;  /* null or default xml namespace in E4X */
    JSObject        *blockChain;    /* active compile-time block scopes */
};

typedef struct JSInlineFrame {
    JSStackFrame    frame;          /* base struct */
    jsval           *rvp;           /* ptr to caller's return value slot */
    void            *mark;          /* mark before inline frame */
    void            *hookData;      /* debugger call hook data */
    JSVersion       callerVersion;  /* dynamic version of calling script */
} JSInlineFrame;

/* JS stack frame flags. */
#define JSFRAME_CONSTRUCTING  0x01  /* frame is for a constructor invocation */
#define JSFRAME_INTERNAL      0x02  /* internal call, not invoked by a script */
#define JSFRAME_ASSIGNING     0x04  /* a complex (not simplex JOF_ASSIGNING) op
                                       is currently assigning to a property */
#define JSFRAME_DEBUGGER      0x08  /* frame for JS_EvaluateInStackFrame */
#define JSFRAME_EVAL          0x10  /* frame for obj_eval */
#define JSFRAME_SCRIPT_OBJECT 0x20  /* compiling source for a Script object */
#define JSFRAME_YIELDING      0x40  /* js_Interpret dispatched JSOP_YIELD */
#define JSFRAME_FILTERING     0x80  /* XML filtering predicate expression */
#define JSFRAME_ITERATOR      0x100 /* trying to get an iterator for for-in */
#define JSFRAME_POP_BLOCKS    0x200 /* scope chain contains blocks to pop */
#define JSFRAME_GENERATOR     0x400 /* frame belongs to generator-iterator */
#define JSFRAME_ROOTED_ARGV   0x800 /* frame.argv is rooted by the caller */

#define JSFRAME_OVERRIDE_SHIFT 24   /* override bit-set params; see jsfun.c */
#define JSFRAME_OVERRIDE_BITS  8

#define JSFRAME_SPECIAL       (JSFRAME_DEBUGGER | JSFRAME_EVAL)

extern JS_FRIEND_API(jsval *)
js_AllocStack(JSContext *cx, uintN nslots, void **markp);

extern JS_FRIEND_API(void)
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
extern JSBool
js_ComputeThis(JSContext *cx, jsval *argv);

/*
 * NB: js_Invoke requires that cx is currently running JS (i.e., that cx->fp
 * is non-null), and that vp points to the callee, |this| parameter, and
 * actual arguments of the call. [vp .. vp + 2 + argc) must belong to the last
 * JS stack segment that js_AllocStack or js_AllocRawStack allocated. The
 * function may use the space available after vp + 2 + argc in the stack
 * segment for temporaries so the caller should not use that space for values
 * that must be preserved across the call.
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
#define JSINVOKE_INTERNAL       JSFRAME_INTERNAL
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
js_CheckRedeclaration(JSContext *cx, JSObject *obj, jsid id, uintN attrs,
                      JSObject **objp, JSProperty **propp);

extern JSBool
js_StrictlyEqual(jsval lval, jsval rval);

extern JSBool
js_InvokeConstructor(JSContext *cx, jsval *vp, uintN argc);

extern JSBool
js_Interpret(JSContext *cx, jsbytecode *pc, jsval *result);

JS_END_EXTERN_C

#endif /* jsinterp_h___ */
