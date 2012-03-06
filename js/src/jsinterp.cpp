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
#include "jsutil.h"
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
#include "jsgcmark.h"
#include "jsinterp.h"
#include "jsiter.h"
#include "jslock.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsopcode.h"
#include "jspropertycache.h"
#include "jsscope.h"
#include "jsscript.h"
#include "jsstr.h"
#include "jslibmath.h"

#include "frontend/BytecodeEmitter.h"
#ifdef JS_METHODJIT
#include "methodjit/MethodJIT.h"
#include "methodjit/Logging.h"
#endif
#include "vm/Debugger.h"

#include "jsatominlines.h"
#include "jsinferinlines.h"
#include "jsinterpinlines.h"
#include "jsobjinlines.h"
#include "jsopcodeinlines.h"
#include "jsprobes.h"
#include "jspropertycacheinlines.h"
#include "jsscopeinlines.h"
#include "jsscriptinlines.h"
#include "jstypedarrayinlines.h"

#include "vm/Stack-inl.h"
#include "vm/String-inl.h"

#if JS_HAS_XML_SUPPORT
#include "jsxml.h"
#endif

#include "jsautooplen.h"

#if defined(JS_METHODJIT) && defined(JS_MONOIC)
#include "methodjit/MonoIC.h"
#endif

using namespace js;
using namespace js::gc;
using namespace js::types;

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
 * This lazy cloning is implemented in GetScopeChain, which is also used in
 * some other cases --- entering 'with' blocks, for example.
 */
JSObject *
js::GetScopeChain(JSContext *cx, StackFrame *fp)
{
    StaticBlockObject *sharedBlock = fp->maybeBlockChain();

    if (!sharedBlock) {
        /*
         * Don't force a call object for a lightweight function call, but do
         * insist that there is a call object for a heavyweight function call.
         */
        JS_ASSERT_IF(fp->isNonEvalFunctionFrame() && fp->fun()->isHeavyweight(),
                     fp->hasCallObj());
        return &fp->scopeChain();
    }

    /*
     * We have one or more lexical scopes to reflect into fp->scopeChain, so
     * make sure there's a call object at the current head of the scope chain,
     * if this frame is a call frame.
     *
     * Also, identify the innermost compiler-allocated block we needn't clone.
     */
    JSObject *limitBlock, *limitClone;
    if (fp->isNonEvalFunctionFrame() && !fp->hasCallObj()) {
        JS_ASSERT_IF(fp->scopeChain().isClonedBlock(), fp->scopeChain().getPrivate() != fp);
        if (!CallObject::createForFunction(cx, fp))
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
        while (limitClone->isWith())
            limitClone = &limitClone->asWith().enclosingScope();
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
     * create() leaves the clone's enclosingScope unset. We set it below.
     */
    ClonedBlockObject *innermostNewChild = ClonedBlockObject::create(cx, *sharedBlock, fp);
    if (!innermostNewChild)
        return NULL;

    /*
     * Clone our way towards outer scopes until we reach the innermost
     * enclosing function, or the innermost block we've already cloned.
     */
    ClonedBlockObject *newChild = innermostNewChild;
    for (;;) {
        JS_ASSERT(newChild->getProto() == sharedBlock);
        sharedBlock = sharedBlock->enclosingBlock();

        /* Sometimes limitBlock will be NULL, so check that first.  */
        if (sharedBlock == limitBlock || !sharedBlock)
            break;

        /* As in the call above, we don't know the real parent yet.  */
        ClonedBlockObject *clone = ClonedBlockObject::create(cx, *sharedBlock, fp);
        if (!clone)
            return NULL;

        if (!newChild->setEnclosingScope(cx, *clone))
            return NULL;
        newChild = clone;
    }
    if (!newChild->setEnclosingScope(cx, fp->scopeChain()))
        return NULL;


    /*
     * If we found a limit block belonging to this frame, then we should have
     * found it in blockChain.
     */
    JS_ASSERT_IF(limitBlock &&
                 limitBlock->isClonedBlock() &&
                 limitClone->getPrivate() == js_FloatingFrameIfGenerator(cx, fp),
                 sharedBlock);

    /* Place our newly cloned blocks at the head of the scope chain.  */
    fp->setScopeChainNoCallObj(*innermostNewChild);
    return innermostNewChild;
}

JSObject *
js::GetScopeChain(JSContext *cx)
{
    /*
     * Note: we don't need to expand inline frames here, because frames are
     * only inlined when the caller and callee share the same scope chain.
     */
    StackFrame *fp = js_GetTopStackFrame(cx, FRAME_EXPAND_NONE);
    if (!fp) {
        /*
         * There is no code active on this context. In place of an actual
         * scope chain, use the context's global object, which is set in
         * js_InitFunctionAndObjectClasses, and which represents the default
         * scope chain for the embedding. See also js_FindClassObject.
         *
         * For embeddings that use the inner and outer object hooks, the inner
         * object represents the ultimate global object, with the outer object
         * acting as a stand-in.
         */
        JSObject *obj = cx->globalObject;
        if (!obj) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_INACTIVE);
            return NULL;
        }

        OBJ_TO_INNER_OBJECT(cx, obj);
        return obj;
    }
    return GetScopeChain(cx, fp);
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
bool
js::BoxNonStrictThis(JSContext *cx, const CallReceiver &call)
{
    /*
     * Check for SynthesizeFrame poisoning and fast constructors which
     * didn't check their callee properly.
     */
    Value &thisv = call.thisv();
    JS_ASSERT(!thisv.isMagic());

#ifdef DEBUG
    JSFunction *fun = call.callee().isFunction() ? call.callee().toFunction() : NULL;
    JS_ASSERT_IF(fun && fun->isInterpreted(), !fun->inStrictMode());
#endif

    if (thisv.isNullOrUndefined()) {
        JSObject *thisp = call.callee().global().thisObject(cx);
        if (!thisp)
            return false;
        call.thisv().setObject(*thisp);
        return true;
    }

    if (!thisv.isObject())
        return !!js_PrimitiveToObject(cx, &thisv);

    return true;
}

#if JS_HAS_NO_SUCH_METHOD

const uint32_t JSSLOT_FOUND_FUNCTION  = 0;
const uint32_t JSSLOT_SAVED_ID        = 1;

Class js_NoSuchMethodClass = {
    "NoSuchMethod",
    JSCLASS_HAS_RESERVED_SLOTS(2) | JSCLASS_IS_ANONYMOUS,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub,
};

/*
 * When JSOP_CALLPROP or JSOP_CALLELEM does not find the method property of
 * the base object, we search for the __noSuchMethod__ method in the base.
 * If it exists, we store the method and the property's id into an object of
 * NoSuchMethod class and store this object into the callee's stack slot.
 * Later, Invoke will recognise such an object and transfer control to
 * NoSuchMethod that invokes the method like:
 *
 *   this.__noSuchMethod__(id, args)
 *
 * where id is the name of the method that this invocation attempted to
 * call by name, and args is an Array containing this invocation's actual
 * parameters.
 */
bool
js::OnUnknownMethod(JSContext *cx, JSObject *obj, Value idval, Value *vp)
{
    jsid id = ATOM_TO_JSID(cx->runtime->atomState.noSuchMethodAtom);
    AutoValueRooter tvr(cx);
    if (!js_GetMethod(cx, obj, id, JSGET_NO_METHOD_BARRIER, tvr.addr()))
        return false;
    TypeScript::MonitorUnknown(cx, cx->fp()->script(), cx->regs().pc);

    if (tvr.value().isPrimitive()) {
        *vp = tvr.value();
    } else {
#if JS_HAS_XML_SUPPORT
        /* Extract the function name from function::name qname. */
        if (idval.isObject()) {
            obj = &idval.toObject();
            if (js_GetLocalNameFromFunctionQName(obj, &id, cx))
                idval = IdToValue(id);
        }
#endif

        obj = NewObjectWithClassProto(cx, &js_NoSuchMethodClass, NULL, NULL);
        if (!obj)
            return false;

        obj->setSlot(JSSLOT_FOUND_FUNCTION, tvr.value());
        obj->setSlot(JSSLOT_SAVED_ID, idval);
        vp->setObject(*obj);
    }
    return true;
}

static JSBool
NoSuchMethod(JSContext *cx, unsigned argc, Value *vp)
{
    InvokeArgsGuard args;
    if (!cx->stack.pushInvokeArgs(cx, 2, &args))
        return JS_FALSE;

    JS_ASSERT(vp[0].isObject());
    JS_ASSERT(vp[1].isObject());
    JSObject *obj = &vp[0].toObject();
    JS_ASSERT(obj->getClass() == &js_NoSuchMethodClass);

    args.calleev() = obj->getSlot(JSSLOT_FOUND_FUNCTION);
    args.thisv() = vp[1];
    args[0] = obj->getSlot(JSSLOT_SAVED_ID);
    JSObject *argsobj = NewDenseCopiedArray(cx, argc, vp + 2);
    if (!argsobj)
        return JS_FALSE;
    args[1].setObject(*argsobj);
    JSBool ok = Invoke(cx, args);
    vp[0] = args.rval();
    return ok;
}

#endif /* JS_HAS_NO_SUCH_METHOD */

bool
js::RunScript(JSContext *cx, JSScript *script, StackFrame *fp)
{
    JS_ASSERT(script);
    JS_ASSERT(fp == cx->fp());
    JS_ASSERT(fp->script() == script);
#ifdef JS_METHODJIT_SPEW
    JMCheckLogging();
#endif

    /* FIXME: Once bug 470510 is fixed, make this an assert. */
    if (script->compileAndGo) {
        if (fp->scopeChain().global().isCleared()) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CLEARED_SCOPE);
            return false;
        }
    }

#ifdef DEBUG
    struct CheckStackBalance {
        JSContext *cx;
        StackFrame *fp;
        JSObject *enumerators;
        CheckStackBalance(JSContext *cx)
          : cx(cx), fp(cx->fp()), enumerators(cx->enumerators)
        {}
        ~CheckStackBalance() {
            JS_ASSERT(fp == cx->fp());
            JS_ASSERT_IF(!fp->isGeneratorFrame(), enumerators == cx->enumerators);
        }
    } check(cx);
#endif

#ifdef JS_METHODJIT
    mjit::CompileStatus status;
    status = mjit::CanMethodJIT(cx, script, script->code, fp->isConstructing(),
                                mjit::CompileRequest_Interpreter);
    if (status == mjit::Compile_Error)
        return false;

    if (status == mjit::Compile_Okay)
        return mjit::JaegerShot(cx, false);
#endif

    return Interpret(cx, fp);
}

/*
 * Find a function reference and its 'this' value implicit first parameter
 * under argc arguments on cx's stack, and call the function.  Push missing
 * required arguments, allocate declared local variables, and pop everything
 * when done.  Then push the return value.
 */
bool
js::InvokeKernel(JSContext *cx, CallArgs args, MaybeConstruct construct)
{
    JS_ASSERT(args.length() <= StackSpace::ARGS_LENGTH_MAX);

    JS_ASSERT(!cx->compartment->activeAnalysis);

    /* MaybeConstruct is a subset of InitialFrameFlags */
    InitialFrameFlags initial = (InitialFrameFlags) construct;

    if (args.calleev().isPrimitive()) {
        js_ReportIsNotFunction(cx, &args.calleev(), ToReportFlags(initial));
        return false;
    }

    JSObject &callee = args.callee();
    Class *clasp = callee.getClass();

    /* Invoke non-functions. */
    if (JS_UNLIKELY(clasp != &FunctionClass)) {
#if JS_HAS_NO_SUCH_METHOD
        if (JS_UNLIKELY(clasp == &js_NoSuchMethodClass))
            return NoSuchMethod(cx, args.length(), args.base());
#endif
        JS_ASSERT_IF(construct, !clasp->construct);
        if (!clasp->call) {
            js_ReportIsNotFunction(cx, &args.calleev(), ToReportFlags(initial));
            return false;
        }
        return CallJSNative(cx, clasp->call, args);
    }

    /* Invoke native functions. */
    JSFunction *fun = callee.toFunction();
    JS_ASSERT_IF(construct, !fun->isNativeConstructor());
    if (fun->isNative())
        return CallJSNative(cx, fun->u.n.native, args);

    TypeMonitorCall(cx, args, construct);

    /* Get pointer to new frame/slots, prepare arguments. */
    InvokeFrameGuard ifg;
    if (!cx->stack.pushInvokeFrame(cx, args, initial, &ifg))
        return false;

    /* Now that the new frame is rooted, maybe create a call object. */
    StackFrame *fp = ifg.fp();
    if (!fp->functionPrologue(cx))
        return false;

    /* Run function until JSOP_STOP, JSOP_RETURN or error. */
    JSBool ok = RunScript(cx, fun->script(), fp);

    /* Propagate the return value out. */
    args.rval() = fp->returnValue();
    JS_ASSERT_IF(ok && construct, !args.rval().isPrimitive());
    return ok;
}

bool
js::Invoke(JSContext *cx, const Value &thisv, const Value &fval, unsigned argc, Value *argv,
           Value *rval)
{
    InvokeArgsGuard args;
    if (!cx->stack.pushInvokeArgs(cx, argc, &args))
        return false;

    args.calleev() = fval;
    args.thisv() = thisv;
    PodCopy(args.array(), argv, argc);

    if (args.thisv().isObject()) {
        /*
         * We must call the thisObject hook in case we are not called from the
         * interpreter, where a prior bytecode has computed an appropriate
         * |this| already.
         */
        JSObject *thisp = args.thisv().toObject().thisObject(cx);
        if (!thisp)
             return false;
        args.thisv().setObject(*thisp);
    }

    if (!Invoke(cx, args))
        return false;

    *rval = args.rval();
    return true;
}

bool
js::InvokeConstructorKernel(JSContext *cx, const CallArgs &argsRef)
{
    JS_ASSERT(!FunctionClass.construct);
    CallArgs args = argsRef;

    args.thisv().setMagic(JS_IS_CONSTRUCTING);

    if (args.calleev().isObject()) {
        JSObject *callee = &args.callee();
        Class *clasp = callee->getClass();
        if (clasp == &FunctionClass) {
            JSFunction *fun = callee->toFunction();

            if (fun->isNativeConstructor()) {
                Probes::calloutBegin(cx, fun);
                bool ok = CallJSNativeConstructor(cx, fun->u.n.native, args);
                Probes::calloutEnd(cx, fun);
                return ok;
            }

            if (!fun->isInterpretedConstructor())
                goto error;

            if (!InvokeKernel(cx, args, CONSTRUCT))
                return false;

            JS_ASSERT(args.rval().isObject());
            return true;
        }
        if (clasp->construct)
            return CallJSNativeConstructor(cx, clasp->construct, args);
    }

error:
    js_ReportIsNotFunction(cx, &args.calleev(), JSV2F_CONSTRUCT);
    return false;
}

bool
js::InvokeConstructor(JSContext *cx, const Value &fval, unsigned argc, Value *argv, Value *rval)
{
    InvokeArgsGuard args;
    if (!cx->stack.pushInvokeArgs(cx, argc, &args))
        return false;

    args.calleev() = fval;
    args.thisv().setMagic(JS_THIS_POISON);
    PodCopy(args.array(), argv, argc);

    if (!InvokeConstructor(cx, args))
        return false;

    *rval = args.rval();
    return true;
}

