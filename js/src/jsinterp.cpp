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
#include "methodjit/MethodJIT.h"
#include "methodjit/Logging.h"

#include "jsatominlines.h"
#include "jscntxtinlines.h"
#include "jsinterpinlines.h"
#include "jsobjinlines.h"
#include "jsprobes.h"
#include "jspropertycacheinlines.h"
#include "jsscopeinlines.h"
#include "jsscriptinlines.h"
#include "jsstrinlines.h"

#if JS_HAS_XML_SUPPORT
#include "jsxml.h"
#endif

#include "jsautooplen.h"

using namespace js;

/* jsinvoke_cpp___ indicates inclusion from jsinvoke.cpp. */
#if !JS_LONE_INTERPRET ^ defined jsinvoke_cpp___

#ifdef DEBUG
jsbytecode *const JSStackFrame::sInvalidpc = (jsbytecode *)0xbeef;
JSObject *const JSStackFrame::sInvalidScopeChain = (JSObject *)0xbeef;
#endif

/*
 * We can't determine in advance which local variables can live on the stack and
 * be freed when their dynamic scope ends, and which will be closed over and
 * need to live in the heap.  So we place variables on the stack initially, note
 * when they are closed over, and copy those that are out to the heap when we
 * leave their dynamic scope.
 *
 * The bytecode compiler produces a tree of block objects accompanying each
 * JSScript representing those lexical blocks in the script that have let-bound
 * variables associated with them.  These block objects are never modified, and
 * never become part of any function's scope chain.  Their parent slots point to
 * the innermost block that encloses them, or are NULL in the outermost blocks
 * within a function or in eval or global code.
 *
 * When we are in the static scope of such a block, blockChain points to its
 * compiler-allocated block object; otherwise, it is NULL.
 *
 * scopeChain is the current scope chain, including 'call' and 'block' objects
 * for those function calls and lexical blocks whose static scope we are
 * currently executing in, and 'with' objects for with statements; the chain is
 * typically terminated by a global object.  However, as an optimization, the
 * young end of the chain omits block objects we have not yet cloned.  To create
 * a closure, we clone the missing blocks from blockChain (which is always
 * current), place them at the head of scopeChain, and use that for the
 * closure's scope chain.  If we never close over a lexical block, we never
 * place a mutable clone of it on scopeChain.
 *
 * This lazy cloning is implemented in js_GetScopeChain, which is also used in
 * some other cases --- entering 'with' blocks, for example.
 */
