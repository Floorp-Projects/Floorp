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
#include "jsarena.h"
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
#include "jsemit.h"
#include "jsscope.h"
#include "jsscript.h"
#include "jsstr.h"
#include "jsstaticcheck.h"
#include "jstracer.h"
#include "jslibmath.h"
#include "jsvector.h"
#ifdef JS_METHODJIT
#include "methodjit/MethodJIT.h"
#include "methodjit/MethodJIT-inl.h"
#include "methodjit/Logging.h"
#endif
#include "ion/Ion.h"
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

/*
 * This computes the blockChain by iterating through the bytecode
 * of the current script until it reaches the PC. Each time it sees
 * an ENTERBLOCK or LEAVEBLOCK instruction, it records the new
 * blockChain. A faster variant of this function that doesn't
 * require bytecode scanning appears below.
 */
JSObject *
js::GetBlockChain(JSContext *cx, StackFrame *fp)
{
    if (!fp->isScriptFrame())
        return NULL;

    /* Assume that imacros don't affect blockChain */
    jsbytecode *target = fp->hasImacropc() ? fp->imacropc() : fp->pcQuadratic(cx->stack);

    JSScript *script = fp->script();
    jsbytecode *start = script->code;

    /*
     * If the debugger asks for the scope chain at a pc where we are about to
     * fix it up, advance target past the fixup. See bug 672804.
     */
    JSOp op = js_GetOpcode(cx, script, target);
    while (op == JSOP_NOP || op == JSOP_INDEXBASE || op == JSOP_INDEXBASE1 ||
           op == JSOP_INDEXBASE2 || op == JSOP_INDEXBASE3 ||
           op == JSOP_BLOCKCHAIN || op == JSOP_NULLBLOCKCHAIN)
    {
        target += js_CodeSpec[op].length;
        op = js_GetOpcode(cx, script, target);
    }
    JS_ASSERT(target >= start && target < start + script->length);

    JSObject *blockChain = NULL;
    uintN indexBase = 0;
    ptrdiff_t oplen;
    for (jsbytecode *pc = start; pc < target; pc += oplen) {
        JSOp op = js_GetOpcode(cx, script, pc);
        const JSCodeSpec *cs = &js_CodeSpec[op];
        oplen = cs->length;
        if (oplen < 0)
            oplen = js_GetVariableBytecodeLength(pc);

        if (op == JSOP_INDEXBASE)
            indexBase = GET_INDEXBASE(pc);
        else if (op == JSOP_INDEXBASE1 || op == JSOP_INDEXBASE2 || op == JSOP_INDEXBASE3)
            indexBase = (op - JSOP_INDEXBASE1 + 1) << 16;
        else if (op == JSOP_RESETBASE || op == JSOP_RESETBASE0)
            indexBase = 0;
        else if (op == JSOP_ENTERBLOCK)
            blockChain = script->getObject(indexBase + GET_INDEX(pc));
        else if (op == JSOP_LEAVEBLOCK || op == JSOP_LEAVEBLOCKEXPR)
            blockChain = blockChain->getParent();
        else if (op == JSOP_BLOCKCHAIN)
            blockChain = script->getObject(indexBase + GET_INDEX(pc));
        else if (op == JSOP_NULLBLOCKCHAIN)
            blockChain = NULL;
    }

    return blockChain;
}

/*
 * This function computes the current blockChain, but only in
 * the special case where a BLOCKCHAIN or NULLBLOCKCHAIN
 * instruction appears immediately after the current PC.
 * We ensure this happens for a few important ops like DEFFUN.
 * |oplen| is the length of opcode at the current PC.
 */