bool
js::InvokeGetterOrSetter(JSContext *cx, JSObject *obj, const Value &fval, unsigned argc, Value *argv,
                         Value *rval)
{
    /*
     * Invoke could result in another try to get or set the same id again, see
     * bug 355497.
     */
    JS_CHECK_RECURSION(cx, return false);

    return Invoke(cx, ObjectValue(*obj), fval, argc, argv, rval);
}

bool
js::ExecuteKernel(JSContext *cx, JSScript *script, JSObject &scopeChain, const Value &thisv,
                  ExecuteType type, StackFrame *evalInFrame, Value *result)
{
    JS_ASSERT_IF(evalInFrame, type == EXECUTE_DEBUG);

    Root<JSScript*> scriptRoot(cx, &script);

    if (script->isEmpty()) {
        if (result)
            result->setUndefined();
        return true;
    }

    ExecuteFrameGuard efg;
    if (!cx->stack.pushExecuteFrame(cx, script, thisv, scopeChain, type, evalInFrame, &efg))
        return false;

    if (!script->ensureRanAnalysis(cx, &scopeChain))
        return false;

    /* Give strict mode eval its own fresh lexical environment. */
    StackFrame *fp = efg.fp();
    if (fp->isStrictEvalFrame() && !CallObject::createForStrictEval(cx, fp))
        return false;

    Probes::startExecution(cx, script);

    TypeScript::SetThis(cx, script, fp->thisValue());

    bool ok = RunScript(cx, script, fp);

    if (fp->isStrictEvalFrame())
        js_PutCallObject(fp);

    Probes::stopExecution(cx, script);

    /* Propgate the return value out. */
    if (result)
        *result = fp->returnValue();
    return ok;
}

bool
js::Execute(JSContext *cx, JSScript *script, JSObject &scopeChainArg, Value *rval)
{
    /* The scope chain could be anything, so innerize just in case. */
    JSObject *scopeChain = &scopeChainArg;
    OBJ_TO_INNER_OBJECT(cx, scopeChain);
    if (!scopeChain)
        return false;

    /* If we were handed a non-native object, complain bitterly. */
    if (!scopeChain->isNative()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NON_NATIVE_SCOPE);
        return false;
    }
    JS_ASSERT(!scopeChain->getOps()->defineProperty);

    /* The VAROBJFIX option makes varObj == globalObj in global code. */
    if (!cx->hasRunOption(JSOPTION_VAROBJFIX)) {
        if (!scopeChain->setVarObj(cx))
            return false;
    }

    /* Use the scope chain as 'this', modulo outerization. */
    JSObject *thisObj = scopeChain->thisObject(cx);
    if (!thisObj)
        return false;
    Value thisv = ObjectValue(*thisObj);

    return ExecuteKernel(cx, script, *scopeChain, thisv, EXECUTE_GLOBAL,
                         NULL /* evalInFrame */, rval);
}

JSBool
js::HasInstance(JSContext *cx, JSObject *obj, const Value *v, JSBool *bp)
{
    Class *clasp = obj->getClass();
    if (clasp->hasInstance)
        return clasp->hasInstance(cx, obj, v, bp);
    js_ReportValueError(cx, JSMSG_BAD_INSTANCEOF_RHS,
                        JSDVG_SEARCH_STACK, ObjectValue(*obj), NULL);
    return JS_FALSE;
}

bool
js::LooselyEqual(JSContext *cx, const Value &lval, const Value &rval, bool *result)
{
#if JS_HAS_XML_SUPPORT
    if (JS_UNLIKELY(lval.isObject() && lval.toObject().isXML()) ||
                    (rval.isObject() && rval.toObject().isXML())) {
        JSBool res;
        if (!js_TestXMLEquality(cx, lval, rval, &res))
            return false;
        *result = !!res;
        return true;
    }
#endif

    if (SameType(lval, rval)) {
        if (lval.isString()) {
            JSString *l = lval.toString();
            JSString *r = rval.toString();
            return EqualStrings(cx, l, r, result);
        }

        if (lval.isDouble()) {
            double l = lval.toDouble(), r = rval.toDouble();
            *result = (l == r);
            return true;
        }

        if (lval.isObject()) {
            JSObject *l = &lval.toObject();
            JSObject *r = &rval.toObject();

            if (JSEqualityOp eq = l->getClass()->ext.equality) {
                JSBool res;
                if (!eq(cx, l, &rval, &res))
                    return false;
                *result = !!res;
                return true;
            }

            *result = l == r;
            return true;
        }

        *result = lval.payloadAsRawUint32() == rval.payloadAsRawUint32();
        return true;
    }

    if (lval.isNullOrUndefined()) {
        *result = rval.isNullOrUndefined();
        return true;
    }

    if (rval.isNullOrUndefined()) {
        *result = false;
        return true;
    }

    Value lvalue = lval;
    Value rvalue = rval;

    if (!ToPrimitive(cx, &lvalue))
        return false;
    if (!ToPrimitive(cx, &rvalue))
        return false;

    if (lvalue.isString() && rvalue.isString()) {
        JSString *l = lvalue.toString();
        JSString *r = rvalue.toString();
        return EqualStrings(cx, l, r, result);
    }

    double l, r;
    if (!ToNumber(cx, lvalue, &l) || !ToNumber(cx, rvalue, &r))
        return false;
    *result = (l == r);
    return true;
}

