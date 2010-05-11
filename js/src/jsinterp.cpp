/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
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

#define __STDC_LIMIT_MACROS

/*
 * JavaScript bytecode interpreter.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "jstypes.h"
#include "jsstdint.h"
#include "jsarena.h" /* Added by JSIFY */
#include "jsutil.h" /* Added by JSIFY */
#include "jsprf.h"
#include "jsapi.h"
#include "jsarray.h"
#include "jsatom.h"
#include "jsbool.h"
#include "jscntxt.h"
#include "jsdate.h"
#include "jsversion.h"
#include "jsdbgapi.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jsinterp.h"
#include "jsiter.h"
#include "jslock.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsopcode.h"
#include "jspropertycache.h"
#include "jsscan.h"
#include "jsscope.h"
#include "jsscript.h"
#include "jsstr.h"
#include "jsstaticcheck.h"
#include "jstracer.h"
#include "jslibmath.h"
#include "jsvector.h"

#include "jsatominlines.h"
#include "jspropertycacheinlines.h"
#include "jsobjinlines.h"
#include "jsscopeinlines.h"
#include "jsscriptinlines.h"
#include "jsstrinlines.h"
#include "jscntxtinlines.h"

#ifdef INCLUDE_MOZILLA_DTRACE
#include "jsdtracef.h"
#endif

#if JS_HAS_XML_SUPPORT
#include "jsxml.h"
#endif

#include "jsautooplen.h"

using namespace js;

/* jsinvoke_cpp___ indicates inclusion from jsinvoke.cpp. */
#if !JS_LONE_INTERPRET ^ defined jsinvoke_cpp___

#ifdef DEBUG
jsbytecode *const JSStackFrame::sInvalidPC = (jsbytecode *)0xbeef;
#endif

JSObject *
js_GetScopeChain(JSContext *cx, JSStackFrame *fp)
{
    JSObject *sharedBlock = fp->blockChain;

    if (!sharedBlock) {
        /*
         * Don't force a call object for a lightweight function call, but do
         * insist that there is a call object for a heavyweight function call.
         */
        JS_ASSERT(!fp->fun ||
                  !(fp->fun->flags & JSFUN_HEAVYWEIGHT) ||
                  fp->callobj);
        JS_ASSERT(fp->scopeChain);
        return fp->scopeChain;
    }

    /* We don't handle cloning blocks on trace.  */
    LeaveTrace(cx);

    /*
     * We have one or more lexical scopes to reflect into fp->scopeChain, so
     * make sure there's a call object at the current head of the scope chain,
     * if this frame is a call frame.
     *
     * Also, identify the innermost compiler-allocated block we needn't clone.
     */
    JSObject *limitBlock, *limitClone;
    if (fp->fun && !fp->callobj) {
        JS_ASSERT_IF(fp->scopeChain->getClass() == &js_BlockClass,
                     fp->scopeChain->getPrivate() != js_FloatingFrameIfGenerator(cx, fp));
        if (!js_GetCallObject(cx, fp))
            return NULL;

        /* We know we must clone everything on blockChain. */
        limitBlock = limitClone = NULL;
    } else {
        /*
         * scopeChain includes all blocks whose static scope we're within that
         * have already been cloned.  Find the innermost such block.  Its
         * prototype should appear on blockChain; we'll clone blockChain up
         * to, but not including, that prototype.
         */
        limitClone = fp->scopeChain;
        while (limitClone->getClass() == &js_WithClass)
            limitClone = limitClone->getParent();
        JS_ASSERT(limitClone);

        /*
         * It may seem like we don't know enough about limitClone to be able
         * to just grab its prototype as we do here, but it's actually okay.
         *
         * If limitClone is a block object belonging to this frame, then its
         * prototype is the innermost entry in blockChain that we have already
         * cloned, and is thus the place to stop when we clone below.
         *
         * Otherwise, there are no blocks for this frame on scopeChain, and we
         * need to clone the whole blockChain.  In this case, limitBlock can
         * point to any object known not to be on blockChain, since we simply
         * loop until we hit limitBlock or NULL.  If limitClone is a block, it
         * isn't a block from this function, since blocks can't be nested
         * within themselves on scopeChain (recursion is dynamic nesting, not
         * static nesting).  If limitClone isn't a block, its prototype won't
         * be a block either.  So we can just grab limitClone's prototype here
         * regardless of its type or which frame it belongs to.
         */
        limitBlock = limitClone->getProto();

        /* If the innermost block has already been cloned, we are done. */
        if (limitBlock == sharedBlock)
            return fp->scopeChain;
    }

    /*
     * Special-case cloning the innermost block; this doesn't have enough in
     * common with subsequent steps to include in the loop.
     *
     * js_CloneBlockObject leaves the clone's parent slot uninitialized. We
     * populate it below.
     */
    JSObject *innermostNewChild = js_CloneBlockObject(cx, sharedBlock, fp);
    if (!innermostNewChild)
        return NULL;
    AutoObjectRooter tvr(cx, innermostNewChild);

    /*
     * Clone our way towards outer scopes until we reach the innermost
     * enclosing function, or the innermost block we've already cloned.
     */
    JSObject *newChild = innermostNewChild;
    for (;;) {
        JS_ASSERT(newChild->getProto() == sharedBlock);
        sharedBlock = sharedBlock->getParent();

        /* Sometimes limitBlock will be NULL, so check that first.  */
        if (sharedBlock == limitBlock || !sharedBlock)
            break;

        /* As in the call above, we don't know the real parent yet.  */
        JSObject *clone = js_CloneBlockObject(cx, sharedBlock, fp);
        if (!clone)
            return NULL;

        newChild->setParent(clone);
        newChild = clone;
    }
    newChild->setParent(fp->scopeChain);


    /*
     * If we found a limit block belonging to this frame, then we should have
     * found it in blockChain.
     */
    JS_ASSERT_IF(limitBlock &&
                 limitBlock->getClass() == &js_BlockClass &&
                 limitClone->getPrivate() == js_FloatingFrameIfGenerator(cx, fp),
                 sharedBlock);

    /* Place our newly cloned blocks at the head of the scope chain.  */
    fp->scopeChain = innermostNewChild;
    return fp->scopeChain;
}

JSBool
js_GetPrimitiveThis(JSContext *cx, Value *vp, Class *clasp, Value *thisvp)
{
    Value *p = &vp[1];
    if (p->isObject()) {
        JSObject *obj = ComputeThisObjectFromVp(cx, vp);
        if (!InstanceOf(cx, obj, clasp, vp + 2))
            return JS_FALSE;
        p = &obj->fslots[JSSLOT_PRIMITIVE_THIS];
    }
    thisvp->copy(*p);
    return JS_TRUE;
}

/* Some objects (e.g., With) delegate 'this' to another object. */
static inline bool
CallThisObjectHook(JSContext *cx, ObjPtr obj, Value *argv)
{
    obj = JSObject::thisObject(cx, obj);
    if (!obj)
        return false;
    argv[-1].copy(obj);
    return true;
}

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
JS_STATIC_INTERPRET bool
ComputeGlobalThis(JSContext *cx, Value *argv)
{
    JSObject *thisp;
    if (argv[-2].isPrimitive() || !argv[-2].asObject().getParent())
        thisp = cx->globalObject;
    else
        thisp = argv[-2].asObject().getGlobal();
    return CallThisObjectHook(cx, NonFunObjPtr(*thisp), argv);
}

static bool
ComputeThis(JSContext *cx, Value *argv)
{
    JS_ASSERT(!argv[-1].isNull());
    if (!argv[-1].isObject())
        return js_PrimitiveToObject(cx, &argv[-1]);

    ObjPtr thisp = argv[-1].asObjPtr();
    if (thisp->getClass() == &js_CallClass || thisp->getClass() == &js_BlockClass)
        return ComputeGlobalThis(cx, argv);

    return CallThisObjectHook(cx, thisp, argv);
}

namespace js {

bool
ComputeThisFromArgv(JSContext *cx, Value *argv)
{
    JS_ASSERT(!argv[-1].isMagic());  // check for SynthesizeFrame poisoning
    if (argv[-1].isNull())
        return ComputeGlobalThis(cx, argv);
    return ComputeThis(cx, argv);
}

}

#if JS_HAS_NO_SUCH_METHOD

const uint32 JSSLOT_FOUND_FUNCTION  = JSSLOT_PRIVATE;
const uint32 JSSLOT_SAVED_ID        = JSSLOT_PRIVATE + 1;