JSObject *
js_GetScopeChain(JSContext *cx, JSStackFrame *fp)
{
    JSObject *sharedBlock = fp->maybeBlockChain();

    if (!sharedBlock) {
        /*
         * Don't force a call object for a lightweight function call, but do
         * insist that there is a call object for a heavyweight function call.
         */
        JS_ASSERT_IF(fp->isFunctionFrame() && fp->fun()->isHeavyweight(),
                     fp->hasCallObj());
        return &fp->scopeChain();
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
    if (fp->isFunctionFrame() && !fp->hasCallObj()) {
        JS_ASSERT_IF(fp->scopeChain().isClonedBlock(),
                     fp->scopeChain().getPrivate() != js_FloatingFrameIfGenerator(cx, fp));
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
        limitClone = &fp->scopeChain();
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
            return &fp->scopeChain();
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
    newChild->setParent(&fp->scopeChain());


    /*
     * If we found a limit block belonging to this frame, then we should have
     * found it in blockChain.
     */
    JS_ASSERT_IF(limitBlock &&
                 limitBlock->isBlock() &&
                 limitClone->getPrivate() == js_FloatingFrameIfGenerator(cx, fp),
                 sharedBlock);

    /* Place our newly cloned blocks at the head of the scope chain.  */
    fp->setScopeChainNoCallObj(*innermostNewChild);
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
JS_STATIC_INTERPRET bool
ComputeGlobalThis(JSContext *cx, Value *argv)
{
    JSObject *thisp = argv[-2].toObject().getGlobal()->thisObject(cx);
    if (!thisp)
        return false;
    argv[-1].setObject(*thisp);
    return true;
}

namespace js {

bool
ComputeThisFromArgv(JSContext *cx, Value *argv)
{
    /*
     * Check for SynthesizeFrame poisoning and fast constructors which
     * didn't check their vp properly.
     */
    JS_ASSERT(!argv[-1].isMagic());

    if (argv[-1].isNull())
        return ComputeGlobalThis(cx, argv);

    if (!argv[-1].isObject())
        return !!js_PrimitiveToObject(cx, &argv[-1]);

    JS_ASSERT(IsSaneThisObject(argv[-1].toObject()));
    return true;
}

}

#if JS_HAS_NO_SUCH_METHOD

const uint32 JSSLOT_FOUND_FUNCTION  = JSSLOT_PRIVATE;
const uint32 JSSLOT_SAVED_ID        = JSSLOT_PRIVATE + 1;

Class js_NoSuchMethodClass = {
    "NoSuchMethod",
    JSCLASS_HAS_RESERVED_SLOTS(2) | JSCLASS_IS_ANONYMOUS,
    PropertyStub,   /* addProperty */
    PropertyStub,   /* delProperty */
    PropertyStub,   /* getProperty */
    PropertyStub,   /* setProperty */
    EnumerateStub,
    ResolveStub,
    ConvertStub,
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
JSBool
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
        obj->init(&js_NoSuchMethodClass, NULL, NULL, tvr.value(), cx);
        obj->fslots[JSSLOT_SAVED_ID] = vp[0];
        vp[0].setObject(*obj);
    }
    return true;
}

static JS_REQUIRES_STACK JSBool
NoSuchMethod(JSContext *cx, uintN argc, Value *vp, uint32 flags)
{
    InvokeArgsGuard args;
    if (!cx->stack().pushInvokeArgs(cx, 2, &args))
        return JS_FALSE;

    JS_ASSERT(vp[0].isObject());
    JS_ASSERT(vp[1].isObject());
    JSObject *obj = &vp[0].toObject();
    JS_ASSERT(obj->getClass() == &js_NoSuchMethodClass);

    args.callee() = obj->fslots[JSSLOT_FOUND_FUNCTION];
    args.thisv() = vp[1];
    args[0] = obj->fslots[JSSLOT_SAVED_ID];
    JSObject *argsobj = js_NewArrayObject(cx, argc, vp + 2);
    if (!argsobj)
        return JS_FALSE;
    args[1].setObject(*argsobj);
    JSBool ok = (flags & JSINVOKE_CONSTRUCT)
                ? InvokeConstructor(cx, args)
                : Invoke(cx, args, flags);
    vp[0] = args.rval();
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

struct AutoInterpPreparer  {
    JSContext *cx;
    JSScript *script;

    AutoInterpPreparer(JSContext *cx, JSScript *script)
      : cx(cx), script(script)
    {
        cx->interpLevel++;
    }

    ~AutoInterpPreparer()
    {
        --cx->interpLevel;
    }
};

JS_REQUIRES_STACK bool
RunScript(JSContext *cx, JSScript *script, JSFunction *fun, JSObject &scopeChain)
{
    JS_ASSERT(script);

#ifdef JS_METHODJIT_SPEW
    JMCheckLogging();
#endif

    AutoInterpPreparer prepareInterp(cx, script);

#ifdef JS_METHODJIT
    mjit::CompileStatus status = mjit::CanMethodJIT(cx, script, fun, &scopeChain);
    if (status == mjit::Compile_Error)
        return JS_FALSE;

    if (status == mjit::Compile_Okay)
        return mjit::JaegerShot(cx);
#endif

    return Interpret(cx, cx->fp());
}

/*
 * Find a function reference and its 'this' value implicit first parameter
 * under argc arguments on cx's stack, and call the function.  Push missing
 * required arguments, allocate declared local variables, and pop everything
 * when done.  Then push the return value.
 */
JS_REQUIRES_STACK bool
Invoke(JSContext *cx, const CallArgs &argsRef, uint32 flags)
{
    CallArgs args = argsRef;
    JS_ASSERT(args.argc() <= JS_ARGS_LENGTH_MAX);

    if (args.callee().isPrimitive()) {
        js_ReportIsNotFunction(cx, &args.callee(), flags & JSINVOKE_FUNFLAGS);
        return false;
    }

    JSObject &callee = args.callee().toObject();
    Class *clasp = callee.getClass();

    /* Invoke non-functions. */
    if (JS_UNLIKELY(clasp != &js_FunctionClass)) {
#if JS_HAS_NO_SUCH_METHOD
        if (JS_UNLIKELY(clasp == &js_NoSuchMethodClass))
            return NoSuchMethod(cx, args.argc(), args.base(), 0);
#endif
        JS_ASSERT_IF(flags & JSINVOKE_CONSTRUCT, !clasp->construct);
        if (!clasp->call) {
            js_ReportIsNotFunction(cx, &args.callee(), flags);
            return false;
        }
        return CallJSNative(cx, clasp->call, args.argc(), args.base());
    }

    /* Invoke native functions. */
    JSFunction *fun = callee.getFunctionPrivate();
    JS_ASSERT_IF(flags & JSINVOKE_CONSTRUCT, !fun->isConstructor());
    if (fun->isNative()) {
        JS_ASSERT(args.thisv().isObjectOrNull() || PrimitiveThisTest(fun, args.thisv()));
        return CallJSNative(cx, fun->u.n.native, args.argc(), args.base());
    }

    JS_ASSERT(fun->isInterpreted());
    JSScript *script = fun->u.i.script;

    /* Handle the empty-script special case. */
    if (JS_UNLIKELY(script->isEmpty())) {
        if (flags & JSINVOKE_CONSTRUCT) {
            JS_ASSERT(args.thisv().isObject());
            args.rval() = args.thisv();
        } else {
            args.rval().setUndefined();
        }
        return true;
    }

    JS_ASSERT_IF(flags & JSINVOKE_CONSTRUCT, args.thisv().isObject());

    /* Get pointer to new frame/slots, prepare arguments. */
    InvokeFrameGuard frame;
    if (JS_UNLIKELY(!cx->stack().getInvokeFrame(cx, args, fun, script, &flags, &frame)))
        return false;

    /* Initialize frame, locals. */
    JSStackFrame *fp = frame.fp();
    fp->initCallFrame(cx, callee, fun, args.argc(), flags);
    SetValueRangeToUndefined(fp->slots(), script->nfixed);

    /* Officially push fp. frame's destructor pops. */
    cx->stack().pushInvokeFrame(cx, args, &frame);

    /* Now that the new frame is rooted, maybe create a call object. */
    if (fun->isHeavyweight() && !js_GetCallObject(cx, fp))
        return false;

    /*
     * Compute |this|. Currently, this must happen after the frame is pushed
     * and fp->scopeChain is correct because the thisObject hook may call
     * JS_GetScopeChain.
     */
    Value &thisv = fp->functionThis();
    JS_ASSERT_IF(flags & JSINVOKE_CONSTRUCT, !thisv.isPrimitive());
    if (thisv.isObject() && !(flags & JSINVOKE_CONSTRUCT)) {
        /*
         * We must call the thisObject hook in case we are not called from the
         * interpreter, where a prior bytecode has computed an appropriate
         * |this| already.
         */
        JSObject *thisp = thisv.toObject().thisObject(cx);
        if (!thisp)
             return false;
        JS_ASSERT(IsSaneThisObject(*thisp));
        thisv.setObject(*thisp);
    }

    JSInterpreterHook hook = cx->debugHooks->callHook;
    void *hookData = NULL;
    if (JS_UNLIKELY(hook != NULL))
        hookData = hook(cx, fp, JS_TRUE, 0, cx->debugHooks->callHookData);

    /* Run function until JSOP_STOP, JSOP_RETURN or error. */
    JSBool ok;
    {
        AutoPreserveEnumerators preserve(cx);
        Probes::enterJSFun(cx, fun);
        ok = RunScript(cx, script, fun, fp->scopeChain());
        Probes::exitJSFun(cx, fun);
    }

    if (JS_UNLIKELY(hookData != NULL)) {
        hook = cx->debugHooks->callHook;
        if (hook)
            hook(cx, fp, JS_FALSE, &ok, hookData);
    }

    PutActivationObjects(cx, fp);

    args.rval() = fp->returnValue();
    return ok;
}

bool
ExternalInvoke(JSContext *cx, const Value &thisv, const Value &fval,
               uintN argc, Value *argv, Value *rval)
{
    LeaveTrace(cx);

    InvokeArgsGuard args;
    if (!cx->stack().pushInvokeArgs(cx, argc, &args))
        return JS_FALSE;

    args.callee() = fval;
    args.thisv() = thisv;
    memcpy(args.argv(), argv, argc * sizeof(Value));

    if (!Invoke(cx, args, 0))
        return JS_FALSE;

    *rval = args.rval();

    return JS_TRUE;
}

bool
ExternalGetOrSet(JSContext *cx, JSObject *obj, jsid id, const Value &fval,
                 JSAccessMode mode, uintN argc, Value *argv, Value *rval)
{
    LeaveTrace(cx);

    /*
     * ExternalInvoke could result in another try to get or set the same id
     * again, see bug 355497.
     */
    JS_CHECK_RECURSION(cx, return JS_FALSE);

    return ExternalInvoke(cx, obj, fval, argc, argv, rval);
}

bool
Execute(JSContext *cx, JSObject *chain, JSScript *script,
        JSStackFrame *prev, uintN flags, Value *result)
{
    JS_ASSERT(chain);
    JS_ASSERT_IF(prev, !prev->isDummyFrame());

    if (script->isEmpty()) {
        if (result)
            result->setUndefined();
        return JS_TRUE;
    }

    LeaveTrace(cx);

    /*
     * Get a pointer to new frame/slots. This memory is not "claimed", so the
     * code before pushExecuteFrame must not reenter the interpreter.
     */
    ExecuteFrameGuard frame;
    if (!cx->stack().getExecuteFrame(cx, script, &frame))
        return false;

    /* Initialize frame and locals. */
    JSObject *initialVarObj;
    if (prev) {
        JS_ASSERT(chain == &prev->scopeChain());
        frame.fp()->initEvalFrame(script, prev, flags);

        /*
         * We want to call |prev->varobj()|, but this requires knowing the
         * CallStackSegment of |prev|. If |prev == cx->fp()|, the callstack is
         * simply the context's active callstack, so we can use
         * |prev->varobj(cx)|.  When |prev != cx->fp()|, we need to do a slow
         * linear search. Luckily, this only happens with EvaluateInFrame.
         */
        initialVarObj = (prev == cx->maybefp())
                        ? &prev->varobj(cx)
                        : &prev->varobj(cx->containingSegment(prev));
    } else {
        /* The scope chain could be anything, so innerize just in case. */
        JSObject *innerizedChain = chain;
        OBJ_TO_INNER_OBJECT(cx, innerizedChain);
        if (!innerizedChain)
            return false;

        /* Initialize frame. */
        frame.fp()->initGlobalFrame(script, *innerizedChain, flags);

        /* Compute 'this'. */
        JSObject *thisp = chain->thisObject(cx);
        if (!thisp)
            return false;
        frame.fp()->globalThis().setObject(*thisp);

        initialVarObj = (cx->options & JSOPTION_VAROBJFIX)
                        ? chain->getGlobal()
                        : chain;
    }
    JS_ASSERT(!initialVarObj->getOps()->defineProperty);

    /* Initialize fixed slots (GVAR ops expect NULL). */
    SetValueRangeToNull(frame.fp()->slots(), script->nfixed);

#if JS_HAS_SHARP_VARS
    JS_STATIC_ASSERT(SHARP_NSLOTS == 2);
    if (script->hasSharps) {
        JS_ASSERT(script->nfixed >= SHARP_NSLOTS);
        Value *sharps = &frame.fp()->slots()[script->nfixed - SHARP_NSLOTS];
        if (prev && prev->script()->hasSharps) {
            JS_ASSERT(prev->numFixed() >= SHARP_NSLOTS);
            int base = (prev->isFunctionFrame() && !prev->isEvalOrDebuggerFrame())
                       ? prev->fun()->sharpSlotBase(cx)
                       : prev->numFixed() - SHARP_NSLOTS;
            if (base < 0)
                return false;
            sharps[0] = prev->slots()[base];
            sharps[1] = prev->slots()[base + 1];
        } else {
            sharps[0].setUndefined();
            sharps[1].setUndefined();
        }
    }
#endif

    /* Officially push |fp|. |frame|'s destructor pops. */
    cx->stack().pushExecuteFrame(cx, initialVarObj, &frame);

    /* Now that the frame has been pushed, we can call the thisObject hook. */
    if (!prev) {
        JSObject *thisp = chain->thisObject(cx);
        if (!thisp)
            return false;
        frame.fp()->globalThis().setObject(*thisp);
    }

    Probes::startExecution(cx, script);

    void *hookData = NULL;
    if (JSInterpreterHook hook = cx->debugHooks->executeHook)
        hookData = hook(cx, frame.fp(), JS_TRUE, 0, cx->debugHooks->executeHookData);

    /* Run script until JSOP_STOP or error. */
    AutoPreserveEnumerators preserve(cx);
    JSBool ok = RunScript(cx, script, NULL, frame.fp()->scopeChain());
    if (result)
        *result = frame.fp()->returnValue();

    if (hookData) {
        if (JSInterpreterHook hook = cx->debugHooks->executeHook)
            hook(cx, frame.fp(), JS_FALSE, &ok, hookData);
    }

    Probes::stopExecution(cx, script);

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
        oldAttrs = ((Shape *) prop)->attributes();

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
HasInstance(JSContext *cx, JSObject *obj, const Value *v, JSBool *bp)
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
        if (lval.isUndefined())
                return true;
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
        return v.toObject().typeOf(cx);
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
InvokeConstructor(JSContext *cx, const CallArgs &argsRef)
{
    JS_ASSERT(!js_FunctionClass.construct);
    CallArgs args = argsRef;

    JSObject *callee;
    if (args.callee().isPrimitive() || !(callee = &args.callee().toObject())->getParent()) {
        js_ReportIsNotFunction(cx, &args.callee(), JSV2F_CONSTRUCT);
        return false;
    }

    /* Handle the fast-constructors cases before falling into the general case . */
    Class *clasp = callee->getClass();
    if (clasp == &js_FunctionClass) {
        JSFunction *fun = callee->getFunctionPrivate();
        if (fun->isConstructor()) {
            args.thisv().setMagicWithObjectOrNullPayload(NULL);
            return CallJSNativeConstructor(cx, fun->u.n.native, args.argc(), args.base());
        }
    } else if (clasp->construct) {
        args.thisv().setMagicWithObjectOrNullPayload(NULL);
        return CallJSNativeConstructor(cx, clasp->construct, args.argc(), args.base());
    }

    /* Construct 'this'. */
    JSObject *obj = js_NewInstance(cx, callee);
    if (!obj)
        return false;
    args.thisv().setObject(*obj);

    if (!Invoke(cx, args, JSINVOKE_CONSTRUCT))
        return false;

    /* Check the return value and if it's primitive, force it to be obj. */
    if (args.rval().isPrimitive()) {
        if (callee->getClass() != &js_FunctionClass) {
            /* native [[Construct]] returning primitive is error */
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                 JSMSG_BAD_NEW_RESULT,
                                 js_ValueToPrintableString(cx, args.rval()));
            return false;
        }
        args.rval().setObject(*obj);
    }

    JS_RUNTIME_METER(cx->runtime, constructs);
    return true;
}

bool
InvokeConstructorWithGivenThis(JSContext *cx, JSObject *thisobj, const Value &fval,
                               uintN argc, Value *argv, Value *rval)
{
    LeaveTrace(cx);

    InvokeArgsGuard args;
    if (!cx->stack().pushInvokeArgs(cx, argc, &args))
        return JS_FALSE;

    args.callee() = fval;
    /* Initialize args.thisv on all paths below. */
    memcpy(args.argv(), argv, argc * sizeof(Value));

    /* Handle the fast-constructor cases before calling the general case. */
    JSObject &callee = fval.toObject();
    Class *clasp = callee.getClass();
    JSFunction *fun;
    bool ok;
    if (clasp == &js_FunctionClass && (fun = callee.getFunctionPrivate())->isConstructor()) {
        args.thisv().setMagicWithObjectOrNullPayload(thisobj);
        ok = CallJSNativeConstructor(cx, fun->u.n.native, args.argc(), args.base());
    } else if (clasp->construct) {
        args.thisv().setMagicWithObjectOrNullPayload(thisobj);
        ok = CallJSNativeConstructor(cx, clasp->construct, args.argc(), args.base());
    } else {
        args.thisv().setObjectOrNull(thisobj);
        ok = Invoke(cx, args, JSINVOKE_CONSTRUCT);
    }

    *rval = args.rval();
    return ok;
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
        if (obj->isXMLId()) {
            *idp = OBJECT_TO_JSID(obj);
            return JS_TRUE;
        }
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
    JSStackFrame *fp = cx->fp();
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

    fp->setScopeChainNoCallObj(*withobj);
    return JS_TRUE;
}

JS_STATIC_INTERPRET JS_REQUIRES_STACK void
js_LeaveWith(JSContext *cx)
{
    JSObject *withobj;

    withobj = &cx->fp()->scopeChain();
    JS_ASSERT(withobj->getClass() == &js_WithClass);
    JS_ASSERT(withobj->getPrivate() == js_FloatingFrameIfGenerator(cx, cx->fp()));
    JS_ASSERT(OBJ_BLOCK_DEPTH(cx, withobj) >= 0);
    withobj->setPrivate(NULL);
    cx->fp()->setScopeChainNoCallObj(*withobj->getParent());
}

JS_REQUIRES_STACK Class *
js_IsActiveWithOrBlock(JSContext *cx, JSObject *obj, int stackDepth)
{
    Class *clasp;

    clasp = obj->getClass();
    if ((clasp == &js_WithClass || clasp == &js_BlockClass) &&
        obj->getPrivate() == js_FloatingFrameIfGenerator(cx, cx->fp()) &&
        OBJ_BLOCK_DEPTH(cx, obj) >= stackDepth) {
        return clasp;
    }
    return NULL;
}

/*
 * Unwind block and scope chains to match the given depth. The function sets
 * fp->sp on return to stackDepth.
 */
JS_REQUIRES_STACK JSBool
js_UnwindScope(JSContext *cx, jsint stackDepth, JSBool normalUnwind)
{
    JSObject *obj;
    Class *clasp;

    JS_ASSERT(stackDepth >= 0);
    JS_ASSERT(cx->fp()->base() + stackDepth <= cx->regs->sp);

    JSStackFrame *fp = cx->fp();
    for (obj = fp->maybeBlockChain(); obj; obj = obj->getParent()) {
        JS_ASSERT(obj->isStaticBlock());
        if (OBJ_BLOCK_DEPTH(cx, obj) < stackDepth)
            break;
    }
    fp->setBlockChain(obj);

    for (;;) {
        clasp = js_IsActiveWithOrBlock(cx, &fp->scopeChain(), stackDepth);
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
js::GetUpvar(JSContext *cx, uintN closureLevel, UpvarCookie cookie)
{
    JS_ASSERT(closureLevel >= cookie.level() && cookie.level() > 0);
    const uintN targetLevel = closureLevel - cookie.level();
    JS_ASSERT(targetLevel < UpvarCookie::UPVAR_LEVEL_LIMIT);

    JSStackFrame *fp = cx->findFrameAtLevel(targetLevel);
    uintN slot = cookie.slot();
    Value *vp;

    if (!fp->isFunctionFrame() || fp->isEvalFrame()) {
        vp = fp->slots() + fp->numFixed();
    } else if (slot < fp->numFormalArgs()) {
        vp = fp->formalArgs();
    } else if (slot == UpvarCookie::CALLEE_SLOT) {
        vp = &fp->calleeValue();
        slot = 0;
    } else {
        slot -= fp->numFormalArgs();
        JS_ASSERT(slot < fp->numSlots());
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
    fp = cx->fp();
    regs = cx->regs;

    /*
     * Operations in prologues don't produce interesting values, and
     * js_DecompileValueGenerator isn't set up to handle them anyway.
     */
    if (cx->tracePrevPc && regs->pc >= fp->script()->main) {
        JSOp tracePrevOp = JSOp(*cx->tracePrevPc);
        ndefs = js_GetStackDefs(cx, &js_CodeSpec[tracePrevOp], tracePrevOp,
                                fp->script(), cx->tracePrevPc);

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
            js_PCToLineNumber(cx, fp->script(),
                              fp->hasImacropc() ? fp->imacropc() : regs->pc));
    js_Disassemble1(cx, fp->script(), regs->pc,
                    regs->pc - fp->script()->code,
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
            JSFunction *fun = obj->getFunctionPrivate();
            if (FUN_INTERPRETED(fun))
                return FUNCTION_INTERPRETED;
            return FUNCTION_FASTNATIVE;
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
                                      "fun:interp", "fun:native"
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
#define PUSH_OBJECT_OR_NULL(obj) regs.sp++->setObjectOrNull(obj)
#define PUSH_HOLE()              regs.sp++->setMagic(JS_ARRAY_HOLE)
#define POP_COPY_TO(v)           v = *--regs.sp
#define POP_RETURN_VALUE()       regs.fp->setReturnValue(*--regs.sp)

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

    const Shape *shape = (Shape *) prop;
    if (entry->vword.isSlot()) {
        JS_ASSERT(entry->vword.toSlot() == shape->slot);
        JS_ASSERT(!shape->isMethod());
    } else if (entry->vword.isShape()) {
        JS_ASSERT(entry->vword.toShape() == shape);
        JS_ASSERT_IF(shape->isMethod(),
                     &shape->methodObject() == &pobj->lockedGetSlot(shape->slot).toObject());
    } else {
        Value v;
        JS_ASSERT(entry->vword.isFunObj());
        JS_ASSERT(!entry->vword.isNull());
        JS_ASSERT(pobj->brandedOrHasMethodBarrier());
        JS_ASSERT(shape->hasDefaultGetterOrIsMethod());
        JS_ASSERT(pobj->containsSlot(shape->slot));
        v = pobj->lockedGetSlot(shape->slot);
        JS_ASSERT(&entry->vword.toFunObj() == &v.toObject());

        if (shape->isMethod()) {
            JS_ASSERT(js_CodeSpec[*regs.pc].format & JOF_CALLOP);
            JS_ASSERT(&shape->methodObject() == &v.toObject());
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
JS_STATIC_ASSERT(JSOP_GETUPVAR_LENGTH == JSOP_CALLUPVAR_LENGTH);
JS_STATIC_ASSERT(JSOP_GETUPVAR_DBG_LENGTH == JSOP_CALLUPVAR_DBG_LENGTH);
JS_STATIC_ASSERT(JSOP_GETUPVAR_DBG_LENGTH == JSOP_GETUPVAR_LENGTH);
JS_STATIC_ASSERT(JSOP_GETFCSLOT_LENGTH == JSOP_CALLFCSLOT_LENGTH);
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
    if (iterobj->getClass() == &js_IteratorClass) {
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
    if (iterobj->getClass() == &js_IteratorClass) {
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
Interpret(JSContext *cx, JSStackFrame *entryFrame, uintN inlineCallCount, uintN interpFlags)
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
#ifdef JS_TRACER
    JS_CHECK_RECURSION(cx, do {
            if (TRACE_RECORDER(cx))
                AbortRecording(cx, "too much recursion");
            return JS_FALSE;
        } while (0););
#else
    JS_CHECK_RECURSION(cx, return JS_FALSE);
#endif

#define LOAD_ATOM(PCOFF, atom)                                                \
    JS_BEGIN_MACRO                                                            \
        JS_ASSERT(regs.fp->hasImacropc()                                      \
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
        script = regs.fp->script();                                           \
        argv = regs.fp->maybeFormalArgs();                                    \
        atoms = FrameAtomBase(cx, regs.fp);                                   \
        JS_ASSERT(cx->regs == &regs);                                         \
        if (cx->throwing)                                                     \
            goto error;                                                       \
    JS_END_MACRO

#define MONITOR_BRANCH()                                                      \
    JS_BEGIN_MACRO                                                            \
        if (TRACING_ENABLED(cx)) {                                            \
            MonitorResult r = MonitorLoopEdge(cx, inlineCallCount);           \
            if (r == MONITOR_RECORDING) {                                     \
                JS_ASSERT(TRACE_RECORDER(cx));                                \
                MONITOR_BRANCH_TRACEVIS;                                      \
                ENABLE_INTERRUPTS();                                          \
                CLEAR_LEAVE_ON_TRACE_POINT();                                 \
            }                                                                 \
            RESTORE_INTERP_VARS();                                            \
            JS_ASSERT_IF(cx->throwing, r == MONITOR_ERROR);                   \
            if (r == MONITOR_ERROR)                                           \
                goto error;                                                   \
        }                                                                     \
    JS_END_MACRO

#else /* !JS_TRACER */

#define MONITOR_BRANCH() ((void) 0)

#endif /* !JS_TRACER */

    /*
     * Prepare to call a user-supplied branch handler, and abort the script
     * if it returns false.
     */
#define CHECK_BRANCH()                                                        \
    JS_BEGIN_MACRO                                                            \
        if (JS_THREAD_DATA(cx)->interruptFlags && !js_HandleExecutionInterrupt(cx)) \
            goto error;                                                       \
    JS_END_MACRO

#ifndef TRACE_RECORDER
#define TRACE_RECORDER(cx) (false)
#endif

#ifdef JS_METHODJIT
# define LEAVE_ON_SAFE_POINT()                                                \
    do {                                                                      \
        JS_ASSERT_IF(leaveOnSafePoint, !TRACE_RECORDER(cx));                  \
        if (leaveOnSafePoint && !regs.fp->hasImacropc() &&                    \
            script->nmap && script->nmap[regs.pc - script->code]) {           \
            JS_ASSERT(!TRACE_RECORDER(cx));                                   \
            interpReturnOK = true;                                            \
            goto stop_recording;                                              \
        }                                                                     \
    } while (0)
#else
# define LEAVE_ON_SAFE_POINT() /* nop */
#endif

#define BRANCH(n)                                                             \
    JS_BEGIN_MACRO                                                            \
        regs.pc += (n);                                                       \
        op = (JSOp) *regs.pc;                                                 \
        if ((n) <= 0) {                                                       \
            CHECK_BRANCH();                                                   \
            if (op == JSOP_NOP) {                                             \
                if (TRACE_RECORDER(cx)) {                                     \
                    MONITOR_BRANCH();                                         \
                    op = (JSOp) *regs.pc;                                     \
                }                                                             \
            } else if (op == JSOP_TRACE) {                                    \
                MONITOR_BRANCH();                                             \
                op = (JSOp) *regs.pc;                                         \
            }                                                                 \
        }                                                                     \
        LEAVE_ON_SAFE_POINT();                                                \
        DO_OP();                                                              \
    JS_END_MACRO

#define CHECK_INTERRUPT_HANDLER()                                             \
    JS_BEGIN_MACRO                                                            \
        if (cx->debugHooks->interruptHook)                                    \
            ENABLE_INTERRUPTS();                                              \
    JS_END_MACRO

    /* Check for too deep of a native thread stack. */
    JS_CHECK_RECURSION(cx, return JS_FALSE);

    MUST_FLOW_THROUGH("exit");
    ++cx->interpLevel;

    /* Repoint cx->regs to a local variable for faster access. */
    JSFrameRegs *const prevContextRegs = cx->regs;
    JSFrameRegs regs = *cx->regs;
    cx->setCurrentRegs(&regs);

    /* Copy in hot values that change infrequently. */
    JSRuntime *const rt = cx->runtime;
    JSScript *script = regs.fp->script();
    Value *argv = regs.fp->maybeFormalArgs();
    CHECK_INTERRUPT_HANDLER();

    JS_ASSERT(!script->isEmpty());
    JS_ASSERT(script->length > 1);

#if defined(JS_TRACER) && defined(JS_METHODJIT)
    bool leaveOnSafePoint = !!(interpFlags & JSINTERP_SAFEPOINT);
#endif
#ifdef JS_METHODJIT
# define CLEAR_LEAVE_ON_TRACE_POINT() ((void) (leaveOnSafePoint = false))
#else
# define CLEAR_LEAVE_ON_TRACE_POINT() ((void) 0)
#endif

    if (!entryFrame)
        entryFrame = regs.fp;

    /*
     * Initialize the index segment register used by LOAD_ATOM and
     * GET_FULL_INDEX macros below. As a register we use a pointer based on
     * the atom map to turn frequently executed LOAD_ATOM into simple array
     * access. For less frequent object and regexp loads we have to recover
     * the segment from atoms pointer first.
     */
    JSAtom **atoms = script->atomMap.vector;

#if JS_HAS_GENERATORS
    if (JS_UNLIKELY(regs.fp->isGeneratorFrame())) {
        JS_ASSERT(prevContextRegs == &cx->generatorFor(regs.fp)->regs);
        JS_ASSERT((size_t) (regs.pc - script->code) <= script->length);
        JS_ASSERT((size_t) (regs.sp - regs.fp->base()) <= StackDepth(script));

        /*
         * To support generator_throw and to catch ignored exceptions,
         * fail if cx->throwing is set.
         */
        if (cx->throwing)
            goto error;
    }
#endif

#ifdef JS_TRACER
    /*
     * The method JIT may have already initiated a recording, in which case
     * there should already be a valid recorder. Otherwise...
     * we cannot reenter the interpreter while recording.
     */
    if (interpFlags & JSINTERP_RECORD) {
        JS_ASSERT(TRACE_RECORDER(cx));
        ENABLE_INTERRUPTS();
    } else if (TRACE_RECORDER(cx)) {
        AbortRecording(cx, "attempt to reenter interpreter while recording");
    }

    if (regs.fp->hasImacropc())
        atoms = COMMON_ATOMS_START(&rt->atomState);
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
                regs.fp->setReturnValue(rval);
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

            if (interpFlags & (JSINTERP_RECORD | JSINTERP_SAFEPOINT)) {
                switch (status) {
                  case ARECORD_IMACRO_ABORTED:
                  case ARECORD_ABORTED:
                  case ARECORD_COMPLETED:
                  case ARECORD_STOP:
                    leaveOnSafePoint = true;
                    LEAVE_ON_SAFE_POINT();
                    break;
                  default:
                    break;
                }
            }

            switch (status) {
              case ARECORD_CONTINUE:
                moreInterrupts = true;
                break;
              case ARECORD_IMACRO:
              case ARECORD_IMACRO_ABORTED:
                atoms = COMMON_ATOMS_START(&rt->atomState);
                op = JSOp(*regs.pc);
                CLEAR_LEAVE_ON_TRACE_POINT();
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
#if JS_HAS_XML_SUPPORT
ADD_EMPTY_CASE(JSOP_STARTXML)
ADD_EMPTY_CASE(JSOP_STARTXMLEXPR)
#endif
ADD_EMPTY_CASE(JSOP_UNUSED180)
END_EMPTY_CASES

BEGIN_CASE(JSOP_TRACE)
#ifdef JS_METHODJIT
    LEAVE_ON_SAFE_POINT();
#endif
END_CASE(JSOP_TRACE)

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
    JS_ASSERT(regs.fp->base() <= regs.sp);
    JSObject *obj = regs.fp->maybeBlockChain();
    JS_ASSERT_IF(obj,
                 OBJ_BLOCK_DEPTH(cx, obj) + OBJ_BLOCK_COUNT(cx, obj)
                 <= (size_t) (regs.sp - regs.fp->base()));
    for (obj = &regs.fp->scopeChain(); obj; obj = obj->getParent()) {
        Class *clasp = obj->getClass();
        if (clasp != &js_BlockClass && clasp != &js_WithClass)
            continue;
        if (obj->getPrivate() != js_FloatingFrameIfGenerator(cx, regs.fp))
            break;
        JS_ASSERT(regs.fp->base() + OBJ_BLOCK_DEPTH(cx, obj)
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
    POP_RETURN_VALUE();
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
    regs.sp[-1].setObject(regs.fp->scopeChain());
END_CASE(JSOP_ENTERWITH)

BEGIN_CASE(JSOP_LEAVEWITH)
    JS_ASSERT(&regs.sp[-1].toObject() == &regs.fp->scopeChain());
    regs.sp--;
    js_LeaveWith(cx);
END_CASE(JSOP_LEAVEWITH)

BEGIN_CASE(JSOP_RETURN)
    POP_RETURN_VALUE();
    /* FALL THROUGH */

BEGIN_CASE(JSOP_RETRVAL)    /* fp return value already set */
BEGIN_CASE(JSOP_STOP)
{
    /*
     * When the inlined frame exits with an exception or an error, ok will be
     * false after the inline_return label.
     */
    CHECK_BRANCH();

#ifdef JS_TRACER
    if (regs.fp->hasImacropc()) {
        /*
         * If we are at the end of an imacro, return to its caller in the
         * current frame.
         */
        JS_ASSERT(op == JSOP_STOP);
        JS_ASSERT((uintN)(regs.sp - regs.fp->slots()) <= script->nslots);
        jsbytecode *imacpc = regs.fp->imacropc();
        regs.pc = imacpc + js_CodeSpec[*imacpc].length;
        regs.fp->clearImacropc();
        LEAVE_ON_SAFE_POINT();
        atoms = script->atomMap.vector;
        op = JSOp(*regs.pc);
        DO_OP();
    }
#endif

    interpReturnOK = true;
    if (entryFrame != regs.fp)
  inline_return:
    {
        JS_ASSERT(!regs.fp->hasBlockChain());
        JS_ASSERT(!js_IsActiveWithOrBlock(cx, &regs.fp->scopeChain(), 0));
        if (JS_UNLIKELY(regs.fp->hasHookData())) {
            if (JSInterpreterHook hook = cx->debugHooks->callHook) {
                hook(cx, regs.fp, JS_FALSE, &interpReturnOK, regs.fp->hookData());
                CHECK_INTERRUPT_HANDLER();
            }
        }

        PutActivationObjects(cx, regs.fp);

        Probes::exitJSFun(cx, regs.fp->maybeFun());

        /*
         * If inline-constructing, replace primitive rval with the new object
         * passed in via |this|, and instrument this constructor invocation.
         */
        if (regs.fp->isConstructing()) {
            if (regs.fp->returnValue().isPrimitive())
                regs.fp->setReturnValue(ObjectValue(regs.fp->constructorThis()));
            JS_RUNTIME_METER(cx->runtime, constructs);
        }

        Value *newsp = regs.fp->actualArgs() - 1;
        newsp[-1] = regs.fp->returnValue();
        cx->stack().popInlineFrame(cx, regs.fp->prev(), newsp);

        /* Sync interpreter registers. */
        script = regs.fp->script();
        argv = regs.fp->maybeFormalArgs();
        atoms = FrameAtomBase(cx, regs.fp);

        /* Resume execution in the calling frame. */
        JS_ASSERT(inlineCallCount);
        inlineCallCount--;
        if (JS_LIKELY(interpReturnOK)) {
            JS_ASSERT(js_CodeSpec[js_GetOpcode(cx, script, regs.pc)].length
                      == JSOP_CALL_LENGTH);
            TRACE_0(LeaveFrame);
            len = JSOP_CALL_LENGTH;
            DO_NEXT_OP(len);
        }
        goto error;
    } else {
        JS_ASSERT(regs.sp == regs.fp->base());
        if (regs.fp->isConstructing() && regs.fp->returnValue().isPrimitive())
            regs.fp->setReturnValue(ObjectValue(regs.fp->constructorThis()));

#ifdef JS_TRACER
        /* Hack: re-push rval so either JIT will read it properly. */
        regs.fp->setBailedAtReturn();
        if (TRACE_RECORDER(cx)) {
            AbortRecording(cx, "recording out of Interpret");
            interpReturnOK = true;
            goto stop_recording;
        }
#endif
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
    JS_ASSERT(regs.sp > regs.fp->base());
    uintN flags = regs.pc[1];
    if (!js_ValueToIterator(cx, flags, &regs.sp[-1]))
        goto error;
    CHECK_INTERRUPT_HANDLER();
    JS_ASSERT(!regs.sp[-1].isPrimitive());
}
END_CASE(JSOP_ITER)

BEGIN_CASE(JSOP_MOREITER)
{
    JS_ASSERT(regs.sp - 1 >= regs.fp->base());
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
    JS_ASSERT(regs.sp - 1 >= regs.fp->base());
    bool ok = !!js_CloseIterator(cx, &regs.sp[-1].toObject());
    regs.sp--;
    if (!ok)
        goto error;
}
END_CASE(JSOP_ENDITER)

BEGIN_CASE(JSOP_FORARG)
{
    JS_ASSERT(regs.sp - 1 >= regs.fp->base());
    uintN slot = GET_ARGNO(regs.pc);
    JS_ASSERT(slot < regs.fp->numFormalArgs());
    JS_ASSERT(regs.sp[-1].isObject());
    if (!IteratorNext(cx, &regs.sp[-1].toObject(), &argv[slot]))
        goto error;
}
END_CASE(JSOP_FORARG)

BEGIN_CASE(JSOP_FORLOCAL)
{
    JS_ASSERT(regs.sp - 1 >= regs.fp->base());
    uintN slot = GET_SLOTNO(regs.pc);
    JS_ASSERT(slot < regs.fp->numSlots());
    JS_ASSERT(regs.sp[-1].isObject());
    if (!IteratorNext(cx, &regs.sp[-1].toObject(), &regs.fp->slots()[slot]))
        goto error;
}
END_CASE(JSOP_FORLOCAL)

BEGIN_CASE(JSOP_FORNAME)
{
    JS_ASSERT(regs.sp - 1 >= regs.fp->base());
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
    JS_ASSERT(regs.sp - 2 >= regs.fp->base());
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
    JS_ASSERT(regs.sp - 1 >= regs.fp->base());
    JS_ASSERT(regs.sp[-1].isObject());
    PUSH_NULL();
    if (!IteratorNext(cx, &regs.sp[-2].toObject(), &regs.sp[-1]))
        goto error;
END_CASE(JSOP_FORELEM)

BEGIN_CASE(JSOP_DUP)
{
    JS_ASSERT(regs.sp > regs.fp->base());
    const Value &rref = regs.sp[-1];
    PUSH_COPY(rref);
}
END_CASE(JSOP_DUP)

BEGIN_CASE(JSOP_DUP2)
{
    JS_ASSERT(regs.sp - 2 >= regs.fp->base());
    const Value &lref = regs.sp[-2];
    const Value &rref = regs.sp[-1];
    PUSH_COPY(lref);
    PUSH_COPY(rref);
}
END_CASE(JSOP_DUP2)

BEGIN_CASE(JSOP_SWAP)
{
    JS_ASSERT(regs.sp - 2 >= regs.fp->base());
    Value &lref = regs.sp[-2];
    Value &rref = regs.sp[-1];
    lref.swap(rref);
}
END_CASE(JSOP_SWAP)

BEGIN_CASE(JSOP_PICK)
{
    jsint i = regs.pc[1];
    JS_ASSERT(regs.sp - (i+1) >= regs.fp->base());
    Value lval = regs.sp[-(i+1)];
    memmove(regs.sp - (i+1), regs.sp - i, sizeof(Value)*i);
    regs.sp[-1] = lval;
}
END_CASE(JSOP_PICK)

#define NATIVE_GET(cx,obj,pobj,shape,getHow,vp)                               \
    JS_BEGIN_MACRO                                                            \
        if (shape->hasDefaultGetter()) {                                      \
            /* Fast path for Object instance properties. */                   \
            JS_ASSERT((shape)->slot != SHAPE_INVALID_SLOT ||                  \
                      !shape->hasDefaultSetter());                            \
            if (((shape)->slot != SHAPE_INVALID_SLOT))                        \
                *(vp) = (pobj)->lockedGetSlot((shape)->slot);                 \
            else                                                              \
                (vp)->setUndefined();                                         \
        } else {                                                              \
            if (!js_NativeGet(cx, obj, pobj, shape, getHow, vp))              \
                goto error;                                                   \
        }                                                                     \
    JS_END_MACRO

#define NATIVE_SET(cx,obj,shape,entry,vp)                                     \
    JS_BEGIN_MACRO                                                            \
        TRACE_2(SetPropHit, entry, shape);                                    \
        if (shape->hasDefaultSetter() &&                                      \
            (shape)->slot != SHAPE_INVALID_SLOT &&                            \
            !(obj)->brandedOrHasMethodBarrier()) {                            \
            /* Fast path for, e.g., plain Object instance properties. */      \
            (obj)->lockedSetSlot((shape)->slot, *vp);                         \
        } else {                                                              \
            if (!js_NativeSet(cx, obj, shape, false, vp))                     \
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
    JSObject &obj = regs.fp->varobj(cx);
    const Value &ref = regs.sp[-1];
    if (!obj.defineProperty(cx, ATOM_TO_JSID(atom), ref,
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

BEGIN_CASE(JSOP_BINDGNAME)
    PUSH_OBJECT(*regs.fp->scopeChain().getGlobal());
END_CASE(JSOP_BINDGNAME)

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
        obj = &regs.fp->scopeChain();
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
        obj = js_FindIdentifierBase(cx, &regs.fp->scopeChain(), id);
        if (!obj)
            goto error;
    } while (0);
    PUSH_OBJECT(*obj);
}
END_CASE(JSOP_BINDNAME)

BEGIN_CASE(JSOP_IMACOP)
    JS_ASSERT(JS_UPTRDIFF(regs.fp->imacropc(), script->code) < script->length);
    op = JSOp(*regs.fp->imacropc());
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
    if (EqualityOp eq = l->getClass()->ext.equality) {                        \
        if (!eq(cx, l, &rval, &cond))                                         \
            goto error;                                                       \
        cond = cond OP JS_TRUE;                                               \
    } else
#else
#define XML_EQUALITY_OP(OP)             /* nothing */
#define EXTENDED_EQUALITY_OP(OP)        /* nothing */
#endif

#define EQUALITY_OP(OP, IFNAN)                                                \
    JS_BEGIN_MACRO                                                            \
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
BEGIN_CASE(JSOP_INCGNAME)
BEGIN_CASE(JSOP_DECGNAME)
BEGIN_CASE(JSOP_GNAMEINC)
BEGIN_CASE(JSOP_GNAMEDEC)
{
    obj = &regs.fp->scopeChain();
    if (js_CodeSpec[op].format & JOF_GNAME)
        obj = obj->getGlobal();

    JSObject *obj2;
    PropertyCacheEntry *entry;
    JS_PROPERTY_CACHE(cx).test(cx, regs.pc, obj, obj2, entry, atom);
    if (!atom) {
        ASSERT_VALID_PROPERTY_CACHE_HIT(0, obj, obj2, entry);
        if (obj == obj2 && entry->vword.isSlot()) {
            uint32 slot = entry->vword.toSlot();
            JS_ASSERT(obj->containsSlot(slot));
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
    JS_ASSERT((cs->format & JOF_TMPSLOT_MASK) >= JOF_TMPSLOT2);
    Value &ref = regs.sp[-1];
    int32_t tmp;
    if (JS_LIKELY(ref.isInt32() && CanIncDecWithoutOverflow(tmp = ref.toInt32()))) {
        int incr = (cs->format & JOF_INC) ? 1 : -1;
        if (cs->format & JOF_POST)
            ref.getInt32Ref() = tmp + incr;
        else
            ref.getInt32Ref() = tmp += incr;
        regs.fp->setAssigning();
        JSBool ok = obj->setProperty(cx, id, &ref);
        regs.fp->clearAssigning();
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
        regs.fp->setAssigning();
        JSBool ok = obj->setProperty(cx, id, &regs.sp[-1]);
        regs.fp->clearAssigning();
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

BEGIN_CASE(JSOP_INCGLOBAL)
    incr =  1; incr2 =  1; goto do_bound_global_incop;
BEGIN_CASE(JSOP_DECGLOBAL)
    incr = -1; incr2 = -1; goto do_bound_global_incop;
BEGIN_CASE(JSOP_GLOBALINC)
    incr =  1; incr2 =  0; goto do_bound_global_incop;
BEGIN_CASE(JSOP_GLOBALDEC)
    incr = -1; incr2 =  0; goto do_bound_global_incop;

  do_bound_global_incop:
    uint32 slot;
    slot = GET_SLOTNO(regs.pc);
    slot = script->getGlobalSlot(slot);
    JSObject *obj;
    obj = regs.fp->scopeChain().getGlobal();
    vp = &obj->getSlotRef(slot);
    goto do_int_fast_incop;
END_CASE(JSOP_INCGLOBAL)

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
    slot = GET_ARGNO(regs.pc);
    JS_ASSERT(slot < regs.fp->numFormalArgs());
    METER_SLOT_OP(op, slot);
    vp = argv + slot;
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
    JS_ASSERT(slot < regs.fp->numSlots());
    vp = regs.fp->slots() + slot;
    METER_SLOT_OP(op, slot);
    vp = regs.fp->slots() + slot;

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

BEGIN_CASE(JSOP_THIS)
    if (!regs.fp->computeThisObject(cx))
        goto error;
    PUSH_COPY(regs.fp->thisValue());
END_CASE(JSOP_THIS)

BEGIN_CASE(JSOP_UNBRANDTHIS)
{
    JSObject *obj = regs.fp->computeThisObject(cx);
    if (!obj)
        goto error;
    if (obj->isNative() && !obj->unbrand(cx))
        goto error;
}
END_CASE(JSOP_UNBRANDTHIS)

{
    JSObject *obj;
    Value *vp;
    jsint i;

BEGIN_CASE(JSOP_GETTHISPROP)
    obj = regs.fp->computeThisObject(cx);
    if (!obj)
        goto error;
    i = 0;
    PUSH_NULL();
    goto do_getprop_with_obj;

BEGIN_CASE(JSOP_GETARGPROP)
{
    i = ARGNO_LEN;
    uint32 slot = GET_ARGNO(regs.pc);
    JS_ASSERT(slot < regs.fp->numFormalArgs());
    PUSH_COPY(argv[slot]);
    goto do_getprop_body;
}

BEGIN_CASE(JSOP_GETLOCALPROP)
{
    i = SLOTNO_LEN;
    uint32 slot = GET_SLOTNO(regs.pc);
    JS_ASSERT(slot < script->nslots);
    PUSH_COPY(regs.fp->slots()[slot]);
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
                    JS_ASSERT(obj2->containsSlot(slot));
                    rval = obj2->lockedGetSlot(slot);
                } else {
                    JS_ASSERT(entry->vword.isShape());
                    const Shape *shape = entry->vword.toShape();
                    NATIVE_GET(cx, obj, obj2, shape,
                               regs.fp->hasImacropc() ? JSGET_NO_METHOD_BARRIER : JSGET_METHOD_BARRIER,
                               &rval);
                }
                break;
            }

            jsid id = ATOM_TO_JSID(atom);
            if (JS_LIKELY(!aobj->getOps()->getProperty)
                ? !js_GetPropertyHelper(cx, obj, id,
                                        (regs.fp->hasImacropc() ||
                                         regs.pc[JSOP_GETPROP_LENGTH + i] == JSOP_IFEQ)
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
            uint32 length = obj->getArgsInitialLength();
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
            JS_ASSERT(obj2->containsSlot(slot));
            rval = obj2->lockedGetSlot(slot);
        } else {
            JS_ASSERT(entry->vword.isShape());
            const Shape *shape = entry->vword.toShape();
            NATIVE_GET(cx, &objv.toObject(), obj2, shape, JSGET_NO_METHOD_BARRIER, &rval);
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
                          JS_LIKELY(!aobj->getOps()->getProperty)
                          ? JSGET_CACHE_RESULT | JSGET_NO_METHOD_BARRIER
                          : JSGET_NO_METHOD_BARRIER,
                          &rval)) {
            goto error;
        }
        regs.sp[-1] = objv;
        regs.sp[-2] = rval;
    } else {
        JS_ASSERT(!objv.toObject().getOps()->getProperty);
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
    JS_ASSERT(regs.sp - regs.fp->slots() >= 1);
    if (!regs.sp[-1].toObject().unbrand(cx))
        goto error;
END_CASE(JSOP_UNBRAND)

BEGIN_CASE(JSOP_SETGNAME)
BEGIN_CASE(JSOP_SETNAME)
BEGIN_CASE(JSOP_SETPROP)
BEGIN_CASE(JSOP_SETMETHOD)
{
    Value rval = regs.sp[-1];
    JS_ASSERT_IF(op == JSOP_SETMETHOD, IsFunctionObject(rval));
    Value &lref = regs.sp[-2];
    JS_ASSERT_IF(op == JSOP_SETNAME, lref.isObject());
    JSObject *obj;
    VALUE_TO_OBJECT(cx, &lref, obj);

    JS_ASSERT_IF(op == JSOP_SETGNAME, obj == regs.fp->scopeChain().getGlobal());

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
            JS_ASSERT(!obj->sealed());

            /*
             * Fast property cache hit, only partially confirmed by
             * testForSet. We know that the entry applies to regs.pc and
             * that obj's shape matches.
             *
             * The entry predicts either a new property to be added
             * directly to obj by this set, or on an existing "own"
             * property, or on a prototype property that has a setter.
             */
            const Shape *shape = entry->vword.toShape();
            JS_ASSERT_IF(shape->isDataDescriptor(), shape->writable());
            JS_ASSERT_IF(shape->hasSlot(), entry->vcapTag() == 0);

            /*
             * Fastest path: check whether obj already has the cached shape and
             * call NATIVE_SET and break to get out of the do-while(0). But we
             * can call NATIVE_SET only for a direct or proto-setter hit.
             */
            if (!entry->adding()) {
                if (entry->vcapTag() == 0 ||
                    ((obj2 = obj->getProto()) && obj2->shape() == entry->vshape()))
                {
#ifdef DEBUG
                    if (entry->directHit()) {
                        JS_ASSERT(obj->nativeContains(*shape));
                    } else {
                        JS_ASSERT(obj2->nativeContains(*shape));
                        JS_ASSERT(entry->vcapTag() == 1);
                        JS_ASSERT(entry->kshape != entry->vshape());
                        JS_ASSERT(!shape->hasSlot());
                    }
#endif

                    PCMETER(cache->pchits++);
                    PCMETER(cache->setpchits++);
                    NATIVE_SET(cx, obj, shape, entry, &rval);
                    break;
                }
            } else {
                if (obj->nativeEmpty()) {
                    /*
                     * We check that cx owns obj here and will continue to own
                     * it after ensureClassReservedSlotsForEmptyObject returns
                     * so we can continue to skip JS_UNLOCK_OBJ calls.
                     */
                    JS_ASSERT(CX_OWNS_OBJECT_TITLE(cx, obj));
                    bool ok = obj->ensureClassReservedSlotsForEmptyObject(cx);
                    JS_ASSERT(CX_OWNS_OBJECT_TITLE(cx, obj));
                    if (!ok)
                        goto error;
                }

                uint32 slot;
                if (shape->previous() == obj->lastProperty() &&
                    entry->vshape() == rt->protoHazardShape &&
                    shape->hasDefaultSetter()) {
                    slot = shape->slot;
                    JS_ASSERT(slot == obj->slotSpan());

                    /*
                     * Fast path: adding a plain old property that was once at
                     * the frontier of the property tree, whose slot is next to
                     * claim among the already-allocated slots in obj, where
                     * shape->table has not been created yet.
                     */
                    PCMETER(cache->pchits++);
                    PCMETER(cache->addpchits++);

                    if (slot < obj->numSlots()) {
                        JS_ASSERT(obj->getSlot(slot).isUndefined());
                    } else {
                        if (!obj->allocSlot(cx, &slot))
                            goto error;
                        JS_ASSERT(slot == shape->slot);
                    }

                    /* Simply extend obj's property tree path with shape! */
                    obj->extend(cx, shape);

                    /*
                     * No method change check here because here we are adding a
                     * new property, not updating an existing slot's value that
                     * might contain a method of a branded shape.
                     */
                    TRACE_2(SetPropHit, entry, shape);
                    obj->lockedSetSlot(slot, rval);

                    /*
                     * Purge the property cache of the id we may have just
                     * shadowed in obj's scope and proto chains.
                     */
                    js_PurgeScopeChain(cx, obj, shape->id);
                    break;
                }
            }
            PCMETER(cache->setpcmisses++);
            atom = NULL;
        } else if (!atom) {
            /*
             * Slower property cache hit, fully confirmed by testForSet (in the
             * slow path, via fullTest).
             */
            ASSERT_VALID_PROPERTY_CACHE_HIT(0, obj, obj2, entry);
            const Shape *shape = NULL;
            if (obj == obj2) {
                shape = entry->vword.toShape();
                JS_ASSERT(shape->writable());
                JS_ASSERT(!obj2->sealed());
                NATIVE_SET(cx, obj, shape, entry, &rval);
            }
            if (shape)
                break;
        }

        if (!atom)
            LOAD_ATOM(0, atom);
        jsid id = ATOM_TO_JSID(atom);
        if (entry && JS_LIKELY(!obj->getOps()->setProperty)) {
            uintN defineHow;
            if (op == JSOP_SETMETHOD)
                defineHow = JSDNP_CACHE_RESULT | JSDNP_SET_METHOD;
            else if (op == JSOP_SETNAME)
                defineHow = JSDNP_CACHE_RESULT | JSDNP_UNQUALIFIED;
            else
                defineHow = JSDNP_CACHE_RESULT;
            if (!js_SetPropertyHelper(cx, obj, id, defineHow, &rval))
                goto error;
        } else {
            if (!obj->setProperty(cx, id, &rval))
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
        } else if (obj->isArguments()) {
            uint32 arg = uint32(i);

            if (arg < obj->getArgsInitialLength()) {
                JSStackFrame *afp = (JSStackFrame *) obj->getPrivate();
                if (afp) {
                    copyFrom = &afp->canonicalActualArg(arg);
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
    Value rval;
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
    rval = regs.sp[-1];
    if (!obj->setProperty(cx, id, &rval))
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
    Value rval = regs.sp[-3];
    if (!obj->setProperty(cx, id, &rval))
        goto error;
    regs.sp -= 3;
}
END_CASE(JSOP_ENUMELEM)

{
    JSFunction *newfun;
    JSObject *callee;
    uint32 flags;
    uintN argc;
    Value *vp;

BEGIN_CASE(JSOP_NEW)
{
    /* Get immediate argc and find the constructor function. */
    argc = GET_ARGC(regs.pc);
    vp = regs.sp - (2 + argc);
    JS_ASSERT(vp >= regs.fp->base());

    /*
     * Assign lval, callee, and newfun exactly as the code at inline_call: expects to
     * find them, to avoid nesting a js_Interpret call via js_InvokeConstructor.
     */
    if (IsFunctionObject(vp[0], &callee)) {
        newfun = callee->getFunctionPrivate();
        if (newfun->isInterpreted()) {
            /* Root as we go using vp[1]. */
            if (!callee->getProperty(cx,
                                     ATOM_TO_JSID(cx->runtime->atomState.classPrototypeAtom),
                                     &vp[1])) {
                goto error;
            }
            JSObject *proto = vp[1].isObject() ? &vp[1].toObject() : NULL;
            JSObject *obj2 = NewNonFunction<WithProto::Class>(cx, &js_ObjectClass, proto, callee->getParent());
            if (!obj2)
                goto error;

            if (newfun->u.i.script->isEmpty()) {
                vp[0].setObject(*obj2);
                regs.sp = vp + 1;
                goto end_new;
            }

            vp[1].setObject(*obj2);
            flags = JSFRAME_CONSTRUCTING;
            goto inline_call;
        }
    }

    if (!InvokeConstructor(cx, InvokeArgsAlreadyOnTheStack(vp, argc)))
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

    if (IsFunctionObject(*vp, &callee)) {
        newfun = callee->getFunctionPrivate();

        /* Clear frame flags since this is not a constructor call. */
        flags = 0;
        if (newfun->isInterpreted())
      inline_call:
        {
            JSScript *newscript = newfun->u.i.script;
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

            /* Get pointer to new frame/slots, prepare arguments. */
            StackSpace &stack = cx->stack();
            JSStackFrame *newfp = stack.getInlineFrame(cx, regs.sp, argc, newfun,
                                                       newscript, &flags);
            if (JS_UNLIKELY(!newfp))
                goto error;
            JS_ASSERT_IF(!vp[1].isPrimitive(), IsSaneThisObject(vp[1].toObject()));

            /* Initialize frame, locals. */
            newfp->initCallFrame(cx, *callee, newfun, argc, flags);
            SetValueRangeToUndefined(newfp->slots(), newscript->nfixed);

            /* Officially push the frame. */
            stack.pushInlineFrame(cx, newscript, newfp, &regs);

            /* Refresh interpreter locals. */
            JS_ASSERT(newfp == regs.fp);
            script = newscript;
            argv = regs.fp->formalArgsEnd() - newfun->nargs;
            atoms = script->atomMap.vector;

            /* Now that the new frame is rooted, maybe create a call object. */
            if (newfun->isHeavyweight() && !js_GetCallObject(cx, regs.fp))
                goto error;

            /* Call the debugger hook if present. */
            if (JSInterpreterHook hook = cx->debugHooks->callHook) {
                regs.fp->setHookData(hook(cx, regs.fp, JS_TRUE, 0,
                                          cx->debugHooks->callHookData));
                CHECK_INTERRUPT_HANDLER();
            }

            inlineCallCount++;
            JS_RUNTIME_METER(rt, inlineCalls);

            Probes::enterJSFun(cx, newfun);

            TRACE_0(EnterFrame);

#ifdef JS_METHODJIT
            /* Try to ensure methods are method JIT'd.  */
            {
                JSObject *scope = &regs.fp->scopeChain();
                mjit::CompileStatus status = mjit::CanMethodJIT(cx, newscript, newfun, scope);
                if (status == mjit::Compile_Error)
                    goto error;
                if (!TRACE_RECORDER(cx) && status == mjit::Compile_Okay) {
                    if (!mjit::JaegerShot(cx))
                        goto error;
                    interpReturnOK = true;
                    goto inline_return;
                }
            }
#endif

            /* Load first op and dispatch it (safe since JSOP_STOP). */
            op = (JSOp) *regs.pc;
            DO_OP();
        }

        JS_ASSERT(vp[1].isObjectOrNull() || PrimitiveThisTest(newfun, vp[1]));

        Probes::enterJSFun(cx, newfun);
        JSBool ok = newfun->u.n.native(cx, argc, vp);
        Probes::exitJSFun(cx, newfun);
        regs.sp = vp + 1;
        if (!ok)
            goto error;
        TRACE_0(NativeCallComplete);
        goto end_call;
    }

    bool ok;
    ok = Invoke(cx, InvokeArgsAlreadyOnTheStack(vp, argc), 0);
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
    JSBool ok = Invoke(cx, InvokeArgsAlreadyOnTheStack(vp, argc), 0);
    if (ok)
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_LEFTSIDE_OF_ASS);
    goto error;
}
END_CASE(JSOP_SETCALL)

#define SLOW_PUSH_THISV(cx, obj)                                            \
    JS_BEGIN_MACRO                                                          \
        Class *clasp;                                                       \
        JSObject *thisp = obj;                                              \
        if (!thisp->getParent() ||                                          \
            (clasp = thisp->getClass()) == &js_CallClass ||                 \
            clasp == &js_BlockClass ||                                      \
            clasp == &js_DeclEnvClass) {                                    \
            /* Normal case: thisp is global or an activation record. */     \
            /* Callee determines |this|. */                                 \
            thisp = NULL;                                                   \
        } else {                                                            \
            thisp = thisp->thisObject(cx);                                  \
            if (!thisp)                                                     \
                goto error;                                                 \
        }                                                                   \
        PUSH_OBJECT_OR_NULL(thisp);                                         \
    JS_END_MACRO

BEGIN_CASE(JSOP_GETGNAME)
BEGIN_CASE(JSOP_CALLGNAME)
BEGIN_CASE(JSOP_NAME)
BEGIN_CASE(JSOP_CALLNAME)
{
    JSObject *obj = &regs.fp->scopeChain();

    const Shape *shape;
    Value rval;

    PropertyCacheEntry *entry;
    JSObject *obj2;
    JSAtom *atom;
    JS_PROPERTY_CACHE(cx).test(cx, regs.pc, obj, obj2, entry, atom);
    if (!atom) {
        ASSERT_VALID_PROPERTY_CACHE_HIT(0, obj, obj2, entry);
        if (entry->vword.isFunObj()) {
            PUSH_OBJECT(entry->vword.toFunObj());
        } else if (entry->vword.isSlot()) {
            uintN slot = entry->vword.toSlot();
            JS_ASSERT(obj2->containsSlot(slot));
            PUSH_COPY(obj2->lockedGetSlot(slot));
        } else {
            JS_ASSERT(entry->vword.isShape());
            shape = entry->vword.toShape();
            NATIVE_GET(cx, obj, obj2, shape, JSGET_METHOD_BARRIER, &rval);
            PUSH_COPY(rval);
        }

        /*
         * Push results, the same as below, but with a prop$ hit there
         * is no need to test for the unusual and uncacheable case where
         * the caller determines |this|.
         */
#if DEBUG
        Class *clasp;
        JS_ASSERT(!obj->getParent() ||
                  (clasp = obj->getClass()) == &js_CallClass ||
                  clasp == &js_BlockClass ||
                  clasp == &js_DeclEnvClass);
#endif
        if (op == JSOP_CALLNAME || op == JSOP_CALLGNAME)
            PUSH_NULL();
        len = JSOP_NAME_LENGTH;
        DO_NEXT_OP(len);
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
        shape = (Shape *)prop;
        JSObject *normalized = obj;
        if (normalized->getClass() == &js_WithClass && !shape->hasDefaultGetter())
            normalized = js_UnwrapWithObject(cx, normalized);
        NATIVE_GET(cx, normalized, obj2, shape, JSGET_METHOD_BARRIER, &rval);
        JS_UNLOCK_OBJ(cx, obj2);
    }

    PUSH_COPY(rval);

    /* obj must be on the scope chain, thus not a function. */
    if (op == JSOP_CALLNAME || op == JSOP_CALLGNAME)
        SLOW_PUSH_THISV(cx, obj);
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
    JS_ASSERT(!regs.fp->hasImacropc());
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
    if (!js_GetClassPrototype(cx, &regs.fp->scopeChain(), JSProto_RegExp, &proto))
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
    JS_ASSERT(!regs.fp->hasImacropc());
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
        regs.fp->setReturnValue(rval);
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
    if (!js_GetArgsValue(cx, regs.fp, &rval))
        goto error;
    PUSH_COPY(rval);
}
END_CASE(JSOP_ARGUMENTS)

BEGIN_CASE(JSOP_ARGSUB)
{
    jsid id = INT_TO_JSID(GET_ARGNO(regs.pc));
    Value rval;
    if (!js_GetArgsProperty(cx, regs.fp, id, &rval))
        goto error;
    PUSH_COPY(rval);
}
END_CASE(JSOP_ARGSUB)

BEGIN_CASE(JSOP_ARGCNT)
{
    jsid id = ATOM_TO_JSID(rt->atomState.lengthAtom);
    Value rval;
    if (!js_GetArgsProperty(cx, regs.fp, id, &rval))
        goto error;
    PUSH_COPY(rval);
}
END_CASE(JSOP_ARGCNT)

BEGIN_CASE(JSOP_GETARG)
BEGIN_CASE(JSOP_CALLARG)
{
    uint32 slot = GET_ARGNO(regs.pc);
    JS_ASSERT(slot < regs.fp->numFormalArgs());
    METER_SLOT_OP(op, slot);
    PUSH_COPY(argv[slot]);
    if (op == JSOP_CALLARG)
        PUSH_NULL();
}
END_CASE(JSOP_GETARG)

BEGIN_CASE(JSOP_SETARG)
{
    uint32 slot = GET_ARGNO(regs.pc);
    JS_ASSERT(slot < regs.fp->numFormalArgs());
    METER_SLOT_OP(op, slot);
    argv[slot] = regs.sp[-1];
}
END_SET_CASE(JSOP_SETARG)

BEGIN_CASE(JSOP_GETLOCAL)
{
    uint32 slot = GET_SLOTNO(regs.pc);
    JS_ASSERT(slot < script->nslots);
    PUSH_COPY(regs.fp->slots()[slot]);
}
END_CASE(JSOP_GETLOCAL)

BEGIN_CASE(JSOP_CALLLOCAL)
{
    uint32 slot = GET_SLOTNO(regs.pc);
    JS_ASSERT(slot < script->nslots);
    PUSH_COPY(regs.fp->slots()[slot]);
    PUSH_NULL();
}
END_CASE(JSOP_CALLLOCAL)

BEGIN_CASE(JSOP_SETLOCAL)
{
    uint32 slot = GET_SLOTNO(regs.pc);
    JS_ASSERT(slot < script->nslots);
    regs.fp->slots()[slot] = regs.sp[-1];
}
END_SET_CASE(JSOP_SETLOCAL)

BEGIN_CASE(JSOP_GETUPVAR)
BEGIN_CASE(JSOP_CALLUPVAR)
{
    JSUpvarArray *uva = script->upvars();

    uintN index = GET_UINT16(regs.pc);
    JS_ASSERT(index < uva->length);

    const Value &rval = GetUpvar(cx, script->staticLevel, uva->vector[index]);
    PUSH_COPY(rval);

    if (op == JSOP_CALLUPVAR)
        PUSH_NULL();
}
END_CASE(JSOP_GETUPVAR)

BEGIN_CASE(JSOP_GETUPVAR_DBG)
BEGIN_CASE(JSOP_CALLUPVAR_DBG)
{
    JSFunction *fun = regs.fp->fun();
    JS_ASSERT(FUN_KIND(fun) == JSFUN_INTERPRETED);
    JS_ASSERT(fun->u.i.wrapper);

    /* Scope for tempPool mark and local names allocation in it. */
    JSObject *obj, *obj2;
    JSProperty *prop;
    jsid id;
    JSAtom *atom;
    {
        AutoLocalNameArray names(cx, fun);
        if (!names)
            goto error;

        uintN index = fun->countArgsAndVars() + GET_UINT16(regs.pc);
        atom = JS_LOCAL_NAME_TO_ATOM(names[index]);
        id = ATOM_TO_JSID(atom);

        if (!js_FindProperty(cx, id, &obj, &obj2, &prop))
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

BEGIN_CASE(JSOP_GETFCSLOT)
BEGIN_CASE(JSOP_CALLFCSLOT)
{
    JS_ASSERT(regs.fp->isFunctionFrame() && !regs.fp->isEvalFrame());
    uintN index = GET_UINT16(regs.pc);
    JSObject *obj = &argv[-2].toObject();

    JS_ASSERT(index < obj->getFunctionPrivate()->u.i.nupvars);
    PUSH_COPY(obj->getFlatClosureUpvar(index));
    if (op == JSOP_CALLFCSLOT)
        PUSH_NULL();
}
END_CASE(JSOP_GETFCSLOT)

BEGIN_CASE(JSOP_GETGLOBAL)
BEGIN_CASE(JSOP_CALLGLOBAL)
{
    uint32 slot = GET_SLOTNO(regs.pc);
    slot = script->getGlobalSlot(slot);
    JSObject *obj = regs.fp->scopeChain().getGlobal();
    JS_ASSERT(obj->containsSlot(slot));
    PUSH_COPY(obj->getSlot(slot));
    if (op == JSOP_CALLGLOBAL)
        PUSH_NULL();
}
END_CASE(JSOP_GETGLOBAL)

BEGIN_CASE(JSOP_FORGLOBAL)
{
    Value rval;
    if (!IteratorNext(cx, &regs.sp[-1].toObject(), &rval))
        goto error;
    PUSH_COPY(rval);
    uint32 slot = GET_SLOTNO(regs.pc);
    slot = script->getGlobalSlot(slot);
    JSObject *obj = regs.fp->scopeChain().getGlobal();
    JS_ASSERT(obj->containsSlot(slot));
    JS_LOCK_OBJ(cx, obj);
    {
        if (!obj->methodWriteBarrier(cx, slot, rval)) {
            JS_UNLOCK_OBJ(cx, obj);
            goto error;
        }
        obj->lockedSetSlot(slot, rval);
        JS_UNLOCK_OBJ(cx, obj);
    }
    regs.sp--;
}
END_CASE(JSOP_FORGLOBAL)

BEGIN_CASE(JSOP_SETGLOBAL)
{
    uint32 slot = GET_SLOTNO(regs.pc);
    slot = script->getGlobalSlot(slot);
    JSObject *obj = regs.fp->scopeChain().getGlobal();
    JS_ASSERT(obj->containsSlot(slot));
    {
        JS_LOCK_OBJ(cx, obj);
        if (!obj->methodWriteBarrier(cx, slot, regs.sp[-1])) {
            JS_UNLOCK_OBJ(cx, obj);
            goto error;
        }
        obj->lockedSetSlot(slot, regs.sp[-1]);
        JS_UNLOCK_OBJ(cx, obj);
    }
}
END_SET_CASE(JSOP_SETGLOBAL)

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
    JSObject *obj = &regs.fp->varobj(cx);
    JS_ASSERT(!obj->getOps()->defineProperty);
    uintN attrs = JSPROP_ENUMERATE;
    if (!regs.fp->isEvalFrame())
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

    obj2->dropProperty(cx, prop);
}
END_CASE(JSOP_DEFVAR)

BEGIN_CASE(JSOP_DEFFUN)
{
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
        obj2 = &regs.fp->scopeChain();
    } else {
        JS_ASSERT(!FUN_FLAT_CLOSURE(fun));

        /*
         * Inline js_GetScopeChain a bit to optimize for the case of a
         * top-level function.
         */
        if (!regs.fp->hasBlockChain()) {
            obj2 = &regs.fp->scopeChain();
        } else {
            obj2 = js_GetScopeChain(cx, regs.fp);
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
     * ECMA requires functions defined when entering Eval code to be
     * impermanent.
     */
    uintN attrs = regs.fp->isEvalFrame()
                  ? JSPROP_ENUMERATE
                  : JSPROP_ENUMERATE | JSPROP_PERMANENT;

    /*
     * We define the function as a property of the variable object and not the
     * current scope chain even for the case of function expression statements
     * and functions defined by eval inside let or with blocks.
     */
    JSObject *parent = &regs.fp->varobj(cx);

    /*
     * Check for a const property of the same name -- or any kind of property
     * if executing with the strict option.  We check here at runtime as well
     * as at compile-time, to handle eval as well as multiple HTML script tags.
     */
    jsid id = ATOM_TO_JSID(fun->atom);
    JSProperty *prop = NULL;
    JSObject *pobj;
    JSBool ok = CheckRedeclaration(cx, parent, id, attrs, &pobj, &prop);
    if (!ok)
        goto error;

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
    uint32 old;
    bool doSet = (attrs == JSPROP_ENUMERATE);
    JS_ASSERT_IF(doSet, regs.fp->isEvalFrame());
    if (prop) {
        if (parent == pobj &&
            parent->isCall() &&
            (old = ((Shape *) prop)->attributes(),
             !(old & (JSPROP_GETTER|JSPROP_SETTER)) &&
             (old & (JSPROP_ENUMERATE|JSPROP_PERMANENT)) == attrs)) {
            /*
             * js_CheckRedeclaration must reject attempts to add a getter or
             * setter to an existing property without a getter or setter.
             */
            JS_ASSERT(!(attrs & ~(JSPROP_ENUMERATE|JSPROP_PERMANENT)));
            JS_ASSERT(!(old & JSPROP_READONLY));
            doSet = true;
        }
        pobj->dropProperty(cx, prop);
    }
    Value rval = ObjectValue(*obj);
    ok = doSet
         ? parent->setProperty(cx, id, &rval)
         : parent->defineProperty(cx, id, rval, PropertyStub, PropertyStub, attrs);
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

    uintN attrs = regs.fp->isEvalFrame()
                  ? JSPROP_ENUMERATE
                  : JSPROP_ENUMERATE | JSPROP_PERMANENT;

    JSObject &parent = regs.fp->varobj(cx);

    jsid id = ATOM_TO_JSID(fun->atom);
    if (!CheckRedeclaration(cx, &parent, id, attrs, NULL, NULL))
        goto error;

    if ((attrs == JSPROP_ENUMERATE)
        ? !parent.setProperty(cx, id, &rval)
        : !parent.defineProperty(cx, id, rval, PropertyStub, PropertyStub, attrs)) {
        goto error;
    }
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
        obj = CloneFunctionObject(cx, fun, &regs.fp->scopeChain());
        if (!obj)
            goto error;
    } else {
        JSObject *parent = js_GetScopeChain(cx, regs.fp);
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

    regs.fp->slots()[slot].setObject(*obj);
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

    regs.fp->slots()[slot].setObject(*obj);
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
    regs.fp->slots()[slot].setObject(*obj);
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
            parent = &regs.fp->scopeChain();

            if (obj->getParent() == parent) {
                jsbytecode *pc2 = regs.pc + JSOP_LAMBDA_LENGTH;
                JSOp op2 = JSOp(*pc2);

                /*
                 * Optimize var obj = {method: function () { ... }, ...},
                 * this.method = function () { ... }; and other significant
                 * single-use-of-null-closure bytecode sequences.
                 *
                 * WARNING: code in TraceRecorder::record_JSOP_LAMBDA must
                 * match the optimization cases in the following code that
                 * break from the outer do-while(0).
                 */
                if (op2 == JSOP_INITMETHOD) {
#ifdef DEBUG
                    const Value &lref = regs.sp[-1];
                    JS_ASSERT(lref.isObject());
                    JSObject *obj2 = &lref.toObject();
                    JS_ASSERT(obj2->getClass() == &js_ObjectClass);
#endif

                    fun->setMethodAtom(script->getAtom(GET_FULL_INDEX(JSOP_LAMBDA_LENGTH)));
                    JS_FUNCTION_METER(cx, joinedinitmethod);
                    break;
                }

                if (op2 == JSOP_SETMETHOD) {
#ifdef DEBUG
                    op2 = JSOp(pc2[JSOP_SETMETHOD_LENGTH]);
                    JS_ASSERT(op2 == JSOP_POP || op2 == JSOP_POPV);
#endif

                    const Value &lref = regs.sp[-1];
                    if (lref.isObject() && lref.toObject().canHaveMethodBarrier()) {
                        fun->setMethodAtom(script->getAtom(GET_FULL_INDEX(JSOP_LAMBDA_LENGTH)));
                        JS_FUNCTION_METER(cx, joinedsetmethod);
                        break;
                    }
                } else if (fun->joinable()) {
                    if (op2 == JSOP_CALL) {
                        /*
                         * Array.prototype.sort and String.prototype.replace are
                         * optimized as if they are special form. We know that they
                         * won't leak the joined function object in obj, therefore
                         * we don't need to clone that compiler- created function
                         * object for identity/mutation reasons.
                         */
                        int iargc = GET_ARGC(pc2);

                        /*
                         * Note that we have not yet pushed obj as the final argument,
                         * so regs.sp[1 - (iargc + 2)], and not regs.sp[-(iargc + 2)],
                         * is the callee for this JSOP_CALL.
                         */
                        const Value &cref = regs.sp[1 - (iargc + 2)];
                        JSObject *callee;

                        if (IsFunctionObject(cref, &callee)) {
                            JSFunction *calleeFun = GET_FUNCTION_PRIVATE(cx, callee);
                            if (Native native = calleeFun->maybeNative()) {
                                if (iargc == 1 && native == array_sort) {
                                    JS_FUNCTION_METER(cx, joinedsort);
                                    break;
                                }
                                if (iargc == 2 && native == str_replace) {
                                    JS_FUNCTION_METER(cx, joinedreplace);
                                    break;
                                }
                            }
                        }
                    } else if (op2 == JSOP_NULL) {
                        pc2 += JSOP_NULL_LENGTH;
                        op2 = JSOp(*pc2);

                        if (op2 == JSOP_CALL && GET_ARGC(pc2) == 0) {
                            JS_FUNCTION_METER(cx, joinedmodulepat);
                            break;
                        }
                    }
                }
            }

#ifdef DEBUG
            if (rt->functionMeterFilename) {
                // No locking, this is mainly for js shell testing.
                ++rt->functionMeter.unjoined;

                typedef JSRuntime::FunctionCountMap HM;
                HM &h = rt->unjoinedFunctionCountMap;
                HM::AddPtr p = h.lookupForAdd(fun);
                if (!p) {
                    h.add(p, fun, 1);
                } else {
                    JS_ASSERT(p->key == fun);
                    ++p->value;
                }
            }
#endif
        } else {
            parent = js_GetScopeChain(cx, regs.fp);
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
    JS_ASSERT(regs.fp->isFunctionFrame() && !regs.fp->isEvalFrame());
    PUSH_COPY(argv[-2]);
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
        JS_ASSERT(regs.sp - regs.fp->base() >= 2);
        rval = regs.sp[-1];
        i = -1;
        JSAtom *atom;
        LOAD_ATOM(0, atom);
        id = ATOM_TO_JSID(atom);
        goto gs_get_lval;
      }
      default:
        JS_ASSERT(op2 == JSOP_INITELEM);

        JS_ASSERT(regs.sp - regs.fp->base() >= 3);
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
    }

    PUSH_OBJECT(*obj);
    CHECK_INTERRUPT_HANDLER();
}
END_CASE(JSOP_NEWINIT)

BEGIN_CASE(JSOP_ENDINIT)
{
    /* FIXME remove JSOP_ENDINIT bug 588522 */
    JS_ASSERT(regs.sp - regs.fp->base() >= 1);
    JS_ASSERT(regs.sp[-1].isObject());
}
END_CASE(JSOP_ENDINIT)

BEGIN_CASE(JSOP_INITPROP)
BEGIN_CASE(JSOP_INITMETHOD)
{
    /* Load the property's initial value into rval. */
    JS_ASSERT(regs.sp - regs.fp->base() >= 2);
    Value rval = regs.sp[-1];

    /* Load the object being initialized into lval/obj. */
    JSObject *obj = &regs.sp[-2].toObject();
    JS_ASSERT(obj->isNative());

    /*
     * Probe the property cache.
     *
     * We can not assume that the object created by JSOP_NEWINIT is still
     * single-threaded as the debugger can access it from other threads.
     * So check first.
     *
     * On a hit, if the cached shape has a non-default setter, it must be
     * __proto__. If shape->previous() != obj->lastProperty(), there must be a
     * repeated property name. The fast path does not handle these two cases.
     */
    PropertyCacheEntry *entry;
    const Shape *shape;
    if (CX_OWNS_OBJECT_TITLE(cx, obj) &&
        JS_PROPERTY_CACHE(cx).testForInit(rt, regs.pc, obj, &shape, &entry) &&
        shape->hasDefaultSetter() &&
        shape->previous() == obj->lastProperty())
    {
        /* Fast path. Property cache hit. */
        uint32 slot = shape->slot;

        JS_ASSERT(slot == obj->slotSpan());
        JS_ASSERT(slot >= JSSLOT_FREE(obj->getClass()));
        if (slot < obj->numSlots()) {
            JS_ASSERT(obj->getSlot(slot).isUndefined());
        } else {
            if (!obj->allocSlot(cx, &slot))
                goto error;
            JS_ASSERT(slot == shape->slot);
        }

        /* A new object, or one we just extended in a recent initprop op. */
        JS_ASSERT(!obj->lastProperty() ||
                  obj->shape() == obj->lastProperty()->shape);
        obj->extend(cx, shape);

        /*
         * No method change check here because here we are adding a new
         * property, not updating an existing slot's value that might
         * contain a method of a branded shape.
         */
        TRACE_2(SetPropHit, entry, shape);
        obj->lockedSetSlot(slot, rval);
    } else {
        PCMETER(JS_PROPERTY_CACHE(cx).inipcmisses++);

        /* Get the immediate property name into id. */
        JSAtom *atom;
        LOAD_ATOM(0, atom);
        jsid id = ATOM_TO_JSID(atom);

        /* No need to check for duplicate property; the compiler already did. */

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
    JS_ASSERT(regs.sp - regs.fp->base() >= 3);
    const Value &rref = regs.sp[-1];

    /* Find the object being initialized at top of stack. */
    const Value &lref = regs.sp[-3];
    JS_ASSERT(lref.isObject());
    JSObject *obj = &lref.toObject();

    /* Fetch id now that we have obj. */
    jsid id;
    FETCH_ELEMENT_ID(obj, -2, id);

    /* No need to check for duplicate property; the compiler already did. */

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
    JS_ASSERT(slot + 1 < regs.fp->numFixed());
    const Value &lref = regs.fp->slots()[slot];
    JSObject *obj;
    if (lref.isObject()) {
        obj = &lref.toObject();
    } else {
        JS_ASSERT(lref.isUndefined());
        obj = js_NewArrayObject(cx, 0, NULL);
        if (!obj)
            goto error;
        regs.fp->slots()[slot].setObject(*obj);
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
    JS_ASSERT(slot + 1 < regs.fp->numFixed());
    const Value &lref = regs.fp->slots()[slot];
    jsint i = (jsint) GET_UINT16(regs.pc + UINT16_LEN);
    Value rval;
    if (lref.isUndefined()) {
        rval.setUndefined();
    } else {
        JSObject *obj = &regs.fp->slots()[slot].toObject();
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
    JS_ASSERT(slot + 1 < regs.fp->numFixed());
    Value *vp = &regs.fp->slots()[slot];
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
    JS_ASSERT((size_t) (regs.sp - regs.fp->base()) >= 2);
    uint32 slot = GET_UINT16(regs.pc);
    JS_ASSERT(slot + 1 < script->nslots);
    POP_COPY_TO(regs.fp->slots()[slot]);
}
END_CASE(JSOP_SETLOCALPOP)

BEGIN_CASE(JSOP_IFPRIMTOP)
    /*
     * If the top of stack is of primitive type, jump to our target. Otherwise
     * advance to the next opcode.
     */
    JS_ASSERT(regs.sp > regs.fp->base());
    if (regs.sp[-1].isPrimitive()) {
        len = GET_JUMP_OFFSET(regs.pc);
        BRANCH(len);
    }
END_CASE(JSOP_IFPRIMTOP)

BEGIN_CASE(JSOP_PRIMTOP)
    JS_ASSERT(regs.sp > regs.fp->base());
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
    if (!HasInstance(cx, obj, &lref, &cond))
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
            regs.fp->setReturnValue(rval);
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
    if (!js_SetDefaultXMLNamespace(cx, regs.sp[-1]))
        goto error;
    regs.sp--;
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
        SLOW_PUSH_THISV(cx, obj);
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
    JS_ASSERT(obj->isStaticBlock());
    JS_ASSERT(regs.fp->base() + OBJ_BLOCK_DEPTH(cx, obj) == regs.sp);
    Value *vp = regs.sp + OBJ_BLOCK_COUNT(cx, obj);
    JS_ASSERT(regs.sp < vp);
    JS_ASSERT(vp <= regs.fp->slots() + script->nslots);
    SetValueRangeToUndefined(regs.sp, vp);
    regs.sp = vp;

#ifdef DEBUG
    JS_ASSERT(regs.fp->maybeBlockChain() == obj->getParent());

    /*
     * The young end of fp->scopeChain may omit blocks if we haven't closed
     * over them, but if there are any closure blocks on fp->scopeChain, they'd
     * better be (clones of) ancestors of the block we're entering now;
     * anything else we should have popped off fp->scopeChain when we left its
     * static scope.
     */
    JSObject *obj2 = &regs.fp->scopeChain();
    Class *clasp;
    while ((clasp = obj2->getClass()) == &js_WithClass)
        obj2 = obj2->getParent();
    if (clasp == &js_BlockClass &&
        obj2->getPrivate() == js_FloatingFrameIfGenerator(cx, regs.fp)) {
        JSObject *youngestProto = obj2->getProto();
        JS_ASSERT(youngestProto->isStaticBlock());
        JSObject *parent = obj;
        while ((parent = parent->getParent()) != youngestProto)
            JS_ASSERT(parent);
    }
#endif

    regs.fp->setBlockChain(obj);
}
END_CASE(JSOP_ENTERBLOCK)

BEGIN_CASE(JSOP_LEAVEBLOCKEXPR)
BEGIN_CASE(JSOP_LEAVEBLOCK)
{
#ifdef DEBUG
    JS_ASSERT(regs.fp->blockChain()->isStaticBlock());
    uintN blockDepth = OBJ_BLOCK_DEPTH(cx, regs.fp->blockChain());

    JS_ASSERT(blockDepth <= StackDepth(script));
#endif
    /*
     * If we're about to leave the dynamic scope of a block that has been
     * cloned onto fp->scopeChain, clear its private data, move its locals from
     * the stack into the clone, and pop it off the chain.
     */
    JSObject &obj = regs.fp->scopeChain();
    if (obj.getProto() == regs.fp->blockChain()) {
        JS_ASSERT(obj.isClonedBlock());
        if (!js_PutBlockObject(cx, JS_TRUE))
            goto error;
    }

    /* Pop the block chain, too.  */
    regs.fp->setBlockChain(regs.fp->blockChain()->getParent());

    /* Move the result of the expression to the new topmost stack slot. */
    Value *vp = NULL;  /* silence GCC warnings */
    if (op == JSOP_LEAVEBLOCKEXPR)
        vp = &regs.sp[-1];
    regs.sp -= GET_UINT16(regs.pc);
    if (op == JSOP_LEAVEBLOCKEXPR) {
        JS_ASSERT(regs.fp->base() + blockDepth == regs.sp - 1);
        regs.sp[-1] = *vp;
    } else {
        JS_ASSERT(regs.fp->base() + blockDepth == regs.sp);
    }
}
END_CASE(JSOP_LEAVEBLOCK)

#if JS_HAS_GENERATORS
BEGIN_CASE(JSOP_GENERATOR)
{
    JS_ASSERT(!cx->throwing);
    regs.pc += JSOP_GENERATOR_LENGTH;
    JSObject *obj = js_NewGenerator(cx);
    if (!obj)
        goto error;
    JS_ASSERT(!regs.fp->hasCallObj() && !regs.fp->hasArgsObj());
    regs.fp->setReturnValue(ObjectValue(*obj));
    interpReturnOK = true;
    if (entryFrame != regs.fp)
        goto inline_return;
    goto exit;
}

BEGIN_CASE(JSOP_YIELD)
    JS_ASSERT(!cx->throwing);
    JS_ASSERT(regs.fp->isFunctionFrame() && !regs.fp->isEvalFrame());
    if (cx->generatorFor(regs.fp)->state == JSGEN_CLOSING) {
        js_ReportValueError(cx, JSMSG_BAD_GENERATOR_YIELD,
                            JSDVG_SEARCH_STACK, argv[-2], NULL);
        goto error;
    }
    regs.fp->setReturnValue(regs.sp[-1]);
    regs.fp->setYielding();
    regs.pc += JSOP_YIELD_LENGTH;
    interpReturnOK = JS_TRUE;
    goto exit;

BEGIN_CASE(JSOP_ARRAYPUSH)
{
    uint32 slot = GET_UINT16(regs.pc);
    JS_ASSERT(script->nfixed <= slot);
    JS_ASSERT(slot < script->nslots);
    JSObject *obj = &regs.fp->slots()[slot].toObject();
    if (!js_ArrayCompPush(cx, obj, regs.sp[-1]))
        goto error;
    regs.sp--;
}
END_CASE(JSOP_ARRAYPUSH)
#endif /* JS_HAS_GENERATORS */

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
    if (regs.fp->hasImacropc() && cx->throwing) {
        // Handle other exceptions as if they came from the imacro-calling pc.
        regs.pc = regs.fp->imacropc();
        regs.fp->clearImacropc();
        atoms = script->atomMap.vector;
    }
#endif

    JS_ASSERT(size_t((regs.fp->hasImacropc() ? regs.fp->imacropc() : regs.pc) - script->code) <
              script->length);

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
                regs.fp->setReturnValue(rval);
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
            if (tn->stackDepth > regs.sp - regs.fp->base())
                continue;

            /*
             * Set pc to the first bytecode after the the try note to point
             * to the beginning of catch or finally or to [enditer] closing
             * the for-in loop.
             */
            regs.pc = (script)->main + tn->start + tn->length;

            JSBool ok = js_UnwindScope(cx, tn->stackDepth, JS_TRUE);
            JS_ASSERT(regs.sp == regs.fp->base() + tn->stackDepth);
            if (!ok) {
                /*
                 * Restart the handler search with updated pc and stack depth
                 * to properly notify the debugger.
                 */
                goto error;
            }

            switch (tn->kind) {
              case JSTRY_CATCH:
                JS_ASSERT(js_GetOpcode(cx, regs.fp->script(), regs.pc) == JSOP_ENTERBLOCK);

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
                JS_ASSERT(js_GetOpcode(cx, regs.fp->script(), regs.pc) == JSOP_ENDITER);
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
            regs.fp->clearReturnValue();
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
    JS_ASSERT(regs.sp == regs.fp->base());

#ifdef DEBUG
    cx->tracePrevPc = NULL;
#endif

    if (entryFrame != regs.fp)
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
    JS_ASSERT(entryFrame == regs.fp);
    JS_ASSERT(cx->regs == &regs);
    *prevContextRegs = regs;
    cx->setCurrentRegs(prevContextRegs);

#ifdef JS_TRACER
    JS_ASSERT_IF(interpReturnOK && (interpFlags & JSINTERP_RECORD), !TRACE_RECORDER(cx));
    if (TRACE_RECORDER(cx))
        AbortRecording(cx, "recording out of Interpret");
#endif

    JS_ASSERT_IF(!regs.fp->isGeneratorFrame(), !regs.fp->hasBlockChain());
    JS_ASSERT_IF(!regs.fp->isGeneratorFrame(), !js_IsActiveWithOrBlock(cx, &regs.fp->scopeChain(), 0));

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

#ifdef JS_METHODJIT
  stop_recording:
#endif
    JS_ASSERT(cx->regs == &regs);
    *prevContextRegs = regs;
    cx->setCurrentRegs(prevContextRegs);
    return interpReturnOK;
}

} /* namespace js */

#endif /* !defined jsinvoke_cpp___ */