bool
js::StrictlyEqual(JSContext *cx, const Value &lref, const Value &rref, bool *equal)
{
    Value lval = lref, rval = rref;
    if (SameType(lval, rval)) {
        if (lval.isString())
            return EqualStrings(cx, lval.toString(), rval.toString(), equal);
        if (lval.isDouble()) {
            *equal = (lval.toDouble() == rval.toDouble());
            return true;
        }
        if (lval.isObject()) {
            *equal = lval.toObject() == rval.toObject();
            return true;
        }
        if (lval.isUndefined()) {
            *equal = true;
            return true;
        }
        *equal = lval.payloadAsRawUint32() == rval.payloadAsRawUint32();
        return true;
    }

    if (lval.isDouble() && rval.isInt32()) {
        double ld = lval.toDouble();
        double rd = rval.toInt32();
        *equal = (ld == rd);
        return true;
    }
    if (lval.isInt32() && rval.isDouble()) {
        double ld = lval.toInt32();
        double rd = rval.toDouble();
        *equal = (ld == rd);
        return true;
    }

    *equal = false;
    return true;
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
js::SameValue(JSContext *cx, const Value &v1, const Value &v2, bool *same)
{
    if (IsNegativeZero(v1)) {
        *same = IsNegativeZero(v2);
        return true;
    }
    if (IsNegativeZero(v2)) {
        *same = false;
        return true;
    }
    if (IsNaN(v1) && IsNaN(v2)) {
        *same = true;
        return true;
    }
    return StrictlyEqual(cx, v1, v2, same);
}

JSType
js::TypeOfValue(JSContext *cx, const Value &vref)
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
js::ValueToId(JSContext *cx, const Value &v, jsid *idp)
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

/*
 * Enter the new with scope using an object at sp[-1] and associate the depth
 * of the with block with sp + stackIndex.
 */
static bool
EnterWith(JSContext *cx, int stackIndex)
{
    StackFrame *fp = cx->fp();
    Value *sp = cx->regs().sp;
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

    JSObject *parent = GetScopeChain(cx, fp);
    if (!parent)
        return JS_FALSE;

    JSObject *withobj = WithObject::create(cx, fp, *obj, *parent,
                                           sp + stackIndex - fp->base());
    if (!withobj)
        return JS_FALSE;

    fp->setScopeChainNoCallObj(*withobj);
    return JS_TRUE;
}

static void
LeaveWith(JSContext *cx)
{
    WithObject &withobj = cx->fp()->scopeChain().asWith();
    JS_ASSERT(withobj.maybeStackFrame() == js_FloatingFrameIfGenerator(cx, cx->fp()));
    JS_ASSERT(withobj.stackDepth() >= 0);
    withobj.setStackFrame(NULL);
    cx->fp()->setScopeChainNoCallObj(withobj.enclosingScope());
}

bool
js::IsActiveWithOrBlock(JSContext *cx, JSObject &obj, uint32_t stackDepth)
{
    return (obj.isWith() || obj.isBlock()) &&
           obj.getPrivate() == js_FloatingFrameIfGenerator(cx, cx->fp()) &&
           obj.asNestedScope().stackDepth() >= stackDepth;
}

/* Unwind block and scope chains to match the given depth. */
void
js::UnwindScope(JSContext *cx, uint32_t stackDepth)
{
    JS_ASSERT(cx->fp()->base() + stackDepth <= cx->regs().sp);

    StackFrame *fp = cx->fp();
    StaticBlockObject *block = fp->maybeBlockChain();
    while (block) {
        if (block->stackDepth() < stackDepth)
            break;
        block = block->enclosingBlock();
    }
    fp->setBlockChain(block);

    for (;;) {
        JSObject &scopeChain = fp->scopeChain();
        if (!IsActiveWithOrBlock(cx, scopeChain, stackDepth))
            break;
        if (scopeChain.isClonedBlock())
            scopeChain.asClonedBlock().put(cx);
        else
            LeaveWith(cx);
    }
}

/*
 * Find the results of incrementing or decrementing *vp. For pre-increments,
 * both *vp and *vp2 will contain the result on return. For post-increments,
 * vp will contain the original value converted to a number and vp2 will get
 * the result. Both vp and vp2 must be roots.
 */
static bool
DoIncDec(JSContext *cx, const JSCodeSpec *cs, Value *vp, Value *vp2)
{
    if (cs->format & JOF_POST) {
        double d;
        if (!ToNumber(cx, *vp, &d))
            return JS_FALSE;
        vp->setNumber(d);
        (cs->format & JOF_INC) ? ++d : --d;
        vp2->setNumber(d);
        return JS_TRUE;
    }

    double d;
    if (!ToNumber(cx, *vp, &d))
        return JS_FALSE;
    (cs->format & JOF_INC) ? ++d : --d;
    vp->setNumber(d);
    *vp2 = *vp;
    return JS_TRUE;
}

const Value &
js::GetUpvar(JSContext *cx, unsigned closureLevel, UpvarCookie cookie)
{
    JS_ASSERT(closureLevel >= cookie.level() && cookie.level() > 0);
    const unsigned targetLevel = closureLevel - cookie.level();

    StackFrame *fp = FindUpvarFrame(cx, targetLevel);
    unsigned slot = cookie.slot();
    const Value *vp;

    if (!fp->isFunctionFrame() || fp->isEvalFrame()) {
        vp = fp->slots() + fp->numFixed();
    } else if (slot < fp->numFormalArgs()) {
        vp = fp->formalArgs();
    } else if (slot == UpvarCookie::CALLEE_SLOT) {
        vp = &fp->calleev();
        slot = 0;
    } else {
        slot -= fp->numFormalArgs();
        JS_ASSERT(slot < fp->numSlots());
        vp = fp->slots();
    }

    return vp[slot];
}

extern StackFrame *
js::FindUpvarFrame(JSContext *cx, unsigned targetLevel)
{
    StackFrame *fp = cx->fp();
    while (true) {
        JS_ASSERT(fp && fp->isScriptFrame());
        if (fp->script()->staticLevel == targetLevel)
            break;
        fp = fp->prev();
    }
    return fp;
}

#define PUSH_COPY(v)             do { *regs.sp++ = v; assertSameCompartment(cx, regs.sp[-1]); } while (0)
#define PUSH_COPY_SKIP_CHECK(v)  *regs.sp++ = v
#define PUSH_NULL()              regs.sp++->setNull()
#define PUSH_UNDEFINED()         regs.sp++->setUndefined()
#define PUSH_BOOLEAN(b)          regs.sp++->setBoolean(b)
#define PUSH_DOUBLE(d)           regs.sp++->setDouble(d)
#define PUSH_INT32(i)            regs.sp++->setInt32(i)
#define PUSH_STRING(s)           do { regs.sp++->setString(s); assertSameCompartment(cx, regs.sp[-1]); } while (0)
#define PUSH_OBJECT(obj)         do { regs.sp++->setObject(obj); assertSameCompartment(cx, regs.sp[-1]); } while (0)
#define PUSH_OBJECT_OR_NULL(obj) do { regs.sp++->setObjectOrNull(obj); assertSameCompartment(cx, regs.sp[-1]); } while (0)
#define PUSH_HOLE()              regs.sp++->setMagic(JS_ARRAY_HOLE)
#define POP_COPY_TO(v)           v = *--regs.sp
#define POP_RETURN_VALUE()       regs.fp()->setReturnValue(*--regs.sp)

#define VALUE_TO_BOOLEAN(cx, vp, b)                                           \
    JS_BEGIN_MACRO                                                            \
        vp = &regs.sp[-1];                                                    \
        if (vp->isNull()) {                                                   \
            b = false;                                                        \
        } else if (vp->isBoolean()) {                                         \
            b = vp->toBoolean();                                              \
        } else {                                                              \
            b = !!js_ValueToBoolean(*vp);                                     \
        }                                                                     \
    JS_END_MACRO

#define POP_BOOLEAN(cx, vp, b)   do { VALUE_TO_BOOLEAN(cx, vp, b); regs.sp--; } while(0)

#define FETCH_OBJECT(cx, n, obj)                                              \
    JS_BEGIN_MACRO                                                            \
        Value *vp_ = &regs.sp[n];                                             \
        obj = ToObject(cx, (vp_));                                            \
        if (!obj)                                                             \
            goto error;                                                       \
    JS_END_MACRO

/* Test whether v is an int in the range [-2^31 + 1, 2^31 - 2] */
static JS_ALWAYS_INLINE bool
CanIncDecWithoutOverflow(int32_t i)
{
    return (i > JSVAL_INT_MIN) && (i < JSVAL_INT_MAX);
}

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

template<typename T>
class GenericInterruptEnabler : public InterpreterFrames::InterruptEnablerBase {
  public:
    GenericInterruptEnabler(T *variable, T value) : variable(variable), value(value) { }
    void enableInterrupts() const { *variable = value; }

  private:
    T *variable;
    T value;
};

inline InterpreterFrames::InterpreterFrames(JSContext *cx, FrameRegs *regs, 
                                            const InterruptEnablerBase &enabler)
  : context(cx), regs(regs), enabler(enabler)
{
    older = cx->runtime->interpreterFrames;
    cx->runtime->interpreterFrames = this;
}
 
inline InterpreterFrames::~InterpreterFrames()
{
    context->runtime->interpreterFrames = older;
}

#if defined(DEBUG) && !defined(JS_THREADSAFE)
void
js::AssertValidPropertyCacheHit(JSContext *cx,
                                JSObject *start, JSObject *found,
                                PropertyCacheEntry *entry)
{
    jsbytecode *pc;
    cx->stack.currentScript(&pc);

    uint64_t sample = cx->runtime->gcNumber;
    PropertyCacheEntry savedEntry = *entry;

    PropertyName *name = GetNameFromBytecode(cx, pc, JSOp(*pc), js_CodeSpec[*pc]);

    JSObject *obj, *pobj;
    JSProperty *prop;
    JSBool ok;

    if (JOF_OPMODE(*pc) == JOF_NAME)
        ok = FindProperty(cx, name, start, &obj, &pobj, &prop);
    else
        ok = LookupProperty(cx, start, name, &pobj, &prop);
    JS_ASSERT(ok);

    if (cx->runtime->gcNumber != sample)
        JS_PROPERTY_CACHE(cx).restore(&savedEntry);
    JS_ASSERT(prop);
    JS_ASSERT(pobj == found);

    const Shape *shape = (Shape *) prop;
    JS_ASSERT(entry->prop == shape);
}
#endif /* DEBUG && !JS_THREADSAFE */

/*
 * Ensure that the intrepreter switch can close call-bytecode cases in the
 * same way as non-call bytecodes.
 */
JS_STATIC_ASSERT(JSOP_NAME_LENGTH == JSOP_CALLNAME_LENGTH);
JS_STATIC_ASSERT(JSOP_GETFCSLOT_LENGTH == JSOP_CALLFCSLOT_LENGTH);
JS_STATIC_ASSERT(JSOP_GETARG_LENGTH == JSOP_CALLARG_LENGTH);
JS_STATIC_ASSERT(JSOP_GETLOCAL_LENGTH == JSOP_CALLLOCAL_LENGTH);
JS_STATIC_ASSERT(JSOP_XMLNAME_LENGTH == JSOP_CALLXMLNAME_LENGTH);

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

/*
 * Inline fast paths for iteration. js_IteratorMore and js_IteratorNext handle
 * all cases, but we inline the most frequently taken paths here.
 */
static inline bool
IteratorMore(JSContext *cx, JSObject *iterobj, bool *cond, Value *rval)
{
    if (iterobj->isIterator()) {
        NativeIterator *ni = iterobj->getNativeIterator();
        if (ni->isKeyIter()) {
            *cond = (ni->props_cursor < ni->props_end);
            return true;
        }
    }
    if (!js_IteratorMore(cx, iterobj, rval))
        return false;
    *cond = rval->isTrue();
    return true;
}

static inline bool
IteratorNext(JSContext *cx, JSObject *iterobj, Value *rval)
{
    if (iterobj->isIterator()) {
        NativeIterator *ni = iterobj->getNativeIterator();
        if (ni->isKeyIter()) {
            JS_ASSERT(ni->props_cursor < ni->props_end);
            rval->setString(*ni->current());
            ni->incCursor();
            return true;
        }
    }
    return js_IteratorNext(cx, iterobj, rval);
}

/*
 * For bytecodes which push values and then fall through, make sure the
 * types of the pushed values are consistent with type inference information.
 */
static inline void
TypeCheckNextBytecode(JSContext *cx, JSScript *script, unsigned n, const FrameRegs &regs)
{
#ifdef DEBUG
    if (cx->typeInferenceEnabled() &&
        n == GetBytecodeLength(regs.pc)) {
        TypeScript::CheckBytecode(cx, script, regs.pc, regs.sp);
    }
#endif
}

JS_NEVER_INLINE bool
js::Interpret(JSContext *cx, StackFrame *entryFrame, InterpMode interpMode)
{
    JSAutoResolveFlags rf(cx, RESOLVE_INFER);

    gc::MaybeVerifyBarriers(cx, true);

    JS_ASSERT(!cx->compartment->activeAnalysis);

#if JS_THREADED_INTERP
#define CHECK_PCCOUNT_INTERRUPTS() JS_ASSERT_IF(script->pcCounters, jumpTable == interruptJumpTable)
#else
#define CHECK_PCCOUNT_INTERRUPTS() JS_ASSERT_IF(script->pcCounters, switchMask == -1)
#endif

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

    typedef GenericInterruptEnabler<void * const *> InterruptEnabler;
    InterruptEnabler interruptEnabler(&jumpTable, interruptJumpTable);

# define DO_OP()            JS_BEGIN_MACRO                                    \
                                CHECK_PCCOUNT_INTERRUPTS();                   \
                                js::gc::MaybeVerifyBarriers(cx);              \
                                JS_EXTENSION_(goto *jumpTable[op]);           \
                            JS_END_MACRO
# define DO_NEXT_OP(n)      JS_BEGIN_MACRO                                    \
                                TypeCheckNextBytecode(cx, script, n, regs);   \
                                op = (JSOp) *(regs.pc += (n));                \
                                DO_OP();                                      \
                            JS_END_MACRO

# define BEGIN_CASE(OP)     L_##OP:
# define END_CASE(OP)       DO_NEXT_OP(OP##_LENGTH);
# define END_VARLEN_CASE    DO_NEXT_OP(len);
# define ADD_EMPTY_CASE(OP) BEGIN_CASE(OP)                                    \
                                JS_ASSERT(js_CodeSpec[OP].length == 1);       \
                                op = (JSOp) *++regs.pc;                       \
                                DO_OP();

# define END_EMPTY_CASES

#else /* !JS_THREADED_INTERP */

    register int switchMask = 0;
    int switchOp;
    typedef GenericInterruptEnabler<int> InterruptEnabler;
    InterruptEnabler interruptEnabler(&switchMask, -1);

# define DO_OP()            goto do_op
# define DO_NEXT_OP(n)      JS_BEGIN_MACRO                                    \
                                JS_ASSERT((n) == len);                        \
                                goto advance_pc;                              \
                            JS_END_MACRO

# define BEGIN_CASE(OP)     case OP:
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
# define END_CASE_LEN6      len = 6; goto advance_pc;
# define END_CASE_LEN7      len = 7; goto advance_pc;
# define END_VARLEN_CASE    goto advance_pc;
# define ADD_EMPTY_CASE(OP) BEGIN_CASE(OP)
# define END_EMPTY_CASES    goto advance_pc_by_one;

#endif /* !JS_THREADED_INTERP */

#define ENABLE_INTERRUPTS() (interruptEnabler.enableInterrupts())

#define LOAD_ATOM(PCOFF, atom)                                                \
    JS_BEGIN_MACRO                                                            \
        JS_ASSERT((size_t)(atoms - script->atoms) <                           \
                  (size_t)(script->natoms - GET_UINT32_INDEX(regs.pc + PCOFF)));\
        atom = atoms[GET_UINT32_INDEX(regs.pc + PCOFF)];                      \
    JS_END_MACRO

#define LOAD_NAME(PCOFF, name)                                                \
    JS_BEGIN_MACRO                                                            \
        JSAtom *atom;                                                         \
        LOAD_ATOM((PCOFF), atom);                                             \
        name = atom->asPropertyName();                                        \
    JS_END_MACRO

#define LOAD_DOUBLE(PCOFF, dbl)                                               \
    (dbl = script->getConst(GET_UINT32_INDEX(regs.pc + (PCOFF))).toDouble())

#if defined(JS_METHODJIT)
    bool useMethodJIT = false;
#endif

#ifdef JS_METHODJIT

#define RESET_USE_METHODJIT()                                                 \
    JS_BEGIN_MACRO                                                            \
        useMethodJIT = cx->methodJitEnabled &&                                \
           (interpMode == JSINTERP_NORMAL ||                                  \
            interpMode == JSINTERP_REJOIN ||                                  \
            interpMode == JSINTERP_SKIP_TRAP);                                \
    JS_END_MACRO

#define CHECK_PARTIAL_METHODJIT(status)                                       \
    JS_BEGIN_MACRO                                                            \
        switch (status) {                                                     \
          case mjit::Jaeger_UnfinishedAtTrap:                                 \
            interpMode = JSINTERP_SKIP_TRAP;                                  \
            /* FALLTHROUGH */                                                 \
          case mjit::Jaeger_Unfinished:                                       \
            op = (JSOp) *regs.pc;                                             \
            RESTORE_INTERP_VARS_CHECK_EXCEPTION();                            \
            DO_OP();                                                          \
          default:;                                                           \
        }                                                                     \
    JS_END_MACRO

#else

#define RESET_USE_METHODJIT() ((void) 0)

#endif

#define RESTORE_INTERP_VARS()                                                 \
    JS_BEGIN_MACRO                                                            \
        SET_SCRIPT(regs.fp()->script());                                      \
        argv = regs.fp()->maybeFormalArgs();                                  \
        atoms = FrameAtomBase(cx, regs.fp());                                 \
        JS_ASSERT(&cx->regs() == &regs);                                      \
    JS_END_MACRO

#define RESTORE_INTERP_VARS_CHECK_EXCEPTION()                                 \
    JS_BEGIN_MACRO                                                            \
        RESTORE_INTERP_VARS();                                                \
        if (cx->isExceptionPending())                                         \
            goto error;                                                       \
        CHECK_INTERRUPT_HANDLER();                                            \
    JS_END_MACRO

    /*
     * Prepare to call a user-supplied branch handler, and abort the script
     * if it returns false.
     */
#define CHECK_BRANCH()                                                        \
    JS_BEGIN_MACRO                                                            \
        if (cx->runtime->interrupt && !js_HandleExecutionInterrupt(cx))       \
            goto error;                                                       \
    JS_END_MACRO

#define BRANCH(n)                                                             \
    JS_BEGIN_MACRO                                                            \
        regs.pc += (n);                                                       \
        op = (JSOp) *regs.pc;                                                 \
        if ((n) <= 0)                                                         \
            goto check_backedge;                                              \
        DO_OP();                                                              \
    JS_END_MACRO

#define SET_SCRIPT(s)                                                         \
    JS_BEGIN_MACRO                                                            \
        script = (s);                                                         \
        if (script->hasAnyBreakpointsOrStepMode())                            \
            ENABLE_INTERRUPTS();                                              \
        if (script->pcCounters)                                               \
            ENABLE_INTERRUPTS();                                              \
        JS_ASSERT_IF(interpMode == JSINTERP_SKIP_TRAP,                        \
                     script->hasAnyBreakpointsOrStepMode());                  \
    JS_END_MACRO

#define CHECK_INTERRUPT_HANDLER()                                             \
    JS_BEGIN_MACRO                                                            \
        if (cx->runtime->debugHooks.interruptHook)                            \
            ENABLE_INTERRUPTS();                                              \
    JS_END_MACRO

    /* Repoint cx->regs to a local variable for faster access. */
    FrameRegs regs = cx->regs();
    PreserveRegsGuard interpGuard(cx, regs);

    /*
     * Help Debugger find frames running scripts that it has put in
     * single-step mode.
     */
    InterpreterFrames interpreterFrame(cx, &regs, interruptEnabler);

    /* Copy in hot values that change infrequently. */
    JSRuntime *const rt = cx->runtime;
    JSScript *script;
    SET_SCRIPT(regs.fp()->script());
    Value *argv = regs.fp()->maybeFormalArgs();
    CHECK_INTERRUPT_HANDLER();

    if (rt->profilingScripts)
        ENABLE_INTERRUPTS();

    if (!entryFrame)
        entryFrame = regs.fp();

    /*
     * Initialize the index segment register used by LOAD_ATOM and
     * GET_FULL_INDEX macros below. As a register we use a pointer based on
     * the atom map to turn frequently executed LOAD_ATOM into simple array
     * access. For less frequent object loads we have to recover the segment
     * from atoms pointer first.
     */
    JSAtom **atoms = script->atoms;

#if JS_HAS_GENERATORS
    if (JS_UNLIKELY(regs.fp()->isGeneratorFrame())) {
        JS_ASSERT((size_t) (regs.pc - script->code) <= script->length);
        JS_ASSERT((size_t) (regs.sp - regs.fp()->base()) <= StackDepth(script));

        /*
         * To support generator_throw and to catch ignored exceptions,
         * fail if cx->isExceptionPending() is true.
         */
        if (cx->isExceptionPending())
            goto error;
    }
#endif

    /* State communicated between non-local jumps: */
    bool interpReturnOK;

    /* Don't call the script prologue if executing between Method and Trace JIT. */
    if (interpMode == JSINTERP_NORMAL) {
        StackFrame *fp = regs.fp();
        JS_ASSERT_IF(!fp->isGeneratorFrame(), regs.pc == script->code);
        if (!ScriptPrologueOrGeneratorResume(cx, fp, UseNewTypeAtEntry(cx, fp)))
            goto error;
        if (cx->compartment->debugMode()) {
            JSTrapStatus status = ScriptDebugPrologue(cx, fp);
            switch (status) {
              case JSTRAP_CONTINUE:
                break;
              case JSTRAP_RETURN:
                interpReturnOK = true;
                goto forced_return;
              case JSTRAP_THROW:
              case JSTRAP_ERROR:
                goto error;
              default:
                JS_NOT_REACHED("bad ScriptDebugPrologue status");
            }
        }
    }

    /* The REJOIN mode acts like the normal mode, except the prologue is skipped. */
    if (interpMode == JSINTERP_REJOIN)
        interpMode = JSINTERP_NORMAL;

    CHECK_INTERRUPT_HANDLER();

    RESET_USE_METHODJIT();

    /*
     * It is important that "op" be initialized before calling DO_OP because
     * it is possible for "op" to be specially assigned during the normal
     * processing of an opcode while looping. We rely on DO_NEXT_OP to manage
     * "op" correctly in all other cases.
     */
    JSOp op;
    int32_t len;
    len = 0;

    /* Check for too deep of a native thread stack. */
    JS_CHECK_RECURSION(cx, goto error);

    DO_NEXT_OP(len);

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
        CHECK_PCCOUNT_INTERRUPTS();
        js::gc::MaybeVerifyBarriers(cx);
        switchOp = int(op) | switchMask;
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

        if (cx->runtime->profilingScripts) {
            if (!script->pcCounters)
                script->initCounts(cx);
            moreInterrupts = true;
        }

        if (script->pcCounters) {
            OpcodeCounts counts = script->getCounts(regs.pc);
            counts.get(OpcodeCounts::BASE_INTERP)++;
            moreInterrupts = true;
        }

        JSInterruptHook hook = cx->runtime->debugHooks.interruptHook;
        if (hook || script->stepModeEnabled()) {
            Value rval;
            JSTrapStatus status = JSTRAP_CONTINUE;
            if (hook)
                status = hook(cx, script, regs.pc, &rval, cx->runtime->debugHooks.interruptHookData);
            if (status == JSTRAP_CONTINUE && script->stepModeEnabled())
                status = Debugger::onSingleStep(cx, &rval);
            switch (status) {
              case JSTRAP_ERROR:
                goto error;
              case JSTRAP_CONTINUE:
                break;
              case JSTRAP_RETURN:
                regs.fp()->setReturnValue(rval);
                interpReturnOK = true;
                goto forced_return;
              case JSTRAP_THROW:
                cx->setPendingException(rval);
                goto error;
              default:;
            }
            moreInterrupts = true;
        }

        if (script->hasAnyBreakpointsOrStepMode())
            moreInterrupts = true;

        if (script->hasBreakpointsAt(regs.pc) && interpMode != JSINTERP_SKIP_TRAP) {
            Value rval;
            JSTrapStatus status = Debugger::onTrap(cx, &rval);
            switch (status) {
              case JSTRAP_ERROR:
                goto error;
              case JSTRAP_RETURN:
                regs.fp()->setReturnValue(rval);
                interpReturnOK = true;
                goto forced_return;
              case JSTRAP_THROW:
                cx->setPendingException(rval);
                goto error;
              default:
                break;
            }
            JS_ASSERT(status == JSTRAP_CONTINUE);
            CHECK_INTERRUPT_HANDLER();
            JS_ASSERT(rval.isInt32() && rval.toInt32() == op);
        }

        interpMode = JSINTERP_NORMAL;

#if JS_THREADED_INTERP
        jumpTable = moreInterrupts ? interruptJumpTable : normalJumpTable;
        JS_EXTENSION_(goto *normalJumpTable[op]);
#else
        switchMask = moreInterrupts ? -1 : 0;
        switchOp = int(op);
        goto do_switch;
#endif
    }

/* No-ops for ease of decompilation. */
ADD_EMPTY_CASE(JSOP_NOP)
ADD_EMPTY_CASE(JSOP_UNUSED0)
ADD_EMPTY_CASE(JSOP_UNUSED1)
ADD_EMPTY_CASE(JSOP_UNUSED2)
ADD_EMPTY_CASE(JSOP_UNUSED3)
ADD_EMPTY_CASE(JSOP_UNUSED4)
ADD_EMPTY_CASE(JSOP_UNUSED5)
ADD_EMPTY_CASE(JSOP_UNUSED6)
ADD_EMPTY_CASE(JSOP_UNUSED7)
ADD_EMPTY_CASE(JSOP_UNUSED8)
ADD_EMPTY_CASE(JSOP_UNUSED9)
ADD_EMPTY_CASE(JSOP_UNUSED10)
ADD_EMPTY_CASE(JSOP_UNUSED11)
ADD_EMPTY_CASE(JSOP_UNUSED12)
ADD_EMPTY_CASE(JSOP_UNUSED13)
ADD_EMPTY_CASE(JSOP_UNUSED14)
ADD_EMPTY_CASE(JSOP_UNUSED15)
ADD_EMPTY_CASE(JSOP_UNUSED16)
ADD_EMPTY_CASE(JSOP_UNUSED17)
ADD_EMPTY_CASE(JSOP_UNUSED18)
ADD_EMPTY_CASE(JSOP_UNUSED19)
ADD_EMPTY_CASE(JSOP_UNUSED20)
ADD_EMPTY_CASE(JSOP_UNUSED21)
ADD_EMPTY_CASE(JSOP_UNUSED22)
ADD_EMPTY_CASE(JSOP_UNUSED23)
ADD_EMPTY_CASE(JSOP_CONDSWITCH)
ADD_EMPTY_CASE(JSOP_TRY)
#if JS_HAS_XML_SUPPORT
ADD_EMPTY_CASE(JSOP_STARTXML)
ADD_EMPTY_CASE(JSOP_STARTXMLEXPR)
#endif
ADD_EMPTY_CASE(JSOP_LOOPHEAD)
ADD_EMPTY_CASE(JSOP_LOOPENTRY)
END_EMPTY_CASES

BEGIN_CASE(JSOP_LABEL)
END_CASE(JSOP_LABEL)

check_backedge:
{
    CHECK_BRANCH();
    if (op != JSOP_LOOPHEAD)
        DO_OP();

#ifdef JS_METHODJIT
    if (!useMethodJIT)
        DO_OP();
    mjit::CompileStatus status =
        mjit::CanMethodJIT(cx, script, regs.pc, regs.fp()->isConstructing(),
                           mjit::CompileRequest_Interpreter);
    if (status == mjit::Compile_Error)
        goto error;
    if (status == mjit::Compile_Okay) {
        void *ncode =
            script->nativeCodeForPC(regs.fp()->isConstructing(), regs.pc);
        JS_ASSERT(ncode);
        mjit::JaegerStatus status =
            mjit::JaegerShotAtSafePoint(cx, ncode, true);
        CHECK_PARTIAL_METHODJIT(status);
        interpReturnOK = (status == mjit::Jaeger_Returned);
        if (entryFrame != regs.fp())
            goto jit_return;
        regs.fp()->setFinishedInInterpreter();
        goto leave_on_safe_point;
    }
    if (status == mjit::Compile_Abort)
        useMethodJIT = false;
#endif /* JS_METHODJIT */

    DO_OP();
}

/* ADD_EMPTY_CASE is not used here as JSOP_LINENO_LENGTH == 3. */
BEGIN_CASE(JSOP_LINENO)
END_CASE(JSOP_LINENO)

BEGIN_CASE(JSOP_UNDEFINED)
    PUSH_UNDEFINED();
END_CASE(JSOP_UNDEFINED)

BEGIN_CASE(JSOP_POP)
    regs.sp--;
END_CASE(JSOP_POP)

BEGIN_CASE(JSOP_POPN)
{
    regs.sp -= GET_UINT16(regs.pc);
#ifdef DEBUG
    JS_ASSERT(regs.fp()->base() <= regs.sp);
    StaticBlockObject *block = regs.fp()->maybeBlockChain();
    JS_ASSERT_IF(block,
                 block->stackDepth() + block->slotCount()
                 <= (size_t) (regs.sp - regs.fp()->base()));
    for (JSObject *obj = &regs.fp()->scopeChain(); obj; obj = obj->enclosingScope()) {
        if (!obj->isBlock() || !obj->isWith())
            continue;
        if (obj->getPrivate() != js_FloatingFrameIfGenerator(cx, regs.fp()))
            break;
        JS_ASSERT(regs.fp()->base() + obj->asBlock().stackDepth()
                  + (obj->isBlock() ? obj->asBlock().slotCount() : 1)
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
    if (!EnterWith(cx, -1))
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
    regs.sp[-1].setObject(regs.fp()->scopeChain());
END_CASE(JSOP_ENTERWITH)

BEGIN_CASE(JSOP_LEAVEWITH)
    JS_ASSERT(regs.sp[-1].toObject() == regs.fp()->scopeChain());
    regs.sp--;
    LeaveWith(cx);
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

    interpReturnOK = true;
    if (entryFrame != regs.fp())
  inline_return:
    {
        JS_ASSERT(!regs.fp()->hasBlockChain());
        JS_ASSERT(!IsActiveWithOrBlock(cx, regs.fp()->scopeChain(), 0));

        if (cx->compartment->debugMode())
            interpReturnOK = ScriptDebugEpilogue(cx, regs.fp(), interpReturnOK);
 
        interpReturnOK = ScriptEpilogue(cx, regs.fp(), interpReturnOK);

        /* The JIT inlines ScriptEpilogue. */
#ifdef JS_METHODJIT
  jit_return:
#endif

        /* The results of lowered call/apply frames need to be shifted. */
        bool shiftResult = regs.fp()->loweredCallOrApply();

        cx->stack.popInlineFrame(regs);

        RESTORE_INTERP_VARS();

        JS_ASSERT(*regs.pc == JSOP_NEW || *regs.pc == JSOP_CALL ||
                  *regs.pc == JSOP_FUNCALL || *regs.pc == JSOP_FUNAPPLY);

        /* Resume execution in the calling frame. */
        RESET_USE_METHODJIT();
        if (JS_LIKELY(interpReturnOK)) {
            TypeScript::Monitor(cx, script, regs.pc, regs.sp[-1]);

            if (shiftResult) {
                regs.sp[-2] = regs.sp[-1];
                regs.sp--;
            }

            len = JSOP_CALL_LENGTH;
            DO_NEXT_OP(len);
        }

        /* Increment pc so that |sp - fp->slots == ReconstructStackDepth(pc)|. */
        regs.pc += JSOP_CALL_LENGTH;
        goto error;
    } else {
        JS_ASSERT(regs.sp == regs.fp()->base());
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
    Value *_;
    VALUE_TO_BOOLEAN(cx, _, cond);
    if (cond == true) {
        len = GET_JUMP_OFFSET(regs.pc);
        DO_NEXT_OP(len);
    }
}
END_CASE(JSOP_OR)

BEGIN_CASE(JSOP_AND)
{
    bool cond;
    Value *_;
    VALUE_TO_BOOLEAN(cx, _, cond);
    if (cond == false) {
        len = GET_JUMP_OFFSET(regs.pc);
        DO_NEXT_OP(len);
    }
}
END_CASE(JSOP_AND)

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
        unsigned diff_ = (unsigned) GET_UINT8(regs.pc) - (unsigned) JSOP_IFEQ;         \
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
    if (!obj->lookupGeneric(cx, id, &obj2, &prop))
        goto error;
    bool cond = prop != NULL;
    TRY_BRANCH_AFTER_COND(cond, 2);
    regs.sp--;
    regs.sp[-1].setBoolean(cond);
}
END_CASE(JSOP_IN)

BEGIN_CASE(JSOP_ITER)
{
    JS_ASSERT(regs.sp > regs.fp()->base());
    uint8_t flags = GET_UINT8(regs.pc);
    if (!ValueToIterator(cx, flags, &regs.sp[-1]))
        goto error;
    CHECK_INTERRUPT_HANDLER();
    JS_ASSERT(!regs.sp[-1].isPrimitive());
}
END_CASE(JSOP_ITER)

BEGIN_CASE(JSOP_MOREITER)
{
    JS_ASSERT(regs.sp - 1 >= regs.fp()->base());
    JS_ASSERT(regs.sp[-1].isObject());
    PUSH_NULL();
    bool cond;
    if (!IteratorMore(cx, &regs.sp[-2].toObject(), &cond, &regs.sp[-1]))
        goto error;
    CHECK_INTERRUPT_HANDLER();
    regs.sp[-1].setBoolean(cond);
}
END_CASE(JSOP_MOREITER)

BEGIN_CASE(JSOP_ITERNEXT)
{
    Value *itervp = regs.sp - GET_INT8(regs.pc);
    JS_ASSERT(itervp >= regs.fp()->base());
    JS_ASSERT(itervp->isObject());
    PUSH_NULL();
    if (!IteratorNext(cx, &itervp->toObject(), &regs.sp[-1]))
        goto error;
}
END_CASE(JSOP_ITERNEXT)

BEGIN_CASE(JSOP_ENDITER)
{
    JS_ASSERT(regs.sp - 1 >= regs.fp()->base());
    bool ok = CloseIterator(cx, &regs.sp[-1].toObject());
    regs.sp--;
    if (!ok)
        goto error;
}
END_CASE(JSOP_ENDITER)

BEGIN_CASE(JSOP_DUP)
{
    JS_ASSERT(regs.sp > regs.fp()->base());
    const Value &rref = regs.sp[-1];
    PUSH_COPY(rref);
}
END_CASE(JSOP_DUP)

BEGIN_CASE(JSOP_DUP2)
{
    JS_ASSERT(regs.sp - 2 >= regs.fp()->base());
    const Value &lref = regs.sp[-2];
    const Value &rref = regs.sp[-1];
    PUSH_COPY(lref);
    PUSH_COPY(rref);
}
END_CASE(JSOP_DUP2)

BEGIN_CASE(JSOP_SWAP)
{
    JS_ASSERT(regs.sp - 2 >= regs.fp()->base());
    Value &lref = regs.sp[-2];
    Value &rref = regs.sp[-1];
    lref.swap(rref);
}
END_CASE(JSOP_SWAP)

BEGIN_CASE(JSOP_PICK)
{
    unsigned i = GET_UINT8(regs.pc);
    JS_ASSERT(regs.sp - (i + 1) >= regs.fp()->base());
    Value lval = regs.sp[-int(i + 1)];
    memmove(regs.sp - (i + 1), regs.sp - i, sizeof(Value) * i);
    regs.sp[-1] = lval;
}
END_CASE(JSOP_PICK)

BEGIN_CASE(JSOP_SETCONST)
{
    PropertyName *name;
    LOAD_NAME(0, name);
    JSObject &obj = regs.fp()->varObj();
    const Value &ref = regs.sp[-1];
    if (!obj.defineProperty(cx, name, ref,
                            JS_PropertyStub, JS_StrictPropertyStub,
                            JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY)) {
        goto error;
    }
}
END_CASE(JSOP_SETCONST);

#if JS_HAS_DESTRUCTURING
BEGIN_CASE(JSOP_ENUMCONSTELEM)
{
    const Value &ref = regs.sp[-3];
    JSObject *obj;
    FETCH_OBJECT(cx, -2, obj);
    jsid id;
    FETCH_ELEMENT_ID(obj, -1, id);
    if (!obj->defineGeneric(cx, id, ref,
                            JS_PropertyStub, JS_StrictPropertyStub,
                            JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY)) {
        goto error;
    }
    regs.sp -= 3;
}
END_CASE(JSOP_ENUMCONSTELEM)
#endif

BEGIN_CASE(JSOP_BINDGNAME)
    PUSH_OBJECT(regs.fp()->scopeChain().global());
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
        obj = &regs.fp()->scopeChain();
        if (obj->isGlobal())
            break;

        PropertyName *name;
        LOAD_NAME(0, name);

        obj = FindIdentifierBase(cx, &regs.fp()->scopeChain(), name);
        if (!obj)
            goto error;
    } while (0);
    PUSH_OBJECT(*obj);
}
END_CASE(JSOP_BINDNAME)

#define BITWISE_OP(OP)                                                        \
    JS_BEGIN_MACRO                                                            \
        int32_t i, j;                                                         \
        if (!ToInt32(cx, regs.sp[-2], &i))                                    \
            goto error;                                                       \
        if (!ToInt32(cx, regs.sp[-1], &j))                                    \
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

#define EQUALITY_OP(OP)                                                       \
    JS_BEGIN_MACRO                                                            \
        Value rval = regs.sp[-1];                                             \
        Value lval = regs.sp[-2];                                             \
        bool cond;                                                            \
        if (!LooselyEqual(cx, lval, rval, &cond))                             \
            goto error;                                                       \
        cond = cond OP JS_TRUE;                                               \
        TRY_BRANCH_AFTER_COND(cond, 2);                                       \
        regs.sp--;                                                            \
        regs.sp[-1].setBoolean(cond);                                         \
    JS_END_MACRO

BEGIN_CASE(JSOP_EQ)
    EQUALITY_OP(==);
END_CASE(JSOP_EQ)

BEGIN_CASE(JSOP_NE)
    EQUALITY_OP(!=);
END_CASE(JSOP_NE)

#undef EQUALITY_OP

#define STRICT_EQUALITY_OP(OP, COND)                                          \
    JS_BEGIN_MACRO                                                            \
        const Value &rref = regs.sp[-1];                                      \
        const Value &lref = regs.sp[-2];                                      \
        bool equal;                                                           \
        if (!StrictlyEqual(cx, lref, rref, &equal))                           \
            goto error;                                                       \
        COND = equal OP JS_TRUE;                                              \
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

#undef STRICT_EQUALITY_OP

BEGIN_CASE(JSOP_LT)
{
    bool cond;
    const Value &lref = regs.sp[-2];
    const Value &rref = regs.sp[-1];
    if (!LessThanOperation(cx, lref, rref, &cond))
        goto error;
    TRY_BRANCH_AFTER_COND(cond, 2);
    regs.sp[-2].setBoolean(cond);
    regs.sp--;
}
END_CASE(JSOP_LT)

BEGIN_CASE(JSOP_LE)
{
    bool cond;
    const Value &lref = regs.sp[-2];
    const Value &rref = regs.sp[-1];
    if (!LessThanOrEqualOperation(cx, lref, rref, &cond))
        goto error;
    TRY_BRANCH_AFTER_COND(cond, 2);
    regs.sp[-2].setBoolean(cond);
    regs.sp--;
}
END_CASE(JSOP_LE)

BEGIN_CASE(JSOP_GT)
{
    bool cond;
    const Value &lref = regs.sp[-2];
    const Value &rref = regs.sp[-1];
    if (!GreaterThanOperation(cx, lref, rref, &cond))
        goto error;
    TRY_BRANCH_AFTER_COND(cond, 2);
    regs.sp[-2].setBoolean(cond);
    regs.sp--;
}
END_CASE(JSOP_GT)

BEGIN_CASE(JSOP_GE)
{
    bool cond;
    const Value &lref = regs.sp[-2];
    const Value &rref = regs.sp[-1];
    if (!GreaterThanOrEqualOperation(cx, lref, rref, &cond))
        goto error;
    TRY_BRANCH_AFTER_COND(cond, 2);
    regs.sp[-2].setBoolean(cond);
    regs.sp--;
}
END_CASE(JSOP_GE)

#define SIGNED_SHIFT_OP(OP)                                                   \
    JS_BEGIN_MACRO                                                            \
        int32_t i, j;                                                         \
        if (!ToInt32(cx, regs.sp[-2], &i))                                    \
            goto error;                                                       \
        if (!ToInt32(cx, regs.sp[-1], &j))                                    \
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
    if (!ToUint32(cx, regs.sp[-2], &u))
        goto error;
    int32_t j;
    if (!ToInt32(cx, regs.sp[-1], &j))
        goto error;

    u >>= (j & 31);

    regs.sp--;
    if (!regs.sp[-1].setNumber(uint32_t(u)))
        TypeScript::MonitorOverflow(cx, script, regs.pc);
}
END_CASE(JSOP_URSH)