Class js_NoSuchMethodClass = {
    "NoSuchMethod",
    JSCLASS_HAS_RESERVED_SLOTS(2) | JSCLASS_IS_ANONYMOUS,
    PropertyStub,     PropertyStub,     PropertyStub,      PropertyStub,
    EnumerateStub,    ResolveStub,      ConvertStub,       NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

/*
 * When JSOP_CALLPROP or JSOP_CALLELEM does not find the method property of
 * the base object, we search for the __noSuchMethod__ method in the base.
 * If it exists, we store the method and the property's id into an object of
 * NoSuchMethod class and store this object into the callee's stack slot.
 * Later, js_Invoke will recognise such an object and transfer control to
 * NoSuchMethod that invokes the method like:
 *
 *   this.__noSuchMethod__(id, args)
 *
 * where id is the name of the method that this invocation attempted to
 * call by name, and args is an Array containing this invocation's actual
 * parameters.
 */
JS_STATIC_INTERPRET JSBool
js_OnUnknownMethod(JSContext *cx, Value *vp)
{
    JS_ASSERT(!vp[1].isPrimitive());

    JSObject *obj = &vp[1].asObject();
    jsid id = ATOM_TO_JSID(cx->runtime->atomState.noSuchMethodAtom);
    AutoValueRooter tvr(cx);
    if (!js_GetMethod(cx, obj, id, JSGET_NO_METHOD_BARRIER, tvr.addr()))
        return false;
    if (tvr.value().isPrimitive()) {
        vp[0].copy(tvr.value());
    } else {
#if JS_HAS_XML_SUPPORT
        /* Extract the function name from function::name qname. */
        if (vp[0].isObject()) {
            obj = &vp[0].asObject();
            if (!js_IsFunctionQName(cx, obj, &id))
                return false;
            if (id != 0)
                vp[0].copy(IdToValue(id));
        }
#endif
        obj = js_NewObjectWithGivenProto(cx, &js_NoSuchMethodClass,
                                                   NULL, NULL);
        if (!obj)
            return false;
        obj->fslots[JSSLOT_FOUND_FUNCTION].copy(tvr.value());
        obj->fslots[JSSLOT_SAVED_ID].copy(vp[0]);
        vp[0].setNonFunObj(*obj);
    }
    return true;
}

static JS_REQUIRES_STACK JSBool
NoSuchMethod(JSContext *cx, uintN argc, Value *vp, uint32 flags)
{
    InvokeArgsGuard args;
    if (!cx->stack().pushInvokeArgs(cx, 2, args))
        return JS_FALSE;

    JS_ASSERT(!vp[0].isPrimitive());
    JS_ASSERT(!vp[1].isPrimitive());
    JSObject *obj = &vp[0].asObject();
    JS_ASSERT(obj->getClass() == &js_NoSuchMethodClass);

    Value *invokevp = args.getvp();
    invokevp[0].copy(obj->fslots[JSSLOT_FOUND_FUNCTION]);
    invokevp[1].copy(vp[1]);
    invokevp[2].copy(obj->fslots[JSSLOT_SAVED_ID]);
    JSObject *argsobj = js_NewArrayObject(cx, argc, vp + 2);
    if (!argsobj)
        return JS_FALSE;
    invokevp[3].setNonFunObj(*argsobj);
    JSBool ok = (flags & JSINVOKE_CONSTRUCT)
                 ? InvokeConstructor(cx, args, JS_TRUE)
                 : Invoke(cx, args, flags);
    vp[0].copy(invokevp[0]);
    return ok;
}

#endif /* JS_HAS_NO_SUCH_METHOD */

namespace js {

const Value::MaskType PrimitiveValue::Masks[PrimitiveValue::THISP_ARRAY_SIZE] = {
    0,                                                          /* 000 */
    Value::StringMask,                                          /* 001 */
    Value::NumberMask,                                          /* 010 */
    Value::NumberMask | Value::StringMask,                      /* 011 */
    Value::BooleanMask,                                         /* 100 */
    Value::BooleanMask | Value::StringMask,                     /* 101 */
    Value::BooleanMask | Value::NumberMask,                     /* 110 */
    Value::BooleanMask | Value::NumberMask | Value::StringMask  /* 111 */
};

/*
 * Find a function reference and its 'this' object implicit first parameter
 * under argc arguments on cx's stack, and call the function.  Push missing
 * required arguments, allocate declared local variables, and pop everything
 * when done.  Then push the return value.
 */
JS_REQUIRES_STACK bool
Invoke(JSContext *cx, const InvokeArgsGuard &args, uintN flags)
{
    Value *vp = args.getvp();
    uintN argc = args.getArgc();

    JS_ASSERT(argc <= JS_ARGS_LENGTH_MAX);
    JS_ASSERT(!vp[1].isUndefined());

    if (vp[0].isPrimitive()) {
        js_ReportIsNotFunction(cx, vp, flags & JSINVOKE_FUNFLAGS);
        return false;
    }

    JSObject *funobj = &vp[0].asObject();
    JSObject *parent = funobj->getParent();
    Class *clasp = funobj->getClass();

    /* Filled by the following if/else statement. */
    Native native;
    JSFunction *fun;
    JSScript *script;
    if (clasp != &js_FunctionClass) {
#if JS_HAS_NO_SUCH_METHOD
        if (clasp == &js_NoSuchMethodClass)
            return NoSuchMethod(cx, argc, vp, flags);
#endif

        /* Function is inlined, all other classes use object ops. */
        const JSObjectOps *ops = funobj->map->ops;

        fun = NULL;
        script = NULL;

        /* Try a call or construct native object op. */
        if (flags & JSINVOKE_CONSTRUCT) {
            if (!vp[1].isObjectOrNull()) {
                if (!js_PrimitiveToObject(cx, &vp[1]))
                    return false;
            }
            native = ops->construct;
        } else {
            native = ops->call;
        }
        if (!native) {
            js_ReportIsNotFunction(cx, vp, flags & JSINVOKE_FUNFLAGS);
            return false;
        }
    } else {
        /* Get private data and set derived locals from it. */
        fun = GET_FUNCTION_PRIVATE(cx, funobj);
        if (FUN_INTERPRETED(fun)) {
            native = NULL;
            script = fun->u.i.script;
            JS_ASSERT(script);

            if (script->isEmpty()) {
                if (flags & JSINVOKE_CONSTRUCT) {
                    JS_ASSERT(vp[1].isObject());
                    vp->copy(vp[1]);
                } else {
                    vp->setUndefined();
                }
                return true;
            }
        } else {
            native = fun->u.n.native;
            script = NULL;
        }

        if (JSFUN_BOUND_METHOD_TEST(fun->flags)) {
            /* Handle bound method special case. */
            SetObject(&vp[1], parent);
        } else if (vp[1].isPrimitive()) {
            JS_ASSERT(!(flags & JSINVOKE_CONSTRUCT));
            if (PrimitiveThisTest(fun, vp[1]))
                goto start_call;
        }
    }

    if (flags & JSINVOKE_CONSTRUCT) {
        JS_ASSERT(vp[1].isObject());
    } else {
        /*
         * We must call js_ComputeThis in case we are not called from the
         * interpreter, where a prior bytecode has computed an appropriate
         * |this| already.
         *
         * But we need to compute |this| eagerly only for so-called "slow"
         * (i.e., not fast) native functions. Fast natives must use either
         * JS_THIS or JS_THIS_OBJECT, and scripted functions will go through
         * the appropriate this-computing bytecode, e.g., JSOP_THIS.
         */
        if (native && (!fun || !(fun->flags & JSFUN_FAST_NATIVE))) {
            if (!ComputeThisFromArgv(cx, vp + 2))
                return false;
            flags |= JSFRAME_COMPUTED_THIS;
        }
    }

  start_call:
    if (native && fun && fun->isFastNative()) {
#ifdef DEBUG_NOT_THROWING
        JSBool alreadyThrowing = cx->throwing;
#endif
        JSBool ok = ((FastNative) native)(cx, argc, vp);
        JS_RUNTIME_METER(cx->runtime, nativeCalls);
#ifdef DEBUG_NOT_THROWING
        if (ok && !alreadyThrowing)
            ASSERT_NOT_THROWING(cx);
#endif
        return ok;
    }

    /* Calculate slot usage. */
    uintN nmissing;
    uintN nvars;
    if (fun) {
        if (fun->isInterpreted()) {
            uintN minargs = fun->nargs;
            nmissing = minargs > argc ? minargs - argc : 0;
            nvars = fun->u.i.nvars;
        } else if (fun->isFastNative()) {
            nvars = nmissing = 0;
        } else {
            uintN minargs = fun->nargs;
            nmissing = (minargs > argc ? minargs - argc : 0) + fun->u.n.extra;
            nvars = 0;
        }
    } else {
        nvars = nmissing = 0;
    }

    uintN nslots = script ? script->nslots : 0;

    /*
     * Get a pointer to new frame/slots. This memory is not "claimed", so the
     * code before pushInvokeFrame must not reenter the interpreter.
     */
    JSFrameRegs regs;
    InvokeFrameGuard frame;
    if (!cx->stack().getInvokeFrame(cx, args, nmissing, nslots, frame))
        return false;
    JSStackFrame *fp = frame.getFrame();

    /* Initialize missing missing arguments and new local variables. */
    Value *missing = vp + 2 + argc;
    for (Value *v = missing, *end = missing + nmissing; v != end; ++v)
        v->setUndefined();
    for (Value *v = fp->slots(), *end = v + nvars; v != end; ++v)
        v->setUndefined();

    /* Initialize frame. */
    fp->thisv.copy(vp[1]);
    fp->callobj = NULL;
    fp->argsobj = NULL;
    fp->script = script;
    fp->fun = fun;
    fp->argc = argc;
    fp->argv = vp + 2;
    if (flags & JSINVOKE_CONSTRUCT)
        fp->rval.copy(fp->thisv);
    else
        fp->rval.setUndefined();
    fp->annotation = NULL;
    fp->scopeChain = NULL;
    fp->blockChain = NULL;
    fp->imacpc = NULL;
    fp->flags = flags;
    fp->displaySave = NULL;

    /* Initialize regs. */
    if (script) {
        regs.pc = script->code;
        regs.sp = fp->slots() + script->nfixed;
    } else {
        regs.pc = NULL;
        regs.sp = fp->slots();
    }

    /* Officially push |fp|. |frame|'s destructor pops. */
    cx->stack().pushInvokeFrame(cx, args, frame, regs);

    /* Now that the frame has been pushed, fix up the scope chain. */
    if (native) {
        /* Slow natives expect the caller's scopeChain as their scopeChain. */
        if (JSStackFrame *down = fp->down)
            fp->scopeChain = down->scopeChain;

        /* Ensure that we have a scope chain. */
        if (!fp->scopeChain)
            fp->scopeChain = parent;
    } else {
        /* Use parent scope so js_GetCallObject can find the right "Call". */
        fp->scopeChain = parent;
        if (fun->isHeavyweight() && !js_GetCallObject(cx, fp))
            return false;
    }

    /* Call the hook if present after we fully initialized the frame. */
    JSInterpreterHook hook = cx->debugHooks->callHook;
    void *hookData = NULL;
    if (hook)
        hookData = hook(cx, fp, JS_TRUE, 0, cx->debugHooks->callHookData);

#ifdef INCLUDE_MOZILLA_DTRACE
    /* DTrace function entry, non-inlines */
    if (JAVASCRIPT_FUNCTION_ENTRY_ENABLED())
        jsdtrace_function_entry(cx, fp, fun);
    if (JAVASCRIPT_FUNCTION_INFO_ENABLED())
        jsdtrace_function_info(cx, fp, fp->down, fun);
    if (JAVASCRIPT_FUNCTION_ARGS_ENABLED())
        jsdtrace_function_args(cx, fp, fun, fp->argc, fp->argv);
#endif

    /* Call the function, either a native method or an interpreted script. */
    JSBool ok;
    if (native) {
#ifdef DEBUG_NOT_THROWING
        JSBool alreadyThrowing = cx->throwing;
#endif
        /* Primitive |this| should not be passed to slow natives. */
        JSObject *thisp = &fp->thisv.asObject();
        ok = native(cx, thisp, fp->argc, fp->argv, &fp->rval);
        JS_ASSERT(cx->fp == fp);
        JS_RUNTIME_METER(cx->runtime, nativeCalls);
#ifdef DEBUG_NOT_THROWING
        if (ok && !alreadyThrowing)
            ASSERT_NOT_THROWING(cx);
#endif
    } else {
        JS_ASSERT(script);
        ok = Interpret(cx);
    }

#ifdef INCLUDE_MOZILLA_DTRACE
    /* DTrace function return, non-inlines */
    if (JAVASCRIPT_FUNCTION_RVAL_ENABLED())
        jsdtrace_function_rval(cx, fp, fun, fp->rval);
    if (JAVASCRIPT_FUNCTION_RETURN_ENABLED())
        jsdtrace_function_return(cx, fp, fun);
#endif

    if (hookData) {
        hook = cx->debugHooks->callHook;
        if (hook)
            hook(cx, fp, JS_FALSE, &ok, hookData);
    }

    fp->putActivationObjects(cx);
    vp->copy(fp->rval);
    return ok;
}

JS_REQUIRES_STACK JS_FRIEND_API(bool)
InvokeFriendAPI(JSContext *cx, const InvokeArgsGuard &args, uintN flags)
{
    return Invoke(cx, args, flags);
}

bool
InternalInvoke(JSContext *cx, JSObject *obj, const Value &fval, uintN flags,
               uintN argc, const Value *argv, Value *rval)
{
    LeaveTrace(cx);

    InvokeArgsGuard args;
    if (!cx->stack().pushInvokeArgs(cx, argc, args))
        return JS_FALSE;

    args.getvp()[0].copy(fval);
    SetObject(&args.getvp()[1], obj);
    memcpy(args.getvp() + 2, argv, argc * sizeof(*argv));

    if (!Invoke(cx, args, flags))
        return JS_FALSE;

    /*
     * Store *rval in the a scoped local root if a scope is open, else in
     * the lastInternalResult pigeon-hole GC root, solely so users of
     * InternalInvoke and its direct and indirect (js_ValueToString for
     * example) callers do not need to manage roots for local, temporary
     * references to such results.
     */
    rval->copy(*args.getvp());
    if (rval->isGCThing()) {
        JSLocalRootStack *lrs = JS_THREAD_DATA(cx)->localRootStack;
        if (lrs) {
            if (js_PushLocalRoot(cx, lrs, rval->asGCThing()) < 0)
                return JS_FALSE;
        } else {
            cx->weakRoots.lastInternalResult = rval->asGCThing();
        }
    }
    return JS_TRUE;
}

bool
InternalGetOrSet(JSContext *cx, JSObject *obj, jsid id, const Value &fval,
                 JSAccessMode mode, uintN argc, const Value *argv,
                 Value *rval)
{
    LeaveTrace(cx);

    /*
     * InternalInvoke could result in another try to get or set the same id
     * again, see bug 355497.
     */
    JS_CHECK_RECURSION(cx, return JS_FALSE);

    return InternalCall(cx, obj, fval, argc, argv, rval);
}

bool
Execute(JSContext *cx, JSObject *chain, JSScript *script,
        JSStackFrame *down, uintN flags, Value *result)
{
    if (script->isEmpty()) {
        if (result)
            result->setUndefined();
        return JS_TRUE;
    }

    LeaveTrace(cx);

#ifdef INCLUDE_MOZILLA_DTRACE
    struct JSDNotifyGuard {
        JSScript *script;
        JSDNotifyGuard(JSScript *s) : script(s) {
            if (JAVASCRIPT_EXECUTE_START_ENABLED())
                jsdtrace_execute_start(script);
        }
        ~JSDNotifyGuard() {
            if (JAVASCRIPT_EXECUTE_DONE_ENABLED())
                jsdtrace_execute_done(script);
        }

    } jsdNotifyGuard(script);
#endif

    /*
     * Get a pointer to new frame/slots. This memory is not "claimed", so the
     * code before pushExecuteFrame must not reenter the interpreter.
     *
     * N.B. when fp->argv is removed (bug 539144), argv will have to be copied
     * in before execution and copied out after.
     */
    JSFrameRegs regs;
    ExecuteFrameGuard frame;
    if (!cx->stack().getExecuteFrame(cx, down, 0, script->nslots, frame))
        return false;
    JSStackFrame *fp = frame.getFrame();

    /* Initialize fixed slots. */
    memset(fp->slots(), 0, script->nfixed * sizeof(*fp->slots()));
#if JS_HAS_SHARP_VARS
    JS_STATIC_ASSERT(SHARP_NSLOTS == 2);
    if (script->hasSharps) {
        JS_ASSERT(script->nfixed >= SHARP_NSLOTS);
        Value *sharps = &fp->slots()[script->nfixed - SHARP_NSLOTS];
        if (down && down->script && down->script->hasSharps) {
            JS_ASSERT(down->script->nfixed >= SHARP_NSLOTS);
            int base = (down->fun && !(down->flags & JSFRAME_SPECIAL))
                       ? down->fun->sharpSlotBase(cx)
                       : down->script->nfixed - SHARP_NSLOTS;
            if (base < 0)
                return false;
            sharps[0].copy(down->slots()[base]);
            sharps[1].copy(down->slots()[base + 1]);
        } else {
            sharps[0].setUndefined();
            sharps[1].setUndefined();
        }
    }
#endif

    /* Initialize frame. */
    JSObject *initialVarObj;
    if (down) {
        /* Propagate arg state for eval and the debugger API. */
        fp->callobj = down->callobj;
        fp->argsobj = down->argsobj;
        fp->fun = (script->staticLevel > 0) ? down->fun : NULL;
        fp->thisv.copy(down->thisv);
        fp->flags = flags | (down->flags & JSFRAME_COMPUTED_THIS);
        fp->argc = down->argc;
        fp->argv = down->argv;
        fp->annotation = down->annotation;
        fp->scopeChain = chain;

        /*
         * We want to call |down->varobj()|, but this requires knowing the
         * CallStack of |down|. If |down == cx->fp|, the callstack is simply
         * the context's active callstack, so we can use |down->varobj(cx)|.
         * When |down != cx->fp|, we need to do a slow linear search. Luckily,
         * this only happens with indirect eval and JS_EvaluateInStackFrame.
         */
        initialVarObj = (down == cx->fp)
                          ? down->varobj(cx)
                          : down->varobj(cx->containingCallStack(down));
    } else {
        fp->callobj = NULL;
        fp->argsobj = NULL;
        JSObject *obj = chain;
        if (cx->options & JSOPTION_VAROBJFIX) {
            while (JSObject *tmp = obj->getParent())
                obj = tmp;
        }
        fp->fun = NULL;
        JSObject *thisp = chain->thisObject(cx);
        if (!thisp)
            return false;
        SetObject(&fp->thisv, *thisp);
        fp->flags = flags | JSFRAME_COMPUTED_THIS;
        fp->argc = 0;
        fp->argv = NULL;
        fp->annotation = NULL;
        Innerize(cx, &chain);
        if (!chain)
            return false;
        fp->scopeChain = chain;

        initialVarObj = obj;
    }
    fp->script = script;
    fp->imacpc = NULL;
    fp->rval.setUndefined();
    fp->blockChain = NULL;

    /* Initialize regs. */
    regs.pc = script->code;
    regs.sp = fp->base();

    /* Officially push |fp|. |frame|'s destructor pops. */
    cx->stack().pushExecuteFrame(cx, frame, regs, initialVarObj);

    void *hookData = NULL;
    if (JSInterpreterHook hook = cx->debugHooks->executeHook)
        hookData = hook(cx, fp, JS_TRUE, 0, cx->debugHooks->executeHookData);

    JSBool ok = Interpret(cx);
    if (result)
        result->copy(fp->rval);

    if (hookData) {
        if (JSInterpreterHook hook = cx->debugHooks->executeHook)
            hook(cx, fp, JS_FALSE, &ok, hookData);
    }

    return ok;
}

bool
CheckRedeclaration(JSContext *cx, JSObject *obj, jsid id, uintN attrs,
                   JSObject **objp, JSProperty **propp)
{
    JSObject *obj2;
    JSProperty *prop;
    uintN oldAttrs, report;
    bool isFunction;
    const char *type, *name;

    /*
     * Both objp and propp must be either null or given. When given, *propp
     * must be null. This way we avoid an extra "if (propp) *propp = NULL" for
     * the common case of a non-existing property.
     */
    JS_ASSERT(!objp == !propp);
    JS_ASSERT_IF(propp, !*propp);

    /* The JSPROP_INITIALIZER case below may generate a warning. Since we must
     * drop the property before reporting it, we insists on !propp to avoid
     * looking up the property again after the reporting is done.
     */
    JS_ASSERT_IF(attrs & JSPROP_INITIALIZER, attrs == JSPROP_INITIALIZER);
    JS_ASSERT_IF(attrs == JSPROP_INITIALIZER, !propp);

    if (!obj->lookupProperty(cx, id, &obj2, &prop))
        return JS_FALSE;
    if (!prop)
        return JS_TRUE;

    /* Use prop as a speedup hint to obj->getAttributes. */
    if (!obj2->getAttributes(cx, id, prop, &oldAttrs)) {
        obj2->dropProperty(cx, prop);
        return JS_FALSE;
    }

    /*
     * If our caller doesn't want prop, drop it (we don't need it any longer).
     */
    if (!propp) {
        obj2->dropProperty(cx, prop);
        prop = NULL;
    } else {
        *objp = obj2;
        *propp = prop;
    }

    if (attrs == JSPROP_INITIALIZER) {
        /* Allow the new object to override properties. */
        if (obj2 != obj)
            return JS_TRUE;

        /* The property must be dropped already. */
        JS_ASSERT(!prop);
        report = JSREPORT_WARNING | JSREPORT_STRICT;

#ifdef __GNUC__
        isFunction = false;     /* suppress bogus gcc warnings */
#endif
    } else {
        /* We allow redeclaring some non-readonly properties. */
        if (((oldAttrs | attrs) & JSPROP_READONLY) == 0) {
            /* Allow redeclaration of variables and functions. */
            if (!(attrs & (JSPROP_GETTER | JSPROP_SETTER)))
                return JS_TRUE;

            /*
             * Allow adding a getter only if a property already has a setter
             * but no getter and similarly for adding a setter. That is, we
             * allow only the following transitions:
             *
             *   no-property --> getter --> getter + setter
             *   no-property --> setter --> getter + setter
             */
            if ((~(oldAttrs ^ attrs) & (JSPROP_GETTER | JSPROP_SETTER)) == 0)
                return JS_TRUE;

            /*
             * Allow redeclaration of an impermanent property (in which case
             * anyone could delete it and redefine it, willy-nilly).
             */
            if (!(oldAttrs & JSPROP_PERMANENT))
                return JS_TRUE;
        }
        if (prop)
            obj2->dropProperty(cx, prop);

        report = JSREPORT_ERROR;
        isFunction = (oldAttrs & (JSPROP_GETTER | JSPROP_SETTER)) != 0;
        if (!isFunction) {
            Value value;
            if (!obj->getProperty(cx, id, &value))
                return JS_FALSE;
            isFunction = value.isFunObj();
        }
    }

    type = (attrs == JSPROP_INITIALIZER)
           ? "property"
           : (oldAttrs & attrs & JSPROP_GETTER)
           ? js_getter_str
           : (oldAttrs & attrs & JSPROP_SETTER)
           ? js_setter_str
           : (oldAttrs & JSPROP_READONLY)
           ? js_const_str
           : isFunction
           ? js_function_str
           : js_var_str;
    name = js_ValueToPrintableString(cx, IdToValue(id));
    if (!name)
        return JS_FALSE;
    return JS_ReportErrorFlagsAndNumber(cx, report,
                                        js_GetErrorMessage, NULL,
                                        JSMSG_REDECLARED_VAR,
                                        type, name);
}

static JS_ALWAYS_INLINE bool
EqualObjects(JSContext *cx, JSObject *lobj, JSObject *robj)
{
    return lobj == robj ||
           js_GetWrappedObject(cx, lobj) == js_GetWrappedObject(cx, robj);
}

bool
StrictlyEqual(JSContext *cx, const Value &lval, const Value &rval)
{
    JS_STATIC_ASSERT(Value::NullMask == 0);

    Value::MaskType lmask = lval.mask, rmask = rval.mask;
    Value::MaskType maskor = lmask | rmask;

    if (lmask == rmask) {
        if (Value::isSingleton(lmask))
            return true;
        if (lmask == Value::Int32Mask)
            return lval.data.i32 == rval.data.i32;
        if (lmask == Value::DoubleMask)
            return JSDOUBLE_COMPARE(lval.data.dbl, ==, rval.data.dbl, JS_FALSE);
        if (lmask == Value::StringMask)
            return js_EqualStrings(lval.data.str, rval.data.str);
        if (lmask & Value::ObjectMask)
            return EqualObjects(cx, lval.data.obj, rval.data.obj);
        JS_ASSERT(lmask == Value::BooleanMask);
        return lval.data.boo == rval.data.boo;
    }

    /* Watch out for null */

    if (maskor == Value::NumberMask) {
        double ld = lmask == Value::DoubleMask ? lval.data.dbl : lval.data.i32;
        double rd = rmask == Value::DoubleMask ? rval.data.dbl : rval.data.i32;
        return JSDOUBLE_COMPARE(ld, ==, rd, JS_FALSE);
    }

    if (maskor == Value::ObjectMask)
        return EqualObjects(cx, lval.data.obj, rval.data.obj);

    return false;
}

static inline bool
IsNegativeZero(const Value &v)
{
    return v.isDouble() && JSDOUBLE_IS_NEGZERO(v.asDouble());
}

static inline bool
IsNaN(const Value &v)
{
    return v.isDouble() && JSDOUBLE_IS_NaN(v.asDouble());
}

bool
SameValue(JSContext *cx, const Value &v1, const Value &v2)
{
    if (IsNegativeZero(v1))
        return IsNegativeZero(v2);
    if (IsNegativeZero(v2))
        return false;
    if (IsNaN(v1) && IsNaN(v2))
        return true;
    return StrictlyEqual(cx, v1, v2);
}

JSType
TypeOfValue(JSContext *cx, const Value &vref)
{
    Value v;
    v.copy(vref);
    if (v.isNumber())
        return JSTYPE_NUMBER;
    if (v.isString())
        return JSTYPE_STRING;
    if (v.isNull())
        return JSTYPE_OBJECT;
    if (v.isUndefined())
        return JSTYPE_VOID;
    if (v.isObject())
        return v.asObject().map->ops->typeOf(cx, &v.asObject());
    JS_ASSERT(v.isBoolean());
    return JSTYPE_BOOLEAN;
}

bool
InstanceOfSlow(JSContext *cx, JSObject *obj, Class *clasp, Value *argv)
{
    JS_ASSERT(!obj || obj->getClass() != clasp);
    if (argv) {
        JSFunction *fun = js_ValueToFunction(cx, &argv[-2], 0);
        if (fun) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                 JSMSG_INCOMPATIBLE_PROTO,
                                 clasp->name, JS_GetFunctionName(fun),
                                 obj
                                 ? obj->getClass()->name
                                 : js_null_str);
        }
    }
    return false;
}

