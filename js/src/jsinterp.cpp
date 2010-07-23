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
#include "jscntxtinlines.h"
#include "jsdtracef.h"
#include "jsobjinlines.h"
#include "jspropertycacheinlines.h"
#include "jsscopeinlines.h"
#include "jsscriptinlines.h"
#include "jsstrinlines.h"
#include "jscntxtinlines.h"

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
    return innermostNewChild;
}

JSBool
js_GetPrimitiveThis(JSContext *cx, Value *vp, Class *clasp, const Value **vpp)
{
    const Value *p = &vp[1];
    if (p->isObjectOrNull()) {
        JSObject *obj = ComputeThisFromVp(cx, vp);
        if (!InstanceOf(cx, obj, clasp, vp + 2))
            return JS_FALSE;
        *vpp = &obj->getPrimitiveThis();
    } else {
        *vpp = p;
    }
    return JS_TRUE;
}

/* Some objects (e.g., With) delegate 'this' to another object. */
static inline JSObject *
CallThisObjectHook(JSContext *cx, JSObject *obj, Value *argv)
{
    JSObject *thisp = obj->thisObject(cx);
    if (!thisp)
        return NULL;
    argv[-1].setObject(*thisp);
    return thisp;
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
JS_STATIC_INTERPRET JSObject *
ComputeGlobalThis(JSContext *cx, Value *argv)
{
    /* Find the inner global. */
    JSObject *inner;
    if (argv[-2].isPrimitive() || !argv[-2].toObject().getParent()) {
        inner = cx->globalObject;
        OBJ_TO_INNER_OBJECT(cx, inner);
        if (!inner)
            return NULL;
    } else {
        inner = argv[-2].toObject().getGlobal();
    }
    JS_ASSERT(inner->getClass()->flags & JSCLASS_IS_GLOBAL);

    JSObject *scope = JS_GetGlobalForScopeChain(cx);
    if (scope == inner) {
        /*
         * The outer object has not moved along to a new inner object.
         * This means we qualify for the cache slot in the global.
         */
        const Value &thisv = inner->getReservedSlot(JSRESERVED_GLOBAL_THIS);
        if (!thisv.isUndefined()) {
            argv[-1] = thisv;
            return thisv.toObjectOrNull();
        }

        JSObject *stuntThis = CallThisObjectHook(cx, inner, argv);
        JS_ALWAYS_TRUE(js_SetReservedSlot(cx, inner, JSRESERVED_GLOBAL_THIS,
                                          ObjectOrNullValue(stuntThis)));
        return stuntThis;
    }

    return CallThisObjectHook(cx, inner, argv);
}

namespace js {

JSObject *
ComputeThisFromArgv(JSContext *cx, Value *argv)
{
    JS_ASSERT(!argv[-1].isMagic(JS_THIS_POISON));
    if (argv[-1].isNull())
        return ComputeGlobalThis(cx, argv);

    JSObject *thisp;

    JS_ASSERT(!argv[-1].isNull());
    if (argv[-1].isPrimitive()) {
        if (!js_PrimitiveToObject(cx, &argv[-1]))
            return NULL;
        thisp = argv[-1].toObjectOrNull();
        return thisp;
    }

    thisp = &argv[-1].toObject();
    if (thisp->getClass() == &js_CallClass || thisp->getClass() == &js_BlockClass)
        return ComputeGlobalThis(cx, argv);

    return CallThisObjectHook(cx, thisp, argv);
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

    JSObject *obj = &vp[1].toObject();
    jsid id = ATOM_TO_JSID(cx->runtime->atomState.noSuchMethodAtom);
    AutoValueRooter tvr(cx);
    if (!js_GetMethod(cx, obj, id, JSGET_NO_METHOD_BARRIER, tvr.addr()))
        return false;
    if (tvr.value().isPrimitive()) {
        vp[0] = tvr.value();
    } else {
#if JS_HAS_XML_SUPPORT
        /* Extract the function name from function::name qname. */
        if (vp[0].isObject()) {
            obj = &vp[0].toObject();
            if (!js_IsFunctionQName(cx, obj, &id))
                return false;
            if (!JSID_IS_VOID(id))
                vp[0] = IdToValue(id);
        }
#endif
        obj = js_NewGCObject(cx);
        if (!obj)
            return false;

        /*
         * Null map to cause prompt and safe crash if this object were to
         * escape due to a bug. This will make the object appear to be a
         * stillborn instance that needs no finalization, which is sound:
         * NoSuchMethod helper objects own no manually allocated resources.
         */
        obj->map = NULL;
        obj->init(&js_NoSuchMethodClass, NULL, NULL, tvr.value());
        obj->fslots[JSSLOT_SAVED_ID] = vp[0];
        vp[0].setObject(*obj);
    }
    return true;
}

static JS_REQUIRES_STACK JSBool
NoSuchMethod(JSContext *cx, uintN argc, Value *vp, uint32 flags)
{
    InvokeArgsGuard args;
    if (!cx->stack().pushInvokeArgs(cx, 2, args))
        return JS_FALSE;

    JS_ASSERT(vp[0].isObject());
    JS_ASSERT(vp[1].isObject());
    JSObject *obj = &vp[0].toObject();
    JS_ASSERT(obj->getClass() == &js_NoSuchMethodClass);

    Value *invokevp = args.getvp();
    invokevp[0] = obj->fslots[JSSLOT_FOUND_FUNCTION];
    invokevp[1] = vp[1];
    invokevp[2] = obj->fslots[JSSLOT_SAVED_ID];
    JSObject *argsobj = js_NewArrayObject(cx, argc, vp + 2);
    if (!argsobj)
        return JS_FALSE;
    invokevp[3].setObject(*argsobj);
    JSBool ok = (flags & JSINVOKE_CONSTRUCT)
                ? InvokeConstructor(cx, args)
                : Invoke(cx, args, flags);
    vp[0] = invokevp[0];
    return ok;
}

#endif /* JS_HAS_NO_SUCH_METHOD */

namespace js {

class AutoPreserveEnumerators {
    JSContext *cx;
    JSObject *enumerators;

  public:
    AutoPreserveEnumerators(JSContext *cx) : cx(cx), enumerators(cx->enumerators)
    {
    }

    ~AutoPreserveEnumerators()
    {
        cx->enumerators = enumerators;
    }
};

static JS_REQUIRES_STACK bool
callJSNative(JSContext *cx, CallOp callOp, JSObject *thisp, uintN argc, Value *argv, Value *rval)
{
    Value *vp = argv - 2;
    if (callJSFastNative(cx, callOp, argc, vp)) {
        *rval = JS_RVAL(cx, vp);
        return true;
    }
    return false;
}

template <typename T>
static JS_REQUIRES_STACK bool
InvokeCommon(JSContext *cx, JSFunction *fun, JSScript *script, T native,
       const InvokeArgsGuard &args, uintN flags)
{
    uintN argc = args.getArgc();
    Value *vp = args.getvp();

    if (native && fun && fun->isFastNative()) {
#ifdef DEBUG_NOT_THROWING
        JSBool alreadyThrowing = cx->throwing;
#endif
        JSBool ok = callJSFastNative(cx, (FastNative) native, argc, vp);
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

    uintN nfixed = script ? script->nslots : 0;

    /*
     * Get a pointer to new frame/slots. This memory is not "claimed", so the
     * code before pushInvokeFrame must not reenter the interpreter.
     */
    JSFrameRegs regs;
    InvokeFrameGuard frame;
    if (!cx->stack().getInvokeFrame(cx, args, nmissing, nfixed, frame))
        return false;
    JSStackFrame *fp = frame.getFrame();

    /* Initialize missing missing arguments and new local variables. */
    Value *missing = vp + 2 + argc;
    SetValueRangeToUndefined(missing, nmissing);
    SetValueRangeToUndefined(fp->slots(), nvars);

    /* Initialize frame. */
    fp->thisv = vp[1];
    fp->callobj = NULL;
    fp->argsobj = NULL;
    fp->script = script;
    fp->fun = fun;
    fp->argc = argc;
    fp->argv = vp + 2;
    fp->rval = (flags & JSINVOKE_CONSTRUCT) ? fp->thisv : UndefinedValue();
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
    JSObject *parent = vp[0].toObject().getParent();
    if (native) {
        /* Slow natives and call ops expect the caller's scopeChain as their scopeChain. */
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

    DTrace::enterJSFun(cx, fp, fun, fp->down, fp->argc, fp->argv);

    /* Call the function, either a native method or an interpreted script. */
    JSBool ok;
    if (native) {
#ifdef DEBUG_NOT_THROWING
        JSBool alreadyThrowing = cx->throwing;
#endif

        JSObject *thisp = fp->thisv.toObjectOrNull();
        ok = callJSNative(cx, native, thisp, fp->argc, fp->argv, &fp->rval);

        JS_ASSERT(cx->fp == fp);
        JS_RUNTIME_METER(cx->runtime, nativeCalls);
#ifdef DEBUG_NOT_THROWING
        if (ok && !alreadyThrowing)
            ASSERT_NOT_THROWING(cx);
#endif
    } else {
        JS_ASSERT(script);
        AutoPreserveEnumerators preserve(cx);
        ok = Interpret(cx);
    }

    DTrace::exitJSFun(cx, fp, fun, fp->rval);

    if (hookData) {
        hook = cx->debugHooks->callHook;
        if (hook)
            hook(cx, fp, JS_FALSE, &ok, hookData);
    }

    fp->putActivationObjects(cx);
    *vp = fp->rval;
    return ok;
}

static JSBool
DoConstruct(JSContext *cx, JSObject *obj, uintN argc, Value *argv, Value *rval)
{

    Class *clasp = argv[-2].toObject().getClass();
    if (!clasp->construct) {
        js_ReportIsNotFunction(cx, &argv[-2], JSV2F_CONSTRUCT);
        return JS_FALSE;
    }
    return clasp->construct(cx, obj, argc, argv, rval);
}

static JSBool
DoSlowCall(JSContext *cx, uintN argc, Value *vp)
{
    JSStackFrame *fp = cx->fp;
    JSObject *obj = fp->getThisObject(cx);
    if (!obj)
        return false;
    JS_ASSERT(ObjectValue(*obj) == fp->thisv);

    JSObject *callee = &JS_CALLEE(cx, vp).toObject();
    Class *clasp = callee->getClass();
    JS_ASSERT(!(clasp->flags & CLASS_CALL_IS_FAST));
    if (!clasp->call) {
        js_ReportIsNotFunction(cx, &vp[0], 0);
        return JS_FALSE;
    }
    AutoValueRooter rval(cx);
    JSBool ok = clasp->call(cx, obj, argc, JS_ARGV(cx, vp), rval.addr());
    if (ok)
        JS_SET_RVAL(cx, vp, rval.value());
    return ok;
}

/*
 * Find a function reference and its 'this' value implicit first parameter
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

    const Value &v = vp[0];
    if (v.isPrimitive()) {
        js_ReportIsNotFunction(cx, vp, flags & JSINVOKE_FUNFLAGS);
        return false;
    }

    JSObject *funobj = &v.toObject();
    Class *clasp = funobj->getClass();

    if (clasp == &js_FunctionClass) {
        /* Get private data and set derived locals from it. */
        JSFunction *fun = GET_FUNCTION_PRIVATE(cx, funobj);
        Native native;
        JSScript *script;
        if (FUN_INTERPRETED(fun)) {
            native = NULL;
            script = fun->u.i.script;
            JS_ASSERT(script);

            if (script->isEmpty()) {
                if (flags & JSINVOKE_CONSTRUCT) {
                    JS_ASSERT(vp[1].isObject());
                    *vp = vp[1];
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
            vp[1].setObject(*funobj->getParent());
        } else if (!vp[1].isObjectOrNull()) {
            JS_ASSERT(!(flags & JSINVOKE_CONSTRUCT));
            if (PrimitiveThisTest(fun, vp[1]))
                return InvokeCommon(cx, fun, script, native, args, flags);
        }

        if (flags & JSINVOKE_CONSTRUCT) {
            JS_ASSERT(args.getvp()[1].isObject());
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
                Value *vp = args.getvp();
                if (!ComputeThisFromVp(cx, vp))
                    return false;
                flags |= JSFRAME_COMPUTED_THIS;
            }
        }
        return InvokeCommon(cx, fun, script, native, args, flags);
    }

#if JS_HAS_NO_SUCH_METHOD
    if (clasp == &js_NoSuchMethodClass)
        return NoSuchMethod(cx, argc, vp, flags);
#endif

    /* Try a call or construct native object op. */
    if (flags & JSINVOKE_CONSTRUCT) {
        if (!vp[1].isObjectOrNull()) {
            if (!js_PrimitiveToObject(cx, &vp[1]))
                return false;
        }
        return InvokeCommon(cx, NULL, NULL, DoConstruct, args, flags);
    }
    CallOp callOp = (clasp->flags & CLASS_CALL_IS_FAST) ? (CallOp) clasp->call : DoSlowCall;
    return InvokeCommon(cx, NULL, NULL, callOp, args, flags);
}

extern JS_REQUIRES_STACK JS_FRIEND_API(bool)
InvokeFriendAPI(JSContext *cx, const InvokeArgsGuard &args, uintN flags)
{
    return Invoke(cx, args, flags);
}

JSBool
InternalInvoke(JSContext *cx, const Value &thisv, const Value &fval, uintN flags,
                  uintN argc, Value *argv, Value *rval)
{
    LeaveTrace(cx);

    InvokeArgsGuard args;
    if (!cx->stack().pushInvokeArgs(cx, argc, args))
        return JS_FALSE;

    args.getvp()[0] = fval;
    args.getvp()[1] = thisv;
    memcpy(args.getvp() + 2, argv, argc * sizeof(Value));

    if (!Invoke(cx, args, flags))
        return JS_FALSE;

    /*
     * Store *rval in the lastInternalResult pigeon-hole GC root, solely
     * so users of js_InternalInvoke and its direct and indirect
     * (js_ValueToString for example) callers do not need to manage roots
     * for local, temporary references to such results.
     */
    *rval = *args.getvp();
    if (rval->isMarkable())
        cx->weakRoots.lastInternalResult = rval->asGCThing();

    return JS_TRUE;
}

bool
InternalGetOrSet(JSContext *cx, JSObject *obj, jsid id, const Value &fval,
                 JSAccessMode mode, uintN argc, Value *argv, Value *rval)
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

    DTrace::ExecutionScope executionScope(script);
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

    /* Initialize fixed slots (GVAR ops expecte NULL). */
    SetValueRangeToNull(fp->slots(), script->nfixed);

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
            sharps[0] = down->slots()[base];
            sharps[1] = down->slots()[base + 1];
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
        fp->thisv = down->thisv;
        fp->flags = flags | (down->flags & JSFRAME_COMPUTED_THIS);
        fp->argc = down->argc;
        fp->argv = down->argv;
        fp->annotation = down->annotation;
        fp->scopeChain = chain;

        /*
         * We want to call |down->varobj()|, but this requires knowing the
         * CallStackSegment of |down|. If |down == cx->fp|, the callstack is
         * simply the context's active callstack, so we can use
         * |down->varobj(cx)|.  When |down != cx->fp|, we need to do a slow
         * linear search. Luckily, this only happens with indirect eval and
         * JS_EvaluateInStackFrame.
         */
        initialVarObj = (down == cx->fp)
                        ? down->varobj(cx)
                        : down->varobj(cx->containingSegment(down));
    } else {
        fp->callobj = NULL;
        fp->argsobj = NULL;
        fp->fun = NULL;
        fp->thisv.setUndefined();  /* Make GC-safe until initialized below. */
        fp->flags = flags | JSFRAME_COMPUTED_THIS;
        fp->argc = 0;
        fp->argv = NULL;
        fp->annotation = NULL;

        JSObject *innerizedChain = chain;
        OBJ_TO_INNER_OBJECT(cx, innerizedChain);
        if (!innerizedChain)
            return false;
        fp->scopeChain = innerizedChain;

        initialVarObj = (cx->options & JSOPTION_VAROBJFIX)
                        ? chain->getGlobal()
                        : chain;
    }
    JS_ASSERT(initialVarObj->map->ops->defineProperty == js_DefineProperty);

    fp->script = script;
    fp->imacpc = NULL;
    fp->rval.setUndefined();
    fp->blockChain = NULL;

    /* Initialize regs. */
    regs.pc = script->code;
    regs.sp = fp->base();

    /* Officially push |fp|. |frame|'s destructor pops. */
    cx->stack().pushExecuteFrame(cx, frame, regs, initialVarObj);

    /* Now that the frame has been pushed, we can call the thisObject hook. */
    if (!down) {
        JSObject *thisp = chain->thisObject(cx);
        if (!thisp)
            return false;
        fp->thisv.setObject(*thisp);
    }

    void *hookData = NULL;
    if (JSInterpreterHook hook = cx->debugHooks->executeHook)
        hookData = hook(cx, fp, JS_TRUE, 0, cx->debugHooks->executeHookData);

    AutoPreserveEnumerators preserve(cx);
    JSBool ok = Interpret(cx);
    if (result)
        *result = fp->rval;

    if (hookData) {
        if (JSInterpreterHook hook = cx->debugHooks->executeHook)
            hook(cx, fp, JS_FALSE, &ok, hookData);
    }

    return !!ok;
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
     * the common case of a nonexistent property.
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
        return false;
    if (!prop)
        return true;
    if (obj2->isNative()) {
        oldAttrs = ((JSScopeProperty *) prop)->attributes();

        /* If our caller doesn't want prop, unlock obj2. */
        if (!propp)
            JS_UNLOCK_OBJ(cx, obj2);
    } else {
        if (!obj2->getAttributes(cx, id, &oldAttrs))
            return false;
    }

    if (!propp) {
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
            isFunction = IsFunctionObject(value);
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
    return !!JS_ReportErrorFlagsAndNumber(cx, report,
                                          js_GetErrorMessage, NULL,
                                          JSMSG_REDECLARED_VAR,
                                          type, name);
}

JSBool
js_HasInstance(JSContext *cx, JSObject *obj, const Value *v, JSBool *bp)
{
    Class *clasp = obj->getClass();
    if (clasp->hasInstance)
        return clasp->hasInstance(cx, obj, v, bp);
    js_ReportValueError(cx, JSMSG_BAD_INSTANCEOF_RHS,
                        JSDVG_SEARCH_STACK, ObjectValue(*obj), NULL);
    return JS_FALSE;
}

static JS_ALWAYS_INLINE bool
EqualObjects(JSContext *cx, JSObject *lobj, JSObject *robj)
{
    return lobj == robj ||
           lobj->wrappedObject(cx) == robj->wrappedObject(cx);
}

bool
StrictlyEqual(JSContext *cx, const Value &lref, const Value &rref)
{
    Value lval = lref, rval = rref;
    if (SameType(lval, rval)) {
        if (lval.isString())
            return js_EqualStrings(lval.toString(), rval.toString());
        if (lval.isDouble())
            return JSDOUBLE_COMPARE(lval.toDouble(), ==, rval.toDouble(), JS_FALSE);
        if (lval.isObject())
            return EqualObjects(cx, &lval.toObject(), &rval.toObject());
        return lval.payloadAsRawUint32() == rval.payloadAsRawUint32();
    }

    if (lval.isDouble() && rval.isInt32()) {
        double ld = lval.toDouble();
        double rd = rval.toInt32();
        return JSDOUBLE_COMPARE(ld, ==, rd, JS_FALSE);
    }
    if (lval.isInt32() && rval.isDouble()) {
        double ld = lval.toInt32();
        double rd = rval.toDouble();
        return JSDOUBLE_COMPARE(ld, ==, rd, JS_FALSE);
    }

    return false;
}

static inline bool
IsNegativeZero(const Value &v)
{
    return v.isDouble() && JSDOUBLE_IS_NEGZERO(v.toDouble());
}

static inline bool
IsNaN(const Value &v)
{
    return v.isDouble() && JSDOUBLE_IS_NaN(v.toDouble());
}

bool
SameValue(const Value &v1, const Value &v2, JSContext *cx)
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
    Value v = vref;
    if (v.isNumber())
        return JSTYPE_NUMBER;
    if (v.isString())
        return JSTYPE_STRING;
    if (v.isNull())
        return JSTYPE_OBJECT;
    if (v.isUndefined())
        return JSTYPE_VOID;
    if (v.isObject())
        return v.toObject().map->ops->typeOf(cx, &v.toObject());
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
InvokeConstructor(JSContext *cx, const InvokeArgsGuard &args)
{
    JS_ASSERT(!js_FunctionClass.construct);
    Value *vp = args.getvp();

    if (vp->isPrimitive()) {
        /* Use js_ValueToFunction to report an error. */
        JS_ALWAYS_TRUE(!js_ValueToFunction(cx, vp, JSV2F_CONSTRUCT));
        return false;
    }

    JSObject *obj2 = &vp->toObject();

    /*
     * Get the constructor prototype object for this function.
     * Use the nominal 'this' parameter slot, vp[1], as a local
     * root to protect this prototype, in case it has no other
     * strong refs.
     */
    if (!obj2->getProperty(cx, ATOM_TO_JSID(cx->runtime->atomState.classPrototypeAtom), &vp[1]))
        return false;

    const Value &v = vp[1];
    JSObject *proto = v.isObjectOrNull() ? v.toObjectOrNull() : NULL;
    JSObject *parent = obj2->getParent();
    Class *clasp = &js_ObjectClass;

    if (obj2->getClass() == &js_FunctionClass) {
        JSFunction *f = GET_FUNCTION_PRIVATE(cx, obj2);
        if (!f->isInterpreted() && f->u.n.clasp)
            clasp = f->u.n.clasp;
    }

    JSObject *obj = NewObject(cx, clasp, proto, parent);
    if (!obj)
        return JS_FALSE;

    /* Keep |obj| rooted in case vp[1] is overwritten with a primitive. */
    AutoObjectRooter tvr(cx, obj);

    /* Now we have an object with a constructor method; call it. */
    vp[1].setObject(*obj);
    if (!Invoke(cx, args, JSINVOKE_CONSTRUCT))
        return JS_FALSE;

    /* Check the return value and if it's primitive, force it to be obj. */
    const Value &rval = *vp;
    if (rval.isPrimitive()) {
        if (obj2->getClass() != &js_FunctionClass) {
            /* native [[Construct]] returning primitive is error */
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                 JSMSG_BAD_NEW_RESULT,
                                 js_ValueToPrintableString(cx, *vp));
            return JS_FALSE;
        }
        vp->setObject(*obj);
    }

    JS_RUNTIME_METER(cx->runtime, constructs);
    return JS_TRUE;
}

bool
ValueToId(JSContext *cx, const Value &v, jsid *idp)
{
    int32_t i;
    if (ValueFitsInInt32(v, &i) && INT_FITS_IN_JSID(i)) {
        *idp = INT_TO_JSID(i);
        return true;
    }

#if JS_HAS_XML_SUPPORT
    if (v.isObject()) {
        JSObject *obj = &v.toObject();
        if (obj->isXML()) {
            *idp = OBJECT_TO_JSID(obj);
            return JS_TRUE;
        }
        if (!js_IsFunctionQName(cx, obj, idp))
            return JS_FALSE;
        if (!JSID_IS_VOID(*idp))
            return JS_TRUE;
    }
#endif

    return js_ValueToStringId(cx, v, idp);
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
        obj = &sp[-1].toObject();
    } else {
        obj = js_ValueToNonNullObject(cx, sp[-1]);
        if (!obj)
            return JS_FALSE;
        sp[-1].setObject(*obj);
    }

    JSObject *parent = js_GetScopeChain(cx, fp);
    if (!parent)
        return JS_FALSE;

    OBJ_TO_INNER_OBJECT(cx, obj);
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
        if (!ValueToNumber(cx, *vp, &d))
            return JS_FALSE;
        vp->setNumber(d);
        (cs->format & JOF_INC) ? ++d : --d;
        vp2->setNumber(d);
        return JS_TRUE;
    }

    double d;
    if (!ValueToNumber(cx, *vp, &d))
        return JS_FALSE;
    (cs->format & JOF_INC) ? ++d : --d;
    vp->setNumber(d);
    *vp2 = *vp;
    return JS_TRUE;
}