BEGIN_CASE(JSOP_ADD)
{
    Value lval = regs.sp[-2];
    Value rval = regs.sp[-1];
    if (!AddOperation(cx, lval, rval, &regs.sp[-2]))
        goto error;
    regs.sp--;
}
END_CASE(JSOP_ADD)

BEGIN_CASE(JSOP_SUB)
{
    Value lval = regs.sp[-2];
    Value rval = regs.sp[-1];
    if (!SubOperation(cx, lval, rval, &regs.sp[-2]))
        goto error;
    regs.sp--;
}
END_CASE(JSOP_SUB)

BEGIN_CASE(JSOP_MUL)
{
    Value lval = regs.sp[-2];
    Value rval = regs.sp[-1];
    if (!MulOperation(cx, lval, rval, &regs.sp[-2]))
        goto error;
    regs.sp--;
}
END_CASE(JSOP_MUL)

BEGIN_CASE(JSOP_DIV)
{
    Value lval = regs.sp[-2];
    Value rval = regs.sp[-1];
    if (!DivOperation(cx, lval, rval, &regs.sp[-2]))
        goto error;
    regs.sp--;
}
END_CASE(JSOP_DIV)

BEGIN_CASE(JSOP_MOD)
{
    Value lval = regs.sp[-2];
    Value rval = regs.sp[-1];
    if (!ModOperation(cx, lval, rval, &regs.sp[-2]))
        goto error;
    regs.sp--;
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
    if (!ToInt32(cx, regs.sp[-1], &i))
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
     * results, -0.0 or INT32_MAX + 1, are double values.
     */
    Value ref = regs.sp[-1];
    int32_t i;
    if (ref.isInt32() && (i = ref.toInt32()) != 0 && i != INT32_MIN) {
        i = -i;
        regs.sp[-1].setInt32(i);
    } else {
        double d;
        if (!ToNumber(cx, regs.sp[-1], &d))
            goto error;
        d = -d;
        if (!regs.sp[-1].setNumber(d) && !ref.isDouble())
            TypeScript::MonitorOverflow(cx, script, regs.pc);
    }
}
END_CASE(JSOP_NEG)