JS_REQUIRES_STACK bool
InvokeConstructor(JSContext *cx, const InvokeArgsGuard &args, JSBool clampReturn)
{
    JSFunction *fun = NULL;
    Value *vp = args.getvp();

    /* XXX clean up to avoid special cases above ObjectOps layer */
    if (vp->isPrimitive() || vp->isFunObj() ||
        vp->asNonFunObj().map->ops->construct)
    {
        fun = js_ValueToFunction(cx, vp, JSV2F_CONSTRUCT);
        if (!fun)
            return JS_FALSE;
    }

    JSObject *proto;
    JSObject *parent;
    Class *clasp = &js_ObjectClass;
    if (vp->isPrimitive()) {
        proto = NULL;
        parent = NULL;
        fun = NULL;
    } else {
        /*
         * Get the constructor prototype object for this function.
         * Use the nominal 'this' parameter slot, vp[1], as a local
         * root to protect this prototype, in case it has no other
         * strong refs.
         */
        JSObject *obj2 = &vp->asObject();
        if (!obj2->getProperty(cx, ATOM_TO_JSID(cx->runtime->atomState.classPrototypeAtom),
                               &vp[1])) {
            return JS_FALSE;
        }
        const Value &v = vp[1];
        if (v.isObjectOrNull())
            proto = v.asObjectOrNull();
        else
            proto = NULL;
        parent = obj2->getParent();

        if (obj2->getClass() == &js_FunctionClass) {
            JSFunction *f = GET_FUNCTION_PRIVATE(cx, obj2);
            if (!f->isInterpreted() && f->u.n.clasp)
                clasp = f->u.n.clasp;
        }
    }
    JSObject *obj = js_NewObject(cx, clasp, proto, parent);
    if (!obj)
        return JS_FALSE;

    /* Now we have an object with a constructor method; call it. */
    SetObject(&vp[1], *obj);
    if (!Invoke(cx, args, JSINVOKE_CONSTRUCT))
        return JS_FALSE;

    /* Check the return value and if it's primitive, force it to be obj. */
    if (clampReturn && vp->isPrimitive()) {
        if (!fun) {
            /* native [[Construct]] returning primitive is error */
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                 JSMSG_BAD_NEW_RESULT,
                                 js_ValueToPrintableString(cx, *vp));
            return JS_FALSE;
        }
        SetObject(vp, *obj);
    }

    JS_RUNTIME_METER(cx->runtime, constructs);
    return JS_TRUE;
}

} /* namespace js */