const Value &
js_GetUpvar(JSContext *cx, uintN level, UpvarCookie cookie)
{
    level -= cookie.level();
    JS_ASSERT(level < JS_DISPLAY_SIZE);

    JSStackFrame *fp = cx->display[level];
    JS_ASSERT(fp->script);

    uintN slot = cookie.slot();
    Value *vp;

    if (!fp->fun || (fp->flags & JSFRAME_EVAL)) {
        vp = fp->slots() + fp->script->nfixed;
    } else if (slot < fp->fun->nargs) {
        vp = fp->argv;
    } else if (slot == UpvarCookie::CALLEE_SLOT) {
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
                char *bytes = DecompileValueGenerator(cx, n, regs->sp[n], NULL);
                if (bytes) {
                    fprintf(tracefp, "%s %s",
                            (n == -ndefs) ? "  output:" : ",",
                            bytes);
                    cx->free(bytes);
                } else {
                    JS_ClearPendingException(cx);
                }
            }
            fprintf(tracefp, " @ %u\n", (uintN) (regs->sp - fp->base()));
        }
        fprintf(tracefp, "  stack: ");
        for (Value *siter = fp->base(); siter < regs->sp; siter++) {
            str = js_ValueToString(cx, *siter);
            if (!str) {
                fputs("<null>", tracefp);
            } else {
                JS_ClearPendingException(cx);
                js_FileEscapedString(tracefp, str, 0);
            }
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
            char *bytes = DecompileValueGenerator(cx, n, regs->sp[n], NULL);
            if (bytes) {
                fprintf(tracefp, "%s %s",
                        (n == -nuses) ? "  inputs:" : ",",
                        bytes);
                cx->free(bytes);
            } else {
                JS_ClearPendingException(cx);
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
    MeterRepr(JSContext *cx)
    {
        // Note that we simply ignore the possibility of errors (OOMs)
        // using the hash map, since this is only metering code.

        if (!opinputsInitialized) {
            opinputs.init();
            opinputsInitialized = true;
        }

        JSOp op = JSOp(*cx->regs->pc);
        int nuses = js_GetStackUses(&js_CodeSpec[op], op, cx->regs->pc);

        // Build the OpInput.
        OpInput opinput(op);
        for (int i = 0; i < nuses; ++i) {
            jsval v = cx->regs->sp[-nuses+i];
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

#define PUSH_COPY(v)             *regs.sp++ = v
#define PUSH_NULL()              regs.sp++->setNull()
#define PUSH_UNDEFINED()         regs.sp++->setUndefined()
#define PUSH_BOOLEAN(b)          regs.sp++->setBoolean(b)
#define PUSH_DOUBLE(d)           regs.sp++->setDouble(d)
#define PUSH_INT32(i)            regs.sp++->setInt32(i)
#define PUSH_STRING(s)           regs.sp++->setString(s)
#define PUSH_OBJECT(obj)         regs.sp++->setObject(obj)
#define PUSH_OBJECT_OR_NULL(obj) regs.sp++->setObject(obj)
#define PUSH_HOLE()              regs.sp++->setMagic(JS_ARRAY_HOLE)
#define POP_COPY_TO(v)           v = *--regs.sp

#define POP_BOOLEAN(cx, vp, b)                                                \
    JS_BEGIN_MACRO                                                            \
        vp = &regs.sp[-1];                                                    \
        if (vp->isNull()) {                                                   \
            b = false;                                                        \
        } else if (vp->isBoolean()) {                                         \
            b = vp->toBoolean();                                              \
        } else {                                                              \
            b = !!js_ValueToBoolean(*vp);                                     \
        }                                                                     \
        regs.sp--;                                                            \
    JS_END_MACRO

#define VALUE_TO_OBJECT(cx, vp, obj)                                          \
    JS_BEGIN_MACRO                                                            \
        if ((vp)->isObject()) {                                               \
            obj = &(vp)->toObject();                                          \
        } else {                                                              \
            obj = js_ValueToNonNullObject(cx, *(vp));                         \
            if (!obj)                                                         \
                goto error;                                                   \
            (vp)->setObject(*obj);                                            \
        }                                                                     \
    JS_END_MACRO

#define FETCH_OBJECT(cx, n, obj)                                              \
    JS_BEGIN_MACRO                                                            \
        Value *vp_ = &regs.sp[n];                                             \
        VALUE_TO_OBJECT(cx, vp_, obj);                                        \
    JS_END_MACRO

#define DEFAULT_VALUE(cx, n, hint, v)                                         \
    JS_BEGIN_MACRO                                                            \
        JS_ASSERT(v.isObject());                                              \
        JS_ASSERT(v == regs.sp[n]);                                           \
        if (!DefaultValue(cx, &v.toObject(), hint, &regs.sp[n]))              \
            goto error;                                                       \
        v = regs.sp[n];                                                       \
    JS_END_MACRO

/* Test whether v is an int in the range [-2^31 + 1, 2^31 - 2] */
static JS_ALWAYS_INLINE bool
CanIncDecWithoutOverflow(int32_t i)
{
    return (i > JSVAL_INT_MIN) && (i < JSVAL_INT_MAX);
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
# define METER_REPR(cx)         (reprmeter::MeterRepr(cx))
#else
# define METER_REPR(cx)         ((void) 0)
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
                     &sprop->methodObject() == &pobj->lockedGetSlot(sprop->slot).toObject());
    } else {
        Value v;
        JS_ASSERT(entry->vword.isFunObj());
        JS_ASSERT(!entry->vword.isNull());
        JS_ASSERT(pobj->scope()->brandedOrHasMethodBarrier());
        JS_ASSERT(sprop->hasDefaultGetterOrIsMethod());
        JS_ASSERT(SPROP_HAS_VALID_SLOT(sprop, pobj->scope()));
        v = pobj->lockedGetSlot(sprop->slot);
        JS_ASSERT(&entry->vword.toFunObj() == &v.toObject());

        if (sprop->isMethod()) {
            JS_ASSERT(js_CodeSpec[*regs.pc].format & JOF_CALLOP);
            JS_ASSERT(&sprop->methodObject() == &v.toObject());
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

/*
 * Inline fast paths for iteration. js_IteratorMore and js_IteratorNext handle
 * all cases, but we inline the most frequently taken paths here.
 */
static inline bool
IteratorMore(JSContext *cx, JSObject *iterobj, bool *cond, Value *rval)
{
    if (iterobj->getClass() == &js_IteratorClass.base) {
        NativeIterator *ni = (NativeIterator *) iterobj->getPrivate();
        *cond = (ni->props_cursor < ni->props_end);
    } else {
        if (!js_IteratorMore(cx, iterobj, rval))
            return false;
        *cond = rval->isTrue();
    }
    return true;
}

static inline bool
IteratorNext(JSContext *cx, JSObject *iterobj, Value *rval)
{
    if (iterobj->getClass() == &js_IteratorClass.base) {
        NativeIterator *ni = (NativeIterator *) iterobj->getPrivate();
        JS_ASSERT(ni->props_cursor < ni->props_end);
        if (ni->isKeyIter()) {
            jsid id = *ni->currentKey();
            if (JSID_IS_ATOM(id)) {
                rval->setString(JSID_TO_STRING(id));
                ni->incKeyCursor();
                return true;
            }
            /* Take the slow path if we have to stringify a numeric property name. */
        } else {
            *rval = *ni->currentValue();
            ni->incValueCursor();
            return true;
        }
    }
    return js_IteratorNext(cx, iterobj, rval);
}


namespace js {

JS_REQUIRES_STACK bool
Interpret(JSContext *cx)
{
#ifdef MOZ_TRACEVIS
    TraceVisStateObj tvso(cx, S_INTERP);
#endif
    JSAutoResolveFlags rf(cx, JSRESOLVE_INFER);

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
                                METER_REPR(cx);                               \
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
    (obj = script->getObject(GET_FULL_INDEX(PCOFF)))

#define LOAD_FUNCTION(PCOFF)                                                  \
    (fun = script->getFunction(GET_FULL_INDEX(PCOFF)))

#define LOAD_DOUBLE(PCOFF, dbl)                                               \
    (dbl = script->getConst(GET_FULL_INDEX(PCOFF)).toDouble())

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
            MonitorResult r = MonitorLoopEdge(cx, inlineCallCount, reason);   \
            if (r == MONITOR_RECORDING) {                                     \
                JS_ASSERT(TRACE_RECORDER(cx));                                \
                MONITOR_BRANCH_TRACEVIS;                                      \
                ENABLE_INTERRUPTS();                                          \
            }                                                                 \
            RESTORE_INTERP_VARS();                                            \
            JS_ASSERT_IF(cx->throwing, r == MONITOR_ERROR);                   \
            if (r == MONITOR_ERROR)                                           \
                goto error;                                                   \
        }                                                                     \
    JS_END_MACRO

#else /* !JS_TRACER */

#define MONITOR_BRANCH(reason) ((void) 0)

#endif /* !JS_TRACER */

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

#define CHECK_INTERRUPT_HANDLER()                                             \
    JS_BEGIN_MACRO                                                            \
        if (cx->debugHooks->interruptHook)                                    \
            ENABLE_INTERRUPTS();                                              \
    JS_END_MACRO

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
    jsint len;
    len = 0;
#if JS_THREADED_INTERP
    DO_NEXT_OP(len);
#else
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

#if JS_THREADED_INTERP
  interrupt:
#else /* !JS_THREADED_INTERP */
  case -1:
    JS_ASSERT(switchMask == -1);
#endif /* !JS_THREADED_INTERP */
    {
        bool moreInterrupts = false;
        JSInterruptHook hook = cx->debugHooks->interruptHook;
        if (hook) {
#ifdef JS_TRACER
            if (TRACE_RECORDER(cx))
                AbortRecording(cx, "interrupt hook");
#endif
            Value rval;
            switch (hook(cx, script, regs.pc, Jsvalify(&rval),
                         cx->debugHooks->interruptHookData)) {
              case JSTRAP_ERROR:
                goto error;
              case JSTRAP_CONTINUE:
                break;
              case JSTRAP_RETURN:
                fp->rval = rval;
                interpReturnOK = JS_TRUE;
                goto forced_return;
              case JSTRAP_THROW:
                cx->throwing = JS_TRUE;
                cx->exception = rval;
                goto error;
              default:;
            }
            moreInterrupts = true;
        }

#ifdef JS_TRACER
        if (TraceRecorder* tr = TRACE_RECORDER(cx)) {
            AbortableRecordingStatus status = tr->monitorRecording(op);
            JS_ASSERT_IF(cx->throwing, status == ARECORD_ERROR);
            switch (status) {
              case ARECORD_CONTINUE:
                moreInterrupts = true;
                break;
              case ARECORD_IMACRO:
              case ARECORD_IMACRO_ABORTED:
                atoms = COMMON_ATOMS_START(&rt->atomState);
                op = JSOp(*regs.pc);
                if (status == ARECORD_IMACRO)
                    DO_OP();    /* keep interrupting for op. */
                break;
              case ARECORD_ERROR:
                // The code at 'error:' aborts the recording.
                goto error;
              case ARECORD_ABORTED:
              case ARECORD_COMPLETED:
                break;
              case ARECORD_STOP:
                /* A 'stop' error should have already aborted recording. */
              default:
                JS_NOT_REACHED("Bad recording status");
            }
        }
#endif /* !JS_TRACER */

#if JS_THREADED_INTERP
#ifdef MOZ_TRACEVIS
        if (!moreInterrupts)
            ExitTraceVisState(cx, R_ABORT);
#endif
        jumpTable = moreInterrupts ? interruptJumpTable : normalJumpTable;
        JS_EXTENSION_(goto *normalJumpTable[op]);
#else
        switchMask = moreInterrupts ? -1 : 0;
        switchOp = intN(op);
        goto do_switch;
#endif
    }

/* No-ops for ease of decompilation. */
ADD_EMPTY_CASE(JSOP_NOP)
ADD_EMPTY_CASE(JSOP_CONDSWITCH)
ADD_EMPTY_CASE(JSOP_TRY)
ADD_EMPTY_CASE(JSOP_TRACE)
#if JS_HAS_XML_SUPPORT
ADD_EMPTY_CASE(JSOP_STARTXML)
ADD_EMPTY_CASE(JSOP_STARTXMLEXPR)
#endif
END_EMPTY_CASES

/* ADD_EMPTY_CASE is not used here as JSOP_LINENO_LENGTH == 3. */
BEGIN_CASE(JSOP_LINENO)
END_CASE(JSOP_LINENO)

BEGIN_CASE(JSOP_PUSH)
    PUSH_UNDEFINED();
END_CASE(JSOP_PUSH)

BEGIN_CASE(JSOP_POP)
    regs.sp--;
END_CASE(JSOP_POP)

BEGIN_CASE(JSOP_POPN)
{
    regs.sp -= GET_UINT16(regs.pc);
#ifdef DEBUG
    JS_ASSERT(fp->base() <= regs.sp);
    JSObject *obj = fp->blockChain;
    JS_ASSERT_IF(obj,
                 OBJ_BLOCK_DEPTH(cx, obj) + OBJ_BLOCK_COUNT(cx, obj)
                 <= (size_t) (regs.sp - fp->base()));
    for (obj = fp->scopeChain; obj; obj = obj->getParent()) {
        Class *clasp = obj->getClass();
        if (clasp != &js_BlockClass && clasp != &js_WithClass)
            continue;
        if (obj->getPrivate() != js_FloatingFrameIfGenerator(cx, fp))
            break;
        JS_ASSERT(fp->base() + OBJ_BLOCK_DEPTH(cx, obj)
                             + ((clasp == &js_BlockClass)
                                ? OBJ_BLOCK_COUNT(cx, obj)
                                : 1)
                  <= regs.sp);
    }
#endif
}
END_CASE(JSOP_POPN)

BEGIN_CASE(JSOP_SETRVAL)
BEGIN_CASE(JSOP_POPV)
    ASSERT_NOT_THROWING(cx);
    POP_COPY_TO(fp->rval);
END_CASE(JSOP_POPV)

BEGIN_CASE(JSOP_ENTERWITH)
    if (!js_EnterWith(cx, -1))
        goto error;

    /*
     * We must ensure that different "with" blocks have different stack depth
     * associated with them. This allows the try handler search to properly
     * recover the scope chain. Thus we must keep the stack at least at the
     * current level.
     *
     * We set sp[-1] to the current "with" object to help asserting the
     * enter/leave balance in [leavewith].
     */
    regs.sp[-1].setObject(*fp->scopeChain);
END_CASE(JSOP_ENTERWITH)

BEGIN_CASE(JSOP_LEAVEWITH)
    JS_ASSERT(&regs.sp[-1].toObject() == fp->scopeChain);
    regs.sp--;
    js_LeaveWith(cx);
END_CASE(JSOP_LEAVEWITH)

BEGIN_CASE(JSOP_RETURN)
    POP_COPY_TO(fp->rval);
    /* FALL THROUGH */

BEGIN_CASE(JSOP_RETRVAL)    /* fp->rval already set */
BEGIN_CASE(JSOP_STOP)
{
    /*
     * When the inlined frame exits with an exception or an error, ok will be
     * false after the inline_return label.
     */
    ASSERT_NOT_THROWING(cx);
    CHECK_BRANCH();

    if (fp->imacpc) {
        /*
         * If we are at the end of an imacro, return to its caller in the
         * current frame.
         */
        JS_ASSERT(op == JSOP_STOP);
        JS_ASSERT((uintN)(regs.sp - fp->slots()) <= script->nslots);
        regs.pc = fp->imacpc + js_CodeSpec[*fp->imacpc].length;
        fp->imacpc = NULL;
        atoms = script->atomMap.vector;
        op = JSOp(*regs.pc);
        DO_OP();
    }

    JS_ASSERT(regs.sp == fp->base());
    if ((fp->flags & JSFRAME_CONSTRUCTING) && fp->rval.isPrimitive())
        fp->rval = fp->thisv;

    interpReturnOK = true;
    if (inlineCallCount)
  inline_return:
    {
        JS_ASSERT(!fp->blockChain);
        JS_ASSERT(!js_IsActiveWithOrBlock(cx, fp->scopeChain, 0));

        if (JS_LIKELY(script->staticLevel < JS_DISPLAY_SIZE))
            cx->display[script->staticLevel] = fp->displaySave;

        void *hookData = fp->hookData;
        if (JS_UNLIKELY(hookData != NULL)) {
            if (JSInterpreterHook hook = cx->debugHooks->callHook) {
                hook(cx, fp, JS_FALSE, &interpReturnOK, hookData);
                CHECK_INTERRUPT_HANDLER();
            }
        }

        /*
         * If fp has a call object, sync values and clear the back-
         * pointer. This can happen for a lightweight function if it calls eval
         * unexpectedly (in a way that is hidden from the compiler). See bug
         * 325540.
         */
        fp->putActivationObjects(cx);

        DTrace::exitJSFun(cx, fp, fp->fun, fp->rval);

        /* Restore context version only if callee hasn't set version. */
        if (JS_LIKELY(cx->version == currentVersion)) {
            currentVersion = fp->callerVersion;
            if (currentVersion != cx->version)
                js_SetVersion(cx, currentVersion);
        }

        /*
         * If inline-constructing, replace primitive rval with the new object
         * passed in via |this|, and instrument this constructor invocation.
         */
        if (fp->flags & JSFRAME_CONSTRUCTING) {
            if (fp->rval.isPrimitive())
                fp->rval = fp->thisv;
            JS_RUNTIME_METER(cx->runtime, constructs);
        }

        JSStackFrame *down = fp->down;
        bool recursive = fp->script == down->script;

        /* Pop the frame. */
        cx->stack().popInlineFrame(cx, fp, down);

        /* Propagate return value before fp is lost. */
        regs.sp[-1] = fp->rval;

        /* Sync interpreter registers. */
        fp = cx->fp;
        script = fp->script;
        atoms = FrameAtomBase(cx, fp);

        /* Resume execution in the calling frame. */
        inlineCallCount--;
        if (JS_LIKELY(interpReturnOK)) {
            JS_ASSERT(js_CodeSpec[js_GetOpcode(cx, script, regs.pc)].length
                      == JSOP_CALL_LENGTH);
            TRACE_0(LeaveFrame);
            if (!TRACE_RECORDER(cx) && recursive) {
                if (*(regs.pc + JSOP_CALL_LENGTH) == JSOP_TRACE) {
                    regs.pc += JSOP_CALL_LENGTH;
                    MONITOR_BRANCH(Record_LeaveFrame);
                    op = (JSOp)*regs.pc;
                    DO_OP();
                }
            }
            if (*(regs.pc + JSOP_CALL_LENGTH) == JSOP_TRACE ||
                *(regs.pc + JSOP_CALL_LENGTH) == JSOP_NOP) {
                JS_STATIC_ASSERT(JSOP_TRACE_LENGTH == JSOP_NOP_LENGTH);
                regs.pc += JSOP_CALL_LENGTH;
                len = JSOP_TRACE_LENGTH;
            } else {
                len = JSOP_CALL_LENGTH;
            }
            DO_NEXT_OP(len);
        }
        goto error;
    }
    interpReturnOK = true;
    goto exit;
}

BEGIN_CASE(JSOP_DEFAULT)
    regs.sp--;
    /* FALL THROUGH */
BEGIN_CASE(JSOP_GOTO)
{
    len = GET_JUMP_OFFSET(regs.pc);
    BRANCH(len);
}
END_CASE(JSOP_GOTO)

BEGIN_CASE(JSOP_IFEQ)
{
    bool cond;
    Value *_;
    POP_BOOLEAN(cx, _, cond);
    if (cond == false) {
        len = GET_JUMP_OFFSET(regs.pc);
        BRANCH(len);
    }
}
END_CASE(JSOP_IFEQ)

BEGIN_CASE(JSOP_IFNE)
{
    bool cond;
    Value *_;
    POP_BOOLEAN(cx, _, cond);
    if (cond != false) {
        len = GET_JUMP_OFFSET(regs.pc);
        BRANCH(len);
    }
}
END_CASE(JSOP_IFNE)

BEGIN_CASE(JSOP_OR)
{
    bool cond;
    Value *vp;
    POP_BOOLEAN(cx, vp, cond);
    if (cond == true) {
        len = GET_JUMP_OFFSET(regs.pc);
        PUSH_COPY(*vp);
        DO_NEXT_OP(len);
    }
}
END_CASE(JSOP_OR)

BEGIN_CASE(JSOP_AND)
{
    bool cond;
    Value *vp;
    POP_BOOLEAN(cx, vp, cond);
    if (cond == false) {
        len = GET_JUMP_OFFSET(regs.pc);
        PUSH_COPY(*vp);
        DO_NEXT_OP(len);
    }
}
END_CASE(JSOP_AND)

BEGIN_CASE(JSOP_DEFAULTX)
    regs.sp--;
    /* FALL THROUGH */
BEGIN_CASE(JSOP_GOTOX)
{
    len = GET_JUMPX_OFFSET(regs.pc);
    BRANCH(len);
}
END_CASE(JSOP_GOTOX);

BEGIN_CASE(JSOP_IFEQX)
{
    bool cond;
    Value *_;
    POP_BOOLEAN(cx, _, cond);
    if (cond == false) {
        len = GET_JUMPX_OFFSET(regs.pc);
        BRANCH(len);
    }
}
END_CASE(JSOP_IFEQX)

BEGIN_CASE(JSOP_IFNEX)
{
    bool cond;
    Value *_;
    POP_BOOLEAN(cx, _, cond);
    if (cond != false) {
        len = GET_JUMPX_OFFSET(regs.pc);
        BRANCH(len);
    }
}
END_CASE(JSOP_IFNEX)

BEGIN_CASE(JSOP_ORX)
{
    bool cond;
    Value *vp;
    POP_BOOLEAN(cx, vp, cond);
    if (cond == true) {
        len = GET_JUMPX_OFFSET(regs.pc);
        PUSH_COPY(*vp);
        DO_NEXT_OP(len);
    }
}
END_CASE(JSOP_ORX)

BEGIN_CASE(JSOP_ANDX)
{
    bool cond;
    Value *vp;
    POP_BOOLEAN(cx, vp, cond);
    if (cond == JS_FALSE) {
        len = GET_JUMPX_OFFSET(regs.pc);
        PUSH_COPY(*vp);
        DO_NEXT_OP(len);
    }
}
END_CASE(JSOP_ANDX)

/*
 * If the index value at sp[n] is not an int that fits in a jsval, it could
 * be an object (an XML QName, AttributeName, or AnyName), but only if we are
 * compiling with JS_HAS_XML_SUPPORT.  Otherwise convert the index value to a
 * string atom id.
 */
#define FETCH_ELEMENT_ID(obj, n, id)                                          \
    JS_BEGIN_MACRO                                                            \
        const Value &idval_ = regs.sp[n];                                     \
        int32_t i_;                                                           \
        if (ValueFitsInInt32(idval_, &i_) && INT_FITS_IN_JSID(i_)) {          \
            id = INT_TO_JSID(i_);                                             \
        } else {                                                              \
            if (!js_InternNonIntElementId(cx, obj, idval_, &id, &regs.sp[n])) \
                goto error;                                                   \
        }                                                                     \
    JS_END_MACRO

#define TRY_BRANCH_AFTER_COND(cond,spdec)                                     \
    JS_BEGIN_MACRO                                                            \
        JS_ASSERT(js_CodeSpec[op].length == 1);                               \
        uintN diff_ = (uintN) regs.pc[1] - (uintN) JSOP_IFEQ;                 \
        if (diff_ <= 1) {                                                     \
            regs.sp -= spdec;                                                 \
            if (cond == (diff_ != 0)) {                                       \
                ++regs.pc;                                                    \
                len = GET_JUMP_OFFSET(regs.pc);                               \
                BRANCH(len);                                                  \
            }                                                                 \
            len = 1 + JSOP_IFEQ_LENGTH;                                       \
            DO_NEXT_OP(len);                                                  \
        }                                                                     \
    JS_END_MACRO

BEGIN_CASE(JSOP_IN)
{
    const Value &rref = regs.sp[-1];
    if (!rref.isObject()) {
        js_ReportValueError(cx, JSMSG_IN_NOT_OBJECT, -1, rref, NULL);
        goto error;
    }
    JSObject *obj = &rref.toObject();
    jsid id;
    FETCH_ELEMENT_ID(obj, -2, id);
    JSObject *obj2;
    JSProperty *prop;
    if (!obj->lookupProperty(cx, id, &obj2, &prop))
        goto error;
    bool cond = prop != NULL;
    if (prop)
        obj2->dropProperty(cx, prop);
    TRY_BRANCH_AFTER_COND(cond, 2);
    regs.sp--;
    regs.sp[-1].setBoolean(cond);
}
END_CASE(JSOP_IN)

BEGIN_CASE(JSOP_ITER)
{
    JS_ASSERT(regs.sp > fp->base());
    uintN flags = regs.pc[1];
    if (!js_ValueToIterator(cx, flags, &regs.sp[-1]))
        goto error;
    CHECK_INTERRUPT_HANDLER();
    JS_ASSERT(!regs.sp[-1].isPrimitive());
}
END_CASE(JSOP_ITER)

BEGIN_CASE(JSOP_MOREITER)
{
    JS_ASSERT(regs.sp - 1 >= fp->base());
    JS_ASSERT(regs.sp[-1].isObject());
    PUSH_NULL();
    bool cond;
    if (!IteratorMore(cx, &regs.sp[-2].toObject(), &cond, &regs.sp[-1]))
        goto error;
    CHECK_INTERRUPT_HANDLER();
    TRY_BRANCH_AFTER_COND(cond, 1);
    JS_ASSERT(regs.pc[1] == JSOP_IFNEX);
    regs.sp[-1].setBoolean(cond);
}
END_CASE(JSOP_MOREITER)

BEGIN_CASE(JSOP_ENDITER)
{
    JS_ASSERT(regs.sp - 1 >= fp->base());
    bool ok = !!js_CloseIterator(cx, &regs.sp[-1].toObject());
    regs.sp--;
    if (!ok)
        goto error;
}
END_CASE(JSOP_ENDITER)

BEGIN_CASE(JSOP_FORARG)
{
    JS_ASSERT(regs.sp - 1 >= fp->base());
    uintN slot = GET_ARGNO(regs.pc);
    JS_ASSERT(slot < fp->fun->nargs);
    JS_ASSERT(regs.sp[-1].isObject());
    if (!IteratorNext(cx, &regs.sp[-1].toObject(), &fp->argv[slot]))
        goto error;
}
END_CASE(JSOP_FORARG)

BEGIN_CASE(JSOP_FORLOCAL)
{
    JS_ASSERT(regs.sp - 1 >= fp->base());
    uintN slot = GET_SLOTNO(regs.pc);
    JS_ASSERT(slot < fp->script->nslots);
    JS_ASSERT(regs.sp[-1].isObject());
    if (!IteratorNext(cx, &regs.sp[-1].toObject(), &fp->slots()[slot]))
        goto error;
}
END_CASE(JSOP_FORLOCAL)

BEGIN_CASE(JSOP_FORNAME)
{
    JS_ASSERT(regs.sp - 1 >= fp->base());
    JSAtom *atom;
    LOAD_ATOM(0, atom);
    jsid id = ATOM_TO_JSID(atom);
    JSObject *obj, *obj2;
    JSProperty *prop;
    if (!js_FindProperty(cx, id, &obj, &obj2, &prop))
        goto error;
    if (prop)
        obj2->dropProperty(cx, prop);
    {
        AutoValueRooter tvr(cx);
        JS_ASSERT(regs.sp[-1].isObject());
        if (!IteratorNext(cx, &regs.sp[-1].toObject(), tvr.addr()))
            goto error;
        if (!obj->setProperty(cx, id, tvr.addr()))
            goto error;
    }
}
END_CASE(JSOP_FORNAME)

BEGIN_CASE(JSOP_FORPROP)
{
    JS_ASSERT(regs.sp - 2 >= fp->base());
    JSAtom *atom;
    LOAD_ATOM(0, atom);
    jsid id = ATOM_TO_JSID(atom);
    JSObject *obj;
    FETCH_OBJECT(cx, -1, obj);
    {
        AutoValueRooter tvr(cx);
        JS_ASSERT(regs.sp[-2].isObject());
        if (!IteratorNext(cx, &regs.sp[-2].toObject(), tvr.addr()))
            goto error;
        if (!obj->setProperty(cx, id, tvr.addr()))
            goto error;
    }
    regs.sp--;
}
END_CASE(JSOP_FORPROP)

BEGIN_CASE(JSOP_FORELEM)
    /*
     * JSOP_FORELEM simply dups the property identifier at top of stack and
     * lets the subsequent JSOP_ENUMELEM opcode sequence handle the left-hand
     * side expression evaluation and assignment. This opcode exists solely to
     * help the decompiler.
     */
    JS_ASSERT(regs.sp - 1 >= fp->base());
    JS_ASSERT(regs.sp[-1].isObject());
    PUSH_NULL();
    if (!IteratorNext(cx, &regs.sp[-2].toObject(), &regs.sp[-1]))
        goto error;
END_CASE(JSOP_FORELEM)

BEGIN_CASE(JSOP_DUP)
{
    JS_ASSERT(regs.sp > fp->base());
    const Value &rref = regs.sp[-1];
    PUSH_COPY(rref);
}
END_CASE(JSOP_DUP)

BEGIN_CASE(JSOP_DUP2)
{
    JS_ASSERT(regs.sp - 2 >= fp->base());
    const Value &lref = regs.sp[-2];
    const Value &rref = regs.sp[-1];
    PUSH_COPY(lref);
    PUSH_COPY(rref);
}
END_CASE(JSOP_DUP2)

BEGIN_CASE(JSOP_SWAP)
{
    JS_ASSERT(regs.sp - 2 >= fp->base());
    Value &lref = regs.sp[-2];
    Value &rref = regs.sp[-1];
    lref.swap(rref);
}
END_CASE(JSOP_SWAP)

BEGIN_CASE(JSOP_PICK)
{
    jsint i = regs.pc[1];
    JS_ASSERT(regs.sp - (i+1) >= fp->base());
    Value lval = regs.sp[-(i+1)];
    memmove(regs.sp - (i+1), regs.sp - i, sizeof(Value)*i);
    regs.sp[-1] = lval;
}
END_CASE(JSOP_PICK)

#define NATIVE_GET(cx,obj,pobj,sprop,getHow,vp)                               \
    JS_BEGIN_MACRO                                                            \
        if (sprop->hasDefaultGetter()) {                                      \
            /* Fast path for Object instance properties. */                   \
            JS_ASSERT((sprop)->slot != SPROP_INVALID_SLOT ||                  \
                      !sprop->hasDefaultSetter());                            \
            if (((sprop)->slot != SPROP_INVALID_SLOT))                        \
                *(vp) = (pobj)->lockedGetSlot((sprop)->slot);                 \
            else                                                              \
                (vp)->setUndefined();                                         \
        } else {                                                              \
            if (!js_NativeGet(cx, obj, pobj, sprop, getHow, vp))              \
                goto error;                                                   \
        }                                                                     \
    JS_END_MACRO

#define NATIVE_SET(cx,obj,sprop,entry,vp)                                     \
    JS_BEGIN_MACRO                                                            \
        TRACE_2(SetPropHit, entry, sprop);                                    \
        if (sprop->hasDefaultSetter() &&                                      \
            (sprop)->slot != SPROP_INVALID_SLOT &&                            \
            !(obj)->scope()->brandedOrHasMethodBarrier()) {                   \
            /* Fast path for, e.g., plain Object instance properties. */      \
            (obj)->lockedSetSlot((sprop)->slot, *vp);                         \
        } else {                                                              \
            if (!js_NativeSet(cx, obj, sprop, false, vp))                     \
                goto error;                                                   \
        }                                                                     \
    JS_END_MACRO

/*
 * Skip the JSOP_POP typically found after a JSOP_SET* opcode, where oplen is
 * the constant length of the SET opcode sequence, and spdec is the constant
 * by which to decrease the stack pointer to pop all of the SET op's operands.
 *
 * NB: unlike macros that could conceivably be replaced by functions (ignoring
 * goto error), where a call should not have to be braced in order to expand
 * correctly (e.g., in if (cond) FOO(); else BAR()), these three macros lack
 * JS_{BEGIN,END}_MACRO brackets. They are also indented so as to align with
 * nearby opcode code.
 */
#define SKIP_POP_AFTER_SET(oplen,spdec)                                       \
            if (regs.pc[oplen] == JSOP_POP) {                                 \
                regs.sp -= spdec;                                             \
                regs.pc += oplen + JSOP_POP_LENGTH;                           \
                op = (JSOp) *regs.pc;                                         \
                DO_OP();                                                      \
            }

#define END_SET_CASE(OP)                                                      \
            SKIP_POP_AFTER_SET(OP##_LENGTH, 1);                               \
          END_CASE(OP)

#define END_SET_CASE_STORE_RVAL(OP,spdec)                                     \
            SKIP_POP_AFTER_SET(OP##_LENGTH, spdec);                           \
            {                                                                 \
                Value *newsp = regs.sp - ((spdec) - 1);                       \
                newsp[-1] = regs.sp[-1];                                      \
                regs.sp = newsp;                                              \
            }                                                                 \
          END_CASE(OP)

BEGIN_CASE(JSOP_SETCONST)
{
    JSAtom *atom;
    LOAD_ATOM(0, atom);
    JSObject *obj = fp->varobj(cx);
    const Value &ref = regs.sp[-1];
    if (!obj->defineProperty(cx, ATOM_TO_JSID(atom), ref,
                             PropertyStub, PropertyStub,
                             JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY)) {
        goto error;
    }
}
END_SET_CASE(JSOP_SETCONST);

#if JS_HAS_DESTRUCTURING
BEGIN_CASE(JSOP_ENUMCONSTELEM)
{
    const Value &ref = regs.sp[-3];
    JSObject *obj;
    FETCH_OBJECT(cx, -2, obj);
    jsid id;
    FETCH_ELEMENT_ID(obj, -1, id);
    if (!obj->defineProperty(cx, id, ref,
                             PropertyStub, PropertyStub,
                             JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY)) {
        goto error;
    }
    regs.sp -= 3;
}
END_CASE(JSOP_ENUMCONSTELEM)
#endif

BEGIN_CASE(JSOP_BINDNAME)
{
    JSObject *obj;
    do {
        /*
         * We can skip the property lookup for the global object. If the
         * property does not exist anywhere on the scope chain, JSOP_SETNAME
         * adds the property to the global.
         *
         * As a consequence of this optimization for the global object we run
         * its JSRESOLVE_ASSIGNING-tolerant resolve hooks only in JSOP_SETNAME,
         * after the interpreter evaluates the right- hand-side of the
         * assignment, and not here.
         *
         * This should be transparent to the hooks because the script, instead
         * of name = rhs, could have used global.name = rhs given a global
         * object reference, which also calls the hooks only after evaluating
         * the rhs. We desire such resolve hook equivalence between the two
         * forms.
         */
        obj = fp->scopeChain;
        if (!obj->getParent())
            break;

        PropertyCacheEntry *entry;
        JSObject *obj2;
        JSAtom *atom;
        JS_PROPERTY_CACHE(cx).test(cx, regs.pc, obj, obj2, entry, atom);
        if (!atom) {
            ASSERT_VALID_PROPERTY_CACHE_HIT(0, obj, obj2, entry);
            break;
        }

        jsid id = ATOM_TO_JSID(atom);
        obj = js_FindIdentifierBase(cx, fp->scopeChain, id);
        if (!obj)
            goto error;
    } while (0);
    PUSH_OBJECT(*obj);
}
END_CASE(JSOP_BINDNAME)

BEGIN_CASE(JSOP_IMACOP)
    JS_ASSERT(JS_UPTRDIFF(fp->imacpc, script->code) < script->length);
    op = JSOp(*fp->imacpc);
    DO_OP();

#define BITWISE_OP(OP)                                                        \
    JS_BEGIN_MACRO                                                            \
        int32_t i, j;                                                         \
        if (!ValueToECMAInt32(cx, regs.sp[-2], &i))                           \
            goto error;                                                       \
        if (!ValueToECMAInt32(cx, regs.sp[-1], &j))                           \
            goto error;                                                       \
        i = i OP j;                                                           \
        regs.sp--;                                                            \
        regs.sp[-1].setInt32(i);                                              \
    JS_END_MACRO

BEGIN_CASE(JSOP_BITOR)
    BITWISE_OP(|);
END_CASE(JSOP_BITOR)

BEGIN_CASE(JSOP_BITXOR)
    BITWISE_OP(^);
END_CASE(JSOP_BITXOR)

BEGIN_CASE(JSOP_BITAND)
    BITWISE_OP(&);
END_CASE(JSOP_BITAND)

#undef BITWISE_OP

/*
 * NB: These macros can't use JS_BEGIN_MACRO/JS_END_MACRO around their bodies
 * because they begin if/else chains, so callers must not put semicolons after
 * the call expressions!
 */
#if JS_HAS_XML_SUPPORT
#define XML_EQUALITY_OP(OP)                                                   \
    if ((lval.isObject() && lval.toObject().isXML()) ||                       \
        (rval.isObject() && rval.toObject().isXML())) {                       \
        if (!js_TestXMLEquality(cx, lval, rval, &cond))                       \
            goto error;                                                       \
        cond = cond OP JS_TRUE;                                               \
    } else

#define EXTENDED_EQUALITY_OP(OP)                                              \
    if (((clasp = l->getClass())->flags & JSCLASS_IS_EXTENDED) &&             \
        ((ExtendedClass *)clasp)->equality) {                                 \
        if (!((ExtendedClass *)clasp)->equality(cx, l, &rval, &cond))         \
            goto error;                                                       \
        cond = cond OP JS_TRUE;                                               \
    } else
#else
#define XML_EQUALITY_OP(OP)             /* nothing */
#define EXTENDED_EQUALITY_OP(OP)        /* nothing */
#endif

#define EQUALITY_OP(OP, IFNAN)                                                \
    JS_BEGIN_MACRO                                                            \
        Class *clasp;                                                         \
        JSBool cond;                                                          \
        Value rval = regs.sp[-1];                                             \
        Value lval = regs.sp[-2];                                             \
        XML_EQUALITY_OP(OP)                                                   \
        if (SameType(lval, rval)) {                                           \
            if (lval.isString()) {                                            \
                JSString *l = lval.toString(), *r = rval.toString();          \
                cond = js_EqualStrings(l, r) OP JS_TRUE;                      \
            } else if (lval.isDouble()) {                                     \
                double l = lval.toDouble(), r = rval.toDouble();              \
                cond = JSDOUBLE_COMPARE(l, OP, r, IFNAN);                     \
            } else if (lval.isObject()) {                                     \
                JSObject *l = &lval.toObject(), *r = &rval.toObject();        \
                EXTENDED_EQUALITY_OP(OP)                                      \
                cond = l OP r;                                                \
            } else {                                                          \
                cond = lval.payloadAsRawUint32() OP rval.payloadAsRawUint32();\
            }                                                                 \
        } else {                                                              \
            if (lval.isNullOrUndefined()) {                                   \
                cond = rval.isNullOrUndefined() OP true;                      \
            } else if (rval.isNullOrUndefined()) {                            \
                cond = true OP false;                                         \
            } else {                                                          \
                if (lval.isObject())                                          \
                    DEFAULT_VALUE(cx, -2, JSTYPE_VOID, lval);                 \
                if (rval.isObject())                                          \
                    DEFAULT_VALUE(cx, -1, JSTYPE_VOID, rval);                 \
                if (lval.isString() && rval.isString()) {                     \
                    JSString *l = lval.toString(), *r = rval.toString();      \
                    cond = js_EqualStrings(l, r) OP JS_TRUE;                  \
                } else {                                                      \
                    double l, r;                                              \
                    if (!ValueToNumber(cx, lval, &l) ||                       \
                        !ValueToNumber(cx, rval, &r)) {                       \
                        goto error;                                           \
                    }                                                         \
                    cond = JSDOUBLE_COMPARE(l, OP, r, IFNAN);                 \
                }                                                             \
            }                                                                 \
        }                                                                     \
        TRY_BRANCH_AFTER_COND(cond, 2);                                       \
        regs.sp--;                                                            \
        regs.sp[-1].setBoolean(cond);                                         \
    JS_END_MACRO

BEGIN_CASE(JSOP_EQ)
    EQUALITY_OP(==, false);
END_CASE(JSOP_EQ)

BEGIN_CASE(JSOP_NE)
    EQUALITY_OP(!=, true);
END_CASE(JSOP_NE)

#undef EQUALITY_OP
#undef XML_EQUALITY_OP
#undef EXTENDED_EQUALITY_OP

#define STRICT_EQUALITY_OP(OP, COND)                                          \
    JS_BEGIN_MACRO                                                            \
        const Value &rref = regs.sp[-1];                                      \
        const Value &lref = regs.sp[-2];                                      \
        COND = StrictlyEqual(cx, lref, rref) OP true;                         \
        regs.sp--;                                                            \
    JS_END_MACRO

BEGIN_CASE(JSOP_STRICTEQ)
{
    bool cond;
    STRICT_EQUALITY_OP(==, cond);
    regs.sp[-1].setBoolean(cond);
}
END_CASE(JSOP_STRICTEQ)

BEGIN_CASE(JSOP_STRICTNE)
{
    bool cond;
    STRICT_EQUALITY_OP(!=, cond);
    regs.sp[-1].setBoolean(cond);
}
END_CASE(JSOP_STRICTNE)

BEGIN_CASE(JSOP_CASE)
{
    bool cond;
    STRICT_EQUALITY_OP(==, cond);
    if (cond) {
        regs.sp--;
        len = GET_JUMP_OFFSET(regs.pc);
        BRANCH(len);
    }
}
END_CASE(JSOP_CASE)

BEGIN_CASE(JSOP_CASEX)
{
    bool cond;
    STRICT_EQUALITY_OP(==, cond);
    if (cond) {
        regs.sp--;
        len = GET_JUMPX_OFFSET(regs.pc);
        BRANCH(len);
    }
}
END_CASE(JSOP_CASEX)

#undef STRICT_EQUALITY_OP

#define RELATIONAL_OP(OP)                                                     \
    JS_BEGIN_MACRO                                                            \
        Value rval = regs.sp[-1];                                             \
        Value lval = regs.sp[-2];                                             \
        bool cond;                                                            \
        /* Optimize for two int-tagged operands (typical loop control). */    \
        if (lval.isInt32() && rval.isInt32()) {                               \
            cond = lval.toInt32() OP rval.toInt32();                          \
        } else {                                                              \
            if (lval.isObject())                                              \
                DEFAULT_VALUE(cx, -2, JSTYPE_NUMBER, lval);                   \
            if (rval.isObject())                                              \
                DEFAULT_VALUE(cx, -1, JSTYPE_NUMBER, rval);                   \
            if (lval.isString() && rval.isString()) {                         \
                JSString *l = lval.toString(), *r = rval.toString();          \
                cond = js_CompareStrings(l, r) OP 0;                          \
            } else {                                                          \
                double l, r;                                                  \
                if (!ValueToNumber(cx, lval, &l) ||                           \
                    !ValueToNumber(cx, rval, &r)) {                           \
                    goto error;                                               \
                }                                                             \
                cond = JSDOUBLE_COMPARE(l, OP, r, false);                     \
            }                                                                 \
        }                                                                     \
        TRY_BRANCH_AFTER_COND(cond, 2);                                       \
        regs.sp--;                                                            \
        regs.sp[-1].setBoolean(cond);                                         \
    JS_END_MACRO

BEGIN_CASE(JSOP_LT)
    RELATIONAL_OP(<);
END_CASE(JSOP_LT)

BEGIN_CASE(JSOP_LE)
    RELATIONAL_OP(<=);
END_CASE(JSOP_LE)

BEGIN_CASE(JSOP_GT)
    RELATIONAL_OP(>);
END_CASE(JSOP_GT)

BEGIN_CASE(JSOP_GE)
    RELATIONAL_OP(>=);
END_CASE(JSOP_GE)

#undef RELATIONAL_OP

#define SIGNED_SHIFT_OP(OP)                                                   \
    JS_BEGIN_MACRO                                                            \
        int32_t i, j;                                                         \
        if (!ValueToECMAInt32(cx, regs.sp[-2], &i))                           \
            goto error;                                                       \
        if (!ValueToECMAInt32(cx, regs.sp[-1], &j))                           \
            goto error;                                                       \
        i = i OP (j & 31);                                                    \
        regs.sp--;                                                            \
        regs.sp[-1].setInt32(i);                                              \
    JS_END_MACRO

BEGIN_CASE(JSOP_LSH)
    SIGNED_SHIFT_OP(<<);
END_CASE(JSOP_LSH)

BEGIN_CASE(JSOP_RSH)
    SIGNED_SHIFT_OP(>>);
END_CASE(JSOP_RSH)

#undef SIGNED_SHIFT_OP

BEGIN_CASE(JSOP_URSH)
{
    uint32_t u;
    if (!ValueToECMAUint32(cx, regs.sp[-2], &u))
        goto error;
    int32_t j;
    if (!ValueToECMAInt32(cx, regs.sp[-1], &j))
        goto error;

    u >>= (j & 31);

    regs.sp--;
	regs.sp[-1].setNumber(uint32(u));
}
END_CASE(JSOP_URSH)

BEGIN_CASE(JSOP_ADD)
{
    Value rval = regs.sp[-1];
    Value lval = regs.sp[-2];

    if (lval.isInt32() && rval.isInt32()) {
        int32_t l = lval.toInt32(), r = rval.toInt32();
        int32_t sum = l + r;
        regs.sp--;
        if (JS_UNLIKELY(bool((l ^ sum) & (r ^ sum) & 0x80000000)))
            regs.sp[-1].setDouble(double(l) + double(r));
        else
            regs.sp[-1].setInt32(sum);
    } else
#if JS_HAS_XML_SUPPORT
    if (IsXML(lval) && IsXML(rval)) {
        if (!js_ConcatenateXML(cx, &lval.toObject(), &rval.toObject(), &rval))
            goto error;
        regs.sp--;
        regs.sp[-1] = rval;
    } else
#endif
    {
        if (lval.isObject())
            DEFAULT_VALUE(cx, -2, JSTYPE_VOID, lval);
        if (rval.isObject())
            DEFAULT_VALUE(cx, -1, JSTYPE_VOID, rval);
        bool lIsString, rIsString;
        if ((lIsString = lval.isString()) | (rIsString = rval.isString())) {
            JSString *lstr, *rstr;
            if (lIsString) {
                lstr = lval.toString();
            } else {
                lstr = js_ValueToString(cx, lval);
                if (!lstr)
                    goto error;
                regs.sp[-2].setString(lstr);
            }
            if (rIsString) {
                rstr = rval.toString();
            } else {
                rstr = js_ValueToString(cx, rval);
                if (!rstr)
                    goto error;
                regs.sp[-1].setString(rstr);
            }
            JSString *str = js_ConcatStrings(cx, lstr, rstr);
            if (!str)
                goto error;
            regs.sp--;
            regs.sp[-1].setString(str);
        } else {
            double l, r;
            if (!ValueToNumber(cx, lval, &l) || !ValueToNumber(cx, rval, &r))
                goto error;
            l += r;
            regs.sp--;
            regs.sp[-1].setNumber(l);
        }
    }
}
END_CASE(JSOP_ADD)

BEGIN_CASE(JSOP_OBJTOSTR)
{
    const Value &ref = regs.sp[-1];
    if (ref.isObject()) {
        JSString *str = js_ValueToString(cx, ref);
        if (!str)
            goto error;
        regs.sp[-1].setString(str);
    }
}
END_CASE(JSOP_OBJTOSTR)

BEGIN_CASE(JSOP_CONCATN)
{
    JSCharBuffer buf(cx);
    uintN argc = GET_ARGC(regs.pc);
    for (Value *vp = regs.sp - argc; vp < regs.sp; vp++) {
        JS_ASSERT(vp->isPrimitive());
        if (!js_ValueToCharBuffer(cx, *vp, buf))
            goto error;
    }
    JSString *str = js_NewStringFromCharBuffer(cx, buf);
    if (!str)
        goto error;
    regs.sp -= argc - 1;
    regs.sp[-1].setString(str);
}
END_CASE(JSOP_CONCATN)

#define BINARY_OP(OP)                                                         \
    JS_BEGIN_MACRO                                                            \
        double d1, d2;                                                        \
        if (!ValueToNumber(cx, regs.sp[-2], &d1) ||                           \
            !ValueToNumber(cx, regs.sp[-1], &d2)) {                           \
            goto error;                                                       \
        }                                                                     \
        double d = d1 OP d2;                                                  \
        regs.sp--;                                                            \
        regs.sp[-1].setNumber(d);                                             \
    JS_END_MACRO

BEGIN_CASE(JSOP_SUB)
    BINARY_OP(-);
END_CASE(JSOP_SUB)

BEGIN_CASE(JSOP_MUL)
    BINARY_OP(*);
END_CASE(JSOP_MUL)

#undef BINARY_OP

BEGIN_CASE(JSOP_DIV)
{
    double d1, d2;
    if (!ValueToNumber(cx, regs.sp[-2], &d1) ||
        !ValueToNumber(cx, regs.sp[-1], &d2)) {
        goto error;
    }
    regs.sp--;
    if (d2 == 0) {
        const Value *vp;
#ifdef XP_WIN
        /* XXX MSVC miscompiles such that (NaN == 0) */
        if (JSDOUBLE_IS_NaN(d2))
            vp = &rt->NaNValue;
        else
#endif
        if (d1 == 0 || JSDOUBLE_IS_NaN(d1))
            vp = &rt->NaNValue;
        else if (JSDOUBLE_IS_NEG(d1) != JSDOUBLE_IS_NEG(d2))
            vp = &rt->negativeInfinityValue;
        else
            vp = &rt->positiveInfinityValue;
        regs.sp[-1] = *vp;
    } else {
        d1 /= d2;
        regs.sp[-1].setNumber(d1);
    }
}
END_CASE(JSOP_DIV)

BEGIN_CASE(JSOP_MOD)
{
    Value &lref = regs.sp[-2];
    Value &rref = regs.sp[-1];
    int32_t l, r;
    if (lref.isInt32() && rref.isInt32() &&
        (l = lref.toInt32()) >= 0 && (r = rref.toInt32()) > 0) {
        int32_t mod = l % r;
        regs.sp--;
        regs.sp[-1].setInt32(mod);
    } else {
        double d1, d2;
        if (!ValueToNumber(cx, regs.sp[-2], &d1) ||
            !ValueToNumber(cx, regs.sp[-1], &d2)) {
            goto error;
        }
        regs.sp--;
        if (d2 == 0) {
            regs.sp[-1].setDouble(js_NaN);
        } else {
            d1 = js_fmod(d1, d2);
            regs.sp[-1].setDouble(d1);
        }
    }
}
END_CASE(JSOP_MOD)

BEGIN_CASE(JSOP_NOT)
{
    Value *_;
    bool cond;
    POP_BOOLEAN(cx, _, cond);
    PUSH_BOOLEAN(!cond);
}
END_CASE(JSOP_NOT)

BEGIN_CASE(JSOP_BITNOT)
{
    int32_t i;
    if (!ValueToECMAInt32(cx, regs.sp[-1], &i))
        goto error;
    i = ~i;
    regs.sp[-1].setInt32(i);
}
END_CASE(JSOP_BITNOT)

BEGIN_CASE(JSOP_NEG)
{
    /*
     * When the operand is int jsval, INT32_FITS_IN_JSVAL(i) implies
     * INT32_FITS_IN_JSVAL(-i) unless i is 0 or INT32_MIN when the
     * results, -0.0 or INT32_MAX + 1, are jsdouble values.
     */
    const Value &ref = regs.sp[-1];
    int32_t i;
    if (ref.isInt32() && (i = ref.toInt32()) != 0 && i != INT32_MIN) {
        i = -i;
        regs.sp[-1].setInt32(i);
    } else {
        double d;
        if (!ValueToNumber(cx, regs.sp[-1], &d))
            goto error;
        d = -d;
        regs.sp[-1].setDouble(d);
    }
}
END_CASE(JSOP_NEG)

BEGIN_CASE(JSOP_POS)
    if (!ValueToNumber(cx, &regs.sp[-1]))
        goto error;
END_CASE(JSOP_POS)

BEGIN_CASE(JSOP_DELNAME)
{
    JSAtom *atom;
    LOAD_ATOM(0, atom);
    jsid id = ATOM_TO_JSID(atom);
    JSObject *obj, *obj2;
    JSProperty *prop;
    if (!js_FindProperty(cx, id, &obj, &obj2, &prop))
        goto error;

    /* ECMA says to return true if name is undefined or inherited. */
    PUSH_BOOLEAN(true);
    if (prop) {
        obj2->dropProperty(cx, prop);
        if (!obj->deleteProperty(cx, id, &regs.sp[-1]))
            goto error;
    }
}
END_CASE(JSOP_DELNAME)

BEGIN_CASE(JSOP_DELPROP)
{
    JSAtom *atom;
    LOAD_ATOM(0, atom);
    jsid id = ATOM_TO_JSID(atom);

    JSObject *obj;
    FETCH_OBJECT(cx, -1, obj);

    Value rval;
    if (!obj->deleteProperty(cx, id, &rval))
        goto error;

    regs.sp[-1] = rval;
}
END_CASE(JSOP_DELPROP)

BEGIN_CASE(JSOP_DELELEM)
{
    /* Fetch the left part and resolve it to a non-null object. */
    JSObject *obj;
    FETCH_OBJECT(cx, -2, obj);

    /* Fetch index and convert it to id suitable for use with obj. */
    jsid id;
    FETCH_ELEMENT_ID(obj, -1, id);

    /* Get or set the element. */
    if (!obj->deleteProperty(cx, id, &regs.sp[-2]))
        goto error;

    regs.sp--;
}
END_CASE(JSOP_DELELEM)

BEGIN_CASE(JSOP_TYPEOFEXPR)
BEGIN_CASE(JSOP_TYPEOF)
{
    const Value &ref = regs.sp[-1];
    JSType type = JS_TypeOfValue(cx, Jsvalify(ref));
    JSAtom *atom = rt->atomState.typeAtoms[type];
    regs.sp[-1].setString(ATOM_TO_STRING(atom));
}
END_CASE(JSOP_TYPEOF)

BEGIN_CASE(JSOP_VOID)
    regs.sp[-1].setUndefined();
END_CASE(JSOP_VOID)

{
    JSObject *obj;
    JSAtom *atom;
    jsid id;
    jsint i;

BEGIN_CASE(JSOP_INCELEM)
BEGIN_CASE(JSOP_DECELEM)
BEGIN_CASE(JSOP_ELEMINC)
BEGIN_CASE(JSOP_ELEMDEC)

    /*
     * Delay fetching of id until we have the object to ensure the proper
     * evaluation order. See bug 372331.
     */
    id = JSID_VOID;
    i = -2;
    goto fetch_incop_obj;

BEGIN_CASE(JSOP_INCPROP)
BEGIN_CASE(JSOP_DECPROP)
BEGIN_CASE(JSOP_PROPINC)
BEGIN_CASE(JSOP_PROPDEC)
    LOAD_ATOM(0, atom);
    id = ATOM_TO_JSID(atom);
    i = -1;

  fetch_incop_obj:
    FETCH_OBJECT(cx, i, obj);
    if (JSID_IS_VOID(id))
        FETCH_ELEMENT_ID(obj, -1, id);
    goto do_incop;

BEGIN_CASE(JSOP_INCNAME)
BEGIN_CASE(JSOP_DECNAME)
BEGIN_CASE(JSOP_NAMEINC)
BEGIN_CASE(JSOP_NAMEDEC)
{
    obj = fp->scopeChain;

    JSObject *obj2;
    PropertyCacheEntry *entry;
    JS_PROPERTY_CACHE(cx).test(cx, regs.pc, obj, obj2, entry, atom);
    if (!atom) {
        ASSERT_VALID_PROPERTY_CACHE_HIT(0, obj, obj2, entry);
        if (obj == obj2 && entry->vword.isSlot()) {
            uint32 slot = entry->vword.toSlot();
            JS_ASSERT(slot < obj->scope()->freeslot);
            Value &rref = obj->getSlotRef(slot);
            int32_t tmp;
            if (JS_LIKELY(rref.isInt32() && CanIncDecWithoutOverflow(tmp = rref.toInt32()))) {
                int32_t inc = tmp + ((js_CodeSpec[op].format & JOF_INC) ? 1 : -1);
                if (!(js_CodeSpec[op].format & JOF_POST))
                    tmp = inc;
                rref.getInt32Ref() = inc;
                PUSH_INT32(tmp);
                len = JSOP_INCNAME_LENGTH;
                DO_NEXT_OP(len);
            }
        }
        LOAD_ATOM(0, atom);
    }

    id = ATOM_TO_JSID(atom);
    JSProperty *prop;
    if (!js_FindPropertyHelper(cx, id, true, &obj, &obj2, &prop))
        goto error;
    if (!prop) {
        atomNotDefined = atom;
        goto atom_not_defined;
    }
    obj2->dropProperty(cx, prop);
}

do_incop:
{
    /*
     * We need a root to store the value to leave on the stack until
     * we have done with obj->setProperty.
     */
    PUSH_NULL();
    if (!obj->getProperty(cx, id, &regs.sp[-1]))
        goto error;

    const JSCodeSpec *cs = &js_CodeSpec[op];
    JS_ASSERT(cs->ndefs == 1);
    JS_ASSERT((cs->format & JOF_TMPSLOT_MASK) == JOF_TMPSLOT2);
    Value &ref = regs.sp[-1];
    int32_t tmp;
    if (JS_LIKELY(ref.isInt32() && CanIncDecWithoutOverflow(tmp = ref.toInt32()))) {
        int incr = (cs->format & JOF_INC) ? 1 : -1;
        if (cs->format & JOF_POST)
            ref.getInt32Ref() = tmp + incr;
        else
            ref.getInt32Ref() = tmp += incr;
        fp->flags |= JSFRAME_ASSIGNING;
        JSBool ok = obj->setProperty(cx, id, &ref);
        fp->flags &= ~JSFRAME_ASSIGNING;
        if (!ok)
            goto error;

        /*
         * We must set regs.sp[-1] to tmp for both post and pre increments
         * as the setter overwrites regs.sp[-1].
         */
        ref.setInt32(tmp);
    } else {
        /* We need an extra root for the result. */
        PUSH_NULL();
        if (!js_DoIncDec(cx, cs, &regs.sp[-2], &regs.sp[-1]))
            goto error;
        fp->flags |= JSFRAME_ASSIGNING;
        JSBool ok = obj->setProperty(cx, id, &regs.sp[-1]);
        fp->flags &= ~JSFRAME_ASSIGNING;
        if (!ok)
            goto error;
        regs.sp--;
    }

    if (cs->nuses == 0) {
        /* regs.sp[-1] already contains the result of name increment. */
    } else {
        regs.sp[-1 - cs->nuses] = regs.sp[-1];
        regs.sp -= cs->nuses;
    }
    len = cs->length;
    DO_NEXT_OP(len);
}
}

{
    int incr, incr2;
    Value *vp;

    /* Position cases so the most frequent i++ does not need a jump. */
BEGIN_CASE(JSOP_DECARG)
    incr = -1; incr2 = -1; goto do_arg_incop;
BEGIN_CASE(JSOP_ARGDEC)
    incr = -1; incr2 =  0; goto do_arg_incop;
BEGIN_CASE(JSOP_INCARG)
    incr =  1; incr2 =  1; goto do_arg_incop;
BEGIN_CASE(JSOP_ARGINC)
    incr =  1; incr2 =  0;

  do_arg_incop:
    // If we initialize in the declaration, MSVC complains that the labels skip init.
    uint32 slot;
    slot = GET_ARGNO(regs.pc);
    JS_ASSERT(slot < fp->fun->nargs);
    METER_SLOT_OP(op, slot);
    vp = fp->argv + slot;
    goto do_int_fast_incop;

BEGIN_CASE(JSOP_DECLOCAL)
    incr = -1; incr2 = -1; goto do_local_incop;
BEGIN_CASE(JSOP_LOCALDEC)
    incr = -1; incr2 =  0; goto do_local_incop;
BEGIN_CASE(JSOP_INCLOCAL)
    incr =  1; incr2 =  1; goto do_local_incop;
BEGIN_CASE(JSOP_LOCALINC)
    incr =  1; incr2 =  0;

  /*
   * do_local_incop comes right before do_int_fast_incop as we want to
   * avoid an extra jump for variable cases as local++ is more frequent
   * than arg++.
   */
  do_local_incop:
    slot = GET_SLOTNO(regs.pc);
    JS_ASSERT(slot < fp->script->nslots);
    vp = fp->slots() + slot;
    METER_SLOT_OP(op, slot);
    vp = fp->slots() + slot;

  do_int_fast_incop:
    int32_t tmp;
    if (JS_LIKELY(vp->isInt32() && CanIncDecWithoutOverflow(tmp = vp->toInt32()))) {
        vp->getInt32Ref() = tmp + incr;
        JS_ASSERT(JSOP_INCARG_LENGTH == js_CodeSpec[op].length);
        SKIP_POP_AFTER_SET(JSOP_INCARG_LENGTH, 0);
        PUSH_INT32(tmp + incr2);
    } else {
        PUSH_COPY(*vp);
        if (!js_DoIncDec(cx, &js_CodeSpec[op], &regs.sp[-1], vp))
            goto error;
    }
    len = JSOP_INCARG_LENGTH;
    JS_ASSERT(len == js_CodeSpec[op].length);
    DO_NEXT_OP(len);
}

/* NB: This macro doesn't use JS_BEGIN_MACRO/JS_END_MACRO around its body. */
#define FAST_GLOBAL_INCREMENT_OP(SLOWOP,INCR,INCR2)                           \
    op2 = SLOWOP;                                                             \
    incr = INCR;                                                              \
    incr2 = INCR2;                                                            \
    goto do_global_incop

{
    JSOp op2;
    int incr, incr2;

BEGIN_CASE(JSOP_DECGVAR)
    FAST_GLOBAL_INCREMENT_OP(JSOP_DECNAME, -1, -1);
BEGIN_CASE(JSOP_GVARDEC)
    FAST_GLOBAL_INCREMENT_OP(JSOP_NAMEDEC, -1,  0);
BEGIN_CASE(JSOP_INCGVAR)
    FAST_GLOBAL_INCREMENT_OP(JSOP_INCNAME,  1,  1);
BEGIN_CASE(JSOP_GVARINC)
    FAST_GLOBAL_INCREMENT_OP(JSOP_NAMEINC,  1,  0);

#undef FAST_GLOBAL_INCREMENT_OP

  do_global_incop:
    JS_ASSERT((js_CodeSpec[op].format & JOF_TMPSLOT_MASK) ==
              JOF_TMPSLOT2);
    uint32 slot = GET_SLOTNO(regs.pc);
    JS_ASSERT(slot < GlobalVarCount(fp));
    METER_SLOT_OP(op, slot);
    const Value &lref = fp->slots()[slot];
    if (lref.isNull()) {
        op = op2;
        DO_OP();
    }
    slot = (uint32)lref.toInt32();
    JS_ASSERT(fp->varobj(cx) == cx->activeSegment()->getInitialVarObj());
    JSObject *varobj = cx->activeSegment()->getInitialVarObj();

    /* XXX all this code assumes that varobj is either a callobj or global and
     * that it cannot be accessed in a MT way. This is either true now or
     * coming soon. */

    Value &rref = varobj->getSlotRef(slot);
    int32_t tmp;
    if (JS_LIKELY(rref.isInt32() && CanIncDecWithoutOverflow(tmp = rref.toInt32()))) {
        PUSH_INT32(tmp + incr2);
        rref.getInt32Ref() = tmp + incr;
    } else {
        PUSH_COPY(rref);
        if (!js_DoIncDec(cx, &js_CodeSpec[op], &regs.sp[-1], &rref))
            goto error;
    }
    len = JSOP_INCGVAR_LENGTH;  /* all gvar incops are same length */
    JS_ASSERT(len == js_CodeSpec[op].length);
    DO_NEXT_OP(len);
}

BEGIN_CASE(JSOP_THIS)
    if (!fp->getThisObject(cx))
        goto error;
    PUSH_COPY(fp->thisv);
END_CASE(JSOP_THIS)

BEGIN_CASE(JSOP_UNBRANDTHIS)
{
    JSObject *obj = fp->getThisObject(cx);
    if (!obj)
        goto error;
    if (!obj->unbrand(cx))
        goto error;
}
END_CASE(JSOP_UNBRANDTHIS)

{
    JSObject *obj;
    Value *vp;
    jsint i;

BEGIN_CASE(JSOP_GETTHISPROP)
    obj = fp->getThisObject(cx);
    if (!obj)
        goto error;
    i = 0;
    PUSH_NULL();
    goto do_getprop_with_obj;

BEGIN_CASE(JSOP_GETARGPROP)
{
    i = ARGNO_LEN;
    uint32 slot = GET_ARGNO(regs.pc);
    JS_ASSERT(slot < fp->fun->nargs);
    PUSH_COPY(fp->argv[slot]);
    goto do_getprop_body;
}

BEGIN_CASE(JSOP_GETLOCALPROP)
{
    i = SLOTNO_LEN;
    uint32 slot = GET_SLOTNO(regs.pc);
    JS_ASSERT(slot < script->nslots);
    PUSH_COPY(fp->slots()[slot]);
    goto do_getprop_body;
}

BEGIN_CASE(JSOP_GETPROP)
BEGIN_CASE(JSOP_GETXPROP)
    i = 0;

  do_getprop_body:
    vp = &regs.sp[-1];

  do_getprop_with_lval:
    VALUE_TO_OBJECT(cx, vp, obj);

  do_getprop_with_obj:
    {
        Value rval;
        do {
            /*
             * We do not impose the method read barrier if in an imacro,
             * assuming any property gets it does (e.g., for 'toString'
             * from JSOP_NEW) will not be leaked to the calling script.
             */
            JSObject *aobj = js_GetProtoIfDenseArray(obj);

            PropertyCacheEntry *entry;
            JSObject *obj2;
            JSAtom *atom;
            JS_PROPERTY_CACHE(cx).test(cx, regs.pc, aobj, obj2, entry, atom);
            if (!atom) {
                ASSERT_VALID_PROPERTY_CACHE_HIT(i, aobj, obj2, entry);
                if (entry->vword.isFunObj()) {
                    rval.setObject(entry->vword.toFunObj());
                } else if (entry->vword.isSlot()) {
                    uint32 slot = entry->vword.toSlot();
                    JS_ASSERT(slot < obj2->scope()->freeslot);
                    rval = obj2->lockedGetSlot(slot);
                } else {
                    JS_ASSERT(entry->vword.isSprop());
                    JSScopeProperty *sprop = entry->vword.toSprop();
                    NATIVE_GET(cx, obj, obj2, sprop,
                               fp->imacpc ? JSGET_NO_METHOD_BARRIER : JSGET_METHOD_BARRIER,
                               &rval);
                }
                break;
            }

            jsid id = ATOM_TO_JSID(atom);
            if (JS_LIKELY(aobj->map->ops->getProperty == js_GetProperty)
                ? !js_GetPropertyHelper(cx, obj, id,
                                        fp->imacpc
                                        ? JSGET_CACHE_RESULT | JSGET_NO_METHOD_BARRIER
                                        : JSGET_CACHE_RESULT | JSGET_METHOD_BARRIER,
                                        &rval)
                : !obj->getProperty(cx, id, &rval)) {
                goto error;
            }
        } while (0);

        regs.sp[-1] = rval;
        JS_ASSERT(JSOP_GETPROP_LENGTH + i == js_CodeSpec[op].length);
        len = JSOP_GETPROP_LENGTH + i;
    }
END_VARLEN_CASE

BEGIN_CASE(JSOP_LENGTH)
    vp = &regs.sp[-1];
    if (vp->isString()) {
        vp->setInt32(vp->toString()->length());
    } else if (vp->isObject()) {
        JSObject *obj = &vp->toObject();
        if (obj->isArray()) {
            jsuint length = obj->getArrayLength();
            regs.sp[-1].setNumber(length);
        } else if (obj->isArguments() && !obj->isArgsLengthOverridden()) {
            uint32 length = obj->getArgsLength();
            JS_ASSERT(length < INT32_MAX);
            regs.sp[-1].setInt32(int32_t(length));
        } else {
            i = -2;
            goto do_getprop_with_lval;
        }
    } else {
        i = -2;
        goto do_getprop_with_lval;
    }
END_CASE(JSOP_LENGTH)

}

BEGIN_CASE(JSOP_CALLPROP)
{
    Value lval = regs.sp[-1];

    Value objv;
    if (lval.isObject()) {
        objv = lval;
    } else {
        JSProtoKey protoKey;
        if (lval.isString()) {
            protoKey = JSProto_String;
        } else if (lval.isNumber()) {
            protoKey = JSProto_Number;
        } else if (lval.isBoolean()) {
            protoKey = JSProto_Boolean;
        } else {
            JS_ASSERT(lval.isNull() || lval.isUndefined());
            js_ReportIsNullOrUndefined(cx, -1, lval, NULL);
            goto error;
        }
        JSObject *pobj;
        if (!js_GetClassPrototype(cx, NULL, protoKey, &pobj))
            goto error;
        objv.setObject(*pobj);
    }

    JSObject *aobj = js_GetProtoIfDenseArray(&objv.toObject());
    Value rval;

    PropertyCacheEntry *entry;
    JSObject *obj2;
    JSAtom *atom;
    JS_PROPERTY_CACHE(cx).test(cx, regs.pc, aobj, obj2, entry, atom);
    if (!atom) {
        ASSERT_VALID_PROPERTY_CACHE_HIT(0, aobj, obj2, entry);
        if (entry->vword.isFunObj()) {
            rval.setObject(entry->vword.toFunObj());
        } else if (entry->vword.isSlot()) {
            uint32 slot = entry->vword.toSlot();
            JS_ASSERT(slot < obj2->scope()->freeslot);
            rval = obj2->lockedGetSlot(slot);
        } else {
            JS_ASSERT(entry->vword.isSprop());
            JSScopeProperty *sprop = entry->vword.toSprop();
            NATIVE_GET(cx, &objv.toObject(), obj2, sprop, JSGET_NO_METHOD_BARRIER, &rval);
        }
        regs.sp[-1] = rval;
        PUSH_COPY(lval);
        goto end_callprop;
    }

    /*
     * Cache miss: use the immediate atom that was loaded for us under
     * PropertyCache::test.
     */
    jsid id;
    id = ATOM_TO_JSID(atom);

    PUSH_NULL();
    if (lval.isObject()) {
        if (!js_GetMethod(cx, &objv.toObject(), id,
                          JS_LIKELY(aobj->map->ops->getProperty == js_GetProperty)
                          ? JSGET_CACHE_RESULT | JSGET_NO_METHOD_BARRIER
                          : JSGET_NO_METHOD_BARRIER,
                          &rval)) {
            goto error;
        }
        regs.sp[-1] = objv;
        regs.sp[-2] = rval;
    } else {
        JS_ASSERT(objv.toObject().map->ops->getProperty == js_GetProperty);
        if (!js_GetPropertyHelper(cx, &objv.toObject(), id,
                                  JSGET_CACHE_RESULT | JSGET_NO_METHOD_BARRIER,
                                  &rval)) {
            goto error;
        }
        regs.sp[-1] = lval;
        regs.sp[-2] = rval;
    }

  end_callprop:
    /* Wrap primitive lval in object clothing if necessary. */
    if (lval.isPrimitive()) {
        /* FIXME: https://bugzilla.mozilla.org/show_bug.cgi?id=412571 */
        JSObject *funobj;
        if (!IsFunctionObject(rval, &funobj) ||
            !PrimitiveThisTest(GET_FUNCTION_PRIVATE(cx, funobj), lval)) {
            if (!js_PrimitiveToObject(cx, &regs.sp[-1]))
                goto error;
        }
    }
#if JS_HAS_NO_SUCH_METHOD
    if (JS_UNLIKELY(rval.isUndefined())) {
        LOAD_ATOM(0, atom);
        regs.sp[-2].setString(ATOM_TO_STRING(atom));
        if (!js_OnUnknownMethod(cx, regs.sp - 2))
            goto error;
    }
#endif
}
END_CASE(JSOP_CALLPROP)

BEGIN_CASE(JSOP_UNBRAND)
    JS_ASSERT(regs.sp - fp->slots() >= 1);
    if (!regs.sp[-1].toObject().unbrand(cx))
        goto error;
END_CASE(JSOP_UNBRAND)

BEGIN_CASE(JSOP_SETNAME)
BEGIN_CASE(JSOP_SETPROP)
BEGIN_CASE(JSOP_SETMETHOD)
{
    Value &rref = regs.sp[-1];
    JS_ASSERT_IF(op == JSOP_SETMETHOD, IsFunctionObject(rref));
    Value &lref = regs.sp[-2];
    JS_ASSERT_IF(op == JSOP_SETNAME, lref.isObject());
    JSObject *obj;
    VALUE_TO_OBJECT(cx, &lref, obj);

    do {
        PropertyCache *cache = &JS_PROPERTY_CACHE(cx);

        /*
         * Probe the property cache, specializing for two important
         * set-property cases. First:
         *
         *   function f(a, b, c) {
         *     var o = {p:a, q:b, r:c};
         *     return o;
         *   }
         *
         * or similar real-world cases, which evolve a newborn native
         * object predicatably through some bounded number of property
         * additions. And second:
         *
         *   o.p = x;
         *
         * in a frequently executed method or loop body, where p will
         * (possibly after the first iteration) always exist in native
         * object o.
         */
        PropertyCacheEntry *entry;
        JSObject *obj2;
        JSAtom *atom;
        if (cache->testForSet(cx, regs.pc, obj, &entry, &obj2, &atom)) {
            /*
             * Fast property cache hit, only partially confirmed by
             * testForSet. We know that the entry applies to regs.pc and
             * that obj's shape matches.
             *
             * The entry predicts either a new property to be added
             * directly to obj by this set, or on an existing "own"
             * property, or on a prototype property that has a setter.
             */
            JS_ASSERT(entry->vword.isSprop());
            JSScopeProperty *sprop = entry->vword.toSprop();
            JS_ASSERT_IF(sprop->isDataDescriptor(), sprop->writable());
            JS_ASSERT_IF(sprop->hasSlot(), entry->vcapTag() == 0);

            JSScope *scope = obj->scope();
            JS_ASSERT(!scope->sealed());

            /*
             * Fastest path: check whether the cached sprop is already
             * in scope and call NATIVE_SET and break to get out of the
             * do-while(0). But we can call NATIVE_SET only if obj owns
             * scope or sprop is shared.
             */
            bool checkForAdd;
            if (!sprop->hasSlot()) {
                if (entry->vcapTag() == 0 ||
                    ((obj2 = obj->getProto()) &&
                     obj2->isNative() &&
                     obj2->shape() == entry->vshape())) {
                    goto fast_set_propcache_hit;
                }

                /* The cache entry doesn't apply. vshape mismatch. */
                checkForAdd = false;
            } else if (!scope->isSharedEmpty()) {
                if (sprop == scope->lastProperty() || scope->hasProperty(sprop)) {
                  fast_set_propcache_hit:
                    PCMETER(cache->pchits++);
                    PCMETER(cache->setpchits++);
                    NATIVE_SET(cx, obj, sprop, entry, &rref);
                    break;
                }
                checkForAdd = sprop->hasSlot() && sprop->parent == scope->lastProperty();
            } else {
                /*
                 * We check that cx own obj here and will continue to
                 * own it after js_GetMutableScope returns so we can
                 * continue to skip JS_UNLOCK_OBJ calls.
                 */
                JS_ASSERT(CX_OWNS_OBJECT_TITLE(cx, obj));
                scope = js_GetMutableScope(cx, obj);
                JS_ASSERT(CX_OWNS_OBJECT_TITLE(cx, obj));
                if (!scope)
                    goto error;
                checkForAdd = !sprop->parent;
            }

            uint32 slot;
            if (checkForAdd &&
                entry->vshape() == rt->protoHazardShape &&
                sprop->hasDefaultSetter() &&
                (slot = sprop->slot) == scope->freeslot) {
                /*
                 * Fast path: adding a plain old property that was once
                 * at the frontier of the property tree, whose slot is
                 * next to claim among the allocated slots in obj,
                 * where scope->table has not been created yet.
                 *
                 * We may want to remove hazard conditions above and
                 * inline compensation code here, depending on
                 * real-world workloads.
                 */
                PCMETER(cache->pchits++);
                PCMETER(cache->addpchits++);

                if (slot < obj->numSlots()) {
                    ++scope->freeslot;
                } else {
                    if (!js_AllocSlot(cx, obj, &slot))
                        goto error;
                    JS_ASSERT(slot + 1 == scope->freeslot);
                }

                /*
                 * If this obj's number of reserved slots differed, or
                 * if something created a hash table for scope, we must
                 * pay the price of JSScope::putProperty.
                 *
                 * (A built-in object with a pre-allocated but not fixed
                 * population of reserved slots  hook can cause scopes of the
                 * same shape to have different freeslot values. Arguments,
                 * Block, Call, and certain Function objects pre-allocate
                 * reserveds lots this way. This is what causes the slot !=
                 * sprop->slot case. See js_GetMutableScope. FIXME 558451)
                 */
                if (slot == sprop->slot && !scope->table) {
                    scope->extend(cx, sprop);
                } else {
                    JSScopeProperty *sprop2 =
                        scope->putProperty(cx, sprop->id,
                                           sprop->getter(), sprop->setter(),
                                           slot, sprop->attributes(),
                                           sprop->getFlags(), sprop->shortid);
                    if (!sprop2) {
                        js_FreeSlot(cx, obj, slot);
                        goto error;
                    }
                    sprop = sprop2;
                }

                /*
                 * No method change check here because here we are
                 * adding a new property, not updating an existing
                 * slot's value that might contain a method of a
                 * branded scope.
                 */
                TRACE_2(SetPropHit, entry, sprop);
                obj->lockedSetSlot(slot, rref);

                /*
                 * Purge the property cache of the id we may have just
                 * shadowed in obj's scope and proto chains. We do this
                 * after unlocking obj's scope to avoid lock nesting.
                 */
                js_PurgeScopeChain(cx, obj, sprop->id);
                break;
            }
            PCMETER(cache->setpcmisses++);
            atom = NULL;
        } else if (!atom) {
            /*
             * Slower property cache hit, fully confirmed by testForSet (in
             * the slow path, via fullTest).
             */
            ASSERT_VALID_PROPERTY_CACHE_HIT(0, obj, obj2, entry);
            JSScopeProperty *sprop = NULL;
            if (obj == obj2) {
                sprop = entry->vword.toSprop();
                JS_ASSERT(sprop->writable());
                JS_ASSERT(!obj2->scope()->sealed());
                NATIVE_SET(cx, obj, sprop, entry, &rref);
            }
            if (sprop)
                break;
        }

        if (!atom)
            LOAD_ATOM(0, atom);
        jsid id = ATOM_TO_JSID(atom);
        if (entry && JS_LIKELY(obj->map->ops->setProperty == js_SetProperty)) {
            uintN defineHow;
            if (op == JSOP_SETMETHOD)
                defineHow = JSDNP_CACHE_RESULT | JSDNP_SET_METHOD;
            else if (op == JSOP_SETNAME)
                defineHow = JSDNP_CACHE_RESULT | JSDNP_UNQUALIFIED;
            else
                defineHow = JSDNP_CACHE_RESULT;
            if (!js_SetPropertyHelper(cx, obj, id, defineHow, &rref))
                goto error;
        } else {
            if (!obj->setProperty(cx, id, &rref))
                goto error;
            ABORT_RECORDING(cx, "Non-native set");
        }
    } while (0);
}
END_SET_CASE_STORE_RVAL(JSOP_SETPROP, 2);

BEGIN_CASE(JSOP_GETELEM)
{
    Value &lref = regs.sp[-2];
    Value &rref = regs.sp[-1];
    if (lref.isString() && rref.isInt32()) {
        JSString *str = lref.toString();
        int32_t i = rref.toInt32();
        if (size_t(i) < str->length()) {
            str = JSString::getUnitString(cx, str, size_t(i));
            if (!str)
                goto error;
            regs.sp--;
            regs.sp[-1].setString(str);
            len = JSOP_GETELEM_LENGTH;
            DO_NEXT_OP(len);
        }
    }

    JSObject *obj;
    VALUE_TO_OBJECT(cx, &lref, obj);

    const Value *copyFrom;
    Value rval;
    jsid id;
    if (rref.isInt32()) {
        int32_t i = rref.toInt32();
        if (obj->isDenseArray()) {
            jsuint idx = jsuint(i);

            if (idx < obj->getDenseArrayCapacity()) {
                copyFrom = obj->addressOfDenseArrayElement(idx);
                if (!copyFrom->isMagic())
                    goto end_getelem;

                /* Reload retval from the stack in the rare hole case. */
                copyFrom = &regs.sp[-1];
            }
        } else if (obj->isArguments()
#ifdef JS_TRACER
                   && !GetArgsPrivateNative(obj)
#endif
                  ) {
            uint32 arg = uint32(i);

            if (arg < obj->getArgsLength()) {
                JSStackFrame *afp = (JSStackFrame *) obj->getPrivate();
                if (afp) {
                    copyFrom = &afp->argv[arg];
                    goto end_getelem;
                }

                copyFrom = obj->addressOfArgsElement(arg);
                if (!copyFrom->isMagic())
                    goto end_getelem;
                copyFrom = &regs.sp[-1];
            }
        }
        if (JS_LIKELY(INT_FITS_IN_JSID(i)))
            id = INT_TO_JSID(i);
        else
            goto intern_big_int;
    } else {
      intern_big_int:
        if (!js_InternNonIntElementId(cx, obj, rref, &id))
            goto error;
    }

    if (!obj->getProperty(cx, id, &rval))
        goto error;
    copyFrom = &rval;

  end_getelem:
    regs.sp--;
    regs.sp[-1] = *copyFrom;
}
END_CASE(JSOP_GETELEM)

BEGIN_CASE(JSOP_CALLELEM)
{
    /* Fetch the left part and resolve it to a non-null object. */
    JSObject *obj;
    FETCH_OBJECT(cx, -2, obj);

    /* Fetch index and convert it to id suitable for use with obj. */
    jsid id;
    FETCH_ELEMENT_ID(obj, -1, id);

    /* Get or set the element. */
    if (!js_GetMethod(cx, obj, id, JSGET_NO_METHOD_BARRIER, &regs.sp[-2]))
        goto error;

#if JS_HAS_NO_SUCH_METHOD
    if (JS_UNLIKELY(regs.sp[-2].isUndefined())) {
        regs.sp[-2] = regs.sp[-1];
        regs.sp[-1].setObject(*obj);
        if (!js_OnUnknownMethod(cx, regs.sp - 2))
            goto error;
    } else
#endif
    {
        regs.sp[-1].setObject(*obj);
    }
}
END_CASE(JSOP_CALLELEM)

BEGIN_CASE(JSOP_SETELEM)
{
    JSObject *obj;
    FETCH_OBJECT(cx, -3, obj);
    jsid id;
    FETCH_ELEMENT_ID(obj, -2, id);
    do {
        if (obj->isDenseArray() && JSID_IS_INT(id)) {
            jsuint length = obj->getDenseArrayCapacity();
            jsint i = JSID_TO_INT(id);
            if ((jsuint)i < length) {
                if (obj->getDenseArrayElement(i).isMagic(JS_ARRAY_HOLE)) {
                    if (js_PrototypeHasIndexedProperties(cx, obj))
                        break;
                    if ((jsuint)i >= obj->getArrayLength())
                        obj->setArrayLength(i + 1);
                }
                obj->setDenseArrayElement(i, regs.sp[-1]);
                goto end_setelem;
            }
        }
    } while (0);
    if (!obj->setProperty(cx, id, &regs.sp[-1]))
        goto error;
  end_setelem:;
}
END_SET_CASE_STORE_RVAL(JSOP_SETELEM, 3)

BEGIN_CASE(JSOP_ENUMELEM)
{
    /* Funky: the value to set is under the [obj, id] pair. */
    JSObject *obj;
    FETCH_OBJECT(cx, -2, obj);
    jsid id;
    FETCH_ELEMENT_ID(obj, -1, id);
    if (!obj->setProperty(cx, id, &regs.sp[-3]))
        goto error;
    regs.sp -= 3;
}
END_CASE(JSOP_ENUMELEM)

{
    JSFunction *fun;
    JSObject *obj;
    uintN flags;
    uintN argc;
    Value *vp;

BEGIN_CASE(JSOP_NEW)
{
    /* Get immediate argc and find the constructor function. */
    argc = GET_ARGC(regs.pc);
    vp = regs.sp - (2 + argc);
    JS_ASSERT(vp >= fp->base());

    /*
     * Assign lval, obj, and fun exactly as the code at inline_call: expects to
     * find them, to avoid nesting a js_Interpret call via js_InvokeConstructor.
     */
    if (IsFunctionObject(vp[0], &obj)) {
        fun = GET_FUNCTION_PRIVATE(cx, obj);
        if (fun->isInterpreted()) {
            /* Root as we go using vp[1]. */
            if (!obj->getProperty(cx,
                                  ATOM_TO_JSID(cx->runtime->atomState.classPrototypeAtom),
                                  &vp[1])) {
                goto error;
            }
            JSObject *proto = vp[1].isObject() ? &vp[1].toObject() : NULL;
            JSObject *obj2 = NewObject(cx, &js_ObjectClass, proto, obj->getParent());
            if (!obj2)
                goto error;

            if (fun->u.i.script->isEmpty()) {
                vp[0].setObject(*obj2);
                regs.sp = vp + 1;
                goto end_new;
            }

            vp[1].setObject(*obj2);
            flags = JSFRAME_CONSTRUCTING;
            goto inline_call;
        }
    }

    if (!InvokeConstructor(cx, InvokeArgsGuard(vp, argc)))
        goto error;
    regs.sp = vp + 1;
    CHECK_INTERRUPT_HANDLER();
    TRACE_0(NativeCallComplete);

  end_new:;
}
END_CASE(JSOP_NEW)

BEGIN_CASE(JSOP_CALL)
BEGIN_CASE(JSOP_EVAL)
BEGIN_CASE(JSOP_APPLY)
{
    argc = GET_ARGC(regs.pc);
    vp = regs.sp - (argc + 2);

    if (IsFunctionObject(*vp, &obj)) {
        fun = GET_FUNCTION_PRIVATE(cx, obj);

        /* Clear frame flags since this is not a constructor call. */
        flags = 0;
        if (FUN_INTERPRETED(fun))
      inline_call:
        {
            JSScript *newscript = fun->u.i.script;
            if (JS_UNLIKELY(newscript->isEmpty())) {
                vp->setUndefined();
                regs.sp = vp + 1;
                goto end_call;
            }

            /* Restrict recursion of lightweight functions. */
            if (JS_UNLIKELY(inlineCallCount >= JS_MAX_INLINE_CALL_COUNT)) {
                js_ReportOverRecursed(cx);
                goto error;
            }

            /*
             * Get pointer to new frame/slots, without changing global state.
             * Initialize missing args if there are any.
             */
            StackSpace &stack = cx->stack();
            uintN nfixed = newscript->nslots;
            uintN funargs = fun->nargs;
            JSStackFrame *newfp;
            if (argc < funargs) {
                uintN missing = funargs - argc;
                newfp = stack.getInlineFrame(cx, regs.sp, missing, nfixed);
                if (!newfp)
                    goto error;
                SetValueRangeToUndefined(regs.sp, missing);
            } else {
                newfp = stack.getInlineFrame(cx, regs.sp, 0, nfixed);
                if (!newfp)
                    goto error;
            }

            /* Initialize stack frame. */
            newfp->callobj = NULL;
            newfp->argsobj = NULL;
            newfp->script = newscript;
            newfp->fun = fun;
            newfp->argc = argc;
            newfp->argv = vp + 2;
            newfp->rval.setUndefined();
            newfp->annotation = NULL;
            newfp->scopeChain = obj->getParent();
            newfp->flags = flags;
            newfp->blockChain = NULL;
            if (JS_LIKELY(newscript->staticLevel < JS_DISPLAY_SIZE)) {
                JSStackFrame **disp = &cx->display[newscript->staticLevel];
                newfp->displaySave = *disp;
                *disp = newfp;
            }
            JS_ASSERT(!JSFUN_BOUND_METHOD_TEST(fun->flags));
            newfp->thisv = vp[1];
            newfp->imacpc = NULL;

            /* Push void to initialize local variables. */
            Value *newsp = newfp->base();
            SetValueRangeToUndefined(newfp->slots(), newsp);

            /* Switch version if currentVersion wasn't overridden. */
            newfp->callerVersion = (JSVersion) cx->version;
            if (JS_LIKELY(cx->version == currentVersion)) {
                currentVersion = (JSVersion) newscript->version;
                if (JS_UNLIKELY(currentVersion != cx->version))
                    js_SetVersion(cx, currentVersion);
            }

            /* Push the frame. */
            stack.pushInlineFrame(cx, fp, regs.pc, newfp);

            /* Initializer regs after pushInlineFrame snapshots pc. */
            regs.pc = newscript->code;
            regs.sp = newsp;

            /* Import into locals. */
            JS_ASSERT(newfp == cx->fp);
            fp = newfp;
            script = newscript;
            atoms = script->atomMap.vector;

            /* Now that the new frame is rooted, maybe create a call object. */
            if (fun->isHeavyweight() && !js_GetCallObject(cx, fp))
                goto error;

            /* Call the debugger hook if present. */
            if (JSInterpreterHook hook = cx->debugHooks->callHook) {
                fp->hookData = hook(cx, fp, JS_TRUE, 0,
                                    cx->debugHooks->callHookData);
                CHECK_INTERRUPT_HANDLER();
            } else {
                fp->hookData = NULL;
            }

            inlineCallCount++;
            JS_RUNTIME_METER(rt, inlineCalls);

            DTrace::enterJSFun(cx, fp, fun, fp->down, fp->argc, fp->argv);

#ifdef JS_TRACER
            if (TraceRecorder *tr = TRACE_RECORDER(cx)) {
                AbortableRecordingStatus status = tr->record_EnterFrame(inlineCallCount);
                RESTORE_INTERP_VARS();
                if (StatusAbortsRecorderIfActive(status)) {
                    if (TRACE_RECORDER(cx)) {
                        JS_ASSERT(TRACE_RECORDER(cx) == tr);
                        AbortRecording(cx, "record_EnterFrame failed");
                    }
                    if (status == ARECORD_ERROR)
                        goto error;
                }
            } else if (fp->script == fp->down->script &&
                       *fp->down->savedPC == JSOP_CALL &&
                       *regs.pc == JSOP_TRACE) {
                MONITOR_BRANCH(Record_EnterFrame);
            }
#endif

            /* Load first op and dispatch it (safe since JSOP_STOP). */
            op = (JSOp) *regs.pc;
            DO_OP();
        }

        if (fun->flags & JSFUN_FAST_NATIVE) {
            DTrace::enterJSFun(cx, NULL, fun, fp, argc, vp + 2, vp);

            JS_ASSERT(fun->u.n.extra == 0);
            JS_ASSERT(vp[1].isObjectOrNull() || PrimitiveThisTest(fun, vp[1]));
            JSBool ok = ((FastNative) fun->u.n.native)(cx, argc, vp);
            DTrace::exitJSFun(cx, NULL, fun, *vp, vp);
            regs.sp = vp + 1;
            if (!ok)
                goto error;
            TRACE_0(NativeCallComplete);
            goto end_call;
        }
    }

    bool ok;
    ok = Invoke(cx, InvokeArgsGuard(vp, argc), 0);
    regs.sp = vp + 1;
    CHECK_INTERRUPT_HANDLER();
    if (!ok)
        goto error;
    JS_RUNTIME_METER(rt, nonInlineCalls);
    TRACE_0(NativeCallComplete);

  end_call:;
}
END_CASE(JSOP_CALL)
}

BEGIN_CASE(JSOP_SETCALL)
{
    uintN argc = GET_ARGC(regs.pc);
    Value *vp = regs.sp - argc - 2;
    JSBool ok = Invoke(cx, InvokeArgsGuard(vp, argc), 0);
    if (ok)
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_LEFTSIDE_OF_ASS);
    goto error;
}
END_CASE(JSOP_SETCALL)

BEGIN_CASE(JSOP_NAME)
BEGIN_CASE(JSOP_CALLNAME)
{
    JSObject *obj = fp->scopeChain;

    JSScopeProperty *sprop;
    Value rval;

    PropertyCacheEntry *entry;
    JSObject *obj2;
    JSAtom *atom;
    JS_PROPERTY_CACHE(cx).test(cx, regs.pc, obj, obj2, entry, atom);
    if (!atom) {
        ASSERT_VALID_PROPERTY_CACHE_HIT(0, obj, obj2, entry);
        if (entry->vword.isFunObj()) {
            PUSH_OBJECT(entry->vword.toFunObj());
            goto do_push_obj_if_call;
        }

        if (entry->vword.isSlot()) {
            uintN slot = entry->vword.toSlot();
            JS_ASSERT(slot < obj2->scope()->freeslot);
            PUSH_COPY(obj2->lockedGetSlot(slot));
            goto do_push_obj_if_call;
        }

        JS_ASSERT(entry->vword.isSprop());
        sprop = entry->vword.toSprop();
        goto do_native_get;
    }

    jsid id;
    id = ATOM_TO_JSID(atom);
    JSProperty *prop;
    if (!js_FindPropertyHelper(cx, id, true, &obj, &obj2, &prop))
        goto error;
    if (!prop) {
        /* Kludge to allow (typeof foo == "undefined") tests. */
        JSOp op2 = js_GetOpcode(cx, script, regs.pc + JSOP_NAME_LENGTH);
        if (op2 == JSOP_TYPEOF) {
            PUSH_UNDEFINED();
            len = JSOP_NAME_LENGTH;
            DO_NEXT_OP(len);
        }
        atomNotDefined = atom;
        goto atom_not_defined;
    }

    /* Take the slow path if prop was not found in a native object. */
    if (!obj->isNative() || !obj2->isNative()) {
        obj2->dropProperty(cx, prop);
        if (!obj->getProperty(cx, id, &rval))
            goto error;
    } else {
        sprop = (JSScopeProperty *)prop;
  do_native_get:
        NATIVE_GET(cx, obj, obj2, sprop, JSGET_METHOD_BARRIER, &rval);
        JS_UNLOCK_OBJ(cx, obj2);
    }

    PUSH_COPY(rval);

  do_push_obj_if_call:
    /* obj must be on the scope chain, thus not a function. */
    if (op == JSOP_CALLNAME)
        PUSH_OBJECT(*obj);
}
END_CASE(JSOP_NAME)

BEGIN_CASE(JSOP_UINT16)
    PUSH_INT32((int32_t) GET_UINT16(regs.pc));
END_CASE(JSOP_UINT16)

BEGIN_CASE(JSOP_UINT24)
    PUSH_INT32((int32_t) GET_UINT24(regs.pc));
END_CASE(JSOP_UINT24)

BEGIN_CASE(JSOP_INT8)
    PUSH_INT32(GET_INT8(regs.pc));
END_CASE(JSOP_INT8)

BEGIN_CASE(JSOP_INT32)
    PUSH_INT32(GET_INT32(regs.pc));
END_CASE(JSOP_INT32)

BEGIN_CASE(JSOP_INDEXBASE)
    /*
     * Here atoms can exceed script->atomMap.length as we use atoms as a
     * segment register for object literals as well.
     */
    atoms += GET_INDEXBASE(regs.pc);
END_CASE(JSOP_INDEXBASE)

BEGIN_CASE(JSOP_INDEXBASE1)
BEGIN_CASE(JSOP_INDEXBASE2)
BEGIN_CASE(JSOP_INDEXBASE3)
    atoms += (op - JSOP_INDEXBASE1 + 1) << 16;
END_CASE(JSOP_INDEXBASE3)

BEGIN_CASE(JSOP_RESETBASE0)
BEGIN_CASE(JSOP_RESETBASE)
    atoms = script->atomMap.vector;
END_CASE(JSOP_RESETBASE)

BEGIN_CASE(JSOP_DOUBLE)
{
    JS_ASSERT(!fp->imacpc);
    JS_ASSERT(size_t(atoms - script->atomMap.vector) <= script->atomMap.length);
    double dbl;
    LOAD_DOUBLE(0, dbl);
    PUSH_DOUBLE(dbl);
}
END_CASE(JSOP_DOUBLE)

BEGIN_CASE(JSOP_STRING)
{
    JSAtom *atom;
    LOAD_ATOM(0, atom);
    PUSH_STRING(ATOM_TO_STRING(atom));
}
END_CASE(JSOP_STRING)

BEGIN_CASE(JSOP_OBJECT)
{
    JSObject *obj;
    LOAD_OBJECT(0, obj);
    /* Only XML and RegExp objects are emitted. */
    PUSH_OBJECT(*obj);
}
END_CASE(JSOP_OBJECT)

BEGIN_CASE(JSOP_REGEXP)
{
    /*
     * Push a regexp object cloned from the regexp literal object mapped by the
     * bytecode at pc. ES5 finally fixed this bad old ES3 design flaw which was
     * flouted by many browser-based implementations.
     *
     * We avoid the js_GetScopeChain call here and pass fp->scopeChain as
     * js_GetClassPrototype uses the latter only to locate the global.
     */
    jsatomid index = GET_FULL_INDEX(0);
    JSObject *proto;
    if (!js_GetClassPrototype(cx, fp->scopeChain, JSProto_RegExp, &proto))
        goto error;
    JS_ASSERT(proto);
    JSObject *obj = js_CloneRegExpObject(cx, script->getRegExp(index), proto);
    if (!obj)
        goto error;
    PUSH_OBJECT(*obj);
}
END_CASE(JSOP_REGEXP)

BEGIN_CASE(JSOP_ZERO)
    PUSH_INT32(0);
END_CASE(JSOP_ZERO)

BEGIN_CASE(JSOP_ONE)
    PUSH_INT32(1);
END_CASE(JSOP_ONE)

BEGIN_CASE(JSOP_NULL)
    PUSH_NULL();
END_CASE(JSOP_NULL)

BEGIN_CASE(JSOP_FALSE)
    PUSH_BOOLEAN(false);
END_CASE(JSOP_FALSE)

BEGIN_CASE(JSOP_TRUE)
    PUSH_BOOLEAN(true);
END_CASE(JSOP_TRUE)

{
BEGIN_CASE(JSOP_TABLESWITCH)
{
    jsbytecode *pc2 = regs.pc;
    len = GET_JUMP_OFFSET(pc2);

    /*
     * ECMAv2+ forbids conversion of discriminant, so we will skip to the
     * default case if the discriminant isn't already an int jsval.  (This
     * opcode is emitted only for dense jsint-domain switches.)
     */
    const Value &rref = *--regs.sp;
    int32_t i;
    if (rref.isInt32()) {
        i = rref.toInt32();
    } else {
        double d;
        /* Don't use JSDOUBLE_IS_INT32; treat -0 (double) as 0. */
        if (!rref.isDouble() || (d = rref.toDouble()) != (i = int32_t(rref.toDouble())))
            DO_NEXT_OP(len);
    }

    pc2 += JUMP_OFFSET_LEN;
    jsint low = GET_JUMP_OFFSET(pc2);
    pc2 += JUMP_OFFSET_LEN;
    jsint high = GET_JUMP_OFFSET(pc2);

    i -= low;
    if ((jsuint)i < (jsuint)(high - low + 1)) {
        pc2 += JUMP_OFFSET_LEN + JUMP_OFFSET_LEN * i;
        jsint off = (jsint) GET_JUMP_OFFSET(pc2);
        if (off)
            len = off;
    }
}
END_VARLEN_CASE
}

{
BEGIN_CASE(JSOP_TABLESWITCHX)
{
    jsbytecode *pc2 = regs.pc;
    len = GET_JUMPX_OFFSET(pc2);

    /*
     * ECMAv2+ forbids conversion of discriminant, so we will skip to the
     * default case if the discriminant isn't already an int jsval.  (This
     * opcode is emitted only for dense jsint-domain switches.)
     */
    const Value &rref = *--regs.sp;
    int32_t i;
    if (rref.isInt32()) {
        i = rref.toInt32();
    } else if (rref.isDouble() && rref.toDouble() == 0) {
        /* Treat -0 (double) as 0. */
        i = 0;
    } else {
        DO_NEXT_OP(len);
    }

    pc2 += JUMPX_OFFSET_LEN;
    jsint low = GET_JUMP_OFFSET(pc2);
    pc2 += JUMP_OFFSET_LEN;
    jsint high = GET_JUMP_OFFSET(pc2);

    i -= low;
    if ((jsuint)i < (jsuint)(high - low + 1)) {
        pc2 += JUMP_OFFSET_LEN + JUMPX_OFFSET_LEN * i;
        jsint off = (jsint) GET_JUMPX_OFFSET(pc2);
        if (off)
            len = off;
    }
}
END_VARLEN_CASE
}

{
BEGIN_CASE(JSOP_LOOKUPSWITCHX)
{
    jsint off;
    off = JUMPX_OFFSET_LEN;
    goto do_lookup_switch;

BEGIN_CASE(JSOP_LOOKUPSWITCH)
    off = JUMP_OFFSET_LEN;

  do_lookup_switch:
    /*
     * JSOP_LOOKUPSWITCH and JSOP_LOOKUPSWITCHX are never used if any atom
     * index in it would exceed 64K limit.
     */
    JS_ASSERT(!fp->imacpc);
    JS_ASSERT(atoms == script->atomMap.vector);
    jsbytecode *pc2 = regs.pc;

    Value lval = regs.sp[-1];
    regs.sp--;

    if (!lval.isPrimitive())
        goto end_lookup_switch;

    pc2 += off;
    jsint npairs;
    npairs = (jsint) GET_UINT16(pc2);
    pc2 += UINT16_LEN;
    JS_ASSERT(npairs);  /* empty switch uses JSOP_TABLESWITCH */

    bool match;
#define SEARCH_PAIRS(MATCH_CODE)                                              \
    for (;;) {                                                                \
        Value rval = script->getConst(GET_INDEX(pc2));                        \
        MATCH_CODE                                                            \
        pc2 += INDEX_LEN;                                                     \
        if (match)                                                            \
            break;                                                            \
        pc2 += off;                                                           \
        if (--npairs == 0) {                                                  \
            pc2 = regs.pc;                                                    \
            break;                                                            \
        }                                                                     \
    }

    if (lval.isString()) {
        JSString *str = lval.toString();
        JSString *str2;
        SEARCH_PAIRS(
            match = (rval.isString() &&
                     ((str2 = rval.toString()) == str ||
                      js_EqualStrings(str2, str)));
        )
    } else if (lval.isNumber()) {
        double ldbl = lval.toNumber();
        SEARCH_PAIRS(
            match = rval.isNumber() && ldbl == rval.toNumber();
        )
    } else {
        SEARCH_PAIRS(
            match = (lval == rval);
        )
    }
#undef SEARCH_PAIRS

  end_lookup_switch:
    len = (op == JSOP_LOOKUPSWITCH)
          ? GET_JUMP_OFFSET(pc2)
          : GET_JUMPX_OFFSET(pc2);
}
END_VARLEN_CASE
}

BEGIN_CASE(JSOP_TRAP)
{
    Value rval;
    JSTrapStatus status = JS_HandleTrap(cx, script, regs.pc, Jsvalify(&rval));
    switch (status) {
      case JSTRAP_ERROR:
        goto error;
      case JSTRAP_RETURN:
        fp->rval = rval;
        interpReturnOK = JS_TRUE;
        goto forced_return;
      case JSTRAP_THROW:
        cx->throwing = JS_TRUE;
        cx->exception = rval;
        goto error;
      default:
        break;
    }
    JS_ASSERT(status == JSTRAP_CONTINUE);
    CHECK_INTERRUPT_HANDLER();
    JS_ASSERT(rval.isInt32());
    op = (JSOp) rval.toInt32();
    JS_ASSERT((uintN)op < (uintN)JSOP_LIMIT);
    DO_OP();
}

BEGIN_CASE(JSOP_ARGUMENTS)
{
    Value rval;
    if (!js_GetArgsValue(cx, fp, &rval))
        goto error;
    PUSH_COPY(rval);
}
END_CASE(JSOP_ARGUMENTS)

BEGIN_CASE(JSOP_ARGSUB)
{
    jsid id = INT_TO_JSID(GET_ARGNO(regs.pc));
    Value rval;
    if (!js_GetArgsProperty(cx, fp, id, &rval))
        goto error;
    PUSH_COPY(rval);
}
END_CASE(JSOP_ARGSUB)

BEGIN_CASE(JSOP_ARGCNT)
{
    jsid id = ATOM_TO_JSID(rt->atomState.lengthAtom);
    Value rval;
    if (!js_GetArgsProperty(cx, fp, id, &rval))
        goto error;
    PUSH_COPY(rval);
}
END_CASE(JSOP_ARGCNT)

BEGIN_CASE(JSOP_GETARG)
BEGIN_CASE(JSOP_CALLARG)
{
    uint32 slot = GET_ARGNO(regs.pc);
    JS_ASSERT(slot < fp->fun->nargs);
    METER_SLOT_OP(op, slot);
    PUSH_COPY(fp->argv[slot]);
    if (op == JSOP_CALLARG)
        PUSH_NULL();
}
END_CASE(JSOP_GETARG)

BEGIN_CASE(JSOP_SETARG)
{
    uint32 slot = GET_ARGNO(regs.pc);
    JS_ASSERT(slot < fp->fun->nargs);
    METER_SLOT_OP(op, slot);
    fp->argv[slot] = regs.sp[-1];
}
END_SET_CASE(JSOP_SETARG)

BEGIN_CASE(JSOP_GETLOCAL)
{
    uint32 slot = GET_SLOTNO(regs.pc);
    JS_ASSERT(slot < script->nslots);
    PUSH_COPY(fp->slots()[slot]);
}
END_CASE(JSOP_GETLOCAL)

BEGIN_CASE(JSOP_CALLLOCAL)
{
    uint32 slot = GET_SLOTNO(regs.pc);
    JS_ASSERT(slot < script->nslots);
    PUSH_COPY(fp->slots()[slot]);
    PUSH_NULL();
}
END_CASE(JSOP_CALLLOCAL)

BEGIN_CASE(JSOP_SETLOCAL)
{
    uint32 slot = GET_SLOTNO(regs.pc);
    JS_ASSERT(slot < script->nslots);
    fp->slots()[slot] = regs.sp[-1];
}
END_SET_CASE(JSOP_SETLOCAL)

BEGIN_CASE(JSOP_GETUPVAR)
BEGIN_CASE(JSOP_CALLUPVAR)
{
    JSUpvarArray *uva = script->upvars();

    uintN index = GET_UINT16(regs.pc);
    JS_ASSERT(index < uva->length);

    const Value &rval = js_GetUpvar(cx, script->staticLevel, uva->vector[index]);
    PUSH_COPY(rval);

    if (op == JSOP_CALLUPVAR)
        PUSH_NULL();
}
END_CASE(JSOP_GETUPVAR)

BEGIN_CASE(JSOP_GETUPVAR_DBG)
BEGIN_CASE(JSOP_CALLUPVAR_DBG)
{
    JSFunction *fun = fp->fun;
    JS_ASSERT(FUN_KIND(fun) == JSFUN_INTERPRETED);
    JS_ASSERT(fun->u.i.wrapper);

    /* Scope for tempPool mark and local names allocation in it. */
    JSObject *obj, *obj2;
    JSProperty *prop;
    jsid id;
    JSAtom *atom;
    {
        void *mark = JS_ARENA_MARK(&cx->tempPool);
        jsuword *names = js_GetLocalNameArray(cx, fun, &cx->tempPool);
        if (!names)
            goto error;

        uintN index = fun->countArgsAndVars() + GET_UINT16(regs.pc);
        atom = JS_LOCAL_NAME_TO_ATOM(names[index]);
        id = ATOM_TO_JSID(atom);

        JSBool ok = js_FindProperty(cx, id, &obj, &obj2, &prop);
        JS_ARENA_RELEASE(&cx->tempPool, mark);
        if (!ok)
            goto error;
    }

    if (!prop) {
        atomNotDefined = atom;
        goto atom_not_defined;
    }

    /* Minimize footprint with generic code instead of NATIVE_GET. */
    obj2->dropProperty(cx, prop);
    Value *vp = regs.sp;
    PUSH_NULL();
    if (!obj->getProperty(cx, id, vp))
        goto error;

    if (op == JSOP_CALLUPVAR_DBG)
        PUSH_NULL();
}
END_CASE(JSOP_GETUPVAR_DBG)

BEGIN_CASE(JSOP_GETDSLOT)
BEGIN_CASE(JSOP_CALLDSLOT)
{
    JS_ASSERT(fp->argv);
    JSObject *obj = &fp->argv[-2].toObject();
    JS_ASSERT(obj);
    JS_ASSERT(obj->dslots);

    uintN index = GET_UINT16(regs.pc);
    JS_ASSERT(JS_INITIAL_NSLOTS + index < obj->dslots[-1].toPrivateUint32());
    JS_ASSERT_IF(obj->scope()->object == obj,
                 JS_INITIAL_NSLOTS + index < obj->scope()->freeslot);

    PUSH_COPY(obj->dslots[index]);
    if (op == JSOP_CALLDSLOT)
        PUSH_NULL();
}
END_CASE(JSOP_GETDSLOT)

BEGIN_CASE(JSOP_GETGVAR)
BEGIN_CASE(JSOP_CALLGVAR)
{
    uint32 slot = GET_SLOTNO(regs.pc);
    JS_ASSERT(slot < GlobalVarCount(fp));
    METER_SLOT_OP(op, slot);
    const Value &lval = fp->slots()[slot];
    if (lval.isNull()) {
        op = (op == JSOP_GETGVAR) ? JSOP_NAME : JSOP_CALLNAME;
        DO_OP();
    }
    JS_ASSERT(fp->varobj(cx) == cx->activeSegment()->getInitialVarObj());
    JSObject *varobj = cx->activeSegment()->getInitialVarObj();

    /* XXX all this code assumes that varobj is either a callobj or global and
     * that it cannot be accessed in a MT way. This is either true now or
     * coming soon. */

    slot = (uint32)lval.toInt32();
    const Value &rref = varobj->lockedGetSlot(slot);
    PUSH_COPY(rref);
    if (op == JSOP_CALLGVAR)
        PUSH_NULL();
}
END_CASE(JSOP_GETGVAR)

BEGIN_CASE(JSOP_SETGVAR)
{
    uint32 slot = GET_SLOTNO(regs.pc);
    JS_ASSERT(slot < GlobalVarCount(fp));
    METER_SLOT_OP(op, slot);
    const Value &rref = regs.sp[-1];
    JS_ASSERT(fp->varobj(cx) == cx->activeSegment()->getInitialVarObj());
    JSObject *obj = cx->activeSegment()->getInitialVarObj();
    const Value &lref = fp->slots()[slot];
    if (lref.isNull()) {
        /*
         * Inline-clone and deoptimize JSOP_SETNAME code here because
         * JSOP_SETGVAR has arity 1: [rref], not arity 2: [obj, rref]
         * as JSOP_SETNAME does, where [obj] is due to JSOP_BINDNAME.
         */
#ifdef JS_TRACER
        if (TRACE_RECORDER(cx))
            AbortRecording(cx, "SETGVAR with NULL slot");
#endif
        JSAtom *atom;
        LOAD_ATOM(0, atom);
        jsid id = ATOM_TO_JSID(atom);
        Value rval = rref;
        if (!obj->setProperty(cx, id, &rval))
            goto error;
    } else {
        uint32 slot = (uint32)lref.toInt32();
        JS_LOCK_OBJ(cx, obj);
        JSScope *scope = obj->scope();
        if (!scope->methodWriteBarrier(cx, slot, rref)) {
            JS_UNLOCK_SCOPE(cx, scope);
            goto error;
        }
        obj->lockedSetSlot(slot, rref);
        JS_UNLOCK_SCOPE(cx, scope);
    }
}
END_SET_CASE(JSOP_SETGVAR)

BEGIN_CASE(JSOP_DEFCONST)
BEGIN_CASE(JSOP_DEFVAR)
{
    uint32 index = GET_INDEX(regs.pc);
    JSAtom *atom = atoms[index];

    /*
     * index is relative to atoms at this point but for global var
     * code below we need the absolute value.
     */
    index += atoms - script->atomMap.vector;
    JSObject *obj = fp->varobj(cx);
    JS_ASSERT(obj->map->ops->defineProperty == js_DefineProperty);
    uintN attrs = JSPROP_ENUMERATE;
    if (!(fp->flags & JSFRAME_EVAL))
        attrs |= JSPROP_PERMANENT;
    if (op == JSOP_DEFCONST)
        attrs |= JSPROP_READONLY;

    /* Lookup id in order to check for redeclaration problems. */
    jsid id = ATOM_TO_JSID(atom);
    JSProperty *prop = NULL;
    JSObject *obj2;
    if (op == JSOP_DEFVAR) {
        /*
         * Redundant declaration of a |var|, even one for a non-writable
         * property like |undefined| in ES5, does nothing.
         */
        if (!obj->lookupProperty(cx, id, &obj2, &prop))
            goto error;
    } else {
        if (!CheckRedeclaration(cx, obj, id, attrs, &obj2, &prop))
            goto error;
    }

    /* Bind a variable only if it's not yet defined. */
    if (!prop) {
        if (!js_DefineNativeProperty(cx, obj, id, UndefinedValue(), PropertyStub, PropertyStub,
                                     attrs, 0, 0, &prop)) {
            goto error;
        }
        JS_ASSERT(prop);
        obj2 = obj;
    }

    /*
     * Try to optimize a property we either just created, or found
     * directly in the global object, that is permanent, has a slot,
     * and has stub getter and setter, into a "fast global" accessed
     * by the JSOP_*GVAR opcodes.
     */
    if (!fp->fun &&
        index < GlobalVarCount(fp) &&
        obj2 == obj &&
        obj->isNative()) {
        JSScopeProperty *sprop = (JSScopeProperty *) prop;
        if (!sprop->configurable() &&
            SPROP_HAS_VALID_SLOT(sprop, obj->scope()) &&
            sprop->hasDefaultGetterOrIsMethod() &&
            sprop->hasDefaultSetter()) {
            /*
             * Fast globals use frame variables to map the global name's atom
             * index to the permanent varobj slot number, tagged as a jsval.
             * The atom index for the global's name literal is identical to its
             * variable index.
             */
            fp->slots()[index].setInt32(sprop->slot);
        }
    }

    obj2->dropProperty(cx, prop);
}
END_CASE(JSOP_DEFVAR)

BEGIN_CASE(JSOP_DEFFUN)
{
    PropertyOp getter, setter;
    bool doSet;
    JSObject *pobj;
    JSProperty *prop;
    uint32 old;

    /*
     * A top-level function defined in Global or Eval code (see ECMA-262
     * Ed. 3), or else a SpiderMonkey extension: a named function statement in
     * a compound statement (not at the top statement level of global code, or
     * at the top level of a function body).
     */
    JSFunction *fun;
    LOAD_FUNCTION(0);
    JSObject *obj = FUN_OBJECT(fun);

    JSObject *obj2;
    if (FUN_NULL_CLOSURE(fun)) {
        /*
         * Even a null closure needs a parent for principals finding.
         * FIXME: bug 476950, although debugger users may also demand some kind
         * of scope link for debugger-assisted eval-in-frame.
         */
        obj2 = fp->scopeChain;
    } else {
        JS_ASSERT(!FUN_FLAT_CLOSURE(fun));

        /*
         * Inline js_GetScopeChain a bit to optimize for the case of a
         * top-level function.
         */
        if (!fp->blockChain) {
            obj2 = fp->scopeChain;
        } else {
            obj2 = js_GetScopeChain(cx, fp);
            if (!obj2)
                goto error;
        }
    }

    /*
     * If static link is not current scope, clone fun's object to link to the
     * current scope via parent. We do this to enable sharing of compiled
     * functions among multiple equivalent scopes, amortizing the cost of
     * compilation over a number of executions.  Examples include XUL scripts
     * and event handlers shared among Firefox or other Mozilla app chrome
     * windows, and user-defined JS functions precompiled and then shared among
     * requests in server-side JS.
     */
    if (obj->getParent() != obj2) {
        obj = CloneFunctionObject(cx, fun, obj2);
        if (!obj)
            goto error;
    }

    /*
     * Protect obj from any GC hiding below JSObject::setProperty or
     * JSObject::defineProperty.  All paths from here must flow through the
     * fp->scopeChain code below the parent->defineProperty call.
     */
    MUST_FLOW_THROUGH("restore_scope");
    fp->scopeChain = obj;

    Value rval = ObjectValue(*obj);

    /*
     * ECMA requires functions defined when entering Eval code to be
     * impermanent.
     */
    uintN attrs = (fp->flags & JSFRAME_EVAL)
                  ? JSPROP_ENUMERATE
                  : JSPROP_ENUMERATE | JSPROP_PERMANENT;

    /*
     * Load function flags that are also property attributes.  Getters and
     * setters do not need a slot, their value is stored elsewhere in the
     * property itself, not in obj slots.
     */
    getter = setter = PropertyStub;
    uintN flags = JSFUN_GSFLAG2ATTR(fun->flags);
    if (flags) {
        /* Function cannot be both getter a setter. */
        JS_ASSERT(flags == JSPROP_GETTER || flags == JSPROP_SETTER);
        attrs |= flags | JSPROP_SHARED;
        rval.setUndefined();
        if (flags == JSPROP_GETTER)
            getter = CastAsPropertyOp(obj);
        else
            setter = CastAsPropertyOp(obj);
    }

    /*
     * We define the function as a property of the variable object and not the
     * current scope chain even for the case of function expression statements
     * and functions defined by eval inside let or with blocks.
     */
    JSObject *parent = fp->varobj(cx);
    JS_ASSERT(parent);

    /*
     * Check for a const property of the same name -- or any kind of property
     * if executing with the strict option.  We check here at runtime as well
     * as at compile-time, to handle eval as well as multiple HTML script tags.
     */
    jsid id = ATOM_TO_JSID(fun->atom);
    prop = NULL;
    JSBool ok = CheckRedeclaration(cx, parent, id, attrs, &pobj, &prop);
    if (!ok)
        goto restore_scope;

    /*
     * We deviate from 10.1.2 in ECMA 262 v3 and under eval use for function
     * declarations JSObject::setProperty, not JSObject::defineProperty, to
     * preserve the JSOP_PERMANENT attribute of existing properties and make
     * sure that such properties cannot be deleted.
     *
     * We also use JSObject::setProperty for the existing properties of Call
     * objects with matching attributes to preserve the native getters and
     * setters that store the value of the property in the interpreter frame,
     * see bug 467495.
     */
    doSet = (attrs == JSPROP_ENUMERATE);
    JS_ASSERT_IF(doSet, fp->flags & JSFRAME_EVAL);
    if (prop) {
        if (parent == pobj &&
            parent->getClass() == &js_CallClass &&
            (old = ((JSScopeProperty *) prop)->attributes(),
             !(old & (JSPROP_GETTER|JSPROP_SETTER)) &&
             (old & (JSPROP_ENUMERATE|JSPROP_PERMANENT)) == attrs)) {
            /*
             * js_CheckRedeclaration must reject attempts to add a getter or
             * setter to an existing property without a getter or setter.
             */
            JS_ASSERT(!(attrs & ~(JSPROP_ENUMERATE|JSPROP_PERMANENT)));
            JS_ASSERT(!(old & JSPROP_READONLY));
            doSet = JS_TRUE;
        }
        pobj->dropProperty(cx, prop);
    }
    ok = doSet
         ? parent->setProperty(cx, id, &rval)
         : parent->defineProperty(cx, id, rval, getter, setter, attrs);

  restore_scope:
    /* Restore fp->scopeChain now that obj is defined in fp->callobj. */
    fp->scopeChain = obj2;
    if (!ok)
        goto error;
}
END_CASE(JSOP_DEFFUN)

BEGIN_CASE(JSOP_DEFFUN_FC)
BEGIN_CASE(JSOP_DEFFUN_DBGFC)
{
    JSFunction *fun;
    LOAD_FUNCTION(0);

    JSObject *obj = (op == JSOP_DEFFUN_FC)
                    ? js_NewFlatClosure(cx, fun)
                    : js_NewDebuggableFlatClosure(cx, fun);
    if (!obj)
        goto error;

    Value rval = ObjectValue(*obj);

    uintN attrs = (fp->flags & JSFRAME_EVAL)
                  ? JSPROP_ENUMERATE
                  : JSPROP_ENUMERATE | JSPROP_PERMANENT;

    uintN flags = JSFUN_GSFLAG2ATTR(fun->flags);
    if (flags) {
        attrs |= flags | JSPROP_SHARED;
        rval.setUndefined();
    }

    JSObject *parent = fp->varobj(cx);
    JS_ASSERT(parent);

    jsid id = ATOM_TO_JSID(fun->atom);
    JSBool ok = CheckRedeclaration(cx, parent, id, attrs, NULL, NULL);
    if (ok) {
        if (attrs == JSPROP_ENUMERATE) {
            JS_ASSERT(fp->flags & JSFRAME_EVAL);
            ok = parent->setProperty(cx, id, &rval);
        } else {
            JS_ASSERT(attrs & JSPROP_PERMANENT);

            ok = parent->defineProperty(cx, id, rval,
                                        (flags & JSPROP_GETTER)
                                        ? CastAsPropertyOp(obj)
                                        : PropertyStub,
                                        (flags & JSPROP_SETTER)
                                        ? CastAsPropertyOp(obj)
                                        : PropertyStub,
                                        attrs);
        }
    }

    if (!ok)
        goto error;
}
END_CASE(JSOP_DEFFUN_FC)

BEGIN_CASE(JSOP_DEFLOCALFUN)
{
    /*
     * Define a local function (i.e., one nested at the top level of another
     * function), parented by the current scope chain, stored in a local
     * variable slot that the compiler allocated.  This is an optimization over
     * JSOP_DEFFUN that avoids requiring a call object for the outer function's
     * activation.
     */
    JSFunction *fun;
    LOAD_FUNCTION(SLOTNO_LEN);
    JS_ASSERT(fun->isInterpreted());
    JS_ASSERT(!FUN_FLAT_CLOSURE(fun));
    JSObject *obj = FUN_OBJECT(fun);

    if (FUN_NULL_CLOSURE(fun)) {
        obj = CloneFunctionObject(cx, fun, fp->scopeChain);
        if (!obj)
            goto error;
    } else {
        JSObject *parent = js_GetScopeChain(cx, fp);
        if (!parent)
            goto error;

        if (obj->getParent() != parent) {
#ifdef JS_TRACER
            if (TRACE_RECORDER(cx))
                AbortRecording(cx, "DEFLOCALFUN for closure");
#endif
            obj = CloneFunctionObject(cx, fun, parent);
            if (!obj)
                goto error;
        }
    }

    uint32 slot = GET_SLOTNO(regs.pc);
    TRACE_2(DefLocalFunSetSlot, slot, obj);

    fp->slots()[slot].setObject(*obj);
}
END_CASE(JSOP_DEFLOCALFUN)

BEGIN_CASE(JSOP_DEFLOCALFUN_FC)
{
    JSFunction *fun;
    LOAD_FUNCTION(SLOTNO_LEN);

    JSObject *obj = js_NewFlatClosure(cx, fun);
    if (!obj)
        goto error;

    uint32 slot = GET_SLOTNO(regs.pc);
    TRACE_2(DefLocalFunSetSlot, slot, obj);

    fp->slots()[slot].setObject(*obj);
}
END_CASE(JSOP_DEFLOCALFUN_FC)

BEGIN_CASE(JSOP_DEFLOCALFUN_DBGFC)
{
    JSFunction *fun;
    LOAD_FUNCTION(SLOTNO_LEN);

    JSObject *obj = js_NewDebuggableFlatClosure(cx, fun);
    if (!obj)
        goto error;

    uint32 slot = GET_SLOTNO(regs.pc);
    fp->slots()[slot].setObject(*obj);
}
END_CASE(JSOP_DEFLOCALFUN_DBGFC)

BEGIN_CASE(JSOP_LAMBDA)
{
    /* Load the specified function object literal. */
    JSFunction *fun;
    LOAD_FUNCTION(0);
    JSObject *obj = FUN_OBJECT(fun);

    /* do-while(0) so we can break instead of using a goto. */
    do {
        JSObject *parent;
        if (FUN_NULL_CLOSURE(fun)) {
            parent = fp->scopeChain;

            if (obj->getParent() == parent) {
                op = JSOp(regs.pc[JSOP_LAMBDA_LENGTH]);

                /*
                 * Optimize ({method: function () { ... }, ...}) and
                 * this.method = function () { ... }; bytecode sequences.
                 */
                if (op == JSOP_SETMETHOD) {
#ifdef DEBUG
                    JSOp op2 = JSOp(regs.pc[JSOP_LAMBDA_LENGTH + JSOP_SETMETHOD_LENGTH]);
                    JS_ASSERT(op2 == JSOP_POP || op2 == JSOP_POPV);
#endif

                    const Value &lref = regs.sp[-1];
                    if (lref.isObject() &&
                        lref.toObject().getClass() == &js_ObjectClass) {
                        break;
                    }
                } else if (op == JSOP_INITMETHOD) {
#ifdef DEBUG
                    const Value &lref = regs.sp[-1];
                    JS_ASSERT(lref.isObject());
                    JSObject *obj2 = &lref.toObject();
                    JS_ASSERT(obj2->getClass() == &js_ObjectClass);
                    JS_ASSERT(obj2->scope()->object == obj2);
#endif
                    break;
                }
            }
        } else {
            parent = js_GetScopeChain(cx, fp);
            if (!parent)
                goto error;
        }

        obj = CloneFunctionObject(cx, fun, parent);
        if (!obj)
            goto error;
    } while (0);

    PUSH_OBJECT(*obj);
}
END_CASE(JSOP_LAMBDA)

BEGIN_CASE(JSOP_LAMBDA_FC)
{
    JSFunction *fun;
    LOAD_FUNCTION(0);

    JSObject *obj = js_NewFlatClosure(cx, fun);
    if (!obj)
        goto error;

    PUSH_OBJECT(*obj);
}
END_CASE(JSOP_LAMBDA_FC)

BEGIN_CASE(JSOP_LAMBDA_DBGFC)
{
    JSFunction *fun;
    LOAD_FUNCTION(0);

    JSObject *obj = js_NewDebuggableFlatClosure(cx, fun);
    if (!obj)
        goto error;

    PUSH_OBJECT(*obj);
}
END_CASE(JSOP_LAMBDA_DBGFC)

BEGIN_CASE(JSOP_CALLEE)
    PUSH_COPY(fp->argv[-2]);
END_CASE(JSOP_CALLEE)

BEGIN_CASE(JSOP_GETTER)
BEGIN_CASE(JSOP_SETTER)
{
  do_getter_setter:
    JSOp op2 = (JSOp) *++regs.pc;
    jsid id;
    Value rval;
    jsint i;
    JSObject *obj;
    switch (op2) {
      case JSOP_INDEXBASE:
        atoms += GET_INDEXBASE(regs.pc);
        regs.pc += JSOP_INDEXBASE_LENGTH - 1;
        goto do_getter_setter;
      case JSOP_INDEXBASE1:
      case JSOP_INDEXBASE2:
      case JSOP_INDEXBASE3:
        atoms += (op2 - JSOP_INDEXBASE1 + 1) << 16;
        goto do_getter_setter;

      case JSOP_SETNAME:
      case JSOP_SETPROP:
      {
        JSAtom *atom;
        LOAD_ATOM(0, atom);
        id = ATOM_TO_JSID(atom);
        rval = regs.sp[-1];
        i = -1;
        goto gs_pop_lval;
      }
      case JSOP_SETELEM:
        rval = regs.sp[-1];
        id = JSID_VOID;
        i = -2;
      gs_pop_lval:
        FETCH_OBJECT(cx, i - 1, obj);
        break;

      case JSOP_INITPROP:
      {
        JS_ASSERT(regs.sp - fp->base() >= 2);
        rval = regs.sp[-1];
        i = -1;
        JSAtom *atom;
        LOAD_ATOM(0, atom);
        id = ATOM_TO_JSID(atom);
        goto gs_get_lval;
      }
      default:
        JS_ASSERT(op2 == JSOP_INITELEM);

        JS_ASSERT(regs.sp - fp->base() >= 3);
        rval = regs.sp[-1];
        id = JSID_VOID;
        i = -2;
      gs_get_lval:
      {
        const Value &lref = regs.sp[i-1];
        JS_ASSERT(lref.isObject());
        obj = &lref.toObject();
        break;
      }
    }

    /* Ensure that id has a type suitable for use with obj. */
    if (JSID_IS_VOID(id))
        FETCH_ELEMENT_ID(obj, i, id);

    if (!js_IsCallable(rval)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_BAD_GETTER_OR_SETTER,
                             (op == JSOP_GETTER)
                             ? js_getter_str
                             : js_setter_str);
        goto error;
    }

    /*
     * Getters and setters are just like watchpoints from an access control
     * point of view.
     */
    Value rtmp;
    uintN attrs;
    if (!CheckAccess(cx, obj, id, JSACC_WATCH, &rtmp, &attrs))
        goto error;

    PropertyOp getter, setter;
    if (op == JSOP_GETTER) {
        getter = CastAsPropertyOp(&rval.toObject());
        setter = PropertyStub;
        attrs = JSPROP_GETTER;
    } else {
        getter = PropertyStub;
        setter = CastAsPropertyOp(&rval.toObject());
        attrs = JSPROP_SETTER;
    }
    attrs |= JSPROP_ENUMERATE | JSPROP_SHARED;

    /* Check for a readonly or permanent property of the same name. */
    if (!CheckRedeclaration(cx, obj, id, attrs, NULL, NULL))
        goto error;

    if (!obj->defineProperty(cx, id, UndefinedValue(), getter, setter, attrs))
        goto error;

    regs.sp += i;
    if (js_CodeSpec[op2].ndefs > js_CodeSpec[op2].nuses) {
        JS_ASSERT(js_CodeSpec[op2].ndefs == js_CodeSpec[op2].nuses + 1);
        regs.sp[-1] = rval;
    }
    len = js_CodeSpec[op2].length;
    DO_NEXT_OP(len);
}

BEGIN_CASE(JSOP_HOLE)
    PUSH_HOLE();
END_CASE(JSOP_HOLE)

BEGIN_CASE(JSOP_NEWARRAY)
{
    len = GET_UINT16(regs.pc);
    cx->assertValidStackDepth(len);
    JSObject *obj = js_NewArrayObject(cx, len, regs.sp - len);
    if (!obj)
        goto error;
    regs.sp -= len - 1;
    regs.sp[-1].setObject(*obj);
}
END_CASE(JSOP_NEWARRAY)

BEGIN_CASE(JSOP_NEWINIT)
{
    jsint i = GET_INT8(regs.pc);
    JS_ASSERT(i == JSProto_Array || i == JSProto_Object);
    JSObject *obj;
    if (i == JSProto_Array) {
        obj = js_NewArrayObject(cx, 0, NULL);
        if (!obj)
            goto error;
    } else {
        obj = NewBuiltinClassInstance(cx, &js_ObjectClass);
        if (!obj)
            goto error;

        if (regs.pc[JSOP_NEWINIT_LENGTH] != JSOP_ENDINIT) {
            JS_LOCK_OBJ(cx, obj);
            JSScope *scope = js_GetMutableScope(cx, obj);
            if (!scope) {
                JS_UNLOCK_OBJ(cx, obj);
                goto error;
            }

            /*
             * We cannot assume that js_GetMutableScope above creates a scope
             * owned by cx and skip JS_UNLOCK_SCOPE. A new object debugger
             * hook may add properties to the newly created object, suspend
             * the current request and share the object with other threads.
             */
            JS_UNLOCK_SCOPE(cx, scope);
        }
    }

    PUSH_OBJECT(*obj);
    CHECK_INTERRUPT_HANDLER();
}
END_CASE(JSOP_NEWINIT)

BEGIN_CASE(JSOP_ENDINIT)
{
    /* Re-set the newborn root to the top of this object tree. */
    JS_ASSERT(regs.sp - fp->base() >= 1);
    const Value &lref = regs.sp[-1];
    JS_ASSERT(lref.isObject());
    cx->weakRoots.finalizableNewborns[FINALIZE_OBJECT] = &lref.toObject();
}
END_CASE(JSOP_ENDINIT)

BEGIN_CASE(JSOP_INITPROP)
BEGIN_CASE(JSOP_INITMETHOD)
{
    /* Load the property's initial value into rval. */
    JS_ASSERT(regs.sp - fp->base() >= 2);
    Value rval = regs.sp[-1];

    /* Load the object being initialized into lval/obj. */
    JSObject *obj = &regs.sp[-2].toObject();
    JS_ASSERT(obj->isNative());

    JSScope *scope = obj->scope();

    /*
     * Probe the property cache.
     *
     * We can not assume that the object created by JSOP_NEWINIT is still
     * single-threaded as the debugger can access it from other threads.
     * So check first.
     *
     * On a hit, if the cached sprop has a non-default setter, it must be
     * __proto__. If sprop->parent != scope->lastProperty(), there is a
     * repeated property name. The fast path does not handle these two cases.
     */
    PropertyCacheEntry *entry;
    JSScopeProperty *sprop;
    if (CX_OWNS_OBJECT_TITLE(cx, obj) &&
        JS_PROPERTY_CACHE(cx).testForInit(rt, regs.pc, obj, scope, &sprop, &entry) &&
        sprop->hasDefaultSetter() &&
        sprop->parent == scope->lastProperty())
    {
        /* Fast path. Property cache hit. */
        uint32 slot = sprop->slot;
        JS_ASSERT(slot == scope->freeslot);
        if (slot < obj->numSlots()) {
            ++scope->freeslot;
        } else {
            if (!js_AllocSlot(cx, obj, &slot))
                goto error;
            JS_ASSERT(slot == sprop->slot);
        }

        JS_ASSERT(!scope->lastProperty() ||
                  scope->shape == scope->lastProperty()->shape);
        if (scope->table) {
            JSScopeProperty *sprop2 =
                scope->addProperty(cx, sprop->id, sprop->getter(), sprop->setter(), slot,
                                   sprop->attributes(), sprop->getFlags(), sprop->shortid);
            if (!sprop2) {
                js_FreeSlot(cx, obj, slot);
                goto error;
            }
            JS_ASSERT(sprop2 == sprop);
        } else {
            JS_ASSERT(!scope->isSharedEmpty());
            scope->extend(cx, sprop);
        }

        /*
         * No method change check here because here we are adding a new
         * property, not updating an existing slot's value that might
         * contain a method of a branded scope.
         */
        TRACE_2(SetPropHit, entry, sprop);
        obj->lockedSetSlot(slot, rval);
    } else {
        PCMETER(JS_PROPERTY_CACHE(cx).inipcmisses++);

        /* Get the immediate property name into id. */
        JSAtom *atom;
        LOAD_ATOM(0, atom);
        jsid id = ATOM_TO_JSID(atom);

        /* Set the property named by obj[id] to rval. */
        if (!CheckRedeclaration(cx, obj, id, JSPROP_INITIALIZER, NULL, NULL))
            goto error;

        uintN defineHow = (op == JSOP_INITMETHOD)
                          ? JSDNP_CACHE_RESULT | JSDNP_SET_METHOD
                          : JSDNP_CACHE_RESULT;
        if (!(JS_UNLIKELY(atom == cx->runtime->atomState.protoAtom)
              ? js_SetPropertyHelper(cx, obj, id, defineHow, &rval)
              : js_DefineNativeProperty(cx, obj, id, rval, NULL, NULL,
                                        JSPROP_ENUMERATE, 0, 0, NULL,
                                        defineHow))) {
            goto error;
        }
    }

    /* Common tail for property cache hit and miss cases. */
    regs.sp--;
}
END_CASE(JSOP_INITPROP);

BEGIN_CASE(JSOP_INITELEM)
{
    /* Pop the element's value into rval. */
    JS_ASSERT(regs.sp - fp->base() >= 3);
    const Value &rref = regs.sp[-1];

    /* Find the object being initialized at top of stack. */
    const Value &lref = regs.sp[-3];
    JS_ASSERT(lref.isObject());
    JSObject *obj = &lref.toObject();

    /* Fetch id now that we have obj. */
    jsid id;
    FETCH_ELEMENT_ID(obj, -2, id);

    /*
     * Check for property redeclaration strict warning (we may be in an object
     * initialiser, not an array initialiser).
     */
    if (!CheckRedeclaration(cx, obj, id, JSPROP_INITIALIZER, NULL, NULL))
        goto error;

    /*
     * If rref is a hole, do not call JSObject::defineProperty. In this case,
     * obj must be an array, so if the current op is the last element
     * initialiser, set the array length to one greater than id.
     */
    if (rref.isMagic(JS_ARRAY_HOLE)) {
        JS_ASSERT(obj->isArray());
        JS_ASSERT(JSID_IS_INT(id));
        JS_ASSERT(jsuint(JSID_TO_INT(id)) < JS_ARGS_LENGTH_MAX);
        if (js_GetOpcode(cx, script, regs.pc + JSOP_INITELEM_LENGTH) == JSOP_ENDINIT &&
            !js_SetLengthProperty(cx, obj, (jsuint) (JSID_TO_INT(id) + 1))) {
            goto error;
        }
    } else {
        if (!obj->defineProperty(cx, id, rref, NULL, NULL, JSPROP_ENUMERATE))
            goto error;
    }
    regs.sp -= 2;
}
END_CASE(JSOP_INITELEM)

#if JS_HAS_SHARP_VARS

BEGIN_CASE(JSOP_DEFSHARP)
{
    uint32 slot = GET_UINT16(regs.pc);
    JS_ASSERT(slot + 1 < fp->script->nfixed);
    const Value &lref = fp->slots()[slot];
    JSObject *obj;
    if (lref.isObject()) {
        obj = &lref.toObject();
    } else {
        JS_ASSERT(lref.isUndefined());
        obj = js_NewArrayObject(cx, 0, NULL);
        if (!obj)
            goto error;
        fp->slots()[slot].setObject(*obj);
    }
    jsint i = (jsint) GET_UINT16(regs.pc + UINT16_LEN);
    jsid id = INT_TO_JSID(i);
    const Value &rref = regs.sp[-1];
    if (rref.isPrimitive()) {
        char numBuf[12];
        JS_snprintf(numBuf, sizeof numBuf, "%u", (unsigned) i);
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_BAD_SHARP_DEF, numBuf);
        goto error;
    }
    if (!obj->defineProperty(cx, id, rref, NULL, NULL, JSPROP_ENUMERATE))
        goto error;
}
END_CASE(JSOP_DEFSHARP)

BEGIN_CASE(JSOP_USESHARP)
{
    uint32 slot = GET_UINT16(regs.pc);
    JS_ASSERT(slot + 1 < fp->script->nfixed);
    const Value &lref = fp->slots()[slot];
    jsint i = (jsint) GET_UINT16(regs.pc + UINT16_LEN);
    Value rval;
    if (lref.isUndefined()) {
        rval.setUndefined();
    } else {
        JSObject *obj = &fp->slots()[slot].toObject();
        jsid id = INT_TO_JSID(i);
        if (!obj->getProperty(cx, id, &rval))
            goto error;
    }
    if (!rval.isObjectOrNull()) {
        char numBuf[12];

        JS_snprintf(numBuf, sizeof numBuf, "%u", (unsigned) i);
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_BAD_SHARP_USE, numBuf);
        goto error;
    }
    PUSH_COPY(rval);
}
END_CASE(JSOP_USESHARP)

BEGIN_CASE(JSOP_SHARPINIT)
{
    uint32 slot = GET_UINT16(regs.pc);
    JS_ASSERT(slot + 1 < fp->script->nfixed);
    Value *vp = &fp->slots()[slot];
    Value rval = vp[1];

    /*
     * We peek ahead safely here because empty initialisers get zero
     * JSOP_SHARPINIT ops, and non-empty ones get two: the first comes
     * immediately after JSOP_NEWINIT followed by one or more property
     * initialisers; and the second comes directly before JSOP_ENDINIT.
     */
    if (regs.pc[JSOP_SHARPINIT_LENGTH] != JSOP_ENDINIT) {
        rval.setInt32(rval.isUndefined() ? 1 : rval.toInt32() + 1);
    } else {
        JS_ASSERT(rval.isInt32());
        rval.getInt32Ref() -= 1;
        if (rval.toInt32() == 0)
            vp[0].setUndefined();
    }
    vp[1] = rval;
}
END_CASE(JSOP_SHARPINIT)

#endif /* JS_HAS_SHARP_VARS */

{
BEGIN_CASE(JSOP_GOSUB)
    PUSH_BOOLEAN(false);
    jsint i = (regs.pc - script->main) + JSOP_GOSUB_LENGTH;
    PUSH_INT32(i);
    len = GET_JUMP_OFFSET(regs.pc);
END_VARLEN_CASE
}

{
BEGIN_CASE(JSOP_GOSUBX)
    PUSH_BOOLEAN(false);
    jsint i = (regs.pc - script->main) + JSOP_GOSUBX_LENGTH;
    len = GET_JUMPX_OFFSET(regs.pc);
    PUSH_INT32(i);
END_VARLEN_CASE
}

{
BEGIN_CASE(JSOP_RETSUB)
    /* Pop [exception or hole, retsub pc-index]. */
    Value rval, lval;
    POP_COPY_TO(rval);
    POP_COPY_TO(lval);
    JS_ASSERT(lval.isBoolean());
    if (lval.toBoolean()) {
        /*
         * Exception was pending during finally, throw it *before* we adjust
         * pc, because pc indexes into script->trynotes.  This turns out not to
         * be necessary, but it seems clearer.  And it points out a FIXME:
         * 350509, due to Igor Bukanov.
         */
        cx->throwing = JS_TRUE;
        cx->exception = rval;
        goto error;
    }
    JS_ASSERT(rval.isInt32());
    len = rval.toInt32();
    regs.pc = script->main;
END_VARLEN_CASE
}

BEGIN_CASE(JSOP_EXCEPTION)
    JS_ASSERT(cx->throwing);
    PUSH_COPY(cx->exception);
    cx->throwing = JS_FALSE;
    CHECK_BRANCH();
END_CASE(JSOP_EXCEPTION)

BEGIN_CASE(JSOP_FINALLY)
    CHECK_BRANCH();
END_CASE(JSOP_FINALLY)

BEGIN_CASE(JSOP_THROWING)
    JS_ASSERT(!cx->throwing);
    cx->throwing = JS_TRUE;
    POP_COPY_TO(cx->exception);
END_CASE(JSOP_THROWING)

BEGIN_CASE(JSOP_THROW)
    JS_ASSERT(!cx->throwing);
    CHECK_BRANCH();
    cx->throwing = JS_TRUE;
    POP_COPY_TO(cx->exception);
    /* let the code at error try to catch the exception. */
    goto error;

BEGIN_CASE(JSOP_SETLOCALPOP)
{
    /*
     * The stack must have a block with at least one local slot below the
     * exception object.
     */
    JS_ASSERT((size_t) (regs.sp - fp->base()) >= 2);
    uint32 slot = GET_UINT16(regs.pc);
    JS_ASSERT(slot + 1 < script->nslots);
    POP_COPY_TO(fp->slots()[slot]);
}
END_CASE(JSOP_SETLOCALPOP)

BEGIN_CASE(JSOP_IFPRIMTOP)
    /*
     * If the top of stack is of primitive type, jump to our target. Otherwise
     * advance to the next opcode.
     */
    JS_ASSERT(regs.sp > fp->base());
    if (regs.sp[-1].isPrimitive()) {
        len = GET_JUMP_OFFSET(regs.pc);
        BRANCH(len);
    }
END_CASE(JSOP_IFPRIMTOP)

BEGIN_CASE(JSOP_PRIMTOP)
    JS_ASSERT(regs.sp > fp->base());
    if (regs.sp[-1].isObject()) {
        jsint i = GET_INT8(regs.pc);
        js_ReportValueError2(cx, JSMSG_CANT_CONVERT_TO, -2, regs.sp[-2], NULL,
                             (i == JSTYPE_VOID) ? "primitive type" : JS_TYPE_STR(i));
        goto error;
    }
END_CASE(JSOP_PRIMTOP)

BEGIN_CASE(JSOP_OBJTOP)
    if (regs.sp[-1].isPrimitive()) {
        js_ReportValueError(cx, GET_UINT16(regs.pc), -1, regs.sp[-1], NULL);
        goto error;
    }
END_CASE(JSOP_OBJTOP)

BEGIN_CASE(JSOP_INSTANCEOF)
{
    const Value &rref = regs.sp[-1];
    if (rref.isPrimitive()) {
        js_ReportValueError(cx, JSMSG_BAD_INSTANCEOF_RHS, -1, rref, NULL);
        goto error;
    }
    JSObject *obj = &rref.toObject();
    const Value &lref = regs.sp[-2];
    JSBool cond = JS_FALSE;
    if (!js_HasInstance(cx, obj, &lref, &cond))
        goto error;
    regs.sp--;
    regs.sp[-1].setBoolean(cond);
}
END_CASE(JSOP_INSTANCEOF)

#if JS_HAS_DEBUGGER_KEYWORD
BEGIN_CASE(JSOP_DEBUGGER)
{
    JSDebuggerHandler handler = cx->debugHooks->debuggerHandler;
    if (handler) {
        Value rval;
        switch (handler(cx, script, regs.pc, Jsvalify(&rval), cx->debugHooks->debuggerHandlerData)) {
        case JSTRAP_ERROR:
            goto error;
        case JSTRAP_CONTINUE:
            break;
        case JSTRAP_RETURN:
            fp->rval = rval;
            interpReturnOK = JS_TRUE;
            goto forced_return;
        case JSTRAP_THROW:
            cx->throwing = JS_TRUE;
            cx->exception = rval;
            goto error;
        default:;
        }
        CHECK_INTERRUPT_HANDLER();
    }
}
END_CASE(JSOP_DEBUGGER)
#endif /* JS_HAS_DEBUGGER_KEYWORD */

#if JS_HAS_XML_SUPPORT
BEGIN_CASE(JSOP_DEFXMLNS)
{
    Value rval;
    POP_COPY_TO(rval);
    if (!js_SetDefaultXMLNamespace(cx, rval))
        goto error;
}
END_CASE(JSOP_DEFXMLNS)

BEGIN_CASE(JSOP_ANYNAME)
{
    jsid id;
    if (!js_GetAnyName(cx, &id))
        goto error;
    PUSH_COPY(IdToValue(id));
}
END_CASE(JSOP_ANYNAME)

BEGIN_CASE(JSOP_QNAMEPART)
{
    JSAtom *atom;
    LOAD_ATOM(0, atom);
    PUSH_STRING(ATOM_TO_STRING(atom));
}
END_CASE(JSOP_QNAMEPART)

BEGIN_CASE(JSOP_QNAMECONST)
{
    JSAtom *atom;
    LOAD_ATOM(0, atom);
    Value rval = StringValue(ATOM_TO_STRING(atom));
    Value lval = regs.sp[-1];
    JSObject *obj = js_ConstructXMLQNameObject(cx, lval, rval);
    if (!obj)
        goto error;
    regs.sp[-1].setObject(*obj);
}
END_CASE(JSOP_QNAMECONST)

BEGIN_CASE(JSOP_QNAME)
{
    Value rval = regs.sp[-1];
    Value lval = regs.sp[-2];
    JSObject *obj = js_ConstructXMLQNameObject(cx, lval, rval);
    if (!obj)
        goto error;
    regs.sp--;
    regs.sp[-1].setObject(*obj);
}
END_CASE(JSOP_QNAME)

BEGIN_CASE(JSOP_TOATTRNAME)
{
    Value rval;
    rval = regs.sp[-1];
    if (!js_ToAttributeName(cx, &rval))
        goto error;
    regs.sp[-1] = rval;
}
END_CASE(JSOP_TOATTRNAME)

BEGIN_CASE(JSOP_TOATTRVAL)
{
    Value rval;
    rval = regs.sp[-1];
    JS_ASSERT(rval.isString());
    JSString *str = js_EscapeAttributeValue(cx, rval.toString(), JS_FALSE);
    if (!str)
        goto error;
    regs.sp[-1].setString(str);
}
END_CASE(JSOP_TOATTRVAL)

BEGIN_CASE(JSOP_ADDATTRNAME)
BEGIN_CASE(JSOP_ADDATTRVAL)
{
    Value rval = regs.sp[-1];
    Value lval = regs.sp[-2];
    JSString *str = lval.toString();
    JSString *str2 = rval.toString();
    str = js_AddAttributePart(cx, op == JSOP_ADDATTRNAME, str, str2);
    if (!str)
        goto error;
    regs.sp--;
    regs.sp[-1].setString(str);
}
END_CASE(JSOP_ADDATTRNAME)

BEGIN_CASE(JSOP_BINDXMLNAME)
{
    Value lval;
    lval = regs.sp[-1];
    JSObject *obj;
    jsid id;
    if (!js_FindXMLProperty(cx, lval, &obj, &id))
        goto error;
    regs.sp[-1].setObjectOrNull(obj);
    PUSH_COPY(IdToValue(id));
}
END_CASE(JSOP_BINDXMLNAME)

BEGIN_CASE(JSOP_SETXMLNAME)
{
    JSObject *obj = &regs.sp[-3].toObject();
    Value rval = regs.sp[-1];
    jsid id;
    FETCH_ELEMENT_ID(obj, -2, id);
    if (!obj->setProperty(cx, id, &rval))
        goto error;
    rval = regs.sp[-1];
    regs.sp -= 2;
    regs.sp[-1] = rval;
}
END_CASE(JSOP_SETXMLNAME)

BEGIN_CASE(JSOP_CALLXMLNAME)
BEGIN_CASE(JSOP_XMLNAME)
{
    Value lval = regs.sp[-1];
    JSObject *obj;
    jsid id;
    if (!js_FindXMLProperty(cx, lval, &obj, &id))
        goto error;
    Value rval;
    if (!obj->getProperty(cx, id, &rval))
        goto error;
    regs.sp[-1] = rval;
    if (op == JSOP_CALLXMLNAME)
        PUSH_OBJECT(*obj);
}
END_CASE(JSOP_XMLNAME)

BEGIN_CASE(JSOP_DESCENDANTS)
BEGIN_CASE(JSOP_DELDESC)
{
    JSObject *obj;
    FETCH_OBJECT(cx, -2, obj);
    jsval rval = Jsvalify(regs.sp[-1]);
    if (!js_GetXMLDescendants(cx, obj, rval, &rval))
        goto error;

    if (op == JSOP_DELDESC) {
        regs.sp[-1] = Valueify(rval);   /* set local root */
        if (!js_DeleteXMLListElements(cx, JSVAL_TO_OBJECT(rval)))
            goto error;
        rval = JSVAL_TRUE;                  /* always succeed */
    }

    regs.sp--;
    regs.sp[-1] = Valueify(rval);
}
END_CASE(JSOP_DESCENDANTS)

{
BEGIN_CASE(JSOP_FILTER)
    /*
     * We push the hole value before jumping to [enditer] so we can detect the
     * first iteration and direct js_StepXMLListFilter to initialize filter's
     * state.
     */
    PUSH_HOLE();
    len = GET_JUMP_OFFSET(regs.pc);
    JS_ASSERT(len > 0);
END_VARLEN_CASE
}

BEGIN_CASE(JSOP_ENDFILTER)
{
    bool cond = !regs.sp[-1].isMagic();
    if (cond) {
        /* Exit the "with" block left from the previous iteration. */
        js_LeaveWith(cx);
    }
    if (!js_StepXMLListFilter(cx, cond))
        goto error;
    if (!regs.sp[-1].isNull()) {
        /*
         * Decrease sp after EnterWith returns as we use sp[-1] there to root
         * temporaries.
         */
        JS_ASSERT(IsXML(regs.sp[-1]));
        if (!js_EnterWith(cx, -2))
            goto error;
        regs.sp--;
        len = GET_JUMP_OFFSET(regs.pc);
        JS_ASSERT(len < 0);
        BRANCH(len);
    }
    regs.sp--;
}
END_CASE(JSOP_ENDFILTER);

BEGIN_CASE(JSOP_TOXML)
{
    Value rval = regs.sp[-1];
    JSObject *obj = js_ValueToXMLObject(cx, rval);
    if (!obj)
        goto error;
    regs.sp[-1].setObject(*obj);
}
END_CASE(JSOP_TOXML)

BEGIN_CASE(JSOP_TOXMLLIST)
{
    Value rval = regs.sp[-1];
    JSObject *obj = js_ValueToXMLListObject(cx, rval);
    if (!obj)
        goto error;
    regs.sp[-1].setObject(*obj);
}
END_CASE(JSOP_TOXMLLIST)

BEGIN_CASE(JSOP_XMLTAGEXPR)
{
    Value rval = regs.sp[-1];
    JSString *str = js_ValueToString(cx, rval);
    if (!str)
        goto error;
    regs.sp[-1].setString(str);
}
END_CASE(JSOP_XMLTAGEXPR)

BEGIN_CASE(JSOP_XMLELTEXPR)
{
    Value rval = regs.sp[-1];
    JSString *str;
    if (IsXML(rval)) {
        str = js_ValueToXMLString(cx, rval);
    } else {
        str = js_ValueToString(cx, rval);
        if (str)
            str = js_EscapeElementValue(cx, str);
    }
    if (!str)
        goto error;
    regs.sp[-1].setString(str);
}
END_CASE(JSOP_XMLELTEXPR)

BEGIN_CASE(JSOP_XMLCDATA)
{
    JSAtom *atom;
    LOAD_ATOM(0, atom);
    JSString *str = ATOM_TO_STRING(atom);
    JSObject *obj = js_NewXMLSpecialObject(cx, JSXML_CLASS_TEXT, NULL, str);
    if (!obj)
        goto error;
    PUSH_OBJECT(*obj);
}
END_CASE(JSOP_XMLCDATA)

BEGIN_CASE(JSOP_XMLCOMMENT)
{
    JSAtom *atom;
    LOAD_ATOM(0, atom);
    JSString *str = ATOM_TO_STRING(atom);
    JSObject *obj = js_NewXMLSpecialObject(cx, JSXML_CLASS_COMMENT, NULL, str);
    if (!obj)
        goto error;
    PUSH_OBJECT(*obj);
}
END_CASE(JSOP_XMLCOMMENT)

BEGIN_CASE(JSOP_XMLPI)
{
    JSAtom *atom;
    LOAD_ATOM(0, atom);
    JSString *str = ATOM_TO_STRING(atom);
    Value rval = regs.sp[-1];
    JSString *str2 = rval.toString();
    JSObject *obj = js_NewXMLSpecialObject(cx, JSXML_CLASS_PROCESSING_INSTRUCTION, str, str2);
    if (!obj)
        goto error;
    regs.sp[-1].setObject(*obj);
}
END_CASE(JSOP_XMLPI)

BEGIN_CASE(JSOP_GETFUNNS)
{
    Value rval;
    if (!js_GetFunctionNamespace(cx, &rval))
        goto error;
    PUSH_COPY(rval);
}
END_CASE(JSOP_GETFUNNS)
#endif /* JS_HAS_XML_SUPPORT */

BEGIN_CASE(JSOP_ENTERBLOCK)
{
    JSObject *obj;
    LOAD_OBJECT(0, obj);
    JS_ASSERT(!OBJ_IS_CLONED_BLOCK(obj));
    JS_ASSERT(fp->base() + OBJ_BLOCK_DEPTH(cx, obj) == regs.sp);
    Value *vp = regs.sp + OBJ_BLOCK_COUNT(cx, obj);
    JS_ASSERT(regs.sp < vp);
    JS_ASSERT(vp <= fp->slots() + script->nslots);
    SetValueRangeToUndefined(regs.sp, vp);
    regs.sp = vp;

#ifdef DEBUG
    JS_ASSERT(fp->blockChain == obj->getParent());

    /*
     * The young end of fp->scopeChain may omit blocks if we haven't closed
     * over them, but if there are any closure blocks on fp->scopeChain, they'd
     * better be (clones of) ancestors of the block we're entering now;
     * anything else we should have popped off fp->scopeChain when we left its
     * static scope.
     */
    JSObject *obj2 = fp->scopeChain;
    Class *clasp;
    while ((clasp = obj2->getClass()) == &js_WithClass)
        obj2 = obj2->getParent();
    if (clasp == &js_BlockClass &&
        obj2->getPrivate() == js_FloatingFrameIfGenerator(cx, fp)) {
        JSObject *youngestProto = obj2->getProto();
        JS_ASSERT(!OBJ_IS_CLONED_BLOCK(youngestProto));
        JSObject *parent = obj;
        while ((parent = parent->getParent()) != youngestProto)
            JS_ASSERT(parent);
    }
#endif

    fp->blockChain = obj;
}
END_CASE(JSOP_ENTERBLOCK)

BEGIN_CASE(JSOP_LEAVEBLOCKEXPR)
BEGIN_CASE(JSOP_LEAVEBLOCK)
{
#ifdef DEBUG
    JS_ASSERT(fp->blockChain->getClass() == &js_BlockClass);
    uintN blockDepth = OBJ_BLOCK_DEPTH(cx, fp->blockChain);

    JS_ASSERT(blockDepth <= StackDepth(script));
#endif
    /*
     * If we're about to leave the dynamic scope of a block that has been
     * cloned onto fp->scopeChain, clear its private data, move its locals from
     * the stack into the clone, and pop it off the chain.
     */
    JSObject *obj = fp->scopeChain;
    if (obj->getProto() == fp->blockChain) {
        JS_ASSERT(obj->getClass() == &js_BlockClass);
        if (!js_PutBlockObject(cx, JS_TRUE))
            goto error;
    }

    /* Pop the block chain, too.  */
    fp->blockChain = fp->blockChain->getParent();

    /* Move the result of the expression to the new topmost stack slot. */
    Value *vp = NULL;  /* silence GCC warnings */
    if (op == JSOP_LEAVEBLOCKEXPR)
        vp = &regs.sp[-1];
    regs.sp -= GET_UINT16(regs.pc);
    if (op == JSOP_LEAVEBLOCKEXPR) {
        JS_ASSERT(fp->base() + blockDepth == regs.sp - 1);
        regs.sp[-1] = *vp;
    } else {
        JS_ASSERT(fp->base() + blockDepth == regs.sp);
    }
}
END_CASE(JSOP_LEAVEBLOCK)

#if JS_HAS_GENERATORS
BEGIN_CASE(JSOP_GENERATOR)
{
    ASSERT_NOT_THROWING(cx);
    regs.pc += JSOP_GENERATOR_LENGTH;
    JSObject *obj = js_NewGenerator(cx);
    if (!obj)
        goto error;
    JS_ASSERT(!fp->callobj && !fp->argsobj);
    fp->rval.setObject(*obj);
    interpReturnOK = true;
    if (inlineCallCount != 0)
        goto inline_return;
    goto exit;
}

BEGIN_CASE(JSOP_YIELD)
    ASSERT_NOT_THROWING(cx);
    if (cx->generatorFor(fp)->state == JSGEN_CLOSING) {
        js_ReportValueError(cx, JSMSG_BAD_GENERATOR_YIELD,
                            JSDVG_SEARCH_STACK, fp->argv[-2], NULL);
        goto error;
    }
    fp->rval = regs.sp[-1];
    fp->flags |= JSFRAME_YIELDING;
    regs.pc += JSOP_YIELD_LENGTH;
    interpReturnOK = JS_TRUE;
    goto exit;

BEGIN_CASE(JSOP_ARRAYPUSH)
{
    uint32 slot = GET_UINT16(regs.pc);
    JS_ASSERT(script->nfixed <= slot);
    JS_ASSERT(slot < script->nslots);
    JSObject *obj = &fp->slots()[slot].toObject();
    if (!js_ArrayCompPush(cx, obj, regs.sp[-1]))
        goto error;
    regs.sp--;
}
END_CASE(JSOP_ARRAYPUSH)
#endif /* JS_HAS_GENERATORS */

  L_JSOP_UNUSED180:

#if JS_THREADED_INTERP
  L_JSOP_BACKPATCH:
  L_JSOP_BACKPATCH_POP:

# if !JS_HAS_GENERATORS
  L_JSOP_GENERATOR:
  L_JSOP_YIELD:
  L_JSOP_ARRAYPUSH:
# endif

# if !JS_HAS_SHARP_VARS
  L_JSOP_DEFSHARP:
  L_JSOP_USESHARP:
  L_JSOP_SHARPINIT:
# endif

# if !JS_HAS_DESTRUCTURING
  L_JSOP_ENUMCONSTELEM:
# endif

# if !JS_HAS_XML_SUPPORT
  L_JSOP_CALLXMLNAME:
  L_JSOP_STARTXMLEXPR:
  L_JSOP_STARTXML:
  L_JSOP_DELDESC:
  L_JSOP_GETFUNNS:
  L_JSOP_XMLPI:
  L_JSOP_XMLCOMMENT:
  L_JSOP_XMLCDATA:
  L_JSOP_XMLELTEXPR:
  L_JSOP_XMLTAGEXPR:
  L_JSOP_TOXMLLIST:
  L_JSOP_TOXML:
  L_JSOP_ENDFILTER:
  L_JSOP_FILTER:
  L_JSOP_DESCENDANTS:
  L_JSOP_XMLNAME:
  L_JSOP_SETXMLNAME:
  L_JSOP_BINDXMLNAME:
  L_JSOP_ADDATTRVAL:
  L_JSOP_ADDATTRNAME:
  L_JSOP_TOATTRVAL:
  L_JSOP_TOATTRNAME:
  L_JSOP_QNAME:
  L_JSOP_QNAMECONST:
  L_JSOP_QNAMEPART:
  L_JSOP_ANYNAME:
  L_JSOP_DEFXMLNS:
# endif

  L_JSOP_UNUSED218:

#endif /* !JS_THREADED_INTERP */
#if !JS_THREADED_INTERP
          default:
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
                fp->rval = rval;
                interpReturnOK = JS_TRUE;
                goto forced_return;
              case JSTRAP_THROW:
                cx->exception = rval;
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
                if (JS_UNLIKELY(cx->exception.isMagic(JS_GENERATOR_CLOSING)))
                    break;
#endif

                /*
                 * Don't clear cx->throwing to save cx->exception from GC
                 * until it is pushed to the stack via [exception] in the
                 * catch block.
                 */
                len = 0;
                DO_NEXT_OP(len);

              case JSTRY_FINALLY:
                /*
                 * Push (true, exception) pair for finally to indicate that
                 * [retsub] should rethrow the exception.
                 */
                PUSH_BOOLEAN(true);
                PUSH_COPY(cx->exception);
                cx->throwing = JS_FALSE;
                len = 0;
                DO_NEXT_OP(len);

              case JSTRY_ITER: {
                /* This is similar to JSOP_ENDITER in the interpreter loop. */
                JS_ASSERT(js_GetOpcode(cx, fp->script, regs.pc) == JSOP_ENDITER);
                AutoValueRooter tvr(cx, cx->exception);
                cx->throwing = false;
                ok = js_CloseIterator(cx, &regs.sp[-1].toObject());
                regs.sp -= 1;
                if (!ok)
                    goto error;
                cx->throwing = true;
                cx->exception = tvr.value();
              }
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
                        cx->exception.isMagic(JS_GENERATOR_CLOSING))) {
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