BEGIN_CASE(JSOP_POS)
    if (!ToNumber(cx, &regs.sp[-1]))
        goto error;
    if (!regs.sp[-1].isInt32())
        TypeScript::MonitorOverflow(cx, script, regs.pc);
END_CASE(JSOP_POS)

BEGIN_CASE(JSOP_DELNAME)
{
    PropertyName *name;
    LOAD_NAME(0, name);
    JSObject *obj, *obj2;
    JSProperty *prop;
    if (!FindProperty(cx, name, cx->stack.currentScriptedScopeChain(), &obj, &obj2, &prop))
        goto error;

    /* Strict mode code should never contain JSOP_DELNAME opcodes. */
    JS_ASSERT(!script->strictModeCode);

    /* ECMA says to return true if name is undefined or inherited. */
    PUSH_BOOLEAN(true);
    if (prop) {
        if (!obj->deleteProperty(cx, name, &regs.sp[-1], false))
            goto error;
    }
}
END_CASE(JSOP_DELNAME)

BEGIN_CASE(JSOP_DELPROP)
{
    PropertyName *name;
    LOAD_NAME(0, name);

    JSObject *obj;
    FETCH_OBJECT(cx, -1, obj);

    Value rval;
    if (!obj->deleteProperty(cx, name, &rval, script->strictModeCode))
        goto error;

    regs.sp[-1] = rval;
}
END_CASE(JSOP_DELPROP)

BEGIN_CASE(JSOP_DELELEM)
{
    /* Fetch the left part and resolve it to a non-null object. */
    JSObject *obj;
    FETCH_OBJECT(cx, -2, obj);

    const Value &propval = regs.sp[-1];
    Value &rval = regs.sp[-2];

    if (!obj->deleteByValue(cx, propval, &rval, script->strictModeCode))
        goto error;

    regs.sp--;
}
END_CASE(JSOP_DELELEM)

BEGIN_CASE(JSOP_TOID)
{
    /*
     * Increment or decrement requires use to lookup the same property twice, but we need to avoid
     * the oberservable stringification the second time.
     * There must be an object value below the id, which will not be popped
     * but is necessary in interning the id for XML.
     */
    Value objval = regs.sp[-2];
    Value idval = regs.sp[-1];
    if (!ToIdOperation(cx, objval, idval, &regs.sp[-1]))
        goto error;
}
END_CASE(JSOP_TOID)

BEGIN_CASE(JSOP_TYPEOFEXPR)
BEGIN_CASE(JSOP_TYPEOF)
{
    const Value &ref = regs.sp[-1];
    JSType type = JS_TypeOfValue(cx, ref);
    regs.sp[-1].setString(rt->atomState.typeAtoms[type]);
}
END_CASE(JSOP_TYPEOF)

BEGIN_CASE(JSOP_VOID)
    regs.sp[-1].setUndefined();
END_CASE(JSOP_VOID)

BEGIN_CASE(JSOP_INCELEM)
BEGIN_CASE(JSOP_DECELEM)
BEGIN_CASE(JSOP_ELEMINC)
BEGIN_CASE(JSOP_ELEMDEC)
    /* No-op */
END_CASE(JSOP_INCELEM)

BEGIN_CASE(JSOP_INCPROP)
BEGIN_CASE(JSOP_DECPROP)
BEGIN_CASE(JSOP_PROPINC)
BEGIN_CASE(JSOP_PROPDEC)
BEGIN_CASE(JSOP_INCNAME)
BEGIN_CASE(JSOP_DECNAME)
BEGIN_CASE(JSOP_NAMEINC)
BEGIN_CASE(JSOP_NAMEDEC)
BEGIN_CASE(JSOP_INCGNAME)
BEGIN_CASE(JSOP_DECGNAME)
BEGIN_CASE(JSOP_GNAMEINC)
BEGIN_CASE(JSOP_GNAMEDEC)
    /* No-op */
END_CASE(JSOP_INCPROP)

{
    int incr, incr2;
    uint32_t slot;
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
    slot = GET_ARGNO(regs.pc);
    JS_ASSERT(slot < regs.fp()->numFormalArgs());
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
    JS_ASSERT(slot < regs.fp()->numSlots());
    vp = regs.fp()->slots() + slot;

  do_int_fast_incop:
    int32_t tmp;
    if (JS_LIKELY(vp->isInt32() && CanIncDecWithoutOverflow(tmp = vp->toInt32()))) {
        vp->getInt32Ref() = tmp + incr;
        JS_ASSERT(JSOP_INCARG_LENGTH == js_CodeSpec[op].length);
        PUSH_INT32(tmp + incr2);
    } else {
        PUSH_COPY(*vp);
        if (!DoIncDec(cx, &js_CodeSpec[op], &regs.sp[-1], vp))
            goto error;
        TypeScript::MonitorOverflow(cx, script, regs.pc);
    }
    len = JSOP_INCARG_LENGTH;
    JS_ASSERT(len == js_CodeSpec[op].length);
    DO_NEXT_OP(len);
}

BEGIN_CASE(JSOP_THIS)
    if (!ComputeThis(cx, regs.fp()))
        goto error;
    PUSH_COPY(regs.fp()->thisValue());
END_CASE(JSOP_THIS)

BEGIN_CASE(JSOP_GETPROP)
BEGIN_CASE(JSOP_GETXPROP)
BEGIN_CASE(JSOP_LENGTH)
BEGIN_CASE(JSOP_CALLPROP)
{
    Value rval;
    if (!GetPropertyOperation(cx, regs.pc, regs.sp[-1], &rval))
        goto error;

    TypeScript::Monitor(cx, script, regs.pc, rval);

    regs.sp[-1] = rval;
    assertSameCompartment(cx, regs.sp[-1]);
}
END_CASE(JSOP_GETPROP)

BEGIN_CASE(JSOP_SETGNAME)
BEGIN_CASE(JSOP_SETNAME)
BEGIN_CASE(JSOP_SETPROP)
BEGIN_CASE(JSOP_SETMETHOD)
{
    const Value &rval = regs.sp[-1];
    const Value &lval = regs.sp[-2];

    if (!SetPropertyOperation(cx, regs.pc, lval, rval))
        goto error;

    regs.sp[-2] = regs.sp[-1];
    regs.sp--;
}
END_CASE(JSOP_SETPROP)

BEGIN_CASE(JSOP_GETELEM)
{
    Value &lref = regs.sp[-2];
    Value &rref = regs.sp[-1];
    if (!GetElementOperation(cx, lref, rref, &regs.sp[-2]))
        goto error;
    TypeScript::Monitor(cx, script, regs.pc, regs.sp[-2]);
    regs.sp--;
}
END_CASE(JSOP_GETELEM)

BEGIN_CASE(JSOP_CALLELEM)
{
    /* Find the object on which to look for |this|'s properties. */
    Value thisv = regs.sp[-2];
    JSObject *thisObj = ValuePropertyBearer(cx, regs.fp(), thisv, -2);
    if (!thisObj)
        goto error;

    /* Fetch index and convert it to id suitable for use with obj. */
    jsid id;
    FETCH_ELEMENT_ID(thisObj, -1, id);

    /* Get the method. */
    if (!js_GetMethod(cx, thisObj, id, JSGET_NO_METHOD_BARRIER, &regs.sp[-2]))
        goto error;

#if JS_HAS_NO_SUCH_METHOD
    if (JS_UNLIKELY(regs.sp[-2].isPrimitive()) && thisv.isObject()) {
        if (!OnUnknownMethod(cx, &thisv.toObject(), regs.sp[-1], regs.sp - 2))
            goto error;
    }
#endif

    regs.sp--;

    TypeScript::Monitor(cx, script, regs.pc, regs.sp[-1]);
}
END_CASE(JSOP_CALLELEM)

BEGIN_CASE(JSOP_SETELEM)
{
    JSObject *obj;
    FETCH_OBJECT(cx, -3, obj);
    jsid id;
    FETCH_ELEMENT_ID(obj, -2, id);
    Value &value = regs.sp[-1];
    if (!SetObjectElementOperation(cx, obj, id, value, script->strictModeCode))
        goto error;
    regs.sp[-3] = value;
    regs.sp -= 2;
}
END_CASE(JSOP_SETELEM)

BEGIN_CASE(JSOP_ENUMELEM)
{
    /* Funky: the value to set is under the [obj, id] pair. */
    JSObject *obj;
    FETCH_OBJECT(cx, -2, obj);
    jsid id;
    FETCH_ELEMENT_ID(obj, -1, id);
    Value rval = regs.sp[-3];
    if (!obj->setGeneric(cx, id, &rval, script->strictModeCode))
        goto error;
    regs.sp -= 3;
}
END_CASE(JSOP_ENUMELEM)

BEGIN_CASE(JSOP_EVAL)
{
    CallArgs args = CallArgsFromSp(GET_ARGC(regs.pc), regs.sp);
    if (IsBuiltinEvalForScope(&regs.fp()->scopeChain(), args.calleev())) {
        if (!DirectEval(cx, args))
            goto error;
    } else {
        if (!InvokeKernel(cx, args))
            goto error;
    }
    CHECK_INTERRUPT_HANDLER();
    regs.sp = args.spAfterCall();
    TypeScript::Monitor(cx, script, regs.pc, regs.sp[-1]);
}
END_CASE(JSOP_EVAL)

BEGIN_CASE(JSOP_NEW)
BEGIN_CASE(JSOP_CALL)
BEGIN_CASE(JSOP_FUNCALL)
BEGIN_CASE(JSOP_FUNAPPLY)
{
    CallArgs args = CallArgsFromSp(GET_ARGC(regs.pc), regs.sp);
    JS_ASSERT(args.base() >= regs.fp()->base());

    bool construct = (*regs.pc == JSOP_NEW);

    JSFunction *fun;

    /* Don't bother trying to fast-path calls to scripted non-constructors. */
    if (!IsFunctionObject(args.calleev(), &fun) || !fun->isInterpretedConstructor()) {
        if (construct) {
            if (!InvokeConstructorKernel(cx, args))
                goto error;
        } else {
            if (!InvokeKernel(cx, args))
                goto error;
        }
        regs.sp = args.spAfterCall();
        TypeScript::Monitor(cx, script, regs.pc, regs.sp[-1]);
        CHECK_INTERRUPT_HANDLER();
        len = JSOP_CALL_LENGTH;
        DO_NEXT_OP(len);
    }

    TypeMonitorCall(cx, args, construct);

    InitialFrameFlags initial = construct ? INITIAL_CONSTRUCT : INITIAL_NONE;

    JSScript *newScript = fun->script();

    if (newScript->compileAndGo && newScript->hasClearedGlobal()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CLEARED_SCOPE);
        goto error;
    }

    if (!cx->stack.pushInlineFrame(cx, regs, args, *fun, newScript, initial))
        goto error;

    RESTORE_INTERP_VARS();

    if (!regs.fp()->functionPrologue(cx))
        goto error;

    RESET_USE_METHODJIT();

    bool newType = cx->typeInferenceEnabled() && UseNewType(cx, script, regs.pc);

#ifdef JS_METHODJIT
    if (!newType) {
        /* Try to ensure methods are method JIT'd.  */
        mjit::CompileRequest request = (interpMode == JSINTERP_NORMAL)
                                       ? mjit::CompileRequest_Interpreter
                                       : mjit::CompileRequest_JIT;
        mjit::CompileStatus status = mjit::CanMethodJIT(cx, script, script->code,
                                                        construct, request);
        if (status == mjit::Compile_Error)
            goto error;
        if (status == mjit::Compile_Okay) {
            mjit::JaegerStatus status = mjit::JaegerShot(cx, true);
            CHECK_PARTIAL_METHODJIT(status);
            interpReturnOK = (status == mjit::Jaeger_Returned);
            CHECK_INTERRUPT_HANDLER();
            goto jit_return;
        }
    }