/*
 * Enter the new with scope using an object at sp[-1] and associate the depth
 * of the with block with sp + stackIndex.
 */
JS_STATIC_INTERPRET JS_REQUIRES_STACK JSBool
js_EnterWith(JSContext *cx, jsint stackIndex)
{
    JSStackFrame *fp = cx->fp;
    Value *sp = cx->regs->sp;
    JS_ASSERT(stackIndex < 0);
    JS_ASSERT(fp->base() <= sp + stackIndex);

    JSObject *obj;
    if (sp[-1].isObject()) {
        obj = &sp[-1].asObject();
    } else {
        if (!js_ValueToNonNullObject(cx, sp[-1], &sp[-1]))
            return JS_FALSE;
        obj = &sp[-1].asObject();
    }

    JSObject *parent = js_GetScopeChain(cx, fp);
    if (!parent)
        return JS_FALSE;

    Innerize(cx, &obj);
    if (!obj)
        return JS_FALSE;

    JSObject *withobj = js_NewWithObject(cx, obj, parent,
                                         sp + stackIndex - fp->base());
    if (!withobj)
        return JS_FALSE;

    fp->scopeChain = withobj;
    return JS_TRUE;
}

JS_STATIC_INTERPRET JS_REQUIRES_STACK void
js_LeaveWith(JSContext *cx)
{
    JSObject *withobj;

    withobj = cx->fp->scopeChain;
    JS_ASSERT(withobj->getClass() == &js_WithClass);
    JS_ASSERT(withobj->getPrivate() == js_FloatingFrameIfGenerator(cx, cx->fp));
    JS_ASSERT(OBJ_BLOCK_DEPTH(cx, withobj) >= 0);
    cx->fp->scopeChain = withobj->getParent();
    withobj->setPrivate(NULL);
}

JS_REQUIRES_STACK Class *
js_IsActiveWithOrBlock(JSContext *cx, JSObject *obj, int stackDepth)
{
    Class *clasp;

    clasp = obj->getClass();
    if ((clasp == &js_WithClass || clasp == &js_BlockClass) &&
        obj->getPrivate() == js_FloatingFrameIfGenerator(cx, cx->fp) &&
        OBJ_BLOCK_DEPTH(cx, obj) >= stackDepth) {
        return clasp;
    }
    return NULL;
}

/*
 * Unwind block and scope chains to match the given depth. The function sets
 * fp->sp on return to stackDepth.
 */
JS_STATIC_INTERPRET JS_REQUIRES_STACK JSBool
js_UnwindScope(JSContext *cx, jsint stackDepth, JSBool normalUnwind)
{
    JSObject *obj;
    Class *clasp;

    JS_ASSERT(stackDepth >= 0);
    JS_ASSERT(cx->fp->base() + stackDepth <= cx->regs->sp);

    JSStackFrame *fp = cx->fp;
    for (obj = fp->blockChain; obj; obj = obj->getParent()) {
        JS_ASSERT(obj->getClass() == &js_BlockClass);
        if (OBJ_BLOCK_DEPTH(cx, obj) < stackDepth)
            break;
    }
    fp->blockChain = obj;

    for (;;) {
        obj = fp->scopeChain;
        clasp = js_IsActiveWithOrBlock(cx, obj, stackDepth);
        if (!clasp)
            break;
        if (clasp == &js_BlockClass) {
            /* Don't fail until after we've updated all stacks. */
            normalUnwind &= js_PutBlockObject(cx, normalUnwind);
        } else {
            js_LeaveWith(cx);
        }
    }

    cx->regs->sp = fp->base() + stackDepth;
    return normalUnwind;
}

JS_STATIC_INTERPRET JSBool
js_DoIncDec(JSContext *cx, const JSCodeSpec *cs, Value *vp, Value *vp2)
{
    if (cs->format & JOF_POST) {
        double d;
        if (!ValueToNumber(cx, vp, &d))
            return JS_FALSE;
        JS_ASSERT(vp->asDouble() == d);
        (cs->format & JOF_INC) ? ++d : --d;
        vp2->setDouble(d);
    }

    double d;
    if (!ValueToNumber(cx, vp, &d))
        return JS_FALSE;
    (cs->format & JOF_INC) ? ++d : --d;
    vp->setDouble(d);
    vp2->setDouble(d);
    return JS_TRUE;
}

const Value &
js_GetUpvar(JSContext *cx, uintN level, uintN cookie)
{
    level -= UPVAR_FRAME_SKIP(cookie);
    JS_ASSERT(level < JS_DISPLAY_SIZE);

    JSStackFrame *fp = cx->display[level];
    JS_ASSERT(fp->script);

    uintN slot = UPVAR_FRAME_SLOT(cookie);
    Value *vp;

    if (!fp->fun || (fp->flags & JSFRAME_EVAL)) {
        vp = fp->slots() + fp->script->nfixed;
    } else if (slot < fp->fun->nargs) {
        vp = fp->argv;
    } else if (slot == CALLEE_UPVAR_SLOT) {
        vp = &fp->argv[-2];
        slot = 0;
    } else {
        slot -= fp->fun->nargs;
        JS_ASSERT(slot < fp->script->nslots);
        vp = fp->slots();
    }

    return vp[slot];
}

#ifdef DEBUG

JS_STATIC_INTERPRET JS_REQUIRES_STACK void
js_TraceOpcode(JSContext *cx)
{
    FILE *tracefp;
    JSStackFrame *fp;
    JSFrameRegs *regs;
    intN ndefs, n, nuses;
    JSString *str;
    JSOp op;

    tracefp = (FILE *) cx->tracefp;
    JS_ASSERT(tracefp);
    fp = cx->fp;
    regs = cx->regs;

    /*
     * Operations in prologues don't produce interesting values, and
     * js_DecompileValueGenerator isn't set up to handle them anyway.
     */
    if (cx->tracePrevPc && regs->pc >= fp->script->main) {
        JSOp tracePrevOp = JSOp(*cx->tracePrevPc);
        ndefs = js_GetStackDefs(cx, &js_CodeSpec[tracePrevOp], tracePrevOp,
                                fp->script, cx->tracePrevPc);

        /*
         * If there aren't that many elements on the stack, then we have
         * probably entered a new frame, and printing output would just be
         * misleading.
         */
        if (ndefs != 0 &&
            ndefs < regs->sp - fp->slots()) {
            for (n = -ndefs; n < 0; n++) {
                char *bytes = js_DecompileValueGenerator(cx, n,
                                                         Jsvalify(&regs->sp[n]),
                                                         NULL);
                if (bytes) {
                    fprintf(tracefp, "%s %s",
                            (n == -ndefs) ? "  output:" : ",",
                            bytes);
                    cx->free(bytes);
                }
            }
            fprintf(tracefp, " @ %u\n", (uintN) (regs->sp - fp->base()));
        }
        fprintf(tracefp, "  stack: ");
        for (Value *siter = fp->base(); siter < regs->sp; siter++) {
            str = js_ValueToString(cx, *siter);
            if (!str)
                fputs("<null>", tracefp);
            else
                js_FileEscapedString(tracefp, str, 0);
            fputc(' ', tracefp);
        }
        fputc('\n', tracefp);
    }

    fprintf(tracefp, "%4u: ",
            js_PCToLineNumber(cx, fp->script, fp->imacpc ? fp->imacpc : regs->pc));
    js_Disassemble1(cx, fp->script, regs->pc,
                    regs->pc - fp->script->code,
                    JS_FALSE, tracefp);
    op = (JSOp) *regs->pc;
    nuses = js_GetStackUses(&js_CodeSpec[op], op, regs->pc);
    if (nuses != 0) {
        for (n = -nuses; n < 0; n++) {
            char *bytes = js_DecompileValueGenerator(cx, n,
                                                     Jsvalify(&regs->sp[n]),
                                                     NULL);
            if (bytes) {
                fprintf(tracefp, "%s %s",
                        (n == -nuses) ? "  inputs:" : ",",
                        bytes);
                cx->free(bytes);
            }
        }
        fprintf(tracefp, " @ %u\n", (uintN) (regs->sp - fp->base()));
    }
    cx->tracePrevPc = regs->pc;

    /* It's nice to have complete traces when debugging a crash.  */
    fflush(tracefp);
}