JSObject *
js::GetBlockChainFast(JSContext *cx, StackFrame *fp, JSOp op, size_t oplen)
{
    /* Assume that we're in a script frame. */
    jsbytecode *pc = fp->pcQuadratic(cx->stack);
    JS_ASSERT(js_GetOpcode(cx, fp->script(), pc) == op);

    pc += oplen;
    op = JSOp(*pc);

    /* The fast paths assume no JSOP_RESETBASE/INDEXBASE or JSOP_TRAP noise. */
    if (op == JSOP_NULLBLOCKCHAIN)
        return NULL;
    if (op == JSOP_BLOCKCHAIN)
        return fp->script()->getObject(GET_INDEX(pc));

    return GetBlockChain(cx, fp);
}

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
static JSObject *
GetScopeChainFull(JSContext *cx, StackFrame *fp, JSObject *blockChain)
{
    JSObject *sharedBlock = blockChain;

    if (!sharedBlock) {
        /*
         * Don't force a call object for a lightweight function call, but do
         * insist that there is a call object for a heavyweight function call.
         */
        JS_ASSERT_IF(fp->isNonEvalFunctionFrame() && fp->fun()->isHeavyweight(),
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
    if (fp->isNonEvalFunctionFrame() && !fp->hasCallObj()) {
        JS_ASSERT_IF(fp->scopeChain().isClonedBlock(), fp->scopeChain().getPrivate() != fp);
        if (!CreateFunCallObject(cx, fp))
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

JSObject *
js::GetScopeChain(JSContext *cx, StackFrame *fp)
{
    return GetScopeChainFull(cx, fp, GetBlockChain(cx, fp));
}

JSObject *
js::GetScopeChainFast(JSContext *cx, StackFrame *fp, JSOp op, size_t oplen)
{
    return GetScopeChainFull(cx, fp, GetBlockChainFast(cx, fp, op, oplen));
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

void
js::ReportIncompatibleMethod(JSContext *cx, Value *vp, Class *clasp)
{
    Value &thisv = vp[1];

#ifdef DEBUG
    if (thisv.isObject()) {
        JS_ASSERT(thisv.toObject().getClass() != clasp);
    } else if (thisv.isString()) {
        JS_ASSERT(clasp != &StringClass);
    } else if (thisv.isNumber()) {
        JS_ASSERT(clasp != &NumberClass);
    } else if (thisv.isBoolean()) {
        JS_ASSERT(clasp != &BooleanClass);
    } else {
        JS_ASSERT(thisv.isUndefined() || thisv.isNull());
    }
#endif

    if (JSFunction *fun = js_ValueToFunction(cx, &vp[0], 0)) {
        JSAutoByteString funNameBytes;
        if (const char *funName = GetFunctionNameBytes(cx, fun, &funNameBytes)) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_INCOMPATIBLE_PROTO,
                                 clasp->name, funName, InformalValueTypeName(thisv));
        }
    }
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
    JSFunction *fun = call.callee().isFunction() ? call.callee().getFunctionPrivate() : NULL;
    JS_ASSERT_IF(fun && fun->isInterpreted(), !fun->inStrictMode());
#endif

    if (thisv.isNullOrUndefined()) {
        JSObject *thisp = call.callee().getGlobal()->thisObject(cx);
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

const uint32 JSSLOT_FOUND_FUNCTION  = 0;
const uint32 JSSLOT_SAVED_ID        = 1;

static void
no_such_method_trace(JSTracer *trc, JSObject *obj)
{
    gc::MarkValue(trc, obj->getSlot(JSSLOT_FOUND_FUNCTION), "found function");
    gc::MarkValue(trc, obj->getSlot(JSSLOT_SAVED_ID), "saved id");
}

Class js_NoSuchMethodClass = {
    "NoSuchMethod",
    JSCLASS_HAS_RESERVED_SLOTS(2) | JSCLASS_IS_ANONYMOUS,
    PropertyStub,         /* addProperty */
    PropertyStub,         /* delProperty */
    PropertyStub,         /* getProperty */
    StrictPropertyStub,   /* setProperty */
    EnumerateStub,
    ResolveStub,
    ConvertStub,
    NULL,                 /* finalize */
    NULL,                 /* reserved0 */
    NULL,                 /* checkAccess */
    NULL,                 /* call */
    NULL,                 /* construct */
    NULL,                 /* XDR */
    NULL,                 /* hasInstance */
    no_such_method_trace  /* trace */
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
js::OnUnknownMethod(JSContext *cx, Value *vp)
{
    JS_ASSERT(!vp[1].isPrimitive());

    JSObject *obj = &vp[1].toObject();
    jsid id = ATOM_TO_JSID(cx->runtime->atomState.noSuchMethodAtom);
    AutoValueRooter tvr(cx);
    if (!js_GetMethod(cx, obj, id, JSGET_NO_METHOD_BARRIER, tvr.addr()))
        return false;
    TypeScript::MonitorUnknown(cx, cx->fp()->script(), cx->regs().pc);

    if (tvr.value().isPrimitive()) {
        vp[0] = tvr.value();
    } else {
#if JS_HAS_XML_SUPPORT
        /* Extract the function name from function::name qname. */
        if (vp[0].isObject()) {
            obj = &vp[0].toObject();
            if (js_GetLocalNameFromFunctionQName(obj, &id, cx))
                vp[0] = IdToValue(id);
        }
#endif
        obj = js_NewGCObject(cx, FINALIZE_OBJECT2);
        if (!obj)
            return false;

        obj->init(cx, &js_NoSuchMethodClass, &emptyTypeObject, NULL, NULL, false);
        obj->setSharedNonNativeMap();
        obj->setSlot(JSSLOT_FOUND_FUNCTION, tvr.value());
        obj->setSlot(JSSLOT_SAVED_ID, vp[0]);
        vp[0].setObject(*obj);
    }
    return true;
}

static JS_REQUIRES_STACK JSBool
NoSuchMethod(JSContext *cx, uintN argc, Value *vp)
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

JS_REQUIRES_STACK bool
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
        if (fp->scopeChain().getGlobal()->isCleared()) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CLEARED_SCOPE);
            return false;
        }
    }

#ifdef JS_ION
    if (ion::IsEnabled()) {
        ion::MethodStatus status = ion::Compile(cx, script, fp);
        if (status == ion::Method_Compiled)
            return ion::Cannon(cx, fp);
    }
#endif

#ifdef JS_METHODJIT
    mjit::CompileStatus status;
    status = mjit::CanMethodJIT(cx, script, fp->isConstructing(),
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
js::InvokeKernel(JSContext *cx, const CallArgs &argsRef, MaybeConstruct construct)
{
    CallArgs args = argsRef;
    JS_ASSERT(args.argc() <= StackSpace::ARGS_LENGTH_MAX);

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
            return NoSuchMethod(cx, args.argc(), args.base());
#endif
        JS_ASSERT_IF(construct, !clasp->construct);
        if (!clasp->call) {
            js_ReportIsNotFunction(cx, &args.calleev(), ToReportFlags(initial));
            return false;
        }
        return CallJSNative(cx, clasp->call, args);
    }

    /* Invoke native functions. */
    JSFunction *fun = callee.getFunctionPrivate();
    JS_ASSERT_IF(construct, !fun->isConstructor());
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
    JSBool ok;
    {
        AutoPreserveEnumerators preserve(cx);
        ok = RunScript(cx, fun->script(), fp);
    }

    args.rval() = fp->returnValue();
    JS_ASSERT_IF(ok && construct, !args.rval().isPrimitive());
    return ok;
}

bool
js::Invoke(JSContext *cx, const Value &thisv, const Value &fval, uintN argc, Value *argv,
           Value *rval)
{
    InvokeArgsGuard args;
    if (!cx->stack.pushInvokeArgs(cx, argc, &args))
        return false;

    args.calleev() = fval;
    args.thisv() = thisv;
    memcpy(args.argv(), argv, argc * sizeof(Value));

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
js::InvokeConstructor(JSContext *cx, const Value &fval, uintN argc, Value *argv, Value *rval)
{
    InvokeArgsGuard args;
    if (!cx->stack.pushInvokeArgs(cx, argc, &args))
        return false;

    args.calleev() = fval;
    args.thisv().setMagic(JS_THIS_POISON);
    memcpy(args.argv(), argv, argc * sizeof(Value));

    if (!InvokeConstructor(cx, args))
        return false;

    *rval = args.rval();
    return true;
}

bool
js::InvokeGetterOrSetter(JSContext *cx, JSObject *obj, const Value &fval, uintN argc, Value *argv,
                         Value *rval)
{
    LeaveTrace(cx);

    /*
     * Invoke could result in another try to get or set the same id again, see
     * bug 355497.
     */
    JS_CHECK_RECURSION(cx, return false);

    return Invoke(cx, ObjectValue(*obj), fval, argc, argv, rval);
}

#if JS_HAS_SHARP_VARS
JS_STATIC_ASSERT(SHARP_NSLOTS == 2);

static JS_NEVER_INLINE bool
InitSharpSlots(JSContext *cx, StackFrame *fp)
{
    StackFrame *prev = fp->prev();
    JSScript *script = fp->script();
    JS_ASSERT(script->nfixed >= SHARP_NSLOTS);

    Value *sharps = &fp->slots()[script->nfixed - SHARP_NSLOTS];
    if (!fp->isGlobalFrame() && prev->script()->hasSharps) {
        JS_ASSERT(prev->numFixed() >= SHARP_NSLOTS);
        int base = prev->isNonEvalFunctionFrame()
                   ? prev->fun()->script()->bindings.sharpSlotBase(cx)
                   : prev->numFixed() - SHARP_NSLOTS;
        if (base < 0)
            return false;
        sharps[0] = prev->slots()[base];
        sharps[1] = prev->slots()[base + 1];
    } else {
        sharps[0].setUndefined();
        sharps[1].setUndefined();
    }
    return true;
}
#endif

bool
js::ExecuteKernel(JSContext *cx, JSScript *script, JSObject &scopeChain, const Value &thisv,
                  ExecuteType type, StackFrame *evalInFrame, Value *result)
{
    JS_ASSERT_IF(evalInFrame, type == EXECUTE_DEBUG);

    if (script->isEmpty()) {
        if (result)
            result->setUndefined();
        return true;
    }

    LeaveTrace(cx);

    ExecuteFrameGuard efg;
    if (!cx->stack.pushExecuteFrame(cx, script, thisv, scopeChain, type, evalInFrame, &efg))
        return false;

    /* Give strict mode eval its own fresh lexical environment. */
    StackFrame *fp = efg.fp();
    if (fp->isStrictEvalFrame() && !CreateEvalCallObject(cx, fp))
        return false;

#if JS_HAS_SHARP_VARS
    if (script->hasSharps && !InitSharpSlots(cx, fp))
        return false;
#endif

    Probes::startExecution(cx, script);

    if (!script->ensureRanAnalysis(cx, NULL, &scopeChain))
        return false;

    TypeScript::SetThis(cx, script, fp->thisValue());

    AutoPreserveEnumerators preserve(cx);
    JSBool ok = RunScript(cx, script, fp);
    if (result && ok)
        *result = fp->returnValue();

    if (fp->isStrictEvalFrame())
        js_PutCallObject(fp);

    Probes::stopExecution(cx, script);

    return !!ok;
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
    if (!cx->hasRunOption(JSOPTION_VAROBJFIX))
        scopeChain->makeVarObj();

    /* Use the scope chain as 'this', modulo outerization. */
    JSObject *thisObj = scopeChain->thisObject(cx);
    if (!thisObj)
        return false;
    Value thisv = ObjectValue(*thisObj);

    return ExecuteKernel(cx, script, *scopeChain, thisv, EXECUTE_GLOBAL,
                         NULL /* evalInFrame */, rval);
}

bool
js::CheckRedeclaration(JSContext *cx, JSObject *obj, jsid id, uintN attrs)
{
    JSObject *obj2;
    JSProperty *prop;
    uintN oldAttrs;
    bool isFunction;
    const char *type, *name;

    if (!obj->lookupProperty(cx, id, &obj2, &prop))
        return false;
    if (!prop)
        return true;
    if (obj2->isNative()) {
        oldAttrs = ((Shape *) prop)->attributes();
    } else {
        if (!obj2->getAttributes(cx, id, &oldAttrs))
            return false;
    }

    /* We allow redeclaring some non-readonly properties. */
    if (((oldAttrs | attrs) & JSPROP_READONLY) == 0) {
        /* Allow redeclaration of variables and functions. */
        if (!(attrs & (JSPROP_GETTER | JSPROP_SETTER)))
            return true;

        /*
         * Allow adding a getter only if a property already has a setter
         * but no getter and similarly for adding a setter. That is, we
         * allow only the following transitions:
         *
         *   no-property --> getter --> getter + setter
         *   no-property --> setter --> getter + setter
         */
        if ((~(oldAttrs ^ attrs) & (JSPROP_GETTER | JSPROP_SETTER)) == 0)
            return true;

        /*
         * Allow redeclaration of an impermanent property (in which case
         * anyone could delete it and redefine it, willy-nilly).
         */
        if (!(oldAttrs & JSPROP_PERMANENT))
            return true;
    }

    isFunction = (oldAttrs & (JSPROP_GETTER | JSPROP_SETTER)) != 0;
    if (!isFunction) {
        Value value;
        if (!obj->getProperty(cx, id, &value))
            return JS_FALSE;
        isFunction = IsFunctionObject(value);
    }

    type = (oldAttrs & attrs & JSPROP_GETTER)
           ? js_getter_str
           : (oldAttrs & attrs & JSPROP_SETTER)
           ? js_setter_str
           : (oldAttrs & JSPROP_READONLY)
           ? js_const_str
           : isFunction
           ? js_function_str
           : js_var_str;
    JSAutoByteString bytes;
    name = js_ValueToPrintable(cx, IdToValue(id), &bytes);
    if (!name)
        return false;
    JS_ALWAYS_FALSE(JS_ReportErrorFlagsAndNumber(cx, JSREPORT_ERROR, js_GetErrorMessage, NULL,
                                                 JSMSG_REDECLARED_VAR, type, name));
    return false;
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
js::LooselyEqual(JSContext *cx, const Value &lval, const Value &rval, JSBool *result)
{
#if JS_HAS_XML_SUPPORT
    if (JS_UNLIKELY(lval.isObject() && lval.toObject().isXML()) ||
                    (rval.isObject() && rval.toObject().isXML())) {
        return js_TestXMLEquality(cx, lval, rval, result);
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
            *result = JSDOUBLE_COMPARE(l, ==, r, false);
            return true;
        }

        if (lval.isObject()) {
            JSObject *l = &lval.toObject();
            JSObject *r = &rval.toObject();
            l->assertSpecialEqualitySynced();

            if (EqualityOp eq = l->getClass()->ext.equality) {
                return eq(cx, l, &rval, result);
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
    *result = JSDOUBLE_COMPARE(l, ==, r, false);
    return true;
}

bool
js::StrictlyEqual(JSContext *cx, const Value &lref, const Value &rref, JSBool *equal)
{
    Value lval = lref, rval = rref;
    if (SameType(lval, rval)) {
        if (lval.isString())
            return EqualStrings(cx, lval.toString(), rval.toString(), equal);
        if (lval.isDouble()) {
            *equal = JSDOUBLE_COMPARE(lval.toDouble(), ==, rval.toDouble(), JS_FALSE);
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
        *equal = JSDOUBLE_COMPARE(ld, ==, rd, JS_FALSE);
        return true;
    }
    if (lval.isInt32() && rval.isDouble()) {
        double ld = lval.toInt32();
        double rd = rval.toDouble();
        *equal = JSDOUBLE_COMPARE(ld, ==, rd, JS_FALSE);
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
js::SameValue(JSContext *cx, const Value &v1, const Value &v2, JSBool *same)
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
js::InvokeConstructorKernel(JSContext *cx, const CallArgs &argsRef)
{
    JS_ASSERT(!FunctionClass.construct);
    CallArgs args = argsRef;

    if (args.calleev().isObject()) {
        JSObject *callee = &args.callee();
        Class *clasp = callee->getClass();
        if (clasp == &FunctionClass) {
            JSFunction *fun = callee->getFunctionPrivate();

            if (fun->isConstructor()) {
                args.thisv().setMagicWithObjectOrNullPayload(NULL);
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
        if (clasp->construct) {
            args.thisv().setMagicWithObjectOrNullPayload(NULL);
            return CallJSNativeConstructor(cx, clasp->construct, args);
        }
    }

error:
    js_ReportIsNotFunction(cx, &args.calleev(), JSV2F_CONSTRUCT);
    return false;
}

bool
js::InvokeConstructorWithGivenThis(JSContext *cx, JSObject *thisobj, const Value &fval,
                                   uintN argc, Value *argv, Value *rval)
{
    InvokeArgsGuard args;
    if (!cx->stack.pushInvokeArgs(cx, argc, &args))
        return JS_FALSE;

    args.calleev() = fval;
    /* Initialize args.thisv on all paths below. */
    memcpy(args.argv(), argv, argc * sizeof(Value));

    /* Handle the fast-constructor cases before calling the general case. */
    JSObject &callee = fval.toObject();
    Class *clasp = callee.getClass();
    JSFunction *fun;
    bool ok;
    if (clasp == &FunctionClass && (fun = callee.getFunctionPrivate())->isConstructor()) {
        args.thisv().setMagicWithObjectOrNullPayload(thisobj);
        Probes::calloutBegin(cx, fun);
        ok = CallJSNativeConstructor(cx, fun->u.n.native, args);
        Probes::calloutEnd(cx, fun);
    } else if (clasp->construct) {
        args.thisv().setMagicWithObjectOrNullPayload(thisobj);
        ok = CallJSNativeConstructor(cx, clasp->construct, args);
    } else {
        args.thisv().setObjectOrNull(thisobj);
        ok = Invoke(cx, args, CONSTRUCT);
    }

    *rval = args.rval();
    return ok;
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
EnterWith(JSContext *cx, jsint stackIndex, JSOp op, size_t oplen)
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

    JSObject *parent = GetScopeChainFast(cx, fp, op, oplen);
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

static void
LeaveWith(JSContext *cx)
{
    JSObject *withobj;

    withobj = &cx->fp()->scopeChain();
    JS_ASSERT(withobj->getClass() == &WithClass);
    JS_ASSERT(withobj->getPrivate() == js_FloatingFrameIfGenerator(cx, cx->fp()));
    JS_ASSERT(OBJ_BLOCK_DEPTH(cx, withobj) >= 0);
    withobj->setPrivate(NULL);
    cx->fp()->setScopeChainNoCallObj(*withobj->getParent());
}

bool
js::IsActiveWithOrBlock(JSContext *cx, JSObject &obj, int stackDepth)
{
    return (obj.isWith() || obj.isBlock()) &&
           obj.getPrivate() == js_FloatingFrameIfGenerator(cx, cx->fp()) &&
           OBJ_BLOCK_DEPTH(cx, &obj) >= stackDepth;
}

/*
 * Unwind block and scope chains to match the given depth. The function sets
 * fp->sp on return to stackDepth.
 */
bool
js::UnwindScope(JSContext *cx, jsint stackDepth, JSBool normalUnwind)
{
    JS_ASSERT(stackDepth >= 0);
    JS_ASSERT(cx->fp()->base() + stackDepth <= cx->regs().sp);

    StackFrame *fp = cx->fp();
    for (;;) {
        JSObject &scopeChain = fp->scopeChain();
        if (!IsActiveWithOrBlock(cx, scopeChain, stackDepth))
            break;
        if (scopeChain.isBlock()) {
            /* Don't fail until after we've updated all stacks. */
            normalUnwind &= js_PutBlockObject(cx, normalUnwind);
        } else {
            LeaveWith(cx);
        }
    }

    cx->regs().sp = fp->base() + stackDepth;
    return normalUnwind;
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
js::GetUpvar(JSContext *cx, uintN closureLevel, UpvarCookie cookie)
{
    JS_ASSERT(closureLevel >= cookie.level() && cookie.level() > 0);
    const uintN targetLevel = closureLevel - cookie.level();

    StackFrame *fp = FindUpvarFrame(cx, targetLevel);
    uintN slot = cookie.slot();
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
js::FindUpvarFrame(JSContext *cx, uintN targetLevel)
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
    older = JS_THREAD_DATA(cx)->interpreterFrames;
    JS_THREAD_DATA(cx)->interpreterFrames = this;
}
 
inline InterpreterFrames::~InterpreterFrames()
{
    JS_THREAD_DATA(context)->interpreterFrames = older;
}

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
AssertValidPropertyCacheHit(JSContext *cx, JSScript *script, FrameRegs& regs,
                            ptrdiff_t pcoff, JSObject *start, JSObject *found,
                            PropertyCacheEntry *entry)
{
    uint32 sample = cx->runtime->gcNumber;
    PropertyCacheEntry savedEntry = *entry;

    JSAtom *atom;
    if (pcoff >= 0)
        GET_ATOM_FROM_BYTECODE(script, regs.pc, pcoff, atom);
    else
        atom = cx->runtime->atomState.lengthAtom;

    JSObject *obj, *pobj;
    JSProperty *prop;
    JSBool ok;

    if (JOF_OPMODE(*regs.pc) == JOF_NAME) {
        bool global = js_CodeSpec[*regs.pc].format & JOF_GNAME;
        ok = js_FindProperty(cx, ATOM_TO_JSID(atom), global, &obj, &pobj, &prop);
    } else {
        obj = start;
        ok = js_LookupProperty(cx, obj, ATOM_TO_JSID(atom), &pobj, &prop);
    }
    if (!ok)
        return false;
    if (cx->runtime->gcNumber != sample)
        JS_PROPERTY_CACHE(cx).restore(&savedEntry);
    JS_ASSERT(prop);
    JS_ASSERT(pobj == found);

    const Shape *shape = (Shape *) prop;
    if (entry->vword.isSlot()) {
        JS_ASSERT(entry->vword.toSlot() == shape->slot);
        JS_ASSERT(!shape->isMethod());
    } else if (entry->vword.isShape()) {
        JS_ASSERT(entry->vword.toShape() == shape);
        JS_ASSERT_IF(shape->isMethod(),
                     shape->methodObject() == pobj->nativeGetSlot(shape->slot).toObject());
    } else {
        Value v;
        JS_ASSERT(entry->vword.isFunObj());
        JS_ASSERT(!entry->vword.isNull());
        JS_ASSERT(pobj->brandedOrHasMethodBarrier());
        JS_ASSERT(shape->hasDefaultGetterOrIsMethod());
        JS_ASSERT(pobj->containsSlot(shape->slot));
        v = pobj->nativeGetSlot(shape->slot);
        JS_ASSERT(entry->vword.toFunObj() == v.toObject());

        if (shape->isMethod()) {
            JS_ASSERT(js_CodeSpec[*regs.pc].format & JOF_CALLOP);
            JS_ASSERT(shape->methodObject() == v.toObject());
        }
    }

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
            jsid id = *ni->current();
            if (JSID_IS_ATOM(id)) {
                rval->setString(JSID_TO_STRING(id));
                ni->incCursor();
                return true;
            }
            /* Take the slow path if we have to stringify a numeric property name. */
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
        *regs.pc != JSOP_TRAP &&
        n == analyze::GetBytecodeLength(regs.pc)) {
        TypeScript::CheckBytecode(cx, script, regs.pc, regs.sp);
    }
#endif
}

JS_NEVER_INLINE bool
js::Interpret(JSContext *cx, StackFrame *entryFrame, InterpMode interpMode)
{
#ifdef MOZ_TRACEVIS
    TraceVisStateObj tvso(cx, S_INTERP);
#endif
    JSAutoResolveFlags rf(cx, RESOLVE_INFER);

    JS_ASSERT(!cx->compartment->activeAnalysis);

#define ENABLE_PCCOUNT_INTERRUPTS()     JS_BEGIN_MACRO                        \
                                            if (pcCounts)                     \
                                                ENABLE_INTERRUPTS();          \
                                        JS_END_MACRO

#if JS_THREADED_INTERP
#define CHECK_PCCOUNT_INTERRUPTS() JS_ASSERT_IF(pcCounts, jumpTable == interruptJumpTable)
#else
#define CHECK_PCCOUNT_INTERRUPTS() JS_ASSERT_IF(pcCounts, switchMask == -1)
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

# ifdef JS_TRACER
#  define CHECK_RECORDER()                                                    \
    JS_ASSERT_IF(TRACE_RECORDER(cx), jumpTable == interruptJumpTable)
# else
#  define CHECK_RECORDER()  ((void)0)
# endif

# define DO_OP()            JS_BEGIN_MACRO                                    \
                                CHECK_RECORDER();                             \
                                CHECK_PCCOUNT_INTERRUPTS();                   \
                                JS_EXTENSION_(goto *jumpTable[op]);           \
                            JS_END_MACRO
# define DO_NEXT_OP(n)      JS_BEGIN_MACRO                                    \
                                TypeCheckNextBytecode(cx, script, n, regs);   \
                                op = (JSOp) *(regs.pc += (n));                \
                                DO_OP();                                      \
                            JS_END_MACRO

# define BEGIN_CASE(OP)     L_##OP: CHECK_RECORDER();
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
    typedef GenericInterruptEnabler<intN> InterruptEnabler;
    InterruptEnabler interruptEnabler(&switchMask, -1);

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

#define ENABLE_INTERRUPTS() (interruptEnabler.enableInterrupts())

#define LOAD_ATOM(PCOFF, atom)                                                \
    JS_BEGIN_MACRO                                                            \
        JS_ASSERT(regs.fp()->hasImacropc()                                    \
                  ? atoms == rt->atomState.commonAtomsStart() &&              \
                    GET_INDEX(regs.pc + PCOFF) < js_common_atom_count         \
                  : (size_t)(atoms - script->atoms) <                         \
                    (size_t)(script->natoms - GET_INDEX(regs.pc + PCOFF)));   \
        atom = atoms[GET_INDEX(regs.pc + PCOFF)];                             \
    JS_END_MACRO

#define GET_FULL_INDEX(PCOFF)                                                 \
    (atoms - script->atoms + GET_INDEX(regs.pc + (PCOFF)))

#define LOAD_OBJECT(PCOFF, obj)                                               \
    (obj = script->getObject(GET_FULL_INDEX(PCOFF)))

#define LOAD_FUNCTION(PCOFF)                                                  \
    (fun = script->getFunction(GET_FULL_INDEX(PCOFF)))

#define LOAD_DOUBLE(PCOFF, dbl)                                               \
    (dbl = script->getConst(GET_FULL_INDEX(PCOFF)).toDouble())

#if defined(JS_TRACER) || defined(JS_METHODJIT)
    bool useMethodJIT = false;
#endif

#ifdef JS_METHODJIT

#define RESET_USE_METHODJIT()                                                 \
    JS_BEGIN_MACRO                                                            \
        useMethodJIT = cx->methodJitEnabled &&                                \
            script->getJITStatus(regs.fp()->isConstructing()) != JITScript_Invalid && \
           (interpMode == JSINTERP_NORMAL ||                                  \
            interpMode == JSINTERP_REJOIN ||                                  \
            interpMode == JSINTERP_SKIP_TRAP);                                \
    JS_END_MACRO

#define CHECK_PARTIAL_METHODJIT(status)                                       \
    JS_BEGIN_MACRO                                                            \
        if (status == mjit::Jaeger_Unfinished) {                              \
            op = (JSOp) *regs.pc;                                             \
            RESTORE_INTERP_VARS_CHECK_EXCEPTION();                            \
            DO_OP();                                                          \
        } else if (status == mjit::Jaeger_UnfinishedAtTrap) {                 \
            interpMode = JSINTERP_SKIP_TRAP;                                  \
            JS_ASSERT(JSOp(*regs.pc) == JSOP_TRAP);                           \
            op = JSOP_TRAP;                                                   \
            RESTORE_INTERP_VARS_CHECK_EXCEPTION();                            \
            DO_OP();                                                          \
        }                                                                     \
    JS_END_MACRO

#else

#define RESET_USE_METHODJIT() ((void) 0)

#endif

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

#endif /* !JS_TRACER */

#define RESTORE_INTERP_VARS()                                                 \
    JS_BEGIN_MACRO                                                            \
        SET_SCRIPT(regs.fp()->script());                                      \
        pcCounts = script->pcCounters.get(JSPCCounters::INTERP);              \
        ENABLE_PCCOUNT_INTERRUPTS();                                          \
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
        if (JS_THREAD_DATA(cx)->interruptFlags && !js_HandleExecutionInterrupt(cx)) \
            goto error;                                                       \
    JS_END_MACRO

#if defined(JS_TRACER) && defined(JS_METHODJIT)
# define LEAVE_ON_SAFE_POINT()                                                \
    do {                                                                      \
        JS_ASSERT_IF(leaveOnSafePoint, !TRACE_RECORDER(cx));                  \
        JS_ASSERT_IF(leaveOnSafePoint, !TRACE_PROFILER(cx));                  \
        JS_ASSERT_IF(leaveOnSafePoint, interpMode != JSINTERP_NORMAL);        \
        if (leaveOnSafePoint && !regs.fp()->hasImacropc() &&                  \
            script->maybeNativeCodeForPC(regs.fp()->isConstructing(), regs.pc)) { \
            JS_ASSERT(!TRACE_RECORDER(cx));                                   \
            interpReturnOK = true;                                            \
            goto leave_on_safe_point;                                         \
        }                                                                     \
    } while (0)
#else
# define LEAVE_ON_SAFE_POINT() /* nop */
#endif

#define BRANCH(n)                                                             \
    JS_BEGIN_MACRO                                                            \
        regs.pc += (n);                                                       \
        op = (JSOp) *regs.pc;                                                 \
        if ((n) <= 0)                                                         \
            goto check_backedge;                                              \
        LEAVE_ON_SAFE_POINT();                                                \
        DO_OP();                                                              \
    JS_END_MACRO

#define SET_SCRIPT(s)                                                         \
    JS_BEGIN_MACRO                                                            \
        script = (s);                                                         \
        if (script->stepModeEnabled())                                        \
            ENABLE_INTERRUPTS();                                              \
    JS_END_MACRO

#define CHECK_INTERRUPT_HANDLER()                                             \
    JS_BEGIN_MACRO                                                            \
        if (cx->debugHooks->interruptHook)                                    \
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
    double *pcCounts = script->pcCounters.get(JSPCCounters::INTERP);
    ENABLE_PCCOUNT_INTERRUPTS();
    Value *argv = regs.fp()->maybeFormalArgs();
    CHECK_INTERRUPT_HANDLER();

#if defined(JS_TRACER) && defined(JS_METHODJIT)
    bool leaveOnSafePoint = (interpMode == JSINTERP_SAFEPOINT);
# define CLEAR_LEAVE_ON_TRACE_POINT() ((void) (leaveOnSafePoint = false))
#else
# define CLEAR_LEAVE_ON_TRACE_POINT() ((void) 0)
#endif

    if (!entryFrame)
        entryFrame = regs.fp();

    /*
     * Initialize the index segment register used by LOAD_ATOM and
     * GET_FULL_INDEX macros below. As a register we use a pointer based on
     * the atom map to turn frequently executed LOAD_ATOM into simple array
     * access. For less frequent object and regexp loads we have to recover
     * the segment from atoms pointer first.
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

#ifdef JS_TRACER
    /*
     * The method JIT may have already initiated a recording, in which case
     * there should already be a valid recorder. Otherwise...
     * we cannot reenter the interpreter while recording.
     */
    if (interpMode == JSINTERP_RECORD) {
        JS_ASSERT(TRACE_RECORDER(cx));
        JS_ASSERT(!TRACE_PROFILER(cx));
        ENABLE_INTERRUPTS();
    } else if (interpMode == JSINTERP_PROFILE) {
        ENABLE_INTERRUPTS();
    } else if (TRACE_RECORDER(cx)) {
        AbortRecording(cx, "attempt to reenter interpreter while recording");
    }

    if (regs.fp()->hasImacropc())
        atoms = rt->atomState.commonAtomsStart();
#endif

    /* Don't call the script prologue if executing between Method and Trace JIT. */
    if (interpMode == JSINTERP_NORMAL) {
        StackFrame *fp = regs.fp();
        JS_ASSERT_IF(!fp->isGeneratorFrame(), regs.pc == script->code);
        if (!ScriptPrologueOrGeneratorResume(cx, fp, UseNewTypeAtEntry(cx, fp)))
            goto error;
    }

    /* The REJOIN mode acts like the normal mode, except the prologue is skipped. */
    if (interpMode == JSINTERP_REJOIN)
        interpMode = JSINTERP_NORMAL;

    JS_ASSERT_IF(interpMode == JSINTERP_SKIP_TRAP, JSOp(*regs.pc) == JSOP_TRAP);

    CHECK_INTERRUPT_HANDLER();

    RESET_USE_METHODJIT();

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

    /* Check for too deep of a native thread stack. */
#ifdef JS_TRACER
#ifdef JS_METHODJIT
    JS_CHECK_RECURSION(cx, do {
            if (TRACE_RECORDER(cx))
                AbortRecording(cx, "too much recursion");
            if (TRACE_PROFILER(cx))
                AbortProfiling(cx);
            goto error;
        } while (0););
#else
    JS_CHECK_RECURSION(cx, do {
            if (TRACE_RECORDER(cx))
                AbortRecording(cx, "too much recursion");
            goto error;
        } while (0););
#endif
#else
    JS_CHECK_RECURSION(cx, goto error);
#endif

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
        CHECK_RECORDER();
        CHECK_PCCOUNT_INTERRUPTS();
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

        if (pcCounts) {
            if (!regs.fp()->hasImacropc())
                ++pcCounts[regs.pc - script->code];
            moreInterrupts = true;
        }

        JSInterruptHook hook = cx->debugHooks->interruptHook;
        if (hook || script->stepModeEnabled()) {
#ifdef JS_TRACER
            if (TRACE_RECORDER(cx))
                AbortRecording(cx, "interrupt hook or singleStepMode");
#ifdef JS_METHODJIT
            if (TRACE_PROFILER(cx))
                AbortProfiling(cx);
#endif
#endif
            Value rval;
            JSTrapStatus status = JSTRAP_CONTINUE;
            if (hook) {
                status = hook(cx, script, regs.pc, Jsvalify(&rval),
                              cx->debugHooks->interruptHookData);
            }
            if (status == JSTRAP_CONTINUE && script->stepModeEnabled())
                status = Debugger::onSingleStep(cx, &rval);
            switch (status) {
              case JSTRAP_ERROR:
                goto error;
              case JSTRAP_CONTINUE:
                break;
              case JSTRAP_RETURN:
                regs.fp()->setReturnValue(rval);
                interpReturnOK = JS_TRUE;
                goto forced_return;
              case JSTRAP_THROW:
                cx->setPendingException(rval);
                goto error;
              default:;
            }
            moreInterrupts = true;
        }

#ifdef JS_TRACER
#ifdef JS_METHODJIT
        if (TRACE_PROFILER(cx) && interpMode == JSINTERP_PROFILE) {
            LoopProfile *prof = TRACE_PROFILER(cx);
            JS_ASSERT(!TRACE_RECORDER(cx));
            LoopProfile::ProfileAction act = prof->profileOperation(cx, op);
            switch (act) {
                case LoopProfile::ProfComplete:
                    if (interpMode != JSINTERP_NORMAL) {
                        leaveOnSafePoint = true;
                        LEAVE_ON_SAFE_POINT();
                    }
                    break;
                default:
                    moreInterrupts = true;
                    break;
            }
        }
#endif
        if (TraceRecorder* tr = TRACE_RECORDER(cx)) {
            JS_ASSERT(!TRACE_PROFILER(cx));
            AbortableRecordingStatus status = tr->monitorRecording(op);
            JS_ASSERT_IF(cx->isExceptionPending(), status == ARECORD_ERROR);

            if (interpMode != JSINTERP_NORMAL) {
                JS_ASSERT(interpMode == JSINTERP_RECORD || JSINTERP_SAFEPOINT);
                switch (status) {
                  case ARECORD_IMACRO_ABORTED:
                  case ARECORD_ABORTED:
                  case ARECORD_COMPLETED:
                  case ARECORD_STOP:
#ifdef JS_METHODJIT
                    leaveOnSafePoint = true;
                    LEAVE_ON_SAFE_POINT();
#endif
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
                atoms = rt->atomState.commonAtomsStart();
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
ADD_EMPTY_CASE(JSOP_NULLBLOCKCHAIN)
END_EMPTY_CASES

BEGIN_CASE(JSOP_TRACE)
BEGIN_CASE(JSOP_NOTRACE)
    LEAVE_ON_SAFE_POINT();
END_CASE(JSOP_TRACE)

check_backedge:
{
    CHECK_BRANCH();
    if (op != JSOP_NOTRACE && op != JSOP_TRACE)
        DO_OP();

#ifdef JS_TRACER
    if (TRACING_ENABLED(cx) && (TRACE_RECORDER(cx) || TRACE_PROFILER(cx) || (op == JSOP_TRACE && !useMethodJIT))) {
        MonitorResult r = MonitorLoopEdge(cx, interpMode);
        if (r == MONITOR_RECORDING) {
            JS_ASSERT(TRACE_RECORDER(cx));
            JS_ASSERT(!TRACE_PROFILER(cx));
            MONITOR_BRANCH_TRACEVIS;
            ENABLE_INTERRUPTS();
            CLEAR_LEAVE_ON_TRACE_POINT();
        }
        JS_ASSERT_IF(cx->isExceptionPending(), r == MONITOR_ERROR);
        RESTORE_INTERP_VARS_CHECK_EXCEPTION();
        op = (JSOp) *regs.pc;
        DO_OP();
    }
#endif /* JS_TRACER */

#ifdef JS_METHODJIT
    if (!useMethodJIT)
        DO_OP();
    mjit::CompileStatus status =
        mjit::CanMethodJITAtBranch(cx, script, regs.fp(), regs.pc);
    if (status == mjit::Compile_Error)
        goto error;
    if (status == mjit::Compile_Okay) {
        void *ncode =
            script->nativeCodeForPC(regs.fp()->isConstructing(), regs.pc);
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

BEGIN_CASE(JSOP_NOTEARG)
END_CASE(JSOP_NOTEARG)

/* ADD_EMPTY_CASE is not used here as JSOP_LINENO_LENGTH == 3. */
BEGIN_CASE(JSOP_LINENO)
END_CASE(JSOP_LINENO)

BEGIN_CASE(JSOP_BLOCKCHAIN)
END_CASE(JSOP_BLOCKCHAIN)

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
    JS_ASSERT(regs.fp()->base() <= regs.sp);
    JSObject *obj = GetBlockChain(cx, regs.fp());
    JS_ASSERT_IF(obj,
                 OBJ_BLOCK_DEPTH(cx, obj) + OBJ_BLOCK_COUNT(cx, obj)
                 <= (size_t) (regs.sp - regs.fp()->base()));
    for (obj = &regs.fp()->scopeChain(); obj; obj = obj->getParent()) {
        if (!obj->isBlock() || !obj->isWith())
            continue;
        if (obj->getPrivate() != js_FloatingFrameIfGenerator(cx, regs.fp()))
            break;
        JS_ASSERT(regs.fp()->base() + OBJ_BLOCK_DEPTH(cx, obj)
                  + (obj->isBlock() ? OBJ_BLOCK_COUNT(cx, obj) : 1)
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
    if (!EnterWith(cx, -1, JSOP_ENTERWITH, JSOP_ENTERWITH_LENGTH))
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

#ifdef JS_TRACER
    if (regs.fp()->hasImacropc()) {
        /*
         * If we are at the end of an imacro, return to its caller in the
         * current frame.
         */
        JS_ASSERT(op == JSOP_STOP);
        JS_ASSERT((uintN)(regs.sp - regs.fp()->slots()) <= script->nslots);
        jsbytecode *imacpc = regs.fp()->imacropc();
        regs.pc = imacpc + js_CodeSpec[*imacpc].length;
        if (js_CodeSpec[*imacpc].format & JOF_DECOMPOSE)
            regs.pc += GetDecomposeLength(imacpc, js_CodeSpec[*imacpc].length);
        regs.fp()->clearImacropc();
        LEAVE_ON_SAFE_POINT();
        atoms = script->atoms;
        op = JSOp(*regs.pc);
        DO_OP();
    }
#endif

    interpReturnOK = true;
    if (entryFrame != regs.fp())
  inline_return:
    {
        JS_ASSERT(!regs.fp()->hasImacropc());
        JS_ASSERT(!IsActiveWithOrBlock(cx, regs.fp()->scopeChain(), 0));
        interpReturnOK = ScriptEpilogue(cx, regs.fp(), interpReturnOK);

        /* The JIT inlines ScriptEpilogue. */
#ifdef JS_METHODJIT
  jit_return:
#endif

        /* The results of lowered call/apply frames need to be shifted. */
        bool shiftResult = regs.fp()->loweredCallOrApply();

        cx->stack.popInlineFrame(regs);

        RESTORE_INTERP_VARS();

        JS_ASSERT(*regs.pc == JSOP_TRAP || *regs.pc == JSOP_NEW || *regs.pc == JSOP_CALL ||
                  *regs.pc == JSOP_FUNCALL || *regs.pc == JSOP_FUNAPPLY);

        /* Resume execution in the calling frame. */
        RESET_USE_METHODJIT();
        if (JS_LIKELY(interpReturnOK)) {
            TRACE_0(LeaveFrame);
            TypeScript::Monitor(cx, script, regs.pc, regs.sp[-1]);

            op = JSOp(*regs.pc);
            len = JSOP_CALL_LENGTH;

            if (shiftResult) {
                regs.sp[-2] = regs.sp[-1];
                regs.sp--;
            }

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
    TRY_BRANCH_AFTER_COND(cond, 2);
    regs.sp--;
    regs.sp[-1].setBoolean(cond);
}
END_CASE(JSOP_IN)

BEGIN_CASE(JSOP_ITER)
{
    JS_ASSERT(regs.sp > regs.fp()->base());
    uintN flags = regs.pc[1];
    if (!js_ValueToIterator(cx, flags, &regs.sp[-1]))
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
    bool ok = !!js_CloseIterator(cx, &regs.sp[-1].toObject());
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
    jsint i = regs.pc[1];
    JS_ASSERT(regs.sp - (i+1) >= regs.fp()->base());
    Value lval = regs.sp[-(i+1)];
    memmove(regs.sp - (i+1), regs.sp - i, sizeof(Value)*i);
    regs.sp[-1] = lval;
}
END_CASE(JSOP_PICK)

#define NATIVE_GET(cx,obj,pobj,shape,getHow,vp)                               \
    JS_BEGIN_MACRO                                                            \
        if (shape->isDataDescriptor() && shape->hasDefaultGetter()) {         \
            /* Fast path for Object instance properties. */                   \
            JS_ASSERT((shape)->slot != SHAPE_INVALID_SLOT ||                  \
                      !shape->hasDefaultSetter());                            \
            if (((shape)->slot != SHAPE_INVALID_SLOT))                        \
                *(vp) = (pobj)->nativeGetSlot((shape)->slot);                 \
            else                                                              \
                (vp)->setUndefined();                                         \
        } else {                                                              \
            if (!js_NativeGet(cx, obj, pobj, shape, getHow, vp))              \
                goto error;                                                   \
        }                                                                     \
    JS_END_MACRO

#define NATIVE_SET(cx,obj,shape,entry,strict,vp)                              \
    JS_BEGIN_MACRO                                                            \
        if (shape->hasDefaultSetter() &&                                      \
            (shape)->slot != SHAPE_INVALID_SLOT &&                            \
            !(obj)->brandedOrHasMethodBarrier()) {                            \
            /* Fast path for, e.g., plain Object instance properties. */      \
            (obj)->nativeSetSlotWithType(cx, shape, *vp);                     \
        } else {                                                              \
            if (!js_NativeSet(cx, obj, shape, false, strict, vp))             \
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
    JSObject &obj = regs.fp()->varObj();
    const Value &ref = regs.sp[-1];
    if (!obj.defineProperty(cx, ATOM_TO_JSID(atom), ref,
                            PropertyStub, StrictPropertyStub,
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
                             PropertyStub, StrictPropertyStub,
                             JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY)) {
        goto error;
    }
    regs.sp -= 3;
}
END_CASE(JSOP_ENUMCONSTELEM)
#endif

BEGIN_CASE(JSOP_BINDGNAME)
    PUSH_OBJECT(*regs.fp()->scopeChain().getGlobal());
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
        obj = js_FindIdentifierBase(cx, &regs.fp()->scopeChain(), id);
        if (!obj)
            goto error;
    } while (0);
    PUSH_OBJECT(*obj);
}
END_CASE(JSOP_BINDNAME)

BEGIN_CASE(JSOP_IMACOP)
    JS_ASSERT(JS_UPTRDIFF(regs.fp()->imacropc(), script->code) < script->length);
    op = JSOp(*regs.fp()->imacropc());
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

#define EQUALITY_OP(OP)                                                       \
    JS_BEGIN_MACRO                                                            \
        Value rval = regs.sp[-1];                                             \
        Value lval = regs.sp[-2];                                             \
        JSBool cond;                                                          \
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
        JSBool equal;                                                         \
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
        Value &rval = regs.sp[-1];                                            \
        Value &lval = regs.sp[-2];                                            \
        bool cond;                                                            \
        /* Optimize for two int-tagged operands (typical loop control). */    \
        if (lval.isInt32() && rval.isInt32()) {                               \
            cond = lval.toInt32() OP rval.toInt32();                          \
        } else {                                                              \
            if (!ToPrimitive(cx, JSTYPE_NUMBER, &lval))                       \
                goto error;                                                   \
            if (!ToPrimitive(cx, JSTYPE_NUMBER, &rval))                       \
                goto error;                                                   \
            if (lval.isString() && rval.isString()) {                         \
                JSString *l = lval.toString(), *r = rval.toString();          \
                int32 result;                                                 \
                if (!CompareStrings(cx, l, r, &result))                       \
                    goto error;                                               \
                cond = result OP 0;                                           \
            } else {                                                          \
                double l, r;                                                  \
                if (!ToNumber(cx, lval, &l) || !ToNumber(cx, rval, &r))       \
                    goto error;                                               \
                cond = JSDOUBLE_COMPARE(l, OP, r, false);                     \
            }                                                                 \
        }                                                                     \
        TRY_BRANCH_AFTER_COND(cond, 2);                                       \
        regs.sp[-2].setBoolean(cond);                                         \
        regs.sp--;                                                            \
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
    if (!regs.sp[-1].setNumber(uint32(u)))
        TypeScript::MonitorOverflow(cx, script, regs.pc);
}
END_CASE(JSOP_URSH)

BEGIN_CASE(JSOP_ADD)
{
    Value rval = regs.sp[-1];
    Value lval = regs.sp[-2];

    if (lval.isInt32() && rval.isInt32()) {
        int32_t l = lval.toInt32(), r = rval.toInt32();
        int32_t sum = l + r;
        if (JS_UNLIKELY(bool((l ^ sum) & (r ^ sum) & 0x80000000))) {
            regs.sp[-2].setDouble(double(l) + double(r));
            TypeScript::MonitorOverflow(cx, script, regs.pc);
        } else {
            regs.sp[-2].setInt32(sum);
        }
        regs.sp--;
    } else
#if JS_HAS_XML_SUPPORT
    if (IsXML(lval) && IsXML(rval)) {
        if (!js_ConcatenateXML(cx, &lval.toObject(), &rval.toObject(), &rval))
            goto error;
        regs.sp[-2] = rval;
        regs.sp--;
        TypeScript::MonitorUnknown(cx, script, regs.pc);
    } else
#endif
    {
        /*
         * If either operand is an object, any non-integer result must be
         * reported to inference.
         */
        bool lIsObject = lval.isObject(), rIsObject = rval.isObject();

        if (!ToPrimitive(cx, &lval))
            goto error;
        if (!ToPrimitive(cx, &rval))
            goto error;
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
            if (lIsObject || rIsObject)
                TypeScript::MonitorString(cx, script, regs.pc);
            regs.sp[-2].setString(str);
            regs.sp--;
        } else {
            double l, r;
            if (!ToNumber(cx, lval, &l) || !ToNumber(cx, rval, &r))
                goto error;
            l += r;
            if (!regs.sp[-2].setNumber(l) &&
                (lIsObject || rIsObject || (!lval.isDouble() && !rval.isDouble()))) {
                TypeScript::MonitorOverflow(cx, script, regs.pc);
            }
            regs.sp--;
        }
    }
}
END_CASE(JSOP_ADD)

#define BINARY_OP(OP)                                                         \
    JS_BEGIN_MACRO                                                            \
        Value rval = regs.sp[-1];                                             \
        Value lval = regs.sp[-2];                                             \
        double d1, d2;                                                        \
        if (!ToNumber(cx, lval, &d1) || !ToNumber(cx, rval, &d2))             \
            goto error;                                                       \
        double d = d1 OP d2;                                                  \
        regs.sp--;                                                            \
        if (!regs.sp[-1].setNumber(d) &&                                      \
            !(lval.isDouble() || rval.isDouble())) {                          \
            TypeScript::MonitorOverflow(cx, script, regs.pc);                 \
        }                                                                     \
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
    Value rval = regs.sp[-1];
    Value lval = regs.sp[-2];
    double d1, d2;
    if (!ToNumber(cx, lval, &d1) || !ToNumber(cx, rval, &d2))
        goto error;
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
        TypeScript::MonitorOverflow(cx, script, regs.pc);
    } else {
        d1 /= d2;
        if (!regs.sp[-1].setNumber(d1) &&
            !(lval.isDouble() || rval.isDouble())) {
            TypeScript::MonitorOverflow(cx, script, regs.pc);
        }
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
        if (!ToNumber(cx, regs.sp[-2], &d1) || !ToNumber(cx, regs.sp[-1], &d2))
            goto error;
        regs.sp--;
        if (d2 == 0) {
            regs.sp[-1].setDouble(js_NaN);
        } else {
            d1 = js_fmod(d1, d2);
            regs.sp[-1].setDouble(d1);
        }
        TypeScript::MonitorOverflow(cx, script, regs.pc);
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
    JSAtom *atom;
    LOAD_ATOM(0, atom);
    jsid id = ATOM_TO_JSID(atom);
    JSObject *obj, *obj2;
    JSProperty *prop;
    if (!js_FindProperty(cx, id, false, &obj, &obj2, &prop))
        goto error;

    /* Strict mode code should never contain JSOP_DELNAME opcodes. */
    JS_ASSERT(!script->strictModeCode);

    /* ECMA says to return true if name is undefined or inherited. */
    PUSH_BOOLEAN(true);
    if (prop) {
        if (!obj->deleteProperty(cx, id, &regs.sp[-1], false))
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
    if (!obj->deleteProperty(cx, id, &rval, script->strictModeCode))
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
    if (!obj->deleteProperty(cx, id, &regs.sp[-2], script->strictModeCode))
        goto error;

    regs.sp--;
}
END_CASE(JSOP_DELELEM)

BEGIN_CASE(JSOP_TOID)
{
    /*
     * There must be an object value below the id, which will not be popped
     * but is necessary in interning the id for XML.
     */
    JSObject *obj;
    FETCH_OBJECT(cx, -2, obj);

    jsid id;
    FETCH_ELEMENT_ID(obj, -1, id);

    if (!regs.sp[-1].isInt32())
        TypeScript::MonitorUnknown(cx, script, regs.pc);
}
END_CASE(JSOP_TOID)

BEGIN_CASE(JSOP_TYPEOFEXPR)
BEGIN_CASE(JSOP_TYPEOF)
{
    const Value &ref = regs.sp[-1];
    JSType type = JS_TypeOfValue(cx, Jsvalify(ref));
    JSAtom *atom = rt->atomState.typeAtoms[type];
    regs.sp[-1].setString(atom);
}
END_CASE(JSOP_TYPEOF)

BEGIN_CASE(JSOP_VOID)
    regs.sp[-1].setUndefined();
END_CASE(JSOP_VOID)

{
    /*
     * Property incops are followed by an equivalent decomposed version,
     * and we have the option of running either. If type inference is enabled
     * we run the decomposed version to accumulate observed types and
     * overflows which inference can process, otherwise we run the fat opcode
     * as doing so is faster and is what the tracer needs while recording.
     */
    JSObject *obj;
    JSAtom *atom;
    jsid id;
    jsint i;

BEGIN_CASE(JSOP_INCELEM)
BEGIN_CASE(JSOP_DECELEM)
BEGIN_CASE(JSOP_ELEMINC)
BEGIN_CASE(JSOP_ELEMDEC)

    if (cx->typeInferenceEnabled()) {
        len = JSOP_INCELEM_LENGTH;
        DO_NEXT_OP(len);
    }

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

    if (cx->typeInferenceEnabled()) {
        len = JSOP_INCPROP_LENGTH;
        DO_NEXT_OP(len);
    }

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
    if (cx->typeInferenceEnabled()) {
        len = JSOP_INCNAME_LENGTH;
        DO_NEXT_OP(len);
    }

    obj = &regs.fp()->scopeChain();

    bool global = (js_CodeSpec[op].format & JOF_GNAME);
    if (global)
        obj = obj->getGlobal();

    JSObject *obj2;
    PropertyCacheEntry *entry;
    JS_PROPERTY_CACHE(cx).test(cx, regs.pc, obj, obj2, entry, atom);
    if (!atom) {
        ASSERT_VALID_PROPERTY_CACHE_HIT(0, obj, obj2, entry);
        if (obj == obj2 && entry->vword.isSlot()) {
            uint32 slot = entry->vword.toSlot();
            const Value &rref = obj->nativeGetSlot(slot);
            int32_t tmp;
            if (JS_LIKELY(rref.isInt32() && CanIncDecWithoutOverflow(tmp = rref.toInt32()))) {
                int32_t inc = tmp + ((js_CodeSpec[op].format & JOF_INC) ? 1 : -1);
                if (!(js_CodeSpec[op].format & JOF_POST))
                    tmp = inc;
                obj->nativeSetSlot(slot, Int32Value(inc));
                PUSH_INT32(tmp);
                len = JSOP_INCNAME_LENGTH + GetDecomposeLength(regs.pc, JSOP_INCNAME_LENGTH);
                DO_NEXT_OP(len);
            }
        }
        LOAD_ATOM(0, atom);
    }

    id = ATOM_TO_JSID(atom);
    JSProperty *prop;
    if (!js_FindPropertyHelper(cx, id, true, global, &obj, &obj2, &prop))
        goto error;
    if (!prop) {
        atomNotDefined = atom;
        goto atom_not_defined;
    }
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

    uint32 format = cs->format;
    uint32 setPropFlags = (JOF_MODE(format) == JOF_NAME)
                          ? JSRESOLVE_ASSIGNING
                          : JSRESOLVE_ASSIGNING | JSRESOLVE_QUALIFIED;

    Value &ref = regs.sp[-1];
    int32_t tmp;
    if (JS_LIKELY(ref.isInt32() && CanIncDecWithoutOverflow(tmp = ref.toInt32()))) {
        int incr = (format & JOF_INC) ? 1 : -1;
        if (format & JOF_POST)
            ref.getInt32Ref() = tmp + incr;
        else
            ref.getInt32Ref() = tmp += incr;

        {
            JSAutoResolveFlags rf(cx, setPropFlags);
            if (!obj->setProperty(cx, id, &ref, script->strictModeCode))
                goto error;
        }

        /*
         * We must set regs.sp[-1] to tmp for both post and pre increments
         * as the setter overwrites regs.sp[-1].
         */
        ref.setInt32(tmp);
    } else {
        /* We need an extra root for the result. */
        PUSH_NULL();
        if (!DoIncDec(cx, cs, &regs.sp[-2], &regs.sp[-1]))
            goto error;

        {
            JSAutoResolveFlags rf(cx, setPropFlags);
            if (!obj->setProperty(cx, id, &regs.sp[-1], script->strictModeCode))
                goto error;
        }

        regs.sp--;
    }

    if (cs->nuses == 0) {
        /* regs.sp[-1] already contains the result of name increment. */
    } else {
        regs.sp[-1 - cs->nuses] = regs.sp[-1];
        regs.sp -= cs->nuses;
    }
    len = cs->length + GetDecomposeLength(regs.pc, cs->length);
    DO_NEXT_OP(len);
}
}

{
    int incr, incr2;
    uint32 slot;
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
        SKIP_POP_AFTER_SET(JSOP_INCARG_LENGTH, 0);
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

BEGIN_CASE(JSOP_UNBRANDTHIS)
{
    if (!ComputeThis(cx, regs.fp()))
        goto error;
    Value &thisv = regs.fp()->thisValue();
    if (thisv.isObject()) {
        JSObject *obj = &thisv.toObject();
        if (obj->isNative())
            obj->unbrand(cx);
    }
}
END_CASE(JSOP_UNBRANDTHIS)

BEGIN_CASE(JSOP_GETPROP)
BEGIN_CASE(JSOP_GETXPROP)
BEGIN_CASE(JSOP_LENGTH)
{
    Value rval;
    do {
        Value *vp = &regs.sp[-1];

        if (op == JSOP_LENGTH) {
            /* Optimize length accesses on strings, arrays, and arguments. */
            if (vp->isString()) {
                rval = Int32Value(vp->toString()->length());
                break;
            }
            if (vp->isMagic(JS_LAZY_ARGUMENTS)) {
                rval = Int32Value(regs.fp()->numActualArgs());
                break;
            }
            if (vp->isObject()) {
                JSObject *obj = &vp->toObject();
                if (obj->isArray()) {
                    jsuint length = obj->getArrayLength();
                    rval = NumberValue(length);
                    break;
                }

                if (obj->isArguments()) {
                    ArgumentsObject *argsobj = obj->asArguments();
                    if (!argsobj->hasOverriddenLength()) {
                        uint32 length = argsobj->initialLength();
                        JS_ASSERT(length < INT32_MAX);
                        rval = Int32Value(int32_t(length));
                        break;
                    }
                }

                if (js_IsTypedArray(obj)) {
                    JSObject *tarray = TypedArray::getTypedArray(obj);
                    rval = Int32Value(TypedArray::getLength(tarray));
                    break;
                }
            }
        }

        /*
         * We do not impose the method read barrier if in an imacro,
         * assuming any property gets it does (e.g., for 'toString'
         * from JSOP_NEW) will not be leaked to the calling script.
         */
        JSObject *obj;
        VALUE_TO_OBJECT(cx, vp, obj);
        JSObject *aobj = js_GetProtoIfDenseArray(obj);

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
                rval = obj2->nativeGetSlot(slot);
            } else {
                JS_ASSERT(entry->vword.isShape());
                const Shape *shape = entry->vword.toShape();
                NATIVE_GET(cx, obj, obj2, shape,
                           regs.fp()->hasImacropc() ? JSGET_NO_METHOD_BARRIER : JSGET_METHOD_BARRIER,
                           &rval);
            }
            break;
        }

        jsid id = ATOM_TO_JSID(atom);
        if (JS_LIKELY(!aobj->getOps()->getProperty)
            ? !js_GetPropertyHelper(cx, obj, id,
                                    (regs.fp()->hasImacropc() ||
                                     regs.pc[JSOP_GETPROP_LENGTH] == JSOP_IFEQ)
                                    ? JSGET_CACHE_RESULT | JSGET_NO_METHOD_BARRIER
                                    : JSGET_CACHE_RESULT | JSGET_METHOD_BARRIER,
                                    &rval)
            : !obj->getProperty(cx, id, &rval)) {
            goto error;
        }
    } while (0);

    TypeScript::Monitor(cx, script, regs.pc, rval);

    regs.sp[-1] = rval;
    assertSameCompartment(cx, regs.sp[-1]);
}
END_CASE(JSOP_GETPROP)

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
            rval = obj2->nativeGetSlot(slot);
        } else {
            JS_ASSERT(entry->vword.isShape());
            const Shape *shape = entry->vword.toShape();
            NATIVE_GET(cx, &objv.toObject(), obj2, shape, JSGET_NO_METHOD_BARRIER, &rval);
        }
        regs.sp[-1] = rval;
        assertSameCompartment(cx, regs.sp[-1]);
        PUSH_COPY(lval);
    } else {
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
            assertSameCompartment(cx, regs.sp[-1], regs.sp[-2]);
        } else {
            JS_ASSERT(!objv.toObject().getOps()->getProperty);
            if (!js_GetPropertyHelper(cx, &objv.toObject(), id,
                                      JSGET_CACHE_RESULT | JSGET_NO_METHOD_BARRIER,
                                      &rval)) {
                goto error;
            }
            regs.sp[-1] = lval;
            regs.sp[-2] = rval;
            assertSameCompartment(cx, regs.sp[-1], regs.sp[-2]);
        }
    }
#if JS_HAS_NO_SUCH_METHOD
    if (JS_UNLIKELY(rval.isPrimitive()) && regs.sp[-1].isObject()) {
        LOAD_ATOM(0, atom);
        regs.sp[-2].setString(atom);
        if (!OnUnknownMethod(cx, regs.sp - 2))
            goto error;
    }
#endif
    TypeScript::Monitor(cx, script, regs.pc, rval);
}
END_CASE(JSOP_CALLPROP)

BEGIN_CASE(JSOP_UNBRAND)
    JS_ASSERT(regs.sp - regs.fp()->slots() >= 1);
    regs.sp[-1].toObject().unbrand(cx);
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

    JS_ASSERT_IF(op == JSOP_SETGNAME, obj == regs.fp()->scopeChain().getGlobal());

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
             * Property cache hit, only partially confirmed by testForSet. We
             * know that the entry applies to regs.pc and that obj's shape
             * matches.
             *
             * The entry predicts either a new property to be added directly to
             * obj by this set, or on an existing "own" property, or on a
             * prototype property that has a setter.
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
                        JS_ASSERT(obj->nativeContains(cx, *shape));
                    } else {
                        JS_ASSERT(obj2->nativeContains(cx, *shape));
                        JS_ASSERT(entry->vcapTag() == 1);
                        JS_ASSERT(entry->kshape != entry->vshape());
                        JS_ASSERT(!shape->hasSlot());
                    }
#endif

                    PCMETER(cache->pchits++);
                    PCMETER(cache->setpchits++);
                    NATIVE_SET(cx, obj, shape, entry, script->strictModeCode, &rval);
                    break;
                }
            } else {
                JS_ASSERT(obj->isExtensible());

                if (obj->nativeEmpty()) {
                    if (!obj->ensureClassReservedSlotsForEmptyObject(cx))
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
                    TRACE_1(AddProperty, obj);
                    obj->nativeSetSlotWithType(cx, shape, rval);

                    /*
                     * Purge the property cache of the id we may have just
                     * shadowed in obj's scope and proto chains.
                     */
                    js_PurgeScopeChain(cx, obj, shape->propid);
                    break;
                }
            }
            PCMETER(cache->setpcmisses++);

            LOAD_ATOM(0, atom);
        } else {
            JS_ASSERT(atom);
        }

        jsid id = ATOM_TO_JSID(atom);
        if (entry && JS_LIKELY(!obj->getOps()->setProperty)) {
            uintN defineHow;
            if (op == JSOP_SETMETHOD)
                defineHow = DNP_CACHE_RESULT | DNP_SET_METHOD;
            else if (op == JSOP_SETNAME)
                defineHow = DNP_CACHE_RESULT | DNP_UNQUALIFIED;
            else
                defineHow = DNP_CACHE_RESULT;
            if (!js_SetPropertyHelper(cx, obj, id, defineHow, &rval, script->strictModeCode))
                goto error;
        } else {
            if (!obj->setProperty(cx, id, &rval, script->strictModeCode))
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
            str = JSAtom::getUnitStringForElement(cx, str, size_t(i));
            if (!str)
                goto error;
            regs.sp--;
            regs.sp[-1].setString(str);
            TypeScript::Monitor(cx, script, regs.pc, regs.sp[-1]);
            len = JSOP_GETELEM_LENGTH;
            DO_NEXT_OP(len);
        }
    }

    if (lref.isMagic(JS_LAZY_ARGUMENTS)) {
        if (rref.isInt32() && size_t(rref.toInt32()) < regs.fp()->numActualArgs()) {
            regs.sp--;
            regs.sp[-1] = regs.fp()->canonicalActualArg(rref.toInt32());
            TypeScript::Monitor(cx, script, regs.pc, regs.sp[-1]);
            len = JSOP_GETELEM_LENGTH;
            DO_NEXT_OP(len);
        }
        MarkArgumentsCreated(cx, script);
        JS_ASSERT(!lref.isMagic(JS_LAZY_ARGUMENTS));
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
            if (idx < obj->getDenseArrayInitializedLength()) {
                copyFrom = &obj->getDenseArrayElement(idx);
                if (!copyFrom->isMagic())
                    goto end_getelem;
            }
        } else if (obj->isArguments()) {
            uint32 arg = uint32(i);
            ArgumentsObject *argsobj = obj->asArguments();

            if (arg < argsobj->initialLength()) {
                copyFrom = &argsobj->element(arg);
                if (!copyFrom->isMagic(JS_ARGS_HOLE)) {
                    if (StackFrame *afp = argsobj->maybeStackFrame())
                        copyFrom = &afp->canonicalActualArg(arg);
                    goto end_getelem;
                }
            }
        }
        if (JS_LIKELY(INT_FITS_IN_JSID(i)))
            id = INT_TO_JSID(i);
        else
            goto intern_big_int;
    } else {
        int32_t i;
        if (ValueFitsInInt32(rref, &i) && INT_FITS_IN_JSID(i)) {
            id = INT_TO_JSID(i);
        } else {
          intern_big_int:
            if (!js_InternNonIntElementId(cx, obj, rref, &id))
                goto error;
        }
    }

    if (!obj->getProperty(cx, id, &rval))
        goto error;
    copyFrom = &rval;

    if (!JSID_IS_INT(id))
        TypeScript::MonitorUnknown(cx, script, regs.pc);

  end_getelem:
    regs.sp--;
    regs.sp[-1] = *copyFrom;
    assertSameCompartment(cx, regs.sp[-1]);
    TypeScript::Monitor(cx, script, regs.pc, regs.sp[-1]);
}
END_CASE(JSOP_GETELEM)

BEGIN_CASE(JSOP_CALLELEM)
{
    /* Find the object on which to look for |this|'s properties. */
    Value thisv = regs.sp[-2];
    JSObject *thisObj = ValuePropertyBearer(cx, thisv, -2);
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
        /* For OnUnknownMethod, sp[-2] is the index, and sp[-1] is the object missing it. */
        regs.sp[-2] = regs.sp[-1];
        regs.sp[-1].setObject(*thisObj);
        if (!OnUnknownMethod(cx, regs.sp - 2))
            goto error;
    } else
#endif
    {
        regs.sp[-1] = thisv;
    }

    if (!JSID_IS_INT(id))
        TypeScript::MonitorUnknown(cx, script, regs.pc);
    TypeScript::Monitor(cx, script, regs.pc, regs.sp[-2]);
}
END_CASE(JSOP_CALLELEM)

BEGIN_CASE(JSOP_SETELEM)
BEGIN_CASE(JSOP_SETHOLE)
{
    JSObject *obj;
    FETCH_OBJECT(cx, -3, obj);
    jsid id;
    FETCH_ELEMENT_ID(obj, -2, id);
    Value rval;
    TypeScript::MonitorAssign(cx, script, regs.pc, obj, id, regs.sp[-1]);
    do {
        if (obj->isDenseArray() && JSID_IS_INT(id)) {
            jsuint length = obj->getDenseArrayInitializedLength();
            jsint i = JSID_TO_INT(id);
            if ((jsuint)i < length) {
                if (obj->getDenseArrayElement(i).isMagic(JS_ARRAY_HOLE)) {
                    if (js_PrototypeHasIndexedProperties(cx, obj))
                        break;
                    if ((jsuint)i >= obj->getArrayLength())
                        obj->setArrayLength(cx, i + 1);
                    *regs.pc = JSOP_SETHOLE;
                }
                obj->setDenseArrayElementWithType(cx, i, regs.sp[-1]);
                goto end_setelem;
            } else {
                *regs.pc = JSOP_SETHOLE;
            }
        }
    } while (0);
    rval = regs.sp[-1];
    if (!obj->setProperty(cx, id, &rval, script->strictModeCode))
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
    if (!obj->setProperty(cx, id, &rval, script->strictModeCode))
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

    JSObject *callee;
    JSFunction *fun;

    /* Don't bother trying to fast-path calls to scripted non-constructors. */
    if (!IsFunctionObject(args.calleev(), &callee, &fun) || !fun->isInterpretedConstructor()) {
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
        TRACE_0(NativeCallComplete);
        len = JSOP_CALL_LENGTH;
        DO_NEXT_OP(len);
    }

    TypeMonitorCall(cx, args, construct);

    InitialFrameFlags initial = construct ? INITIAL_CONSTRUCT : INITIAL_NONE;

    JSScript *newScript = fun->script();
    if (!cx->stack.pushInlineFrame(cx, regs, args, *callee, fun, newScript, initial))
        goto error;

    RESTORE_INTERP_VARS();

    if (!regs.fp()->functionPrologue(cx))
        goto error;

    RESET_USE_METHODJIT();
    TRACE_0(EnterFrame);

    bool newType = cx->typeInferenceEnabled() && UseNewType(cx, script, regs.pc);
	
#ifdef JS_ION
    if (!newType && ion::IsEnabled()) {
        ion::MethodStatus status = ion::Compile(cx, script, regs.fp());
        if (status == ion::Method_Compiled) {
            interpReturnOK = ion::Cannon(cx, regs.fp());
            CHECK_INTERRUPT_HANDLER();
            goto jit_return;
        }
    }
#endif

#ifdef JS_METHODJIT
    if (!newType) {
        /* Try to ensure methods are method JIT'd.  */
        mjit::CompileRequest request = (interpMode == JSINTERP_NORMAL)
                                       ? mjit::CompileRequest_Interpreter
                                       : mjit::CompileRequest_JIT;
        mjit::CompileStatus status = mjit::CanMethodJIT(cx, script, construct, request);
        if (status == mjit::Compile_Error)
            goto error;
        if (!TRACE_RECORDER(cx) && !TRACE_PROFILER(cx) && status == mjit::Compile_Okay) {
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

#define PUSH_IMPLICIT_THIS(cx, obj, funval)                                   \
    JS_BEGIN_MACRO                                                            \
        Value v;                                                              \
        if (!ComputeImplicitThis(cx, obj, funval, &v))                        \
            goto error;                                                       \
        PUSH_COPY(v);                                                         \
    JS_END_MACRO                                                              \

BEGIN_CASE(JSOP_GETGNAME)
BEGIN_CASE(JSOP_CALLGNAME)
BEGIN_CASE(JSOP_NAME)
BEGIN_CASE(JSOP_CALLNAME)
{
    JSObject *obj = &regs.fp()->scopeChain();

    bool global = js_CodeSpec[op].format & JOF_GNAME;
    if (global)
        obj = obj->getGlobal();

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
            PUSH_COPY(obj2->nativeGetSlot(slot));
        } else {
            JS_ASSERT(entry->vword.isShape());
            shape = entry->vword.toShape();
            NATIVE_GET(cx, obj, obj2, shape, JSGET_METHOD_BARRIER, &rval);
            PUSH_COPY(rval);
        }

        TypeScript::Monitor(cx, script, regs.pc, regs.sp[-1]);

        JS_ASSERT(obj->isGlobal() || IsCacheableNonGlobalScope(obj));
        if (op == JSOP_CALLNAME || op == JSOP_CALLGNAME)
            PUSH_IMPLICIT_THIS(cx, obj, regs.sp[-1]);
        len = JSOP_NAME_LENGTH;
        DO_NEXT_OP(len);
    }

    jsid id = ATOM_TO_JSID(atom);
    JSProperty *prop;
    if (!js_FindPropertyHelper(cx, id, true, global, &obj, &obj2, &prop))
        goto error;
    if (!prop) {
        /* Kludge to allow (typeof foo == "undefined") tests. */
        JSOp op2 = js_GetOpcode(cx, script, regs.pc + JSOP_NAME_LENGTH);
        if (op2 == JSOP_TYPEOF) {
            PUSH_UNDEFINED();
            TypeScript::Monitor(cx, script, regs.pc, regs.sp[-1]);
            len = JSOP_NAME_LENGTH;
            DO_NEXT_OP(len);
        }
        atomNotDefined = atom;
        goto atom_not_defined;
    }

    /* Take the slow path if prop was not found in a native object. */
    if (!obj->isNative() || !obj2->isNative()) {
        if (!obj->getProperty(cx, id, &rval))
            goto error;
    } else {
        shape = (Shape *)prop;
        JSObject *normalized = obj;
        if (normalized->getClass() == &WithClass && !shape->hasDefaultGetter())
            normalized = js_UnwrapWithObject(cx, normalized);
        NATIVE_GET(cx, normalized, obj2, shape, JSGET_METHOD_BARRIER, &rval);
    }

    PUSH_COPY(rval);
    TypeScript::Monitor(cx, script, regs.pc, rval);

    /* obj must be on the scope chain, thus not a function. */
    if (op == JSOP_CALLNAME || op == JSOP_CALLGNAME)
        PUSH_IMPLICIT_THIS(cx, obj, rval);
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
     * Here atoms can exceed script->natoms as we use atoms as a
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
    atoms = script->atoms;
END_CASE(JSOP_RESETBASE)

BEGIN_CASE(JSOP_DOUBLE)
{
    JS_ASSERT(!regs.fp()->hasImacropc());
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
    JSObject *obj;
    LOAD_OBJECT(0, obj);
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
     * We avoid the GetScopeChain call here and pass fp->scopeChain as
     * js_GetClassPrototype uses the latter only to locate the global.
     */
    jsatomid index = GET_FULL_INDEX(0);
    JSObject *proto;
    if (!js_GetClassPrototype(cx, &regs.fp()->scopeChain(), JSProto_RegExp, &proto))
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
    JS_ASSERT(!regs.fp()->hasImacropc());
    JS_ASSERT(atoms == script->atoms);
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
    len = (op == JSOP_LOOKUPSWITCH)
          ? GET_JUMP_OFFSET(pc2)
          : GET_JUMPX_OFFSET(pc2);
}
END_VARLEN_CASE
}

BEGIN_CASE(JSOP_TRAP)
{
    if (interpMode == JSINTERP_SKIP_TRAP) {
        interpMode = JSINTERP_NORMAL;
        op = JS_GetTrapOpcode(cx, script, regs.pc);
        DO_OP();
    }

    Value rval;
    JSTrapStatus status = Debugger::onTrap(cx, &rval);
    switch (status) {
      case JSTRAP_ERROR:
        goto error;
      case JSTRAP_RETURN:
        regs.fp()->setReturnValue(rval);
        interpReturnOK = JS_TRUE;
        goto forced_return;
      case JSTRAP_THROW:
        cx->setPendingException(rval);
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
    if (cx->typeInferenceEnabled() && !script->strictModeCode) {
        if (!script->ensureRanInference(cx))
            goto error;
        if (script->createdArgs) {
            if (!js_GetArgsValue(cx, regs.fp(), &rval))
                goto error;
        } else {
            rval = MagicValue(JS_LAZY_ARGUMENTS);
        }
    } else {
        if (!js_GetArgsValue(cx, regs.fp(), &rval))
            goto error;
    }
    PUSH_COPY(rval);
}
END_CASE(JSOP_ARGUMENTS)

BEGIN_CASE(JSOP_ARGSUB)
{
    jsid id = INT_TO_JSID(GET_ARGNO(regs.pc));
    Value rval;
    if (!js_GetArgsProperty(cx, regs.fp(), id, &rval))
        goto error;
    PUSH_COPY(rval);
}
END_CASE(JSOP_ARGSUB)

BEGIN_CASE(JSOP_ARGCNT)
{
    jsid id = ATOM_TO_JSID(rt->atomState.lengthAtom);
    Value rval;
    if (!js_GetArgsProperty(cx, regs.fp(), id, &rval))
        goto error;
    PUSH_COPY(rval);
}
END_CASE(JSOP_ARGCNT)

BEGIN_CASE(JSOP_GETARG)
BEGIN_CASE(JSOP_CALLARG)
{
    uint32 slot = GET_ARGNO(regs.pc);
    JS_ASSERT(slot < regs.fp()->numFormalArgs());
    PUSH_COPY(argv[slot]);
    if (op == JSOP_CALLARG)
        PUSH_UNDEFINED();
}
END_CASE(JSOP_GETARG)

BEGIN_CASE(JSOP_SETARG)
{
    uint32 slot = GET_ARGNO(regs.pc);
    JS_ASSERT(slot < regs.fp()->numFormalArgs());
    argv[slot] = regs.sp[-1];
}
END_SET_CASE(JSOP_SETARG)

BEGIN_CASE(JSOP_GETLOCAL)
{
    /*
     * Skip the same-compartment assertion if the local will be immediately
     * popped. We do not guarantee sync for dead locals when coming in from the
     * method JIT, and a GETLOCAL followed by POP is not considered to be
     * a use of the variable.
     */
     uint32 slot = GET_SLOTNO(regs.pc);
     JS_ASSERT(slot < script->nslots);
     PUSH_COPY_SKIP_CHECK(regs.fp()->slots()[slot]);

#ifdef DEBUG
    if (regs.pc[JSOP_GETLOCAL_LENGTH] != JSOP_POP)
        assertSameCompartment(cx, regs.sp[-1]);
#endif
}
END_CASE(JSOP_GETLOCAL)

BEGIN_CASE(JSOP_CALLLOCAL)
{
    uint32 slot = GET_SLOTNO(regs.pc);
    JS_ASSERT(slot < script->nslots);
    PUSH_COPY(regs.fp()->slots()[slot]);
    PUSH_UNDEFINED();
}
END_CASE(JSOP_CALLLOCAL)

BEGIN_CASE(JSOP_SETLOCAL)
{
    uint32 slot = GET_SLOTNO(regs.pc);
    JS_ASSERT(slot < script->nslots);
    regs.fp()->slots()[slot] = regs.sp[-1];
}
END_SET_CASE(JSOP_SETLOCAL)

BEGIN_CASE(JSOP_GETFCSLOT)
BEGIN_CASE(JSOP_CALLFCSLOT)
{
    JS_ASSERT(regs.fp()->isNonEvalFunctionFrame());
    uintN index = GET_UINT16(regs.pc);
    JSObject *obj = &argv[-2].toObject();

    JS_ASSERT(index < obj->getFunctionPrivate()->script()->bindings.countUpvars());
    PUSH_COPY(obj->getFlatClosureUpvar(index));
    TypeScript::Monitor(cx, script, regs.pc, regs.sp[-1]);
    if (op == JSOP_CALLFCSLOT)
        PUSH_UNDEFINED();
}
END_CASE(JSOP_GETFCSLOT)

BEGIN_CASE(JSOP_GETGLOBAL)
BEGIN_CASE(JSOP_CALLGLOBAL)
{
    uint32 slot = GET_SLOTNO(regs.pc);
    slot = script->getGlobalSlot(slot);
    JSObject *obj = regs.fp()->scopeChain().getGlobal();
    JS_ASSERT(obj->containsSlot(slot));
    PUSH_COPY(obj->getSlot(slot));
    TypeScript::Monitor(cx, script, regs.pc, regs.sp[-1]);
    if (op == JSOP_CALLGLOBAL)
        PUSH_UNDEFINED();
}
END_CASE(JSOP_GETGLOBAL)

BEGIN_CASE(JSOP_DEFCONST)
BEGIN_CASE(JSOP_DEFVAR)
{
    uint32 index = GET_INDEX(regs.pc);
    JSAtom *atom = atoms[index];

    JSObject *obj = &regs.fp()->varObj();
    JS_ASSERT(!obj->getOps()->defineProperty);
    uintN attrs = JSPROP_ENUMERATE;
    if (!regs.fp()->isEvalFrame())
        attrs |= JSPROP_PERMANENT;

    /* Lookup id in order to check for redeclaration problems. */
    jsid id = ATOM_TO_JSID(atom);
    bool shouldDefine;
    if (op == JSOP_DEFVAR) {
        /*
         * Redundant declaration of a |var|, even one for a non-writable
         * property like |undefined| in ES5, does nothing.
         */
        JSProperty *prop;
        JSObject *obj2;
        if (!obj->lookupProperty(cx, id, &obj2, &prop))
            goto error;
        shouldDefine = (!prop || obj2 != obj);
    } else {
        JS_ASSERT(op == JSOP_DEFCONST);
        attrs |= JSPROP_READONLY;
        if (!CheckRedeclaration(cx, obj, id, attrs))
            goto error;

        /*
         * As attrs includes readonly, CheckRedeclaration can succeed only
         * if prop does not exist.
         */
        shouldDefine = true;
    }

    /* Bind a variable only if it's not yet defined. */
    if (shouldDefine &&
        !DefineNativeProperty(cx, obj, id, UndefinedValue(), PropertyStub, StrictPropertyStub,
                              attrs, 0, 0)) {
        goto error;
    }
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

        obj2 = GetScopeChainFast(cx, regs.fp(), JSOP_DEFFUN, JSOP_DEFFUN_LENGTH);
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
    if (obj->getParent() != obj2) {
        obj = CloneFunctionObject(cx, fun, obj2, true);
        if (!obj)
            goto error;
    }

    /*
     * ECMA requires functions defined when entering Eval code to be
     * impermanent.
     */
    uintN attrs = regs.fp()->isEvalFrame()
                  ? JSPROP_ENUMERATE
                  : JSPROP_ENUMERATE | JSPROP_PERMANENT;

    /*
     * We define the function as a property of the variable object and not the
     * current scope chain even for the case of function expression statements
     * and functions defined by eval inside let or with blocks.
     */
    JSObject *parent = &regs.fp()->varObj();

    /* ES5 10.5 (NB: with subsequent errata). */
    jsid id = ATOM_TO_JSID(fun->atom);
    JSProperty *prop = NULL;
    JSObject *pobj;
    if (!parent->lookupProperty(cx, id, &pobj, &prop))
        goto error;

    Value rval = ObjectValue(*obj);

    do {
        /* Steps 5d, 5f. */
        if (!prop || pobj != parent) {
            if (!parent->defineProperty(cx, id, rval, PropertyStub, StrictPropertyStub, attrs))
                goto error;
            break;
        }

        /* Step 5e. */
        JS_ASSERT(parent->isNative());
        Shape *shape = reinterpret_cast<Shape *>(prop);
        if (parent->isGlobal()) {
            if (shape->configurable()) {
                if (!parent->defineProperty(cx, id, rval, PropertyStub, StrictPropertyStub, attrs))
                    goto error;
                break;
            }

            if (shape->isAccessorDescriptor() || !shape->writable() || !shape->enumerable()) {
                JSAutoByteString bytes;
                if (const char *name = js_ValueToPrintable(cx, IdToValue(id), &bytes)) {
                    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                         JSMSG_CANT_REDEFINE_PROP, name);
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
        if (!parent->setProperty(cx, id, &rval, script->strictModeCode))
            goto error;
    } while (false);
}
END_CASE(JSOP_DEFFUN)

BEGIN_CASE(JSOP_DEFFUN_FC)
{
    JSFunction *fun;
    LOAD_FUNCTION(0);

    JSObject *obj = js_NewFlatClosure(cx, fun, JSOP_DEFFUN_FC, JSOP_DEFFUN_FC_LENGTH);
    if (!obj)
        goto error;

    Value rval = ObjectValue(*obj);

    uintN attrs = regs.fp()->isEvalFrame()
                  ? JSPROP_ENUMERATE
                  : JSPROP_ENUMERATE | JSPROP_PERMANENT;

    JSObject &parent = regs.fp()->varObj();

    jsid id = ATOM_TO_JSID(fun->atom);
    if (!CheckRedeclaration(cx, &parent, id, attrs))
        goto error;

    if ((attrs == JSPROP_ENUMERATE)
        ? !parent.setProperty(cx, id, &rval, script->strictModeCode)
        : !parent.defineProperty(cx, id, rval, PropertyStub, StrictPropertyStub, attrs)) {
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
    JS_ASSERT(!fun->isFlatClosure());
    JSObject *obj = fun;

    if (fun->isNullClosure()) {
        obj = CloneFunctionObject(cx, fun, &regs.fp()->scopeChain(), true);
        if (!obj)
            goto error;
    } else {
        JSObject *parent = GetScopeChainFast(cx, regs.fp(), JSOP_DEFLOCALFUN,
                                             JSOP_DEFLOCALFUN_LENGTH);
        if (!parent)
            goto error;

        if (obj->getParent() != parent) {
#ifdef JS_TRACER
            if (TRACE_RECORDER(cx))
                AbortRecording(cx, "DEFLOCALFUN for closure");
#endif
            obj = CloneFunctionObject(cx, fun, parent, true);
            if (!obj)
                goto error;
        }
    }

    uint32 slot = GET_SLOTNO(regs.pc);
    TRACE_2(DefLocalFunSetSlot, slot, obj);

    regs.fp()->slots()[slot].setObject(*obj);
}
END_CASE(JSOP_DEFLOCALFUN)

BEGIN_CASE(JSOP_DEFLOCALFUN_FC)
{
    JSFunction *fun;
    LOAD_FUNCTION(SLOTNO_LEN);

    JSObject *obj = js_NewFlatClosure(cx, fun, JSOP_DEFLOCALFUN_FC, JSOP_DEFLOCALFUN_FC_LENGTH);
    if (!obj)
        goto error;

    uint32 slot = GET_SLOTNO(regs.pc);
    TRACE_2(DefLocalFunSetSlot, slot, obj);

    regs.fp()->slots()[slot].setObject(*obj);
}
END_CASE(JSOP_DEFLOCALFUN_FC)

BEGIN_CASE(JSOP_LAMBDA)
{
    /* Load the specified function object literal. */
    JSFunction *fun;
    LOAD_FUNCTION(0);
    JSObject *obj = fun;

    /* do-while(0) so we can break instead of using a goto. */
    do {
        JSObject *parent;
        if (fun->isNullClosure()) {
            parent = &regs.fp()->scopeChain();

            if (fun->joinable()) {
                jsbytecode *pc2 = AdvanceOverBlockchainOp(regs.pc + JSOP_LAMBDA_LENGTH);
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
                    JS_ASSERT(obj2->isObject());
#endif

                    fun->setMethodAtom(script->getAtom(GET_FULL_INDEX(pc2 - regs.pc)));
                    break;
                }

                if (op2 == JSOP_SETMETHOD) {
#ifdef DEBUG
                    op2 = js_GetOpcode(cx, script, pc2 + JSOP_SETMETHOD_LENGTH);
                    JS_ASSERT(op2 == JSOP_POP || op2 == JSOP_POPV);
#endif
                    const Value &lref = regs.sp[-1];
                    if (lref.isObject() && lref.toObject().canHaveMethodBarrier()) {
                        fun->setMethodAtom(script->getAtom(GET_FULL_INDEX(pc2 - regs.pc)));
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
                    JSObject *callee;

                    if (IsFunctionObject(cref, &callee)) {
                        JSFunction *calleeFun = callee->getFunctionPrivate();
                        if (Native native = calleeFun->maybeNative()) {
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
            parent = GetScopeChainFast(cx, regs.fp(), JSOP_LAMBDA, JSOP_LAMBDA_LENGTH);
            if (!parent)
                goto error;
        }

        obj = CloneFunctionObject(cx, fun, parent, true);
        if (!obj)
            goto error;
    } while (0);

    JS_ASSERT(obj->getProto());
    PUSH_OBJECT(*obj);
}
END_CASE(JSOP_LAMBDA)

BEGIN_CASE(JSOP_LAMBDA_FC)
{
    JSFunction *fun;
    LOAD_FUNCTION(0);

    JSObject *obj = js_NewFlatClosure(cx, fun, JSOP_LAMBDA_FC, JSOP_LAMBDA_FC_LENGTH);
    if (!obj)
        goto error;

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
        JS_ASSERT(regs.sp - regs.fp()->base() >= 2);
        rval = regs.sp[-1];
        i = -1;
        JSAtom *atom;
        LOAD_ATOM(0, atom);
        id = ATOM_TO_JSID(atom);
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

    PropertyOp getter;
    StrictPropertyOp setter;
    if (op == JSOP_GETTER) {
        getter = CastAsPropertyOp(&rval.toObject());
        setter = StrictPropertyStub;
        attrs = JSPROP_GETTER;
    } else {
        getter = PropertyStub;
        setter = CastAsStrictPropertyOp(&rval.toObject());
        attrs = JSPROP_SETTER;
    }
    attrs |= JSPROP_ENUMERATE | JSPROP_SHARED;

    /* Check for a readonly or permanent property of the same name. */
    if (!CheckRedeclaration(cx, obj, id, attrs))
        goto error;

    if (!obj->defineProperty(cx, id, UndefinedValue(), getter, setter, attrs))
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
    jsint i = regs.pc[1];

    JS_ASSERT(i == JSProto_Array || i == JSProto_Object);
    JSObject *obj;

    if (i == JSProto_Array) {
        obj = NewDenseEmptyArray(cx);
    } else {
        gc::AllocKind kind = GuessObjectGCKind(0, false);
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
    JSObject *baseobj;
    LOAD_OBJECT(0, baseobj);

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

    /*
     * Probe the property cache.
     *
     * On a hit, if the cached shape has a non-default setter, it must be
     * __proto__. If shape->previous() != obj->lastProperty(), there must be a
     * repeated property name. The fast path does not handle these two cases.
     */
    PropertyCacheEntry *entry;
    const Shape *shape;
    if (JS_PROPERTY_CACHE(cx).testForInit(rt, regs.pc, obj, &shape, &entry) &&
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
                  obj->shape() == obj->lastProperty()->shapeid);
        obj->extend(cx, shape);

        /*
         * No method change check here because here we are adding a new
         * property, not updating an existing slot's value that might
         * contain a method of a branded shape.
         */
        TRACE_1(AddProperty, obj);
        obj->nativeSetSlotWithType(cx, shape, rval);
    } else {
        PCMETER(JS_PROPERTY_CACHE(cx).inipcmisses++);

        /* Get the immediate property name into id. */
        JSAtom *atom;
        LOAD_ATOM(0, atom);
        jsid id = ATOM_TO_JSID(atom);

        uintN defineHow = (op == JSOP_INITMETHOD)
                          ? DNP_CACHE_RESULT | DNP_SET_METHOD
                          : DNP_CACHE_RESULT;
        if (JS_UNLIKELY(atom == cx->runtime->atomState.protoAtom)
            ? !js_SetPropertyHelper(cx, obj, id, defineHow, &rval, script->strictModeCode)
            : !DefineNativeProperty(cx, obj, id, rval, NULL, NULL,
                                    JSPROP_ENUMERATE, 0, 0, defineHow)) {
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
        JS_ASSERT(jsuint(JSID_TO_INT(id)) < StackSpace::ARGS_LENGTH_MAX);
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
    JS_ASSERT(slot + 1 < regs.fp()->numFixed());
    const Value &lref = regs.fp()->slots()[slot];
    JSObject *obj;
    if (lref.isObject()) {
        obj = &lref.toObject();
    } else {
        JS_ASSERT(lref.isUndefined());
        obj = NewDenseEmptyArray(cx);
        if (!obj)
            goto error;
        regs.fp()->slots()[slot].setObject(*obj);
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
    JS_ASSERT(slot + 1 < regs.fp()->numFixed());
    const Value &lref = regs.fp()->slots()[slot];
    jsint i = (jsint) GET_UINT16(regs.pc + UINT16_LEN);
    Value rval;
    if (lref.isUndefined()) {
        rval.setUndefined();
    } else {
        JSObject *obj = &regs.fp()->slots()[slot].toObject();
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
    JS_ASSERT(slot + 1 < regs.fp()->numFixed());
    Value *vp = &regs.fp()->slots()[slot];
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
    jsint i = (regs.pc - script->code) + JSOP_GOSUB_LENGTH;
    PUSH_INT32(i);
    len = GET_JUMP_OFFSET(regs.pc);
END_VARLEN_CASE
}

{
BEGIN_CASE(JSOP_GOSUBX)
    PUSH_BOOLEAN(false);
    jsint i = (regs.pc - script->code) + JSOP_GOSUBX_LENGTH;
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
#if defined(JS_TRACER) && defined(JS_METHODJIT)
    if (interpMode == JSINTERP_PROFILE) {
        leaveOnSafePoint = true;
        LEAVE_ON_SAFE_POINT();
    }
#endif
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
{
    /*
     * The stack must have a block with at least one local slot below the
     * exception object.
     */
    JS_ASSERT((size_t) (regs.sp - regs.fp()->base()) >= 2);
    uint32 slot = GET_UINT16(regs.pc);
    JS_ASSERT(slot + 1 < script->nslots);
    POP_COPY_TO(regs.fp()->slots()[slot]);
}
END_CASE(JSOP_SETLOCALPOP)

BEGIN_CASE(JSOP_IFCANTCALLTOP)
    /*
     * If the top of stack is of primitive type, jump to our target. Otherwise
     * advance to the next opcode.
     */
    JS_ASSERT(regs.sp > regs.fp()->base());
    if (!js_IsCallable(regs.sp[-1])) {
        len = GET_JUMP_OFFSET(regs.pc);
        BRANCH(len);
    }
END_CASE(JSOP_IFCANTCALLTOP)

BEGIN_CASE(JSOP_PRIMTOP)
    JS_ASSERT(regs.sp > regs.fp()->base());
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

BEGIN_CASE(JSOP_DEBUGGER)
{
    JSTrapStatus st = JSTRAP_CONTINUE;
    Value rval;
    if (JSDebuggerHandler handler = cx->debugHooks->debuggerHandler)
        st = handler(cx, script, regs.pc, Jsvalify(&rval), cx->debugHooks->debuggerHandlerData);
    if (st == JSTRAP_CONTINUE)
        st = Debugger::onDebuggerStatement(cx, &rval);
    switch (st) {
      case JSTRAP_ERROR:
        goto error;
      case JSTRAP_CONTINUE:
        break;
      case JSTRAP_RETURN:
        regs.fp()->setReturnValue(rval);
        interpReturnOK = JS_TRUE;
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
    PUSH_STRING(atom);
}
END_CASE(JSOP_QNAMEPART)

BEGIN_CASE(JSOP_QNAMECONST)
{
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
    if (!obj->setProperty(cx, id, &rval, script->strictModeCode))
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
        PUSH_IMPLICIT_THIS(cx, obj, rval);
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
        if (!EnterWith(cx, -2, JSOP_ENDFILTER, JSOP_ENDFILTER_LENGTH))
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
    JSString *str = atom;
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
    JSString *str = atom;
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
    JSString *str = atom;
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
    if (!cx->fp()->scopeChain().getGlobal()->getFunctionNamespace(cx, &rval))
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
    JS_ASSERT(regs.fp()->base() + OBJ_BLOCK_DEPTH(cx, obj) == regs.sp);
    Value *vp = regs.sp + OBJ_BLOCK_COUNT(cx, obj);
    JS_ASSERT(regs.sp < vp);
    JS_ASSERT(vp <= regs.fp()->slots() + script->nslots);
    SetValueRangeToUndefined(regs.sp, vp);
    regs.sp = vp;

#ifdef DEBUG
    /*
     * The young end of fp->scopeChain may omit blocks if we haven't closed
     * over them, but if there are any closure blocks on fp->scopeChain, they'd
     * better be (clones of) ancestors of the block we're entering now;
     * anything else we should have popped off fp->scopeChain when we left its
     * static scope.
     */
    JSObject *obj2 = &regs.fp()->scopeChain();
    while (obj2->isWith())
        obj2 = obj2->getParent();
    if (obj2->isBlock() &&
        obj2->getPrivate() == js_FloatingFrameIfGenerator(cx, regs.fp())) {
        JSObject *youngestProto = obj2->getProto();
        JS_ASSERT(youngestProto->isStaticBlock());
        JSObject *parent = obj;
        while ((parent = parent->getParent()) != youngestProto)
            JS_ASSERT(parent);
    }
#endif
}
END_CASE(JSOP_ENTERBLOCK)

BEGIN_CASE(JSOP_LEAVEBLOCKEXPR)
BEGIN_CASE(JSOP_LEAVEBLOCK)
{
    JSObject *blockChain;
    LOAD_OBJECT(UINT16_LEN, blockChain);
#ifdef DEBUG
    JS_ASSERT(blockChain->isStaticBlock());
    uintN blockDepth = OBJ_BLOCK_DEPTH(cx, blockChain);
    JS_ASSERT(blockDepth <= StackDepth(script));
#endif
    /*
     * If we're about to leave the dynamic scope of a block that has been
     * cloned onto fp->scopeChain, clear its private data, move its locals from
     * the stack into the clone, and pop it off the chain.
     */
    JSObject &obj = regs.fp()->scopeChain();
    if (obj.getProto() == blockChain) {
        JS_ASSERT(obj.isClonedBlock());
        if (!js_PutBlockObject(cx, JS_TRUE))
            goto error;
    }

    /* Move the result of the expression to the new topmost stack slot. */
    Value *vp = NULL;  /* silence GCC warnings */
    if (op == JSOP_LEAVEBLOCKEXPR)
        vp = &regs.sp[-1];
    regs.sp -= GET_UINT16(regs.pc);
    if (op == JSOP_LEAVEBLOCKEXPR) {
        JS_ASSERT(regs.fp()->base() + blockDepth == regs.sp - 1);
        regs.sp[-1] = *vp;
    } else {
        JS_ASSERT(regs.fp()->base() + blockDepth == regs.sp);
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
    interpReturnOK = JS_TRUE;
    goto exit;

BEGIN_CASE(JSOP_ARRAYPUSH)
{
    uint32 slot = GET_UINT16(regs.pc);
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
    JS_ASSERT(&cx->regs() == &regs);
#ifdef JS_TRACER
    if (regs.fp()->hasImacropc() && cx->isExceptionPending()) {
        // Handle exceptions as if they came from the imacro-calling pc.
        regs.pc = regs.fp()->imacropc();
        regs.fp()->clearImacropc();
    }
#endif

    JS_ASSERT(size_t((regs.fp()->hasImacropc() ? regs.fp()->imacropc() : regs.pc) - script->code) <
              script->length);

#ifdef JS_TRACER
    /*
     * This abort could be weakened to permit tracing through exceptions that
     * are thrown and caught within a loop, with the co-operation of the tracer.
     * For now just bail on any sign of trouble.
     */
    if (TRACE_RECORDER(cx))
        AbortRecording(cx, "error or exception while recording");
# ifdef JS_METHODJIT
    if (TRACE_PROFILER(cx))
        AbortProfiling(cx);
# endif
#endif

    if (!cx->isExceptionPending()) {
        /* This is an error, not a catchable exception, quit the frame ASAP. */
        interpReturnOK = JS_FALSE;
    } else {
        JSThrowHook handler;
        JSTryNote *tn, *tnlimit;
        uint32 offset;

        /* Restore atoms local in case we will resume. */
        atoms = script->atoms;

        /* Call debugger throw hook if set. */
        if (cx->debugHooks->throwHook || !cx->compartment->getDebuggees().empty()) {
            Value rval;
            JSTrapStatus st = Debugger::onExceptionUnwind(cx, &rval);
            if (st == JSTRAP_CONTINUE) {
                handler = cx->debugHooks->throwHook;
                if (handler)
                    st = handler(cx, script, regs.pc, Jsvalify(&rval), cx->debugHooks->throwHookData);
            }

            switch (st) {
              case JSTRAP_ERROR:
                cx->clearPendingException();
                goto error;
              case JSTRAP_RETURN:
                cx->clearPendingException();
                regs.fp()->setReturnValue(rval);
                interpReturnOK = JS_TRUE;
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

        offset = (uint32)(regs.pc - script->main());
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

            JSBool ok = UnwindScope(cx, tn->stackDepth, JS_TRUE);
            JS_ASSERT(regs.sp == regs.fp()->base() + tn->stackDepth);
            if (!ok) {
                /*
                 * Restart the handler search with updated pc and stack depth
                 * to properly notify the debugger.
                 */
                goto error;
            }

            switch (tn->kind) {
              case JSTRY_CATCH:
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
                JS_ASSERT(js_GetOpcode(cx, regs.fp()->script(), regs.pc) == JSOP_ENDITER);
                Value v = cx->getPendingException();
                cx->clearPendingException();
                ok = js_CloseIterator(cx, &regs.sp[-1].toObject());
                regs.sp -= 1;
                if (!ok)
                    goto error;
                cx->setPendingException(v);
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
        if (JS_UNLIKELY(cx->isExceptionPending() &&
                        cx->getPendingException().isMagic(JS_GENERATOR_CLOSING))) {
            cx->clearPendingException();
            interpReturnOK = JS_TRUE;
            regs.fp()->clearReturnValue();
        }
#endif
    }

  forced_return:
    /*
     * Unwind the scope making sure that interpReturnOK stays false even when
     * UnwindScope returns true.
     *
     * When a trap handler returns JSTRAP_RETURN, we jump here with
     * interpReturnOK set to true bypassing any finally blocks.
     */
    interpReturnOK &= UnwindScope(cx, 0, interpReturnOK || cx->isExceptionPending());
    JS_ASSERT(regs.sp == regs.fp()->base());

    if (entryFrame != regs.fp())
        goto inline_return;

  exit:
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

#ifdef JS_TRACER
    JS_ASSERT_IF(interpReturnOK && interpMode == JSINTERP_RECORD, !TRACE_RECORDER(cx));
    if (TRACE_RECORDER(cx))
        AbortRecording(cx, "recording out of Interpret");
# ifdef JS_METHODJIT
    if (TRACE_PROFILER(cx))
        AbortProfiling(cx);
# endif
#endif

    JS_ASSERT_IF(!regs.fp()->isGeneratorFrame(),
                 !IsActiveWithOrBlock(cx, regs.fp()->scopeChain(), 0));

#ifdef JS_METHODJIT
    /*
     * This path is used when it's guaranteed the method can be finished
     * inside the JIT.
     */
  leave_on_safe_point:
#endif

    return interpReturnOK;

  atom_not_defined:
    {
        JSAutoByteString printable;
        if (js_AtomToPrintableString(cx, atomNotDefined, &printable))
            js_ReportIsNotDefined(cx, printable.ptr());
    }
    goto error;
}