#endif

    if (!ScriptPrologue(cx, regs.fp(), newType))
        goto error;

    if (cx->compartment->debugMode()) {
        switch (ScriptDebugPrologue(cx, regs.fp())) {
          case JSTRAP_CONTINUE:
            break;
          case JSTRAP_RETURN:
            interpReturnOK = true;
            goto forced_return;
          case JSTRAP_THROW:
          case JSTRAP_ERROR:
            goto error;
          default:
            JS_NOT_REACHED("bad ScriptDebugPrologue status");
        }
    }

    CHECK_INTERRUPT_HANDLER();

    /* Load first op and dispatch it (safe since JSOP_STOP). */
    op = (JSOp) *regs.pc;
    DO_OP();
}

BEGIN_CASE(JSOP_SETCALL)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_LEFTSIDE_OF_ASS);
    goto error;
}
END_CASE(JSOP_SETCALL)

BEGIN_CASE(JSOP_IMPLICITTHIS)
{
    PropertyName *name;
    LOAD_NAME(0, name);

    JSObject *obj, *obj2;
    JSProperty *prop;
    if (!FindPropertyHelper(cx, name, false, cx->stack.currentScriptedScopeChain(), &obj, &obj2, &prop))
        goto error;

    Value v;
    if (!ComputeImplicitThis(cx, obj, &v))
        goto error;
    PUSH_COPY(v);
}
END_CASE(JSOP_IMPLICITTHIS)

BEGIN_CASE(JSOP_GETGNAME)
BEGIN_CASE(JSOP_CALLGNAME)
BEGIN_CASE(JSOP_NAME)
BEGIN_CASE(JSOP_CALLNAME)
{
    Value rval;
    if (!NameOperation(cx, regs.pc, &rval))
        goto error;

    PUSH_COPY(rval);
    TypeScript::Monitor(cx, script, regs.pc, rval);
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

BEGIN_CASE(JSOP_DOUBLE)
{
    double dbl;
    LOAD_DOUBLE(0, dbl);
    PUSH_DOUBLE(dbl);
}
END_CASE(JSOP_DOUBLE)

BEGIN_CASE(JSOP_STRING)
{
    JSAtom *atom;
    LOAD_ATOM(0, atom);
    PUSH_STRING(atom);
}
END_CASE(JSOP_STRING)

BEGIN_CASE(JSOP_OBJECT)
{
    PUSH_OBJECT(*script->getObject(GET_UINT32_INDEX(regs.pc)));
}
END_CASE(JSOP_OBJECT)

BEGIN_CASE(JSOP_REGEXP)
{
    /*
     * Push a regexp object cloned from the regexp literal object mapped by the
     * bytecode at pc.
     */
    uint32_t index = GET_UINT32_INDEX(regs.pc);
    JSObject *proto = regs.fp()->scopeChain().global().getOrCreateRegExpPrototype(cx);
    if (!proto)
        goto error;
    JSObject *obj = CloneRegExpObject(cx, script->getRegExp(index), proto);
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
     * opcode is emitted only for dense int-domain switches.)
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
    int32_t low = GET_JUMP_OFFSET(pc2);
    pc2 += JUMP_OFFSET_LEN;
    int32_t high = GET_JUMP_OFFSET(pc2);

    i -= low;
    if ((uint32_t)i < (uint32_t)(high - low + 1)) {
        pc2 += JUMP_OFFSET_LEN + JUMP_OFFSET_LEN * i;
        int32_t off = (int32_t) GET_JUMP_OFFSET(pc2);
        if (off)
            len = off;
    }
}
END_VARLEN_CASE
}

{
BEGIN_CASE(JSOP_LOOKUPSWITCH)
{
    int32_t off;
    off = JUMP_OFFSET_LEN;

    /*
     * JSOP_LOOKUPSWITCH are never used if any atom index in it would exceed
     * 64K limit.
     */
    JS_ASSERT(atoms == script->atoms);
    jsbytecode *pc2 = regs.pc;

    Value lval = regs.sp[-1];
    regs.sp--;

    int npairs;
    if (!lval.isPrimitive())
        goto end_lookup_switch;

    pc2 += off;
    npairs = GET_UINT16(pc2);
    pc2 += UINT16_LEN;
    JS_ASSERT(npairs);  /* empty switch uses JSOP_TABLESWITCH */

    bool match;
#define SEARCH_PAIRS(MATCH_CODE)                                              \
    for (;;) {                                                                \
        Value rval = script->getConst(GET_UINT32_INDEX(pc2));                 \
        MATCH_CODE                                                            \
        pc2 += UINT32_INDEX_LEN;                                              \
        if (match)                                                            \
            break;                                                            \
        pc2 += off;                                                           \
        if (--npairs == 0) {                                                  \
            pc2 = regs.pc;                                                    \
            break;                                                            \
        }                                                                     \
    }

    if (lval.isString()) {
        JSLinearString *str = lval.toString()->ensureLinear(cx);
        if (!str)
            goto error;
        JSLinearString *str2;
        SEARCH_PAIRS(
            match = (rval.isString() &&
                     ((str2 = &rval.toString()->asLinear()) == str ||
                      EqualStrings(str2, str)));
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
    len = GET_JUMP_OFFSET(pc2);
}
END_VARLEN_CASE
}

BEGIN_CASE(JSOP_ARGUMENTS)
{
    Value rval;
    if (cx->typeInferenceEnabled() && !script->strictModeCode) {
        if (!script->ensureRanInference(cx))
            goto error;
        if (script->createdArgs) {
            ArgumentsObject *arguments = js_GetArgsObject(cx, regs.fp());
            if (!arguments)
                goto error;
            rval = ObjectValue(*arguments);
        } else {
            rval = MagicValue(JS_LAZY_ARGUMENTS);
        }
    } else {
        ArgumentsObject *arguments = js_GetArgsObject(cx, regs.fp());
        if (!arguments)
            goto error;
        rval = ObjectValue(*arguments);
    }
    PUSH_COPY(rval);
}
END_CASE(JSOP_ARGUMENTS)

BEGIN_CASE(JSOP_GETARG)
BEGIN_CASE(JSOP_CALLARG)
    PUSH_COPY(regs.fp()->formalArg(GET_ARGNO(regs.pc)));
END_CASE(JSOP_GETARG)

BEGIN_CASE(JSOP_SETARG)
    regs.fp()->formalArg(GET_ARGNO(regs.pc)) = regs.sp[-1];
END_CASE(JSOP_SETARG)

BEGIN_CASE(JSOP_GETLOCAL)
BEGIN_CASE(JSOP_CALLLOCAL)
    PUSH_COPY_SKIP_CHECK(regs.fp()->localSlot(GET_SLOTNO(regs.pc)));

    /*
     * Skip the same-compartment assertion if the local will be immediately
     * popped. We do not guarantee sync for dead locals when coming in from the
     * method JIT, and a GETLOCAL followed by POP is not considered to be
     * a use of the variable.
     */
    if (regs.pc[JSOP_GETLOCAL_LENGTH] != JSOP_POP)
        assertSameCompartment(cx, regs.sp[-1]);
END_CASE(JSOP_GETLOCAL)

BEGIN_CASE(JSOP_SETLOCAL)
    regs.fp()->localSlot(GET_SLOTNO(regs.pc)) = regs.sp[-1];
END_CASE(JSOP_SETLOCAL)

BEGIN_CASE(JSOP_GETFCSLOT)
BEGIN_CASE(JSOP_CALLFCSLOT)
{
    JS_ASSERT(regs.fp()->isNonEvalFunctionFrame());
    unsigned index = GET_UINT16(regs.pc);
    JSObject *obj = &argv[-2].toObject();

    PUSH_COPY(obj->toFunction()->getFlatClosureUpvar(index));
    TypeScript::Monitor(cx, script, regs.pc, regs.sp[-1]);
}
END_CASE(JSOP_GETFCSLOT)

BEGIN_CASE(JSOP_DEFCONST)
BEGIN_CASE(JSOP_DEFVAR)
{
    PropertyName *dn = atoms[GET_UINT32_INDEX(regs.pc)]->asPropertyName();

    /* ES5 10.5 step 8 (with subsequent errata). */
    unsigned attrs = JSPROP_ENUMERATE;
    if (!regs.fp()->isEvalFrame())
        attrs |= JSPROP_PERMANENT;
    if (op == JSOP_DEFCONST)
        attrs |= JSPROP_READONLY;

    /* Step 8b. */
    JSObject &obj = regs.fp()->varObj();

    if (!DefVarOrConstOperation(cx, obj, dn, attrs))
        goto error;
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
    JSFunction *fun = script->getFunction(GET_UINT32_INDEX(regs.pc));
    JSObject *obj = fun;

    JSObject *obj2;
    if (fun->isNullClosure()) {
        /*
         * Even a null closure needs a parent for principals finding.
         * FIXME: bug 476950, although debugger users may also demand some kind
         * of scope link for debugger-assisted eval-in-frame.
         */
        obj2 = &regs.fp()->scopeChain();
    } else {
        JS_ASSERT(!fun->isFlatClosure());

        obj2 = GetScopeChain(cx, regs.fp());
        if (!obj2)
            goto error;
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
    if (obj->toFunction()->environment() != obj2) {
        obj = CloneFunctionObjectIfNotSingleton(cx, fun, obj2);
        if (!obj)
            goto error;
        JS_ASSERT_IF(script->hasGlobal(), obj->getProto() == fun->getProto());
    }

    /*
     * ECMA requires functions defined when entering Eval code to be
     * impermanent.
     */
    unsigned attrs = regs.fp()->isEvalFrame()
                  ? JSPROP_ENUMERATE
                  : JSPROP_ENUMERATE | JSPROP_PERMANENT;

    /*
     * We define the function as a property of the variable object and not the
     * current scope chain even for the case of function expression statements
     * and functions defined by eval inside let or with blocks.
     */
    JSObject *parent = &regs.fp()->varObj();

    /* ES5 10.5 (NB: with subsequent errata). */
    PropertyName *name = fun->atom->asPropertyName();
    JSProperty *prop = NULL;
    JSObject *pobj;
    if (!parent->lookupProperty(cx, name, &pobj, &prop))
        goto error;

    Value rval = ObjectValue(*obj);

    do {
        /* Steps 5d, 5f. */
        if (!prop || pobj != parent) {
            if (!parent->defineProperty(cx, name, rval,
                                        JS_PropertyStub, JS_StrictPropertyStub, attrs))
            {
                goto error;
            }
            break;
        }

        /* Step 5e. */
        JS_ASSERT(parent->isNative());
        Shape *shape = reinterpret_cast<Shape *>(prop);
        if (parent->isGlobal()) {
            if (shape->configurable()) {
                if (!parent->defineProperty(cx, name, rval,
                                            JS_PropertyStub, JS_StrictPropertyStub, attrs))
                {
                    goto error;
                }
                break;
            }

            if (shape->isAccessorDescriptor() || !shape->writable() || !shape->enumerable()) {
                JSAutoByteString bytes;
                if (js_AtomToPrintableString(cx, name, &bytes)) {
                    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                         JSMSG_CANT_REDEFINE_PROP, bytes.ptr());
                }
                goto error;
            }
        }

        /*
         * Non-global properties, and global properties which we aren't simply
         * redefining, must be set.  First, this preserves their attributes.
         * Second, this will produce warnings and/or errors as necessary if the
         * specified Call object property is not writable (const).
         */

        /* Step 5f. */
        if (!parent->setProperty(cx, name, &rval, script->strictModeCode))
            goto error;
    } while (false);
}
END_CASE(JSOP_DEFFUN)

BEGIN_CASE(JSOP_DEFLOCALFUN)
{
    /*
     * Define a local function (i.e., one nested at the top level of another
     * function), parented by the current scope chain, stored in a local
     * variable slot that the compiler allocated.  This is an optimization over
     * JSOP_DEFFUN that avoids requiring a call object for the outer function's
     * activation.
     */
    JSFunction *fun = script->getFunction(GET_UINT32_INDEX(regs.pc + SLOTNO_LEN));
    JS_ASSERT(fun->isInterpreted());
    JS_ASSERT(!fun->isFlatClosure());

    JSObject *parent;
    if (fun->isNullClosure()) {
        parent = &regs.fp()->scopeChain();
    } else {
        parent = GetScopeChain(cx, regs.fp());
        if (!parent)
            goto error;
    }
    JSObject *obj = CloneFunctionObjectIfNotSingleton(cx, fun, parent);
    if (!obj)
        goto error;

    JS_ASSERT_IF(script->hasGlobal(), obj->getProto() == fun->getProto());

    regs.fp()->varSlot(GET_SLOTNO(regs.pc)) = ObjectValue(*obj);
}
END_CASE(JSOP_DEFLOCALFUN)

BEGIN_CASE(JSOP_DEFLOCALFUN_FC)
{
    JSFunction *fun = script->getFunction(GET_UINT32_INDEX(regs.pc + SLOTNO_LEN));

    JSObject *obj = js_NewFlatClosure(cx, fun);
    if (!obj)
        goto error;

    regs.fp()->varSlot(GET_SLOTNO(regs.pc)) = ObjectValue(*obj);
}
END_CASE(JSOP_DEFLOCALFUN_FC)

BEGIN_CASE(JSOP_LAMBDA)
{
    /* Load the specified function object literal. */
    JSFunction *fun = script->getFunction(GET_UINT32_INDEX(regs.pc));
    JSObject *obj = fun;

    /* do-while(0) so we can break instead of using a goto. */
    do {
        JSObject *parent;
        if (fun->isNullClosure()) {
            parent = &regs.fp()->scopeChain();

            if (fun->joinable()) {
                jsbytecode *pc2 = regs.pc + JSOP_LAMBDA_LENGTH;
                JSOp op2 = JSOp(*pc2);

                /*
                 * Optimize var obj = {method: function () { ... }, ...},
                 * this.method = function () { ... }; and other significant
                 * single-use-of-null-closure bytecode sequences.
                 */
                if (op2 == JSOP_INITMETHOD) {
#ifdef DEBUG
                    const Value &lref = regs.sp[-1];
                    JS_ASSERT(lref.isObject());
                    JSObject *obj2 = &lref.toObject();
                    JS_ASSERT(obj2->isObject());
#endif
                    JS_ASSERT(fun->methodAtom() ==
                              script->getAtom(GET_UINT32_INDEX(regs.pc + JSOP_LAMBDA_LENGTH)));
                    break;
                }

                if (op2 == JSOP_SETMETHOD) {
#ifdef DEBUG
                    op2 = JSOp(pc2[JSOP_SETMETHOD_LENGTH]);
                    JS_ASSERT(op2 == JSOP_POP || op2 == JSOP_POPV);
#endif
                    const Value &lref = regs.sp[-1];
                    if (lref.isObject() && lref.toObject().canHaveMethodBarrier()) {
                        JS_ASSERT(fun->methodAtom() ==
                                  script->getAtom(GET_UINT32_INDEX(regs.pc + JSOP_LAMBDA_LENGTH)));
                        break;
                    }
                } else if (op2 == JSOP_CALL) {
                    /*
                     * Array.prototype.sort and String.prototype.replace are
                     * optimized as if they are special form. We know that they
                     * won't leak the joined function object in obj, therefore
                     * we don't need to clone that compiler-created function
                     * object for identity/mutation reasons.
                     */
                    int iargc = GET_ARGC(pc2);

                    /*
                     * Note that we have not yet pushed obj as the final argument,
                     * so regs.sp[1 - (iargc + 2)], and not regs.sp[-(iargc + 2)],
                     * is the callee for this JSOP_CALL.
                     */
                    const Value &cref = regs.sp[1 - (iargc + 2)];
                    JSFunction *fun;

                    if (IsFunctionObject(cref, &fun)) {
                        if (Native native = fun->maybeNative()) {
                            if ((iargc == 1 && native == array_sort) ||
                                (iargc == 2 && native == str_replace)) {
                                break;
                            }
                        }
                    }
                } else if (op2 == JSOP_NULL) {
                    pc2 += JSOP_NULL_LENGTH;
                    op2 = JSOp(*pc2);

                    if (op2 == JSOP_CALL && GET_ARGC(pc2) == 0)
                        break;
                }
            }
        } else {
            parent = GetScopeChain(cx, regs.fp());
            if (!parent)
                goto error;
        }

        obj = CloneFunctionObjectIfNotSingleton(cx, fun, parent);
        if (!obj)
            goto error;
    } while (0);

    JS_ASSERT(obj->getProto());
    JS_ASSERT_IF(script->hasGlobal(), obj->getProto() == fun->getProto());

    PUSH_OBJECT(*obj);
}
END_CASE(JSOP_LAMBDA)

BEGIN_CASE(JSOP_LAMBDA_FC)
{
    JSFunction *fun = script->getFunction(GET_UINT32_INDEX(regs.pc));

    JSObject *obj = js_NewFlatClosure(cx, fun);
    if (!obj)
        goto error;
    JS_ASSERT_IF(script->hasGlobal(), obj->getProto() == fun->getProto());

    PUSH_OBJECT(*obj);
}
END_CASE(JSOP_LAMBDA_FC)

BEGIN_CASE(JSOP_CALLEE)
    JS_ASSERT(regs.fp()->isNonEvalFunctionFrame());
    PUSH_COPY(argv[-2]);
END_CASE(JSOP_CALLEE)

BEGIN_CASE(JSOP_GETTER)
BEGIN_CASE(JSOP_SETTER)
{
    JSOp op2 = JSOp(*++regs.pc);
    jsid id;
    Value rval;
    int i;
    JSObject *obj;
    switch (op2) {
      case JSOP_SETNAME:
      case JSOP_SETPROP:
      {
        PropertyName *name;
        LOAD_NAME(0, name);
        id = ATOM_TO_JSID(name);
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
        JS_ASSERT(regs.sp - regs.fp()->base() >= 2);
        rval = regs.sp[-1];
        i = -1;
        PropertyName *name;
        LOAD_NAME(0, name);
        id = ATOM_TO_JSID(name);
        goto gs_get_lval;
      }
      default:
        JS_ASSERT(op2 == JSOP_INITELEM);

        JS_ASSERT(regs.sp - regs.fp()->base() >= 3);
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
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_GETTER_OR_SETTER,
                             (op == JSOP_GETTER) ? js_getter_str : js_setter_str);
        goto error;
    }

    /*
     * Getters and setters are just like watchpoints from an access control
     * point of view.
     */
    Value rtmp;
    unsigned attrs;
    if (!CheckAccess(cx, obj, id, JSACC_WATCH, &rtmp, &attrs))
        goto error;

    PropertyOp getter;
    StrictPropertyOp setter;
    if (op == JSOP_GETTER) {
        getter = CastAsPropertyOp(&rval.toObject());
        setter = JS_StrictPropertyStub;
        attrs = JSPROP_GETTER;
    } else {
        getter = JS_PropertyStub;
        setter = CastAsStrictPropertyOp(&rval.toObject());
        attrs = JSPROP_SETTER;
    }
    attrs |= JSPROP_ENUMERATE | JSPROP_SHARED;

    if (!obj->defineGeneric(cx, id, UndefinedValue(), getter, setter, attrs))
        goto error;

    regs.sp += i;
    if (js_CodeSpec[op2].ndefs > js_CodeSpec[op2].nuses) {
        JS_ASSERT(js_CodeSpec[op2].ndefs == js_CodeSpec[op2].nuses + 1);
        regs.sp[-1] = rval;
        assertSameCompartment(cx, regs.sp[-1]);
    }
    len = js_CodeSpec[op2].length;
    DO_NEXT_OP(len);
}

BEGIN_CASE(JSOP_HOLE)
    PUSH_HOLE();
END_CASE(JSOP_HOLE)

BEGIN_CASE(JSOP_NEWINIT)
{
    uint8_t i = GET_UINT8(regs.pc);
    JS_ASSERT(i == JSProto_Array || i == JSProto_Object);

    JSObject *obj;
    if (i == JSProto_Array) {
        obj = NewDenseEmptyArray(cx);
    } else {
        gc::AllocKind kind = GuessObjectGCKind(0);
        obj = NewBuiltinClassInstance(cx, &ObjectClass, kind);
    }
    if (!obj)
        goto error;

    TypeObject *type = TypeScript::InitObject(cx, script, regs.pc, (JSProtoKey) i);
    if (!type)
        goto error;
    obj->setType(type);

    PUSH_OBJECT(*obj);
    CHECK_INTERRUPT_HANDLER();
}
END_CASE(JSOP_NEWINIT)

BEGIN_CASE(JSOP_NEWARRAY)
{
    unsigned count = GET_UINT24(regs.pc);
    JSObject *obj = NewDenseAllocatedArray(cx, count);
    if (!obj)
        goto error;

    TypeObject *type = TypeScript::InitObject(cx, script, regs.pc, JSProto_Array);
    if (!type)
        goto error;
    obj->setType(type);

    PUSH_OBJECT(*obj);
    CHECK_INTERRUPT_HANDLER();
}
END_CASE(JSOP_NEWARRAY)

BEGIN_CASE(JSOP_NEWOBJECT)
{
    JSObject *baseobj = script->getObject(GET_UINT32_INDEX(regs.pc));

    TypeObject *type = TypeScript::InitObject(cx, script, regs.pc, JSProto_Object);
    if (!type)
        goto error;

    JSObject *obj = CopyInitializerObject(cx, baseobj, type);
    if (!obj)
        goto error;

    PUSH_OBJECT(*obj);
    CHECK_INTERRUPT_HANDLER();
}
END_CASE(JSOP_NEWOBJECT)

BEGIN_CASE(JSOP_ENDINIT)
{
    /* FIXME remove JSOP_ENDINIT bug 588522 */
    JS_ASSERT(regs.sp - regs.fp()->base() >= 1);
    JS_ASSERT(regs.sp[-1].isObject());
}
END_CASE(JSOP_ENDINIT)

BEGIN_CASE(JSOP_INITPROP)
BEGIN_CASE(JSOP_INITMETHOD)
{
    /* Load the property's initial value into rval. */
    JS_ASSERT(regs.sp - regs.fp()->base() >= 2);
    Value rval = regs.sp[-1];

    /* Load the object being initialized into lval/obj. */
    JSObject *obj = &regs.sp[-2].toObject();
    JS_ASSERT(obj->isObject());

    JSAtom *atom;
    LOAD_ATOM(0, atom);
    jsid id = ATOM_TO_JSID(atom);

    unsigned defineHow = (op == JSOP_INITMETHOD) ? DNP_SET_METHOD : 0;
    if (JS_UNLIKELY(atom == cx->runtime->atomState.protoAtom)
        ? !js_SetPropertyHelper(cx, obj, id, defineHow, &rval, script->strictModeCode)
        : !DefineNativeProperty(cx, obj, id, rval, NULL, NULL,
                                JSPROP_ENUMERATE, 0, 0, defineHow)) {
        goto error;
    }

    regs.sp--;
}
END_CASE(JSOP_INITPROP);

BEGIN_CASE(JSOP_INITELEM)
{
    /* Pop the element's value into rval. */
    JS_ASSERT(regs.sp - regs.fp()->base() >= 3);
    const Value &rref = regs.sp[-1];

    /* Find the object being initialized at top of stack. */
    const Value &lref = regs.sp[-3];
    JS_ASSERT(lref.isObject());
    JSObject *obj = &lref.toObject();

    /* Fetch id now that we have obj. */
    jsid id;
    FETCH_ELEMENT_ID(obj, -2, id);

    /*
     * If rref is a hole, do not call JSObject::defineProperty. In this case,
     * obj must be an array, so if the current op is the last element
     * initialiser, set the array length to one greater than id.
     */
    if (rref.isMagic(JS_ARRAY_HOLE)) {
        JS_ASSERT(obj->isArray());
        JS_ASSERT(JSID_IS_INT(id));
        JS_ASSERT(uint32_t(JSID_TO_INT(id)) < StackSpace::ARGS_LENGTH_MAX);
        if (JSOp(regs.pc[JSOP_INITELEM_LENGTH]) == JSOP_ENDINIT &&
            !js_SetLengthProperty(cx, obj, (uint32_t) (JSID_TO_INT(id) + 1))) {
            goto error;
        }
    } else {
        if (!obj->defineGeneric(cx, id, rref, NULL, NULL, JSPROP_ENUMERATE))
            goto error;
    }
    regs.sp -= 2;
}
END_CASE(JSOP_INITELEM)

{
BEGIN_CASE(JSOP_GOSUB)
    PUSH_BOOLEAN(false);
    int32_t i = (regs.pc - script->code) + JSOP_GOSUB_LENGTH;
    len = GET_JUMP_OFFSET(regs.pc);
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
        cx->setPendingException(rval);
        goto error;
    }
    JS_ASSERT(rval.isInt32());
    len = rval.toInt32();
    regs.pc = script->code;
END_VARLEN_CASE
}