#endif /* DEBUG */

#ifdef JS_OPMETER

# include <stdlib.h>

# define HIST_NSLOTS            8

/*
 * The second dimension is hardcoded at 256 because we know that many bits fit
 * in a byte, and mainly to optimize away multiplying by JSOP_LIMIT to address
 * any particular row.
 */
static uint32 succeeds[JSOP_LIMIT][256];
static uint32 slot_ops[JSOP_LIMIT][HIST_NSLOTS];

JS_STATIC_INTERPRET void
js_MeterOpcodePair(JSOp op1, JSOp op2)
{
    if (op1 != JSOP_STOP)
        ++succeeds[op1][op2];
}

JS_STATIC_INTERPRET void
js_MeterSlotOpcode(JSOp op, uint32 slot)
{
    if (slot < HIST_NSLOTS)
        ++slot_ops[op][slot];
}

typedef struct Edge {
    const char  *from;
    const char  *to;
    uint32      count;
} Edge;

static int
compare_edges(const void *a, const void *b)
{
    const Edge *ea = (const Edge *) a;
    const Edge *eb = (const Edge *) b;

    return (int32)eb->count - (int32)ea->count;
}

void
js_DumpOpMeters()
{
    const char *name, *from, *style;
    FILE *fp;
    uint32 total, count;
    uint32 i, j, nedges;
    Edge *graph;

    name = getenv("JS_OPMETER_FILE");
    if (!name)
        name = "/tmp/ops.dot";
    fp = fopen(name, "w");
    if (!fp) {
        perror(name);
        return;
    }

    total = nedges = 0;
    for (i = 0; i < JSOP_LIMIT; i++) {
        for (j = 0; j < JSOP_LIMIT; j++) {
            count = succeeds[i][j];
            if (count != 0) {
                total += count;
                ++nedges;
            }
        }
    }

# define SIGNIFICANT(count,total) (200. * (count) >= (total))

    graph = (Edge *) js_calloc(nedges * sizeof graph[0]);
    for (i = nedges = 0; i < JSOP_LIMIT; i++) {
        from = js_CodeName[i];
        for (j = 0; j < JSOP_LIMIT; j++) {
            count = succeeds[i][j];
            if (count != 0 && SIGNIFICANT(count, total)) {
                graph[nedges].from = from;
                graph[nedges].to = js_CodeName[j];
                graph[nedges].count = count;
                ++nedges;
            }
        }
    }
    qsort(graph, nedges, sizeof(Edge), compare_edges);

# undef SIGNIFICANT

    fputs("digraph {\n", fp);
    for (i = 0, style = NULL; i < nedges; i++) {
        JS_ASSERT(i == 0 || graph[i-1].count >= graph[i].count);
        if (!style || graph[i-1].count != graph[i].count) {
            style = (i > nedges * .75) ? "dotted" :
                    (i > nedges * .50) ? "dashed" :
                    (i > nedges * .25) ? "solid" : "bold";
        }
        fprintf(fp, "  %s -> %s [label=\"%lu\" style=%s]\n",
                graph[i].from, graph[i].to,
                (unsigned long)graph[i].count, style);
    }
    js_free(graph);
    fputs("}\n", fp);
    fclose(fp);

    name = getenv("JS_OPMETER_HIST");
    if (!name)
        name = "/tmp/ops.hist";
    fp = fopen(name, "w");
    if (!fp) {
        perror(name);
        return;
    }
    fputs("bytecode", fp);
    for (j = 0; j < HIST_NSLOTS; j++)
        fprintf(fp, "  slot %1u", (unsigned)j);
    putc('\n', fp);
    fputs("========", fp);
    for (j = 0; j < HIST_NSLOTS; j++)
        fputs(" =======", fp);
    putc('\n', fp);
    for (i = 0; i < JSOP_LIMIT; i++) {
        for (j = 0; j < HIST_NSLOTS; j++) {
            if (slot_ops[i][j] != 0) {
                /* Reuse j in the next loop, since we break after. */
                fprintf(fp, "%-8.8s", js_CodeName[i]);
                for (j = 0; j < HIST_NSLOTS; j++)
                    fprintf(fp, " %7lu", (unsigned long)slot_ops[i][j]);
                putc('\n', fp);
                break;
            }
        }
    }
    fclose(fp);
}

#endif /* JS_OPSMETER */

#endif /* !JS_LONE_INTERPRET ^ defined jsinvoke_cpp___ */

#ifndef  jsinvoke_cpp___

#ifdef JS_REPRMETER
// jsval representation metering: this measures the kinds of jsvals that
// are used as inputs to each JSOp.
namespace reprmeter {
    enum Repr {
        NONE,
        INT,
        DOUBLE,
        BOOLEAN_PROPER,
        BOOLEAN_OTHER,
        STRING,
        OBJECT_NULL,
        OBJECT_PLAIN,
        FUNCTION_INTERPRETED,
        FUNCTION_FASTNATIVE,
        FUNCTION_SLOWNATIVE,
        ARRAY_SLOW,
        ARRAY_DENSE
    };

    // Return the |repr| value giving the representation of the given jsval.
    static Repr
    GetRepr(jsval v)
    {
        if (JSVAL_IS_INT(v))
            return INT;
        if (JSVAL_IS_DOUBLE(v))
            return DOUBLE;
        if (JSVAL_IS_SPECIAL(v)) {
            return (v == JSVAL_TRUE || v == JSVAL_FALSE)
                   ? BOOLEAN_PROPER
                   : BOOLEAN_OTHER;
        }
        if (JSVAL_IS_STRING(v))
            return STRING;

        JS_ASSERT(JSVAL_IS_OBJECT(v));

        JSObject *obj = JSVAL_TO_OBJECT(v);
        if (VALUE_IS_FUNCTION(cx, v)) {
            JSFunction *fun = GET_FUNCTION_PRIVATE(cx, obj);
            if (FUN_INTERPRETED(fun))
                return FUNCTION_INTERPRETED;
            if (fun->flags & JSFUN_FAST_NATIVE)
                return FUNCTION_FASTNATIVE;
            return FUNCTION_SLOWNATIVE;
        }
        // This must come before the general array test, because that
        // one subsumes this one.
        if (!obj)
            return OBJECT_NULL;
        if (obj->isDenseArray())
            return ARRAY_DENSE;
        if (obj->isArray())
            return ARRAY_SLOW;
        return OBJECT_PLAIN;
    }

    static const char *reprName[] = { "invalid", "int", "double", "bool", "special",
                                      "string", "null", "object", 
                                      "fun:interp", "fun:fast", "fun:slow",
                                      "array:slow", "array:dense" };

    // Logically, a tuple of (JSOp, repr_1, ..., repr_n) where repr_i is
    // the |repr| of the ith input to the JSOp.
    struct OpInput {
        enum { max_uses = 16 };

        JSOp op;
        Repr uses[max_uses];

        OpInput() : op(JSOp(255)) {
            for (int i = 0; i < max_uses; ++i)
                uses[i] = NONE;
        }

        OpInput(JSOp op) : op(op) {
            for (int i = 0; i < max_uses; ++i)
                uses[i] = NONE;
        }

        // Hash function
        operator uint32() const {
            uint32 h = op;
            for (int i = 0; i < max_uses; ++i)
                h = h * 7 + uses[i] * 13;
            return h;
        }

        bool operator==(const OpInput &opinput) const {
            if (op != opinput.op)
                return false;
            for (int i = 0; i < max_uses; ++i) {
                if (uses[i] != opinput.uses[i])
                    return false;
            }
            return true;
        }

        OpInput &operator=(const OpInput &opinput) {
            op = opinput.op;
            for (int i = 0; i < max_uses; ++i)
                uses[i] = opinput.uses[i];
            return *this;
        }
    };

    typedef HashMap<OpInput, uint64, DefaultHasher<OpInput>, SystemAllocPolicy> OpInputHistogram;

    OpInputHistogram opinputs;
    bool             opinputsInitialized = false;

    // Record an OpInput for the current op. This should be called just
    // before executing the op.
    static void
    MeterRepr(JSStackFrame *fp)
    {
        // Note that we simply ignore the possibility of errors (OOMs)
        // using the hash map, since this is only metering code.

        if (!opinputsInitialized) {
            opinputs.init();
            opinputsInitialized = true;
        }

        JSOp op = JSOp(*fp->regs->pc);
        unsigned nuses = js_GetStackUses(&js_CodeSpec[op], op, fp->regs->pc);

        // Build the OpInput.
        OpInput opinput(op);
        for (unsigned i = 0; i < nuses; ++i) {
            jsval v = fp->regs->sp[-nuses+i];
            opinput.uses[i] = GetRepr(v);
        }

        OpInputHistogram::AddPtr p = opinputs.lookupForAdd(opinput);
        if (p)
            ++p->value;
        else
            opinputs.add(p, opinput, 1);
    }

    void
    js_DumpReprMeter()
    {
        FILE *f = fopen("/tmp/reprmeter.txt", "w");
        JS_ASSERT(f);
        for (OpInputHistogram::Range r = opinputs.all(); !r.empty(); r.popFront()) {
            const OpInput &o = r.front().key;
            uint64 c = r.front().value;
            fprintf(f, "%3d,%s", o.op, js_CodeName[o.op]);
            for (int i = 0; i < OpInput::max_uses && o.uses[i] != NONE; ++i)
                fprintf(f, ",%s", reprName[o.uses[i]]);
            fprintf(f, ",%llu\n", c);
        }
        fclose(f);
    }
}
#endif /* JS_REPRMETER */

#define PUSH_COPY(v)            regs.sp++->copy(v)
#define PUSH_NULL()             regs.sp++->setNull()
#define PUSH_UNDEFINED()        regs.sp++->setUndefined()
#define PUSH_BOOLEAN(b)         regs.sp++->setBoolean(b)
#define PUSH_INT32(i)           regs.sp++->setInt32(i)
#define PUSH_STRING(s)          regs.sp++->setString(s)
#define PUSH_NONFUNOBJ(o)       regs.sp++->setNonFunObj(o)
#define PUSH_FUNOBJ(o)          regs.sp++->setFunObj(o)
#define PUSH_OBJECT(o)          SetObject(regs.sp++, o)
#define PUSH_OBJECT_OR_NULL(o)  SetObject(regs.sp++, o)
#define PUSH_HOLE()             regs.sp++->setMagic(JS_ARRAY_HOLE)
#define POP_COPY_TO(v)          v.copy(*--regs.sp)

/*
 * Push the jsdouble d using sp from the lexical environment. Try to convert d
 * to a jsint that fits in a jsval, otherwise GC-alloc space for it and push a
 * reference.
 */
#define STORE_NUMBER(cx, n, d)                                                \
    JS_BEGIN_MACRO                                                            \
        int32_t i_;                                                           \
        if (JSDOUBLE_IS_INT32(d, i_))                                         \
            regs.sp[n].setInt32(i_);                                          \
        else                                                                  \
            regs.sp[n].setDouble(d);                                          \
    JS_END_MACRO