BEGIN_CASE(JSOP_EXCEPTION)
    PUSH_COPY(cx->getPendingException());
    cx->clearPendingException();
    CHECK_BRANCH();
END_CASE(JSOP_EXCEPTION)

BEGIN_CASE(JSOP_FINALLY)
    CHECK_BRANCH();
END_CASE(JSOP_FINALLY)

BEGIN_CASE(JSOP_THROWING)
{
    JS_ASSERT(!cx->isExceptionPending());
    Value v;
    POP_COPY_TO(v);
    cx->setPendingException(v);
}
END_CASE(JSOP_THROWING)

BEGIN_CASE(JSOP_THROW)
{
    JS_ASSERT(!cx->isExceptionPending());
    CHECK_BRANCH();
    Value v;
    POP_COPY_TO(v);
    cx->setPendingException(v);
    /* let the code at error try to catch the exception. */
    goto error;
}
BEGIN_CASE(JSOP_SETLOCALPOP)
    /*
     * The stack must have a block with at least one local slot below the
     * exception object.
     */
    JS_ASSERT((size_t) (regs.sp - regs.fp()->base()) >= 2);
    POP_COPY_TO(regs.fp()->localSlot(GET_UINT16(regs.pc)));
END_CASE(JSOP_SETLOCALPOP)

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

BEGIN_CASE(JSOP_DEBUGGER)
{
    JSTrapStatus st = JSTRAP_CONTINUE;
    Value rval;
    if (JSDebuggerHandler handler = cx->runtime->debugHooks.debuggerHandler)
        st = handler(cx, script, regs.pc, &rval, cx->runtime->debugHooks.debuggerHandlerData);
    if (st == JSTRAP_CONTINUE)
        st = Debugger::onDebuggerStatement(cx, &rval);
    switch (st) {
      case JSTRAP_ERROR:
        goto error;
      case JSTRAP_CONTINUE:
        break;
      case JSTRAP_RETURN:
        regs.fp()->setReturnValue(rval);
        interpReturnOK = true;
        goto forced_return;
      case JSTRAP_THROW:
        cx->setPendingException(rval);
        goto error;
      default:;
    }
    CHECK_INTERRUPT_HANDLER();
}
END_CASE(JSOP_DEBUGGER)

#if JS_HAS_XML_SUPPORT
BEGIN_CASE(JSOP_DEFXMLNS)
{
    JS_ASSERT(!script->strictModeCode);

    if (!js_SetDefaultXMLNamespace(cx, regs.sp[-1]))
        goto error;
    regs.sp--;
}
END_CASE(JSOP_DEFXMLNS)

BEGIN_CASE(JSOP_ANYNAME)
{
    JS_ASSERT(!script->strictModeCode);

    jsid id;
    if (!js_GetAnyName(cx, &id))
        goto error;
    PUSH_COPY(IdToValue(id));
}
END_CASE(JSOP_ANYNAME)

BEGIN_CASE(JSOP_QNAMEPART)
{
    /*
     * We do not JS_ASSERT(!script->strictModeCode) here because JSOP_QNAMEPART
     * is used for __proto__ and (in contexts where we favor JSOP_*ELEM instead
     * of JSOP_*PROP) obj.prop compiled as obj['prop'].
     */

    JSAtom *atom;
    LOAD_ATOM(0, atom);
    PUSH_STRING(atom);
}
END_CASE(JSOP_QNAMEPART)

BEGIN_CASE(JSOP_QNAMECONST)
{
    JS_ASSERT(!script->strictModeCode);

    JSAtom *atom;
    LOAD_ATOM(0, atom);
    Value rval = StringValue(atom);
    Value lval = regs.sp[-1];
    JSObject *obj = js_ConstructXMLQNameObject(cx, lval, rval);
    if (!obj)
        goto error;
    regs.sp[-1].setObject(*obj);
}
END_CASE(JSOP_QNAMECONST)

BEGIN_CASE(JSOP_QNAME)
{
    JS_ASSERT(!script->strictModeCode);

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
    JS_ASSERT(!script->strictModeCode);

    Value rval;
    rval = regs.sp[-1];
    if (!js_ToAttributeName(cx, &rval))
        goto error;
    regs.sp[-1] = rval;
}
END_CASE(JSOP_TOATTRNAME)

BEGIN_CASE(JSOP_TOATTRVAL)
{
    JS_ASSERT(!script->strictModeCode);

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
    JS_ASSERT(!script->strictModeCode);

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
    JS_ASSERT(!script->strictModeCode);

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
    JS_ASSERT(!script->strictModeCode);

    JSObject *obj = &regs.sp[-3].toObject();
    Value rval = regs.sp[-1];
    jsid id;
    FETCH_ELEMENT_ID(obj, -2, id);
    if (!obj->setGeneric(cx, id, &rval, script->strictModeCode))
        goto error;
    rval = regs.sp[-1];
    regs.sp -= 2;
    regs.sp[-1] = rval;
}
END_CASE(JSOP_SETXMLNAME)

BEGIN_CASE(JSOP_CALLXMLNAME)
BEGIN_CASE(JSOP_XMLNAME)
{
    JS_ASSERT(!script->strictModeCode);

    Value lval = regs.sp[-1];
    JSObject *obj;
    jsid id;
    if (!js_FindXMLProperty(cx, lval, &obj, &id))
        goto error;
    Value rval;
    if (!obj->getGeneric(cx, id, &rval))
        goto error;
    regs.sp[-1] = rval;
    if (op == JSOP_CALLXMLNAME) {
        Value v;
        if (!ComputeImplicitThis(cx, obj, &v))
            goto error;
        PUSH_COPY(v);
    }
}
END_CASE(JSOP_XMLNAME)