#define POP_BOOLEAN(cx, vp, b)                                                \
    JS_BEGIN_MACRO                                                            \
        vp = &regs.sp[-1];                                                    \
        if (vp->isNull()) {                                                   \
            b = false;                                                        \
        } else if (vp->isBoolean()) {                                         \
            b = vp->asBoolean();                                              \
        } else {                                                              \
            b = js_ValueToBoolean(*vp);                                       \
        }                                                                     \
        regs.sp--;                                                            \
    JS_END_MACRO

#define VALUE_TO_OBJECT(cx, vp, obj)                                          \
    JS_BEGIN_MACRO                                                            \
        if ((vp)->isObject()) {                                               \
            obj = &(vp)->asObject();                                          \
        } else {                                                              \
            if (!js_ValueToNonNullObject(cx, *(vp), (vp)))                    \
                goto error;                                                   \
            obj = &(vp)->asObject();                                          \
        }                                                                     \
    JS_END_MACRO

#define FETCH_OBJECT(cx, n, obj)                                              \
    JS_BEGIN_MACRO                                                            \
        Value *vp_ = &regs.sp[n];                                             \
        VALUE_TO_OBJECT(cx, vp_, obj);                                        \
    JS_END_MACRO

#define LOAD_ATOM(PCOFF, atom)                                                \
    JS_BEGIN_MACRO                                                            \
        JS_ASSERT(fp->imacpc                                                  \
                  ? atoms == COMMON_ATOMS_START(&rt->atomState) &&            \
                    GET_INDEX(regs.pc + PCOFF) < js_common_atom_count         \
                  : (size_t)(atoms - script->atomMap.vector) <                \
                    (size_t)(script->atomMap.length -                         \
                             GET_INDEX(regs.pc + PCOFF)));                    \
        atom = atoms[GET_INDEX(regs.pc + PCOFF)];                             \
    JS_END_MACRO

#define GET_FULL_INDEX(PCOFF)                                                 \
    (atoms - script->atomMap.vector + GET_INDEX(regs.pc + PCOFF))

#define LOAD_OBJECT(PCOFF, obj)                                               \
    JS_BEGIN_MACRO                                                            \
        obj = script->getObject(GET_FULL_INDEX(PCOFF));                       \
    JS_END_MACRO

#define LOAD_FUNCTION(PCOFF)                                                  \
    (fun = script->getFunction(GET_FULL_INDEX(PCOFF)))

#ifdef JS_TRACER

#ifdef MOZ_TRACEVIS
#if JS_THREADED_INTERP
#define MONITOR_BRANCH_TRACEVIS                                               \
    JS_BEGIN_MACRO                                                            \
        if (jumpTable != interruptJumpTable)                                  \
            EnterTraceVisState(cx, S_RECORD, R_NONE);                         \
    JS_END_MACRO
#else /* !JS_THREADED_INTERP */
#define MONITOR_BRANCH_TRACEVIS                                               \
    JS_BEGIN_MACRO                                                            \
        EnterTraceVisState(cx, S_RECORD, R_NONE);                             \
    JS_END_MACRO
#endif
#else
#define MONITOR_BRANCH_TRACEVIS
#endif

#define RESTORE_INTERP_VARS()                                                 \
    JS_BEGIN_MACRO                                                            \
        fp = cx->fp;                                                          \
        script = fp->script;                                                  \
        atoms = FrameAtomBase(cx, fp);                                        \
        currentVersion = (JSVersion) script->version;                         \
        JS_ASSERT(cx->regs == &regs);                                         \
        if (cx->throwing)                                                     \
            goto error;                                                       \
    JS_END_MACRO

#define MONITOR_BRANCH(reason)                                                \
    JS_BEGIN_MACRO                                                            \
        if (TRACING_ENABLED(cx)) {                                            \
            if (MonitorLoopEdge(cx, inlineCallCount, reason)) {               \
                JS_ASSERT(TRACE_RECORDER(cx));                                \
                MONITOR_BRANCH_TRACEVIS;                                      \
                ENABLE_INTERRUPTS();                                          \
            }                                                                 \
            RESTORE_INTERP_VARS();                                            \
        }                                                                     \
    JS_END_MACRO

#else /* !JS_TRACER */

#define MONITOR_BRANCH(reason) ((void) 0)

#endif /* !JS_TRACER */

#define CHECK_INTERRUPT_HANDLER()                                             \
    JS_BEGIN_MACRO                                                            \
        if (cx->debugHooks->interruptHook)                                    \
            ENABLE_INTERRUPTS();                                              \
    JS_END_MACRO

    /*
     * Prepare to call a user-supplied branch handler, and abort the script
     * if it returns false.
     */
#define CHECK_BRANCH()                                                        \
    JS_BEGIN_MACRO                                                            \
        if (!JS_CHECK_OPERATION_LIMIT(cx))                                    \
            goto error;                                                       \
    JS_END_MACRO

#ifndef TRACE_RECORDER
#define TRACE_RECORDER(cx) (false)
#endif

#define BRANCH(n)                                                             \
    JS_BEGIN_MACRO                                                            \
        regs.pc += (n);                                                       \
        op = (JSOp) *regs.pc;                                                 \
        if ((n) <= 0) {                                                       \
            CHECK_BRANCH();                                                   \
            if (op == JSOP_NOP) {                                             \
                if (TRACE_RECORDER(cx)) {                                     \
                    MONITOR_BRANCH(Record_Branch);                            \
                    op = (JSOp) *regs.pc;                                     \
                } else {                                                      \
                    op = (JSOp) *++regs.pc;                                   \
                }                                                             \
            } else if (op == JSOP_TRACE) {                                    \
                MONITOR_BRANCH(Record_Branch);                                \
                op = (JSOp) *regs.pc;                                         \
            }                                                                 \
        }                                                                     \
        DO_OP();                                                              \
    JS_END_MACRO

/* Test whether v is an int in the range [-2^31 + 1, 2^31 - 2] */
static JS_ALWAYS_INLINE bool
CanIncDecWithoutOverflow(const Value &v)
{
    return v.isInt32() &&
           ((uint32_t(v.asInt32()) + 1) - uint32_t(JSVAL_INT_MIN)) > 1;
}

/*
 * Conditional assert to detect failure to clear a pending exception that is
 * suppressed (or unintentional suppression of a wanted exception).
 */
#if defined DEBUG_brendan || defined DEBUG_mrbkap || defined DEBUG_shaver
# define DEBUG_NOT_THROWING 1
#endif

#ifdef DEBUG_NOT_THROWING
# define ASSERT_NOT_THROWING(cx) JS_ASSERT(!(cx)->throwing)
#else
# define ASSERT_NOT_THROWING(cx) /* nothing */
#endif

/*
 * Define JS_OPMETER to instrument bytecode succession, generating a .dot file
 * on shutdown that shows the graph of significant predecessor/successor pairs
 * executed, where the edge labels give the succession counts.  The .dot file
 * is named by the JS_OPMETER_FILE envariable, and defaults to /tmp/ops.dot.
 *
 * Bonus feature: JS_OPMETER also enables counters for stack-addressing ops
 * such as JSOP_GETLOCAL, JSOP_INCARG, via METER_SLOT_OP. The resulting counts
 * are written to JS_OPMETER_HIST, defaulting to /tmp/ops.hist.
 */
#ifndef JS_OPMETER
# define METER_OP_INIT(op)      /* nothing */
# define METER_OP_PAIR(op1,op2) /* nothing */
# define METER_SLOT_OP(op,slot) /* nothing */
#else

/*
 * The second dimension is hardcoded at 256 because we know that many bits fit
 * in a byte, and mainly to optimize away multiplying by JSOP_LIMIT to address
 * any particular row.
 */
# define METER_OP_INIT(op)      ((op) = JSOP_STOP)
# define METER_OP_PAIR(op1,op2) (js_MeterOpcodePair(op1, op2))
# define METER_SLOT_OP(op,slot) (js_MeterSlotOpcode(op, slot))

#endif

#ifdef JS_REPRMETER
# define METER_REPR(fp)         (reprmeter::MeterRepr(fp))
#else
# define METER_REPR(fp)         ((void) 0)
#endif /* JS_REPRMETER */

/*
 * Threaded interpretation via computed goto appears to be well-supported by
 * GCC 3 and higher.  IBM's C compiler when run with the right options (e.g.,
 * -qlanglvl=extended) also supports threading.  Ditto the SunPro C compiler.
 * Currently it's broken for JS_VERSION < 160, though this isn't worth fixing.
 * Add your compiler support macros here.
 */
#ifndef JS_THREADED_INTERP
# if JS_VERSION >= 160 && (                                                   \
    __GNUC__ >= 3 ||                                                          \
    (__IBMC__ >= 700 && defined __IBM_COMPUTED_GOTO) ||                       \
    __SUNPRO_C >= 0x570)
#  define JS_THREADED_INTERP 1
# else
#  define JS_THREADED_INTERP 0
# endif
#endif

/*
 * Deadlocks or else bad races are likely if JS_THREADSAFE, so we must rely on
 * single-thread DEBUG js shell testing to verify property cache hits.
 */
#if defined DEBUG && !defined JS_THREADSAFE

# define ASSERT_VALID_PROPERTY_CACHE_HIT(pcoff,obj,pobj,entry)                \
    JS_BEGIN_MACRO                                                            \
        if (!AssertValidPropertyCacheHit(cx, script, regs, pcoff, obj, pobj,  \
                                         entry)) {                            \
            goto error;                                                       \
        }                                                                     \
    JS_END_MACRO

static bool
AssertValidPropertyCacheHit(JSContext *cx, JSScript *script, JSFrameRegs& regs,
                            ptrdiff_t pcoff, JSObject *start, JSObject *found,
                            PropertyCacheEntry *entry)
{
    uint32 sample = cx->runtime->gcNumber;

    JSAtom *atom;
    if (pcoff >= 0)
        GET_ATOM_FROM_BYTECODE(script, regs.pc, pcoff, atom);
    else
        atom = cx->runtime->atomState.lengthAtom;

    JSObject *obj, *pobj;
    JSProperty *prop;
    JSBool ok;

    if (JOF_OPMODE(*regs.pc) == JOF_NAME) {
        ok = js_FindProperty(cx, ATOM_TO_JSID(atom), &obj, &pobj, &prop);
    } else {
        obj = start;
        ok = js_LookupProperty(cx, obj, ATOM_TO_JSID(atom), &pobj, &prop);
    }
    if (!ok)
        return false;
    if (cx->runtime->gcNumber != sample || entry->vshape() != pobj->shape()) {
        pobj->dropProperty(cx, prop);
        return true;
    }
    JS_ASSERT(prop);
    JS_ASSERT(pobj == found);

    JSScopeProperty *sprop = (JSScopeProperty *) prop;
    if (entry->vword.isSlot()) {
        JS_ASSERT(entry->vword.toSlot() == sprop->slot);
        JS_ASSERT(!sprop->isMethod());
    } else if (entry->vword.isSprop()) {
        JS_ASSERT(entry->vword.toSprop() == sprop);
        JS_ASSERT_IF(sprop->isMethod(),
                     sprop->methodValue().isSame(pobj->lockedGetSlot(sprop->slot)));
    } else {
        Value v;
        JS_ASSERT(entry->vword.isFunObj());
        JS_ASSERT(!entry->vword.isNull());
        JS_ASSERT(pobj->scope()->brandedOrHasMethodBarrier());
        JS_ASSERT(sprop->hasDefaultGetterOrIsMethod());
        JS_ASSERT(SPROP_HAS_VALID_SLOT(sprop, pobj->scope()));
        v.copy(pobj->lockedGetSlot(sprop->slot));
        JS_ASSERT(&entry->vword.toFunObj() == &v.asFunObj());

        if (sprop->isMethod()) {
            JS_ASSERT(js_CodeSpec[*regs.pc].format & JOF_CALLOP);
            JS_ASSERT(sprop->methodValue().isSame(v));
        }
    }

    pobj->dropProperty(cx, prop);
    return true;
}