BEGIN_CASE(JSOP_DESCENDANTS)
BEGIN_CASE(JSOP_DELDESC)
{
    JS_ASSERT(!script->strictModeCode);

    JSObject *obj;
    FETCH_OBJECT(cx, -2, obj);
    jsval rval = regs.sp[-1];
    if (!js_GetXMLDescendants(cx, obj, rval, &rval))
        goto error;

    if (op == JSOP_DELDESC) {
        regs.sp[-1] = rval;   /* set local root */
        if (!js_DeleteXMLListElements(cx, JSVAL_TO_OBJECT(rval)))
            goto error;
        rval = JSVAL_TRUE;                  /* always succeed */
    }

    regs.sp--;
    regs.sp[-1] = rval;
}
END_CASE(JSOP_DESCENDANTS)

BEGIN_CASE(JSOP_FILTER)
{
    JS_ASSERT(!script->strictModeCode);

    /*
     * We push the hole value before jumping to [enditer] so we can detect the
     * first iteration and direct js_StepXMLListFilter to initialize filter's
     * state.
     */
    PUSH_HOLE();
    len = GET_JUMP_OFFSET(regs.pc);
    JS_ASSERT(len > 0);
}
END_VARLEN_CASE

BEGIN_CASE(JSOP_ENDFILTER)
{
    JS_ASSERT(!script->strictModeCode);

    bool cond = !regs.sp[-1].isMagic();
    if (cond) {
        /* Exit the "with" block left from the previous iteration. */
        LeaveWith(cx);
    }
    if (!js_StepXMLListFilter(cx, cond))
        goto error;
    if (!regs.sp[-1].isNull()) {
        /*
         * Decrease sp after EnterWith returns as we use sp[-1] there to root
         * temporaries.
         */
        JS_ASSERT(IsXML(regs.sp[-1]));
        if (!EnterWith(cx, -2))
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
    JS_ASSERT(!script->strictModeCode);

    Value rval = regs.sp[-1];
    JSObject *obj = js_ValueToXMLObject(cx, rval);
    if (!obj)
        goto error;
    regs.sp[-1].setObject(*obj);
}
END_CASE(JSOP_TOXML)

BEGIN_CASE(JSOP_TOXMLLIST)
{
    JS_ASSERT(!script->strictModeCode);

    Value rval = regs.sp[-1];
    JSObject *obj = js_ValueToXMLListObject(cx, rval);
    if (!obj)
        goto error;
    regs.sp[-1].setObject(*obj);
}
END_CASE(JSOP_TOXMLLIST)

BEGIN_CASE(JSOP_XMLTAGEXPR)
{
    JS_ASSERT(!script->strictModeCode);

    Value rval = regs.sp[-1];
    JSString *str = ToString(cx, rval);
    if (!str)
        goto error;
    regs.sp[-1].setString(str);
}
END_CASE(JSOP_XMLTAGEXPR)

BEGIN_CASE(JSOP_XMLELTEXPR)
{
    JS_ASSERT(!script->strictModeCode);

    Value rval = regs.sp[-1];
    JSString *str;
    if (IsXML(rval)) {
        str = js_ValueToXMLString(cx, rval);
    } else {
        str = ToString(cx, rval);
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
    JS_ASSERT(!script->strictModeCode);

    JSAtom *atom = script->getAtom(GET_UINT32_INDEX(regs.pc));
    JSObject *obj = js_NewXMLSpecialObject(cx, JSXML_CLASS_TEXT, NULL, atom);
    if (!obj)
        goto error;
    PUSH_OBJECT(*obj);
}
END_CASE(JSOP_XMLCDATA)

BEGIN_CASE(JSOP_XMLCOMMENT)
{
    JS_ASSERT(!script->strictModeCode);

    JSAtom *atom = script->getAtom(GET_UINT32_INDEX(regs.pc));
    JSObject *obj = js_NewXMLSpecialObject(cx, JSXML_CLASS_COMMENT, NULL, atom);
    if (!obj)
        goto error;
    PUSH_OBJECT(*obj);
}
END_CASE(JSOP_XMLCOMMENT)

BEGIN_CASE(JSOP_XMLPI)
{
    JS_ASSERT(!script->strictModeCode);

    JSAtom *atom = script->getAtom(GET_UINT32_INDEX(regs.pc));
    Value rval = regs.sp[-1];
    JSString *str2 = rval.toString();
    JSObject *obj = js_NewXMLSpecialObject(cx, JSXML_CLASS_PROCESSING_INSTRUCTION, atom, str2);
    if (!obj)
        goto error;
    regs.sp[-1].setObject(*obj);
}
END_CASE(JSOP_XMLPI)

BEGIN_CASE(JSOP_GETFUNNS)
{
    JS_ASSERT(!script->strictModeCode);

    Value rval;
    if (!cx->fp()->scopeChain().global().getFunctionNamespace(cx, &rval))
        goto error;
    PUSH_COPY(rval);
}
END_CASE(JSOP_GETFUNNS)
#endif /* JS_HAS_XML_SUPPORT */

BEGIN_CASE(JSOP_ENTERBLOCK)
BEGIN_CASE(JSOP_ENTERLET0)
BEGIN_CASE(JSOP_ENTERLET1)
{
    StaticBlockObject &blockObj = script->getObject(GET_UINT32_INDEX(regs.pc))->asStaticBlock();
    JS_ASSERT(regs.fp()->maybeBlockChain() == blockObj.enclosingBlock());

    if (op == JSOP_ENTERBLOCK) {
        JS_ASSERT(regs.fp()->base() + blockObj.stackDepth() == regs.sp);
        Value *vp = regs.sp + blockObj.slotCount();
        JS_ASSERT(regs.sp < vp);
        JS_ASSERT(vp <= regs.fp()->slots() + script->nslots);
        SetValueRangeToUndefined(regs.sp, vp);
        regs.sp = vp;
    } else if (op == JSOP_ENTERLET0) {
        JS_ASSERT(regs.fp()->base() + blockObj.stackDepth() + blockObj.slotCount()
                  == regs.sp);
    } else if (op == JSOP_ENTERLET1) {
        JS_ASSERT(regs.fp()->base() + blockObj.stackDepth() + blockObj.slotCount()
                  == regs.sp - 1);
    }

#ifdef DEBUG
    JS_ASSERT(regs.fp()->maybeBlockChain() == blockObj.enclosingBlock());

    /*
     * The young end of fp->scopeChain may omit blocks if we haven't closed
     * over them, but if there are any closure blocks on fp->scopeChain, they'd
     * better be (clones of) ancestors of the block we're entering now;
     * anything else we should have popped off fp->scopeChain when we left its
     * static scope.
     */
    JSObject *obj2 = &regs.fp()->scopeChain();
    while (obj2->isWith())
        obj2 = &obj2->asWith().enclosingScope();
    if (obj2->isBlock() &&
        obj2->getPrivate() == js_FloatingFrameIfGenerator(cx, regs.fp()))
    {
        StaticBlockObject &youngestProto = obj2->asClonedBlock().staticBlock();
        StaticBlockObject *parent = &blockObj;
        while ((parent = parent->enclosingBlock()) != &youngestProto)
            JS_ASSERT(parent);
    }
#endif

    regs.fp()->setBlockChain(&blockObj);
}
END_CASE(JSOP_ENTERBLOCK)

BEGIN_CASE(JSOP_LEAVEBLOCK)
BEGIN_CASE(JSOP_LEAVEFORLETIN)
BEGIN_CASE(JSOP_LEAVEBLOCKEXPR)
{
    StaticBlockObject &blockObj = regs.fp()->blockChain();
    JS_ASSERT(blockObj.stackDepth() <= StackDepth(script));

    /*
     * If we're about to leave the dynamic scope of a block that has been
     * cloned onto fp->scopeChain, clear its private data, move its locals from
     * the stack into the clone, and pop it off the chain.
     */
    JSObject &scope = regs.fp()->scopeChain();
    if (scope.getProto() == &blockObj)
        scope.asClonedBlock().put(cx);

    regs.fp()->setBlockChain(blockObj.enclosingBlock());

    if (op == JSOP_LEAVEBLOCK) {
        /* Pop the block's slots. */
        regs.sp -= GET_UINT16(regs.pc);
        JS_ASSERT(regs.fp()->base() + blockObj.stackDepth() == regs.sp);
    } else if (op == JSOP_LEAVEBLOCKEXPR) {
        /* Pop the block's slots maintaining the topmost expr. */
        Value *vp = &regs.sp[-1];
        regs.sp -= GET_UINT16(regs.pc);
        JS_ASSERT(regs.fp()->base() + blockObj.stackDepth() == regs.sp - 1);
        regs.sp[-1] = *vp;
    } else {
        /* Another op will pop; nothing to do here. */
        len = JSOP_LEAVEFORLETIN_LENGTH;
        DO_NEXT_OP(len);
    }
}
END_CASE(JSOP_LEAVEBLOCK)

#if JS_HAS_GENERATORS
BEGIN_CASE(JSOP_GENERATOR)
{
    JS_ASSERT(!cx->isExceptionPending());
    regs.pc += JSOP_GENERATOR_LENGTH;
    JSObject *obj = js_NewGenerator(cx);
    if (!obj)
        goto error;
    JS_ASSERT(!regs.fp()->hasCallObj() && !regs.fp()->hasArgsObj());
    regs.fp()->setReturnValue(ObjectValue(*obj));
    interpReturnOK = true;
    if (entryFrame != regs.fp())
        goto inline_return;
    goto exit;
}

BEGIN_CASE(JSOP_YIELD)
    JS_ASSERT(!cx->isExceptionPending());
    JS_ASSERT(regs.fp()->isNonEvalFunctionFrame());
    if (cx->generatorFor(regs.fp())->state == JSGEN_CLOSING) {
        js_ReportValueError(cx, JSMSG_BAD_GENERATOR_YIELD,
                            JSDVG_SEARCH_STACK, argv[-2], NULL);
        goto error;
    }
    regs.fp()->setReturnValue(regs.sp[-1]);
    regs.fp()->setYielding();
    regs.pc += JSOP_YIELD_LENGTH;
    interpReturnOK = true;
    goto exit;

BEGIN_CASE(JSOP_ARRAYPUSH)
{
    uint32_t slot = GET_UINT16(regs.pc);
    JS_ASSERT(script->nfixed <= slot);
    JS_ASSERT(slot < script->nslots);
    JSObject *obj = &regs.fp()->slots()[slot].toObject();
    if (!js_NewbornArrayPush(cx, obj, regs.sp[-1]))
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
    JS_ASSERT(&cx->regs() == &regs);
    JS_ASSERT(uint32_t(regs.pc - script->code) < script->length);

    if (!cx->isExceptionPending()) {
        /* This is an error, not a catchable exception, quit the frame ASAP. */
        interpReturnOK = false;
    } else {
        JSThrowHook handler;
        JSTryNote *tn, *tnlimit;
        uint32_t offset;

        /* Restore atoms local in case we will resume. */
        atoms = script->atoms;

        /* Call debugger throw hook if set. */
        if (cx->runtime->debugHooks.throwHook || !cx->compartment->getDebuggees().empty()) {
            Value rval;
            JSTrapStatus st = Debugger::onExceptionUnwind(cx, &rval);
            if (st == JSTRAP_CONTINUE) {
                handler = cx->runtime->debugHooks.throwHook;
                if (handler)
                    st = handler(cx, script, regs.pc, &rval, cx->runtime->debugHooks.throwHookData);
            }

            switch (st) {
              case JSTRAP_ERROR:
                cx->clearPendingException();
                goto error;
              case JSTRAP_RETURN:
                cx->clearPendingException();
                regs.fp()->setReturnValue(rval);
                interpReturnOK = true;
                goto forced_return;
              case JSTRAP_THROW:
                cx->setPendingException(rval);
              case JSTRAP_CONTINUE:
              default:;
            }
            CHECK_INTERRUPT_HANDLER();
        }

        /*
         * Look for a try block in script that can catch this exception.
         */
        if (!JSScript::isValidOffset(script->trynotesOffset))
            goto no_catch;

        offset = (uint32_t)(regs.pc - script->main());
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
            if (tn->stackDepth > regs.sp - regs.fp()->base())
                continue;

            /*
             * Set pc to the first bytecode after the the try note to point
             * to the beginning of catch or finally or to [enditer] closing
             * the for-in loop.
             */
            regs.pc = (script)->main() + tn->start + tn->length;

            UnwindScope(cx, tn->stackDepth);
            regs.sp = regs.fp()->base() + tn->stackDepth;

            switch (tn->kind) {
              case JSTRY_CATCH:
                  JS_ASSERT(*regs.pc == JSOP_ENTERBLOCK);

#if JS_HAS_GENERATORS
                /* Catch cannot intercept the closing of a generator. */
                  if (JS_UNLIKELY(cx->getPendingException().isMagic(JS_GENERATOR_CLOSING)))
                    break;
#endif

                /*
                 * Don't clear exceptions to save cx->exception from GC
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
                PUSH_COPY(cx->getPendingException());
                cx->clearPendingException();
                len = 0;
                DO_NEXT_OP(len);

              case JSTRY_ITER: {
                /* This is similar to JSOP_ENDITER in the interpreter loop. */
                JS_ASSERT(JSOp(*regs.pc) == JSOP_ENDITER);
                bool ok = UnwindIteratorForException(cx, &regs.sp[-1].toObject());
                regs.sp -= 1;
                if (!ok)
                    goto error;
              }
           }
        } while (++tn != tnlimit);

      no_catch:
        /*
         * Propagate the exception or error to the caller unless the exception
         * is an asynchronous return from a generator.
         */
        interpReturnOK = false;
#if JS_HAS_GENERATORS
        if (JS_UNLIKELY(cx->isExceptionPending() &&
                        cx->getPendingException().isMagic(JS_GENERATOR_CLOSING))) {
            cx->clearPendingException();
            interpReturnOK = true;
            regs.fp()->clearReturnValue();
        }
#endif
    }

  forced_return:
    UnwindScope(cx, 0);
    regs.sp = regs.fp()->base();

    if (entryFrame != regs.fp())
        goto inline_return;

  exit:
    if (cx->compartment->debugMode())
        interpReturnOK = ScriptDebugEpilogue(cx, regs.fp(), interpReturnOK);
    interpReturnOK = ScriptEpilogueOrGeneratorYield(cx, regs.fp(), interpReturnOK);
    regs.fp()->setFinishedInInterpreter();

    /*
     * At this point we are inevitably leaving an interpreted function or a
     * top-level script, and returning to one of:
     * (a) an "out of line" call made through Invoke;
     * (b) a js_Execute activation;
     * (c) a generator (SendToGenerator, jsiter.c).
     *
     * We must not be in an inline frame. The check above ensures that for the
     * error case and for a normal return, the code jumps directly to parent's
     * frame pc.
     */
    JS_ASSERT(entryFrame == regs.fp());
    if (!regs.fp()->isGeneratorFrame()) {
        JS_ASSERT(!IsActiveWithOrBlock(cx, regs.fp()->scopeChain(), 0));
        JS_ASSERT(!regs.fp()->hasBlockChain());
    }

#ifdef JS_METHODJIT
    /*
     * This path is used when it's guaranteed the method can be finished
     * inside the JIT.
     */
  leave_on_safe_point:
#endif

    gc::MaybeVerifyBarriers(cx, true);
    return interpReturnOK;
}