#else
# define ASSERT_VALID_PROPERTY_CACHE_HIT(pcoff,obj,pobj,entry) ((void) 0)
#endif

/*
 * Ensure that the intrepreter switch can close call-bytecode cases in the
 * same way as non-call bytecodes.
 */
JS_STATIC_ASSERT(JSOP_NAME_LENGTH == JSOP_CALLNAME_LENGTH);
JS_STATIC_ASSERT(JSOP_GETGVAR_LENGTH == JSOP_CALLGVAR_LENGTH);
JS_STATIC_ASSERT(JSOP_GETUPVAR_LENGTH == JSOP_CALLUPVAR_LENGTH);
JS_STATIC_ASSERT(JSOP_GETUPVAR_DBG_LENGTH == JSOP_CALLUPVAR_DBG_LENGTH);
JS_STATIC_ASSERT(JSOP_GETUPVAR_DBG_LENGTH == JSOP_GETUPVAR_LENGTH);
JS_STATIC_ASSERT(JSOP_GETDSLOT_LENGTH == JSOP_CALLDSLOT_LENGTH);
JS_STATIC_ASSERT(JSOP_GETARG_LENGTH == JSOP_CALLARG_LENGTH);
JS_STATIC_ASSERT(JSOP_GETLOCAL_LENGTH == JSOP_CALLLOCAL_LENGTH);
JS_STATIC_ASSERT(JSOP_XMLNAME_LENGTH == JSOP_CALLXMLNAME_LENGTH);

/*
 * Same for debuggable flat closures defined at top level in another function
 * or program fragment.
 */
JS_STATIC_ASSERT(JSOP_DEFFUN_FC_LENGTH == JSOP_DEFFUN_DBGFC_LENGTH);

/*
 * Same for JSOP_SETNAME and JSOP_SETPROP, which differ only slightly but
 * remain distinct for the decompiler. Likewise for JSOP_INIT{PROP,METHOD}.
 */
JS_STATIC_ASSERT(JSOP_SETNAME_LENGTH == JSOP_SETPROP_LENGTH);
JS_STATIC_ASSERT(JSOP_SETNAME_LENGTH == JSOP_SETMETHOD_LENGTH);
JS_STATIC_ASSERT(JSOP_INITPROP_LENGTH == JSOP_INITMETHOD_LENGTH);

/* See TRY_BRANCH_AFTER_COND. */
JS_STATIC_ASSERT(JSOP_IFNE_LENGTH == JSOP_IFEQ_LENGTH);
JS_STATIC_ASSERT(JSOP_IFNE == JSOP_IFEQ + 1);

/* For the fastest case inder JSOP_INCNAME, etc. */
JS_STATIC_ASSERT(JSOP_INCNAME_LENGTH == JSOP_DECNAME_LENGTH);
JS_STATIC_ASSERT(JSOP_INCNAME_LENGTH == JSOP_NAMEINC_LENGTH);
JS_STATIC_ASSERT(JSOP_INCNAME_LENGTH == JSOP_NAMEDEC_LENGTH);

#ifdef JS_TRACER
# define ABORT_RECORDING(cx, reason)                                          \
    JS_BEGIN_MACRO                                                            \
        if (TRACE_RECORDER(cx))                                               \
            AbortRecording(cx, reason);                                       \
    JS_END_MACRO
#else
# define ABORT_RECORDING(cx, reason)    ((void) 0)
#endif

namespace js {

JS_REQUIRES_STACK bool
Interpret(JSContext *cx)
{
# ifdef DEBUG
    /*
     * We call this macro from BEGIN_CASE in threaded interpreters,
     * and before entering the switch in non-threaded interpreters.
     * However, reaching such points doesn't mean we've actually
     * fetched an OP from the instruction stream: some opcodes use
     * 'op=x; DO_OP()' to let another opcode's implementation finish
     * their work, and many opcodes share entry points with a run of
     * consecutive BEGIN_CASEs.
     *
     * Take care to trace OP only when it is the opcode fetched from
     * the instruction stream, so the trace matches what one would
     * expect from looking at the code.  (We do omit POPs after SETs;
     * unfortunate, but not worth fixing.)
     */
#  define TRACE_OPCODE(OP)  JS_BEGIN_MACRO                                    \
                                if (JS_UNLIKELY(cx->tracefp != NULL) &&       \
                                    (OP) == *regs.pc)                         \
                                    js_TraceOpcode(cx);                       \
                            JS_END_MACRO
# else
#  define TRACE_OPCODE(OP)  ((void) 0)
# endif

    /*
     * Macros for threaded interpreter loop
     */
#if JS_THREADED_INTERP
    static void *const normalJumpTable[] = {
# define OPDEF(op,val,name,token,length,nuses,ndefs,prec,format) \
        JS_EXTENSION &&L_##op,
# include "jsopcode.tbl"
# undef OPDEF
    };

    static void *const interruptJumpTable[] = {
# define OPDEF(op,val,name,token,length,nuses,ndefs,prec,format)              \
        JS_EXTENSION &&interrupt,
# include "jsopcode.tbl"
# undef OPDEF
    };

    register void * const *jumpTable = normalJumpTable;

    METER_OP_INIT(op);      /* to nullify first METER_OP_PAIR */

# define ENABLE_INTERRUPTS() ((void) (jumpTable = interruptJumpTable))

# ifdef JS_TRACER
#  define CHECK_RECORDER()                                                    \
    JS_ASSERT_IF(TRACE_RECORDER(cx), jumpTable == interruptJumpTable)
# else
#  define CHECK_RECORDER()  ((void)0)
# endif

# define DO_OP()            JS_BEGIN_MACRO                                    \
                                CHECK_RECORDER();                             \
                                JS_EXTENSION_(goto *jumpTable[op]);           \
                            JS_END_MACRO
# define DO_NEXT_OP(n)      JS_BEGIN_MACRO                                    \
                                METER_OP_PAIR(op, JSOp(regs.pc[n]));          \
                                op = (JSOp) *(regs.pc += (n));                \
                                METER_REPR(fp);                               \
                                DO_OP();                                      \
                            JS_END_MACRO

# define BEGIN_CASE(OP)     L_##OP: TRACE_OPCODE(OP); CHECK_RECORDER();
# define END_CASE(OP)       DO_NEXT_OP(OP##_LENGTH);
# define END_VARLEN_CASE    DO_NEXT_OP(len);
# define ADD_EMPTY_CASE(OP) BEGIN_CASE(OP)                                    \
                                JS_ASSERT(js_CodeSpec[OP].length == 1);       \
                                op = (JSOp) *++regs.pc;                       \
                                DO_OP();

# define END_EMPTY_CASES

#else /* !JS_THREADED_INTERP */

    /*
     * Macros for classic interpreter loop
     */

    register intN switchMask = 0;
    intN switchOp;

# define ENABLE_INTERRUPTS() ((void) (switchMask = -1))

# ifdef JS_TRACER
#  define CHECK_RECORDER()                                                    \
    JS_ASSERT_IF(TRACE_RECORDER(cx), switchMask == -1)
# else
#  define CHECK_RECORDER()  ((void)0)
# endif

# define DO_OP()            goto do_op
# define DO_NEXT_OP(n)      JS_BEGIN_MACRO                                    \
                                JS_ASSERT((n) == len);                        \
                                goto advance_pc;                              \
                            JS_END_MACRO

# define BEGIN_CASE(OP)     case OP: CHECK_RECORDER();
# define END_CASE(OP)       END_CASE_LEN(OP##_LENGTH)
# define END_CASE_LEN(n)    END_CASE_LENX(n)
# define END_CASE_LENX(n)   END_CASE_LEN##n

/*
 * To share the code for all len == 1 cases we use the specialized label with
 * code that falls through to advance_pc: .
 */
# define END_CASE_LEN1      goto advance_pc_by_one;
# define END_CASE_LEN2      len = 2; goto advance_pc;
# define END_CASE_LEN3      len = 3; goto advance_pc;
# define END_CASE_LEN4      len = 4; goto advance_pc;
# define END_CASE_LEN5      len = 5; goto advance_pc;
# define END_VARLEN_CASE    goto advance_pc;
# define ADD_EMPTY_CASE(OP) BEGIN_CASE(OP)
# define END_EMPTY_CASES    goto advance_pc_by_one;

#endif /* !JS_THREADED_INTERP */

#ifdef MOZ_TRACEVIS
    TraceVisStateObj tvso(cx, S_INTERP);
#endif
    JSAutoResolveFlags rf(cx, JSRESOLVE_INFER);

    /* Check for too deep of a native thread stack. */
    JS_CHECK_RECURSION(cx, return JS_FALSE);

    JSRuntime *const rt = cx->runtime;

    /* Set registerized frame pointer and derived script pointer. */
    JSStackFrame *fp = cx->fp;
    JSScript *script = fp->script;
    JS_ASSERT(!script->isEmpty());
    JS_ASSERT(script->length > 1);

    /* Count of JS function calls that nest in this C Interpret frame. */
    uintN inlineCallCount = 0;

    /*
     * Initialize the index segment register used by LOAD_ATOM and
     * GET_FULL_INDEX macros below. As a register we use a pointer based on
     * the atom map to turn frequently executed LOAD_ATOM into simple array
     * access. For less frequent object and regexp loads we have to recover
     * the segment from atoms pointer first.
     */
    JSAtom **atoms = script->atomMap.vector;

    MUST_FLOW_THROUGH("exit");
    ++cx->interpLevel;

    /*
     * Optimized Get and SetVersion for proper script language versioning.
     *
     * If any native method or Class/JSObjectOps hook calls js_SetVersion
     * and changes cx->version, the effect will "stick" and we will stop
     * maintaining currentVersion.  This is relied upon by testsuites, for
     * the most part -- web browsers select version before compiling and not
     * at run-time.
     */
    JSVersion currentVersion = (JSVersion) script->version;
    JSVersion originalVersion = (JSVersion) cx->version;
    if (currentVersion != originalVersion)
        js_SetVersion(cx, currentVersion);

    /* Update the static-link display. */
    if (script->staticLevel < JS_DISPLAY_SIZE) {
        JSStackFrame **disp = &cx->display[script->staticLevel];
        fp->displaySave = *disp;
        *disp = fp;
    }

    /*
     * Load the debugger's interrupt hook here and after calling out to native
     * functions (but not to getters, setters, or other native hooks), so we do
     * not have to reload it each time through the interpreter loop -- we hope
     * the compiler can keep it in a register when it is non-null.
     */
    CHECK_INTERRUPT_HANDLER();

    /*
     * Access to |cx->regs| is very common, so we copy in and repoint to a
     * local variable, and copy out on exit.
     */
    JS_ASSERT(cx->regs);
    JSFrameRegs *const prevContextRegs = cx->regs;
    JSFrameRegs regs = *cx->regs;
    cx->setCurrentRegs(&regs);

#if JS_HAS_GENERATORS
    if (JS_UNLIKELY(fp->isGenerator())) {
        JS_ASSERT(prevContextRegs == &cx->generatorFor(fp)->savedRegs);
        JS_ASSERT((size_t) (regs.pc - script->code) <= script->length);
        JS_ASSERT((size_t) (regs.sp - fp->base()) <= StackDepth(script));

        /*
         * To support generator_throw and to catch ignored exceptions,
         * fail if cx->throwing is set.
         */
        if (cx->throwing) {
#ifdef DEBUG_NOT_THROWING
            if (cx->exception != JSVAL_ARETURN) {
                printf("JS INTERPRETER CALLED WITH PENDING EXCEPTION %lx\n",
                       (unsigned long) cx->exception);
            }
#endif
            goto error;
        }
    }
#endif

#ifdef JS_TRACER
    /* We cannot reenter the interpreter while recording. */
    if (TRACE_RECORDER(cx))
        AbortRecording(cx, "attempt to reenter interpreter while recording");
#endif

    /* State communicated between non-local jumps: */
    JSBool interpReturnOK;
    JSAtom *atomNotDefined;

    /*
     * It is important that "op" be initialized before calling DO_OP because
     * it is possible for "op" to be specially assigned during the normal
     * processing of an opcode while looping. We rely on DO_NEXT_OP to manage
     * "op" correctly in all other cases.
     */
    JSOp op;
#ifdef JS_THREADED_INTERP
    {
        jsint len = 0;
        DO_NEXT_OP(len);
    }
#else
    /* 'len' needs to be live for the non-threaded interpreter loop. */
    jsint len = 0;
    DO_NEXT_OP(len);
#endif

#if JS_THREADED_INTERP
    /*
     * This is a loop, but it does not look like a loop. The loop-closing
     * jump is distributed throughout goto *jumpTable[op] inside of DO_OP.
     * When interrupts are enabled, jumpTable is set to interruptJumpTable
     * where all jumps point to the interrupt label. The latter, after
     * calling the interrupt handler, dispatches through normalJumpTable to
     * continue the normal bytecode processing.
     */

#else /* !JS_THREADED_INTERP */
    for (;;) {
      advance_pc_by_one:
        JS_ASSERT(js_CodeSpec[op].length == 1);
        len = 1;
      advance_pc:
        regs.pc += len;
        op = (JSOp) *regs.pc;

      do_op:
        CHECK_RECORDER();
        TRACE_OPCODE(op);
        switchOp = intN(op) | switchMask;
      do_switch:
        switch (switchOp) {
#endif

/********************** Here we include the operations ***********************/
#include "jsops.cpp"
/*****************************************************************************/

#if !JS_THREADED_INTERP
          default:
#endif
#ifndef JS_TRACER
        bad_opcode:
#endif
          {
            char numBuf[12];
            JS_snprintf(numBuf, sizeof numBuf, "%d", op);
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                 JSMSG_BAD_BYTECODE, numBuf);
            goto error;
          }

#if !JS_THREADED_INTERP
        } /* switch (op) */
    } /* for (;;) */
#endif /* !JS_THREADED_INTERP */

  error:
    JS_ASSERT(cx->regs == &regs);
#ifdef JS_TRACER
    if (fp->imacpc && cx->throwing) {
        // To keep things simple, we hard-code imacro exception handlers here.
        if (*fp->imacpc == JSOP_NEXTITER &&
            InCustomIterNextTryRegion(regs.pc) &&
            js_ValueIsStopIteration(cx->exception)) {
            // If the other NEXTITER imacro, native_iter_next, throws
            // StopIteration, do not catch it here. See bug 547911.

            // pc may point to JSOP_DUP here due to bug 474854.
            JS_ASSERT(*regs.pc == JSOP_CALL || *regs.pc == JSOP_DUP);
            cx->throwing = JS_FALSE;
            cx->exception = JSVAL_VOID;
            regs.sp[-1] = JSVAL_HOLE;
            PUSH(JSVAL_FALSE);
            goto end_imacro;
        }

        // Handle other exceptions as if they came from the imacro-calling pc.
        regs.pc = fp->imacpc;
        fp->imacpc = NULL;
        atoms = script->atomMap.vector;
    }
#endif

    JS_ASSERT((size_t)((fp->imacpc ? fp->imacpc : regs.pc) - script->code) < script->length);

#ifdef JS_TRACER
    /*
     * This abort could be weakened to permit tracing through exceptions that
     * are thrown and caught within a loop, with the co-operation of the tracer.
     * For now just bail on any sign of trouble.
     */
    if (TRACE_RECORDER(cx))
        AbortRecording(cx, "error or exception while recording");
#endif

    if (!cx->throwing) {
        /* This is an error, not a catchable exception, quit the frame ASAP. */
        interpReturnOK = JS_FALSE;
    } else {
        JSThrowHook handler;
        JSTryNote *tn, *tnlimit;
        uint32 offset;

        /* Call debugger throw hook if set. */
        handler = cx->debugHooks->throwHook;
        if (handler) {
            Value rval;
            switch (handler(cx, script, regs.pc, Jsvalify(&rval),
                            cx->debugHooks->throwHookData)) {
              case JSTRAP_ERROR:
                cx->throwing = JS_FALSE;
                goto error;
              case JSTRAP_RETURN:
                cx->throwing = JS_FALSE;
                fp->rval.copy(rval);
                interpReturnOK = JS_TRUE;
                goto forced_return;
              case JSTRAP_THROW:
                cx->exception.copy(rval);
              case JSTRAP_CONTINUE:
              default:;
            }
            CHECK_INTERRUPT_HANDLER();
        }

        /*
         * Look for a try block in script that can catch this exception.
         */
        if (script->trynotesOffset == 0)
            goto no_catch;

        offset = (uint32)(regs.pc - script->main);
        tn = script->trynotes()->vector;
        tnlimit = tn + script->trynotes()->length;
        do {
            if (offset - tn->start >= tn->length)
                continue;

            /*
             * We have a note that covers the exception pc but we must check
             * whether the interpreter has already executed the corresponding
             * handler. This is possible when the executed bytecode
             * implements break or return from inside a for-in loop.
             *
             * In this case the emitter generates additional [enditer] and
             * [gosub] opcodes to close all outstanding iterators and execute
             * the finally blocks. If such an [enditer] throws an exception,
             * its pc can still be inside several nested for-in loops and
             * try-finally statements even if we have already closed the
             * corresponding iterators and invoked the finally blocks.
             *
             * To address this, we make [enditer] always decrease the stack
             * even when its implementation throws an exception. Thus already
             * executed [enditer] and [gosub] opcodes will have try notes
             * with the stack depth exceeding the current one and this
             * condition is what we use to filter them out.
             */
            if (tn->stackDepth > regs.sp - fp->base())
                continue;

            /*
             * Set pc to the first bytecode after the the try note to point
             * to the beginning of catch or finally or to [enditer] closing
             * the for-in loop.
             */
            regs.pc = (script)->main + tn->start + tn->length;

            JSBool ok = js_UnwindScope(cx, tn->stackDepth, JS_TRUE);
            JS_ASSERT(regs.sp == fp->base() + tn->stackDepth);
            if (!ok) {
                /*
                 * Restart the handler search with updated pc and stack depth
                 * to properly notify the debugger.
                 */
                goto error;
            }

            switch (tn->kind) {
              case JSTRY_CATCH:
                JS_ASSERT(js_GetOpcode(cx, fp->script, regs.pc) == JSOP_ENTERBLOCK);

#if JS_HAS_GENERATORS
                /* Catch cannot intercept the closing of a generator. */
                if (JS_UNLIKELY(cx->exception.isMagic(JS_STOP_ITERATION_EXCEPTION)))
                    break;
#endif

                /*
                 * Don't clear cx->throwing to save cx->exception from GC
                 * until it is pushed to the stack via [exception] in the
                 * catch block.
                 */
                {
                    jsint len = 0;
                    DO_NEXT_OP(len);
                }

              case JSTRY_FINALLY:
                /*
                 * Push (true, exception) pair for finally to indicate that
                 * [retsub] should rethrow the exception.
                 */
                PUSH_BOOLEAN(true);
                PUSH_COPY(cx->exception);
                cx->throwing = JS_FALSE;
                {
                    jsint len = 0;
                    DO_NEXT_OP(len);
                }

              case JSTRY_ITER:
                /*
                 * This is similar to JSOP_ENDITER in the interpreter loop,
                 * except the code now uses the stack slot normally used by
                 * JSOP_NEXTITER, namely regs.sp[-1] before the regs.sp -= 2
                 * adjustment and regs.sp[1] after, to save and restore the
                 * pending exception.
                 */
                JS_ASSERT(js_GetOpcode(cx, fp->script, regs.pc) == JSOP_ENDITER);
                regs.sp[-1].copy(cx->exception);
                cx->throwing = JS_FALSE;
                JSBool ok = js_CloseIterator(cx, regs.sp[-2]);
                regs.sp -= 2;
                if (!ok)
                    goto error;
                cx->throwing = JS_TRUE;
                cx->exception.copy(regs.sp[1]);
            }
        } while (++tn != tnlimit);

      no_catch:
        /*
         * Propagate the exception or error to the caller unless the exception
         * is an asynchronous return from a generator.
         */
        interpReturnOK = JS_FALSE;
#if JS_HAS_GENERATORS
        if (JS_UNLIKELY(cx->throwing &&
                        cx->exception.isMagic(JS_STOP_ITERATION_EXCEPTION))) {
            cx->throwing = JS_FALSE;
            interpReturnOK = JS_TRUE;
            fp->rval.setUndefined();
        }
#endif
    }

  forced_return:
    /*
     * Unwind the scope making sure that interpReturnOK stays false even when
     * js_UnwindScope returns true.
     *
     * When a trap handler returns JSTRAP_RETURN, we jump here with
     * interpReturnOK set to true bypassing any finally blocks.
     */
    interpReturnOK &= js_UnwindScope(cx, 0, interpReturnOK || cx->throwing);
    JS_ASSERT(regs.sp == fp->base());

#ifdef DEBUG
    cx->tracePrevPc = NULL;
#endif

    if (inlineCallCount)
        goto inline_return;

  exit:
    /*
     * At this point we are inevitably leaving an interpreted function or a
     * top-level script, and returning to one of:
     * (a) an "out of line" call made through js_Invoke;
     * (b) a js_Execute activation;
     * (c) a generator (SendToGenerator, jsiter.c).
     *
     * We must not be in an inline frame. The check above ensures that for the
     * error case and for a normal return, the code jumps directly to parent's
     * frame pc.
     */
    JS_ASSERT(inlineCallCount == 0);
    JS_ASSERT(cx->regs == &regs);
    *prevContextRegs = regs;
    cx->setCurrentRegs(prevContextRegs);

#ifdef JS_TRACER
    if (TRACE_RECORDER(cx))
        AbortRecording(cx, "recording out of Interpret");
#endif

    JS_ASSERT_IF(!fp->isGenerator(), !fp->blockChain);
    JS_ASSERT_IF(!fp->isGenerator(), !js_IsActiveWithOrBlock(cx, fp->scopeChain, 0));

    /* Undo the remaining effects committed on entry to Interpret. */
    if (script->staticLevel < JS_DISPLAY_SIZE)
        cx->display[script->staticLevel] = fp->displaySave;
    if (cx->version == currentVersion && currentVersion != originalVersion)
        js_SetVersion(cx, originalVersion);
    --cx->interpLevel;

    return interpReturnOK;

  atom_not_defined:
    {
        const char *printable;

        printable = js_AtomToPrintableString(cx, atomNotDefined);
        if (printable)
            js_ReportIsNotDefined(cx, printable);
        goto error;
    }
}

} /* namespace js */

#endif /* !defined jsinvoke_cpp___ */
