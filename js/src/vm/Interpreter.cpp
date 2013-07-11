/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * JavaScript bytecode interpreter.
 */

#include "vm/Interpreter.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/PodOperations.h"

#include <string.h>

#include "jsapi.h"
#include "jsarray.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsdbgapi.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jsiter.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsopcode.h"
#include "jsprf.h"
#include "jsscript.h"
#include "jsstr.h"
#include "builtin/Eval.h"
#include "ion/BaselineJIT.h"
#include "ion/Ion.h"
#include "vm/Debugger.h"
#include "vm/Shape.h"

#include "jsatominlines.h"
#include "jsboolinlines.h"
#include "jsinferinlines.h"
#include "jsscriptinlines.h"

#include "builtin/Iterator-inl.h"
#include "ion/IonFrames-inl.h"
#include "vm/Interpreter-inl.h"
#include "vm/Probes-inl.h"
#include "vm/Stack-inl.h"

#include "jsautooplen.h"

#if JS_TRACE_LOGGING
#include "TraceLogging.h"
#endif

using namespace js;
using namespace js::gc;
using namespace js::types;

using mozilla::DebugOnly;
using mozilla::PodCopy;

/* Some objects (e.g., With) delegate 'this' to another object. */
static inline JSObject *
CallThisObjectHook(JSContext *cx, HandleObject obj, Value *argv)
{
    JSObject *thisp = JSObject::thisObject(cx, obj);
    if (!thisp)
        return NULL;
    argv[-1].setObject(*thisp);
    return thisp;
}

/*
 * Note: when Clang 3.2 (32-bit) inlines the two functions below in Interpret,
 * the conservative stack scanner leaks a ton of memory and this negatively
 * influences performance. The JS_NEVER_INLINE is a temporary workaround until
 * we can remove the conservative scanner. See bug 849526 for more info.
 */
#if defined(__clang__) && defined(JS_CPU_X86)
static JS_NEVER_INLINE bool
#else
static bool
#endif
ToBooleanOp(const FrameRegs &regs)
{
    return ToBoolean(regs.sp[-1]);
}

template <bool Eq>
#if defined(__clang__) && defined(JS_CPU_X86)
static JS_NEVER_INLINE bool
#else
static bool
#endif
LooseEqualityOp(JSContext *cx, FrameRegs &regs)
{
    Value rval = regs.sp[-1];
    Value lval = regs.sp[-2];
    bool cond;
    if (!LooselyEqual(cx, lval, rval, &cond))
        return false;
    cond = (cond == Eq);
    regs.sp--;
    regs.sp[-1].setBoolean(cond);
    return true;
}

bool
js::BoxNonStrictThis(JSContext *cx, MutableHandleValue thisv, bool *modified)
{
    /*
     * Check for SynthesizeFrame poisoning and fast constructors which
     * didn't check their callee properly.
     */
    JS_ASSERT(!thisv.isMagic());
    *modified = false;

    if (thisv.isNullOrUndefined()) {
        Rooted<GlobalObject*> global(cx, cx->global());
        JSObject *thisp = JSObject::thisObject(cx, global);
        if (!thisp)
            return false;
        thisv.set(ObjectValue(*thisp));
        *modified = true;
        return true;
    }

    if (!thisv.isObject()) {
        if (!js_PrimitiveToObject(cx, thisv.address()))
            return false;
        *modified = true;
    }

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
bool
js::BoxNonStrictThis(JSContext *cx, const CallReceiver &call)
{
    /*
     * Check for SynthesizeFrame poisoning and fast constructors which
     * didn't check their callee properly.
     */
    RootedValue thisv(cx, call.thisv());
    JS_ASSERT(!thisv.isMagic());

#ifdef DEBUG
    JSFunction *fun = call.callee().is<JSFunction>() ? &call.callee().as<JSFunction>() : NULL;
    JS_ASSERT_IF(fun && fun->isInterpreted(), !fun->strict());
#endif

    bool modified;
    if (!BoxNonStrictThis(cx, &thisv, &modified))
        return false;
    if (modified)
        call.setThis(thisv);

    return true;
}

#if JS_HAS_NO_SUCH_METHOD

const uint32_t JSSLOT_FOUND_FUNCTION  = 0;
const uint32_t JSSLOT_SAVED_ID        = 1;

Class js_NoSuchMethodClass = {
    "NoSuchMethod",
    JSCLASS_HAS_RESERVED_SLOTS(2) | JSCLASS_IS_ANONYMOUS,
    JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
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
js::OnUnknownMethod(JSContext *cx, HandleObject obj, Value idval_, MutableHandleValue vp)
{
    RootedValue idval(cx, idval_);

    RootedValue value(cx);
    if (!JSObject::getProperty(cx, obj, obj, cx->names().noSuchMethod, &value))
        return false;

    TypeScript::MonitorUnknown(cx);

    if (value.isObject()) {
        JSObject *obj = NewObjectWithClassProto(cx, &js_NoSuchMethodClass, NULL, NULL);
        if (!obj)
            return false;

        obj->setSlot(JSSLOT_FOUND_FUNCTION, value);
        obj->setSlot(JSSLOT_SAVED_ID, idval);
        vp.setObject(*obj);
    }
    return true;
}

static JSBool
NoSuchMethod(JSContext *cx, unsigned argc, Value *vp)
{
    InvokeArgs args(cx);
    if (!args.init(2))
        return JS_FALSE;

    JS_ASSERT(vp[0].isObject());
    JS_ASSERT(vp[1].isObject());
    JSObject *obj = &vp[0].toObject();
    JS_ASSERT(obj->getClass() == &js_NoSuchMethodClass);

    args.setCallee(obj->getSlot(JSSLOT_FOUND_FUNCTION));
    args.setThis(vp[1]);
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

inline bool
GetPropertyOperation(JSContext *cx, StackFrame *fp, HandleScript script, jsbytecode *pc,
                     MutableHandleValue lval, MutableHandleValue vp)
{
    JSOp op = JSOp(*pc);

    if (op == JSOP_LENGTH) {
        if (IsOptimizedArguments(fp, lval.address())) {
            vp.setInt32(fp->numActualArgs());
            return true;
        }

        if (GetLengthProperty(lval, vp))
            return true;
    }

    JSObject *obj = ToObjectFromStack(cx, lval);
    if (!obj)
        return false;

    bool wasObject = lval.isObject();

    RootedId id(cx, NameToId(script->getName(pc)));
    RootedObject nobj(cx, obj);

    if (obj->getOps()->getProperty) {
        if (!JSObject::getGeneric(cx, nobj, nobj, id, vp))
            return false;
    } else {
        if (!GetPropertyHelper(cx, nobj, id, 0, vp))
            return false;
    }

#if JS_HAS_NO_SUCH_METHOD
    if (op == JSOP_CALLPROP &&
        JS_UNLIKELY(vp.isPrimitive()) &&
        wasObject)
    {
        if (!OnUnknownMethod(cx, nobj, IdToValue(id), vp))
            return false;
    }
#endif

    return true;
}

static inline bool
NameOperation(JSContext *cx, StackFrame *fp, jsbytecode *pc, MutableHandleValue vp)
{
    JSObject *obj = fp->scopeChain();
    PropertyName *name = fp->script()->getName(pc);

    /*
     * Skip along the scope chain to the enclosing global object. This is
     * used for GNAME opcodes where the bytecode emitter has determined a
     * name access must be on the global. It also insulates us from bugs
     * in the emitter: type inference will assume that GNAME opcodes are
     * accessing the global object, and the inferred behavior should match
     * the actual behavior even if the id could be found on the scope chain
     * before the global object.
     */
    if (IsGlobalOp(JSOp(*pc)))
        obj = &obj->global();

    Shape *shape = NULL;
    JSObject *scope = NULL, *pobj = NULL;
    if (LookupNameNoGC(cx, name, obj, &scope, &pobj, &shape)) {
        if (FetchNameNoGC(pobj, shape, vp))
            return true;
    }

    RootedObject objRoot(cx, obj), scopeRoot(cx), pobjRoot(cx);
    RootedPropertyName nameRoot(cx, name);
    RootedShape shapeRoot(cx);

    if (!LookupName(cx, nameRoot, objRoot, &scopeRoot, &pobjRoot, &shapeRoot))
        return false;

    /* Kludge to allow (typeof foo == "undefined") tests. */
    JSOp op2 = JSOp(pc[JSOP_NAME_LENGTH]);
    if (op2 == JSOP_TYPEOF)
        return FetchName<true>(cx, scopeRoot, pobjRoot, nameRoot, shapeRoot, vp);
    return FetchName<false>(cx, scopeRoot, pobjRoot, nameRoot, shapeRoot, vp);
}

inline bool
SetPropertyOperation(JSContext *cx, HandleScript script, jsbytecode *pc, HandleValue lval,
                     HandleValue rval)
{
    JS_ASSERT(*pc == JSOP_SETPROP);

    RootedObject obj(cx, ToObjectFromStack(cx, lval));
    if (!obj)
        return false;

    RootedValue rref(cx, rval);

    RootedId id(cx, NameToId(script->getName(pc)));
    if (JS_LIKELY(!obj->getOps()->setProperty)) {
        if (!baseops::SetPropertyHelper(cx, obj, obj, id, 0, &rref, script->strict))
            return false;
    } else {
        if (!JSObject::setGeneric(cx, obj, obj, id, &rref, script->strict))
            return false;
    }

    return true;
}

bool
js::ReportIsNotFunction(JSContext *cx, const Value &v, int numToSkip, MaybeConstruct construct)
{
    unsigned error = construct ? JSMSG_NOT_CONSTRUCTOR : JSMSG_NOT_FUNCTION;
    int spIndex = numToSkip >= 0 ? -(numToSkip + 1) : JSDVG_SEARCH_STACK;

    RootedValue val(cx, v);
    js_ReportValueError3(cx, error, spIndex, val, NullPtr(), NULL, NULL);
    return false;
}

JSObject *
js::ValueToCallable(JSContext *cx, const Value &v, int numToSkip, MaybeConstruct construct)
{
    if (v.isObject()) {
        JSObject *callable = &v.toObject();
        if (callable->isCallable())
            return callable;
    }

    ReportIsNotFunction(cx, v, numToSkip, construct);
    return NULL;
}

static JS_NEVER_INLINE bool
Interpret(JSContext *cx, RunState &state);

StackFrame *
InvokeState::pushInterpreterFrame(JSContext *cx, FrameGuard *fg)
{
    return cx->runtime()->interpreterStack().pushInvokeFrame(cx, args_, initial_, fg);
}

StackFrame *
ExecuteState::pushInterpreterFrame(JSContext *cx, FrameGuard *fg)
{
    return cx->runtime()->interpreterStack().pushExecuteFrame(cx, script_, thisv_, scopeChain_,
                                                              type_, evalInFrame_, fg);
}

bool
js::RunScript(JSContext *cx, RunState &state)
{
    JS_CHECK_RECURSION(cx, return false);

    SPSEntryMarker marker(cx->runtime());

#ifdef JS_ION
    if (ion::IsEnabled(cx)) {
        ion::MethodStatus status = ion::CanEnter(cx, state);
        if (status == ion::Method_Error)
            return false;
        if (status == ion::Method_Compiled) {
            ion::IonExecStatus status = ion::Cannon(cx, state);
            return !IsErrorStatus(status);
        }
    }

    if (ion::IsBaselineEnabled(cx)) {
        ion::MethodStatus status = ion::CanEnterBaselineMethod(cx, state);
        if (status == ion::Method_Error)
            return false;
        if (status == ion::Method_Compiled) {
            ion::IonExecStatus status = ion::EnterBaselineMethod(cx, state);
            return !IsErrorStatus(status);
        }
    }
#endif

    if (state.isInvoke()) {
        InvokeState &invoke = *state.asInvoke();
        TypeMonitorCall(cx, invoke.args(), invoke.constructing());
    }

    return Interpret(cx, state);
}

/*
 * Find a function reference and its 'this' value implicit first parameter
 * under argc arguments on cx's stack, and call the function.  Push missing
 * required arguments, allocate declared local variables, and pop everything
 * when done.  Then push the return value.
 */
bool
js::Invoke(JSContext *cx, CallArgs args, MaybeConstruct construct)
{
    JS_ASSERT(args.length() <= ARGS_LENGTH_MAX);
    JS_ASSERT(!cx->compartment()->activeAnalysis);

    /* We should never enter a new script while cx->iterValue is live. */
    JS_ASSERT(cx->iterValue.isMagic(JS_NO_ITER_VALUE));

    /* MaybeConstruct is a subset of InitialFrameFlags */
    InitialFrameFlags initial = (InitialFrameFlags) construct;

    if (args.calleev().isPrimitive())
        return ReportIsNotFunction(cx, args.calleev().get(), args.length() + 1, construct);

    JSObject &callee = args.callee();
    Class *clasp = callee.getClass();

    /* Invoke non-functions. */
    if (JS_UNLIKELY(clasp != &JSFunction::class_)) {
#if JS_HAS_NO_SUCH_METHOD
        if (JS_UNLIKELY(clasp == &js_NoSuchMethodClass))
            return NoSuchMethod(cx, args.length(), args.base());
#endif
        JS_ASSERT_IF(construct, !clasp->construct);
        if (!clasp->call)
            return ReportIsNotFunction(cx, args.calleev().get(), args.length() + 1, construct);
        return CallJSNative(cx, clasp->call, args);
    }

    /* Invoke native functions. */
    JSFunction *fun = &callee.as<JSFunction>();
    JS_ASSERT_IF(construct, !fun->isNativeConstructor());
    if (fun->isNative())
        return CallJSNative(cx, fun->native(), args);

    if (!fun->getOrCreateScript(cx))
        return false;

    /* Run function until JSOP_STOP, JSOP_RETURN or error. */
    InvokeState state(cx, args, initial);

    // Check to see if useNewType flag should be set for this frame.
    if (construct && cx->typeInferenceEnabled()) {
        ScriptFrameIter iter(cx);
        if (!iter.done()) {
            JSScript *script = iter.script();
            jsbytecode *pc = iter.pc();
            if (UseNewType(cx, script, pc))
                state.setUseNewType();
        }
    }

    JSBool ok = RunScript(cx, state);

    JS_ASSERT_IF(ok && construct, !args.rval().isPrimitive());
    return ok;
}

bool
js::Invoke(JSContext *cx, const Value &thisv, const Value &fval, unsigned argc, Value *argv,
           MutableHandleValue rval)
{
    InvokeArgs args(cx);
    if (!args.init(argc))
        return false;

    args.setCallee(fval);
    args.setThis(thisv);
    PodCopy(args.array(), argv, argc);

    if (args.thisv().isObject()) {
        /*
         * We must call the thisObject hook in case we are not called from the
         * interpreter, where a prior bytecode has computed an appropriate
         * |this| already.
         */
        RootedObject thisObj(cx, &args.thisv().toObject());
        JSObject *thisp = JSObject::thisObject(cx, thisObj);
        if (!thisp)
             return false;
        args.setThis(ObjectValue(*thisp));
    }

    if (!Invoke(cx, args))
        return false;

    rval.set(args.rval());
    return true;
}

bool
js::InvokeConstructor(JSContext *cx, CallArgs args)
{
    JS_ASSERT(!JSFunction::class_.construct);

    args.setThis(MagicValue(JS_IS_CONSTRUCTING));

    if (!args.calleev().isObject())
        return ReportIsNotFunction(cx, args.calleev().get(), args.length() + 1, CONSTRUCT);

    JSObject &callee = args.callee();
    if (callee.is<JSFunction>()) {
        RootedFunction fun(cx, &callee.as<JSFunction>());

        if (fun->isNativeConstructor()) {
            bool ok = CallJSNativeConstructor(cx, fun->native(), args);
            return ok;
        }

        if (!fun->isInterpretedConstructor())
            return ReportIsNotFunction(cx, args.calleev().get(), args.length() + 1, CONSTRUCT);

        if (!Invoke(cx, args, CONSTRUCT))
            return false;

        JS_ASSERT(args.rval().isObject());
        return true;
    }

    Class *clasp = callee.getClass();
    if (!clasp->construct)
        return ReportIsNotFunction(cx, args.calleev().get(), args.length() + 1, CONSTRUCT);

    return CallJSNativeConstructor(cx, clasp->construct, args);
}

bool
js::InvokeConstructor(JSContext *cx, Value fval, unsigned argc, Value *argv, Value *rval)
{
    InvokeArgs args(cx);
    if (!args.init(argc))
        return false;

    args.setCallee(fval);
    args.setThis(MagicValue(JS_THIS_POISON));
    PodCopy(args.array(), argv, argc);

    if (!InvokeConstructor(cx, args))
        return false;

    *rval = args.rval();
    return true;
}

bool
js::InvokeGetterOrSetter(JSContext *cx, JSObject *obj, Value fval, unsigned argc,
                         Value *argv, MutableHandleValue rval)
{
    /*
     * Invoke could result in another try to get or set the same id again, see
     * bug 355497.
     */
    JS_CHECK_RECURSION(cx, return false);

    return Invoke(cx, ObjectValue(*obj), fval, argc, argv, rval);
}

bool
js::ExecuteKernel(JSContext *cx, HandleScript script, JSObject &scopeChainArg, const Value &thisv,
                  ExecuteType type, AbstractFramePtr evalInFrame, Value *result)
{
    JS_ASSERT_IF(evalInFrame, type == EXECUTE_DEBUG);
    JS_ASSERT_IF(type == EXECUTE_GLOBAL, !scopeChainArg.is<ScopeObject>());

    if (script->isEmpty()) {
        if (result)
            result->setUndefined();
        return true;
    }

    TypeScript::SetThis(cx, script, thisv);

    Probes::startExecution(script);
    ExecuteState state(cx, script, thisv, scopeChainArg, type, evalInFrame, result);
    bool ok = RunScript(cx, state);
    Probes::stopExecution(script);

    return ok;
}

bool
js::Execute(JSContext *cx, HandleScript script, JSObject &scopeChainArg, Value *rval)
{
    /* The scope chain could be anything, so innerize just in case. */
    RootedObject scopeChain(cx, &scopeChainArg);
    scopeChain = GetInnerObject(cx, scopeChain);
    if (!scopeChain)
        return false;

    /* Ensure the scope chain is all same-compartment and terminates in a global. */
#ifdef DEBUG
    JSObject *s = scopeChain;
    do {
        assertSameCompartment(cx, s);
        JS_ASSERT_IF(!s->enclosingScope(), s->is<GlobalObject>());
    } while ((s = s->enclosingScope()));
#endif

    /* The VAROBJFIX option makes varObj == globalObj in global code. */
    if (!cx->hasOption(JSOPTION_VAROBJFIX)) {
        if (!scopeChain->setVarObj(cx))
            return false;
    }

    /* Use the scope chain as 'this', modulo outerization. */
    JSObject *thisObj = JSObject::thisObject(cx, scopeChain);
    if (!thisObj)
        return false;
    Value thisv = ObjectValue(*thisObj);

    return ExecuteKernel(cx, script, *scopeChain, thisv, EXECUTE_GLOBAL,
                         NullFramePtr() /* evalInFrame */, rval);
}

bool
js::HasInstance(JSContext *cx, HandleObject obj, HandleValue v, JSBool *bp)
{
    Class *clasp = obj->getClass();
    RootedValue local(cx, v);
    if (clasp->hasInstance)
        return clasp->hasInstance(cx, obj, &local, bp);

    RootedValue val(cx, ObjectValue(*obj));
    js_ReportValueError(cx, JSMSG_BAD_INSTANCEOF_RHS,
                        JSDVG_SEARCH_STACK, val, NullPtr());
    return JS_FALSE;
}

bool
js::LooselyEqual(JSContext *cx, const Value &lval, const Value &rval, bool *result)
{
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
            *result = l == r;
            return true;
        }

        *result = lval.payloadAsRawUint32() == rval.payloadAsRawUint32();
        return true;
    }

    if (lval.isNullOrUndefined()) {
        *result = rval.isNullOrUndefined() ||
                  (rval.isObject() && EmulatesUndefined(&rval.toObject()));
        return true;
    }

    if (rval.isNullOrUndefined()) {
        *result = (lval.isObject() && EmulatesUndefined(&lval.toObject()));
        return true;
    }

    RootedValue lvalue(cx, lval);
    RootedValue rvalue(cx, rval);

    if (!ToPrimitive(cx, &lvalue))
        return false;
    if (!ToPrimitive(cx, &rvalue))
        return false;

    if (lvalue.get().isString() && rvalue.get().isString()) {
        JSString *l = lvalue.get().toString();
        JSString *r = rvalue.get().toString();
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
    return v.isDouble() && mozilla::IsNegativeZero(v.toDouble());
}

static inline bool
IsNaN(const Value &v)
{
    return v.isDouble() && mozilla::IsNaN(v.toDouble());
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
    if (v.isObject()) {
        RootedObject obj(cx, &v.toObject());
        return baseops::TypeOf(cx, obj);
    }
    JS_ASSERT(v.isBoolean());
    return JSTYPE_BOOLEAN;
}

/*
 * Enter the new with scope using an object at sp[-1] and associate the depth
 * of the with block with sp + stackIndex.
 */
static bool
EnterWith(JSContext *cx, AbstractFramePtr frame, HandleValue val, uint32_t stackDepth)
{
    RootedObject obj(cx);
    if (val.isObject()) {
        obj = &val.toObject();
    } else {
        obj = js_ValueToNonNullObject(cx, val);
        if (!obj)
            return false;
    }

    RootedObject scopeChain(cx, frame.scopeChain());
    WithObject *withobj = WithObject::create(cx, obj, scopeChain, stackDepth);
    if (!withobj)
        return false;

    frame.pushOnScopeChain(*withobj);
    return true;
}

/* Unwind block and scope chains to match the given depth. */
void
js::UnwindScope(JSContext *cx, AbstractFramePtr frame, uint32_t stackDepth)
{
    JS_ASSERT_IF(frame.isStackFrame(), frame.asStackFrame() == cx->interpreterFrame());
    JS_ASSERT_IF(frame.isStackFrame(), stackDepth <= cx->interpreterRegs().stackDepth());

    for (ScopeIter si(frame, cx); !si.done(); ++si) {
        switch (si.type()) {
          case ScopeIter::Block:
            if (si.staticBlock().stackDepth() < stackDepth)
                return;
            frame.popBlock(cx);
            break;
          case ScopeIter::With:
            if (si.scope().as<WithObject>().stackDepth() < stackDepth)
                return;
            frame.popWith(cx);
            break;
          case ScopeIter::Call:
          case ScopeIter::StrictEvalScope:
            break;
        }
    }
}

void
js::UnwindForUncatchableException(JSContext *cx, const FrameRegs &regs)
{
    /* c.f. the regular (catchable) TryNoteIter loop in Interpret. */
    for (TryNoteIter tni(cx, regs); !tni.done(); ++tni) {
        JSTryNote *tn = *tni;
        if (tn->kind == JSTRY_ITER) {
            Value *sp = regs.spForStackDepth(tn->stackDepth);
            UnwindIteratorForUncatchableException(cx, &sp[-1].toObject());
        }
    }
}

TryNoteIter::TryNoteIter(JSContext *cx, const FrameRegs &regs)
  : regs(regs),
    script(cx, regs.fp()->script()),
    pcOffset(regs.pc - script->main())
{
    if (script->hasTrynotes()) {
        tn = script->trynotes()->vector;
        tnEnd = tn + script->trynotes()->length;
    } else {
        tn = tnEnd = NULL;
    }
    settle();
}

void
TryNoteIter::operator++()
{
    ++tn;
    settle();
}

bool
TryNoteIter::done() const
{
    return tn == tnEnd;
}

void
TryNoteIter::settle()
{
    for (; tn != tnEnd; ++tn) {
        /* If pc is out of range, try the next one. */
        if (pcOffset - tn->start >= tn->length)
            continue;

        /*
         * We have a note that covers the exception pc but we must check
         * whether the interpreter has already executed the corresponding
         * handler. This is possible when the executed bytecode implements
         * break or return from inside a for-in loop.
         *
         * In this case the emitter generates additional [enditer] and [gosub]
         * opcodes to close all outstanding iterators and execute the finally
         * blocks. If such an [enditer] throws an exception, its pc can still
         * be inside several nested for-in loops and try-finally statements
         * even if we have already closed the corresponding iterators and
         * invoked the finally blocks.
         *
         * To address this, we make [enditer] always decrease the stack even
         * when its implementation throws an exception. Thus already executed
         * [enditer] and [gosub] opcodes will have try notes with the stack
         * depth exceeding the current one and this condition is what we use to
         * filter them out.
         */
        if (tn->stackDepth <= regs.stackDepth())
            break;
    }
}

#define PUSH_COPY(v)             do { *regs.sp++ = v; assertSameCompartmentDebugOnly(cx, regs.sp[-1]); } while (0)
#define PUSH_COPY_SKIP_CHECK(v)  *regs.sp++ = v
#define PUSH_NULL()              regs.sp++->setNull()
#define PUSH_UNDEFINED()         regs.sp++->setUndefined()
#define PUSH_BOOLEAN(b)          regs.sp++->setBoolean(b)
#define PUSH_DOUBLE(d)           regs.sp++->setDouble(d)
#define PUSH_INT32(i)            regs.sp++->setInt32(i)
#define PUSH_STRING(s)           do { regs.sp++->setString(s); assertSameCompartmentDebugOnly(cx, regs.sp[-1]); } while (0)
#define PUSH_OBJECT(obj)         do { regs.sp++->setObject(obj); assertSameCompartmentDebugOnly(cx, regs.sp[-1]); } while (0)
#define PUSH_OBJECT_OR_NULL(obj) do { regs.sp++->setObjectOrNull(obj); assertSameCompartmentDebugOnly(cx, regs.sp[-1]); } while (0)
#define PUSH_HOLE()              regs.sp++->setMagic(JS_ELEMENTS_HOLE)
#define POP_COPY_TO(v)           v = *--regs.sp
#define POP_RETURN_VALUE()       regs.fp()->setReturnValue(*--regs.sp)

#define FETCH_OBJECT(cx, n, obj)                                              \
    JS_BEGIN_MACRO                                                            \
        HandleValue val = HandleValue::fromMarkedLocation(&regs.sp[n]);       \
        obj = ToObjectFromStack(cx, (val));                                   \
        if (!obj)                                                             \
            goto error;                                                       \
    JS_END_MACRO

template<typename T>
class GenericInterruptEnabler : public InterpreterFrames::InterruptEnablerBase {
  public:
    GenericInterruptEnabler(T *variable, T value) : variable(variable), value(value) { }
    void enable() const { *variable = value; }

  private:
    T *variable;
    T value;
};

inline InterpreterFrames::InterpreterFrames(JSContext *cx, FrameRegs *regs,
                                            const InterruptEnablerBase &enabler)
  : context(cx), regs(regs), enabler(enabler)
{
    older = cx->runtime()->interpreterFrames;
    cx->runtime()->interpreterFrames = this;
}

inline InterpreterFrames::~InterpreterFrames()
{
    context->runtime()->interpreterFrames = older;
}

/*
 * Ensure that the interpreter switch can close call-bytecode cases in the
 * same way as non-call bytecodes.
 */
JS_STATIC_ASSERT(JSOP_NAME_LENGTH == JSOP_CALLNAME_LENGTH);
JS_STATIC_ASSERT(JSOP_GETARG_LENGTH == JSOP_CALLARG_LENGTH);
JS_STATIC_ASSERT(JSOP_GETLOCAL_LENGTH == JSOP_CALLLOCAL_LENGTH);

/*
 * Same for JSOP_SETNAME and JSOP_SETPROP, which differ only slightly but
 * remain distinct for the decompiler.
 */
JS_STATIC_ASSERT(JSOP_SETNAME_LENGTH == JSOP_SETPROP_LENGTH);

/* See TRY_BRANCH_AFTER_COND. */
JS_STATIC_ASSERT(JSOP_IFNE_LENGTH == JSOP_IFEQ_LENGTH);
JS_STATIC_ASSERT(JSOP_IFNE == JSOP_IFEQ + 1);

/*
 * Inline fast paths for iteration. js_IteratorMore and js_IteratorNext handle
 * all cases, but we inline the most frequently taken paths here.
 */
bool
js::IteratorMore(JSContext *cx, JSObject *iterobj, bool *cond, MutableHandleValue rval)
{
    if (iterobj->is<PropertyIteratorObject>()) {
        NativeIterator *ni = iterobj->as<PropertyIteratorObject>().getNativeIterator();
        if (ni->isKeyIter()) {
            *cond = (ni->props_cursor < ni->props_end);
            return true;
        }
    }
    Rooted<JSObject*> iobj(cx, iterobj);
    if (!js_IteratorMore(cx, iobj, rval))
        return false;
    *cond = rval.isTrue();
    return true;
}

bool
js::IteratorNext(JSContext *cx, HandleObject iterobj, MutableHandleValue rval)
{
    if (iterobj->is<PropertyIteratorObject>()) {
        NativeIterator *ni = iterobj->as<PropertyIteratorObject>().getNativeIterator();
        if (ni->isKeyIter()) {
            JS_ASSERT(ni->props_cursor < ni->props_end);
            rval.setString(*ni->current());
            ni->incCursor();
            return true;
        }
    }
    return js_IteratorNext(cx, iterobj, rval);
}

FrameGuard::FrameGuard(RunState &state, FrameRegs &regs)
  : state_(state),
    regs_(regs),
    stack_(NULL),
    fp_(NULL)
{ }

FrameGuard::~FrameGuard()
{
    if (state_.isGenerator()) {
        JSGenerator *gen = state_.asGenerator()->gen();
        gen->fp->unsetPushedSPSFrame();
        gen->regs = regs_;
        return;
    }

    if (fp_)
        stack_->releaseFrame(fp_);
}

/*
 * Compute the implicit |this| parameter for a call expression where the callee
 * funval was resolved from an unqualified name reference to a property on obj
 * (an object on the scope chain).
 *
 * We can avoid computing |this| eagerly and push the implicit callee-coerced
 * |this| value, undefined, if any of these conditions hold:
 *
 * 1. The nominal |this|, obj, is a global object.
 *
 * 2. The nominal |this|, obj, has one of Block, Call, or DeclEnv class (this
 *    is what IsCacheableNonGlobalScope tests). Such objects-as-scopes must be
 *    censored with undefined.
 *
 * Otherwise, we bind |this| to obj->thisObject(). Only names inside |with|
 * statements and embedding-specific scope objects fall into this category.
 *
 * If the callee is a strict mode function, then code implementing JSOP_THIS
 * in the interpreter and JITs will leave undefined as |this|. If funval is a
 * function not in strict mode, JSOP_THIS code replaces undefined with funval's
 * global.
 *
 * We set *vp to undefined early to reduce code size and bias this code for the
 * common and future-friendly cases.
 */
inline bool
ComputeImplicitThis(JSContext *cx, HandleObject obj, MutableHandleValue vp)
{
    vp.setUndefined();

    if (obj->is<GlobalObject>())
        return true;

    if (IsCacheableNonGlobalScope(obj))
        return true;

    JSObject *nobj = JSObject::thisObject(cx, obj);
    if (!nobj)
        return false;

    vp.setObject(*nobj);
    return true;
}

static JS_ALWAYS_INLINE bool
AddOperation(JSContext *cx, HandleScript script, jsbytecode *pc,
             MutableHandleValue lhs, MutableHandleValue rhs, Value *res)
{
    if (lhs.isInt32() && rhs.isInt32()) {
        int32_t l = lhs.toInt32(), r = rhs.toInt32();
        int32_t sum = l + r;
        if (JS_UNLIKELY(bool((l ^ sum) & (r ^ sum) & 0x80000000))) {
            res->setDouble(double(l) + double(r));
            types::TypeScript::MonitorOverflow(cx, script, pc);
        } else {
            res->setInt32(sum);
        }
        return true;
    }

    /*
     * If either operand is an object, any non-integer result must be
     * reported to inference.
     */
    bool lIsObject = lhs.isObject(), rIsObject = rhs.isObject();

    if (!ToPrimitive(cx, lhs))
        return false;
    if (!ToPrimitive(cx, rhs))
        return false;
    bool lIsString, rIsString;
    if ((lIsString = lhs.isString()) | (rIsString = rhs.isString())) {
        JSString *lstr, *rstr;
        if (lIsString) {
            lstr = lhs.toString();
        } else {
            lstr = ToString<CanGC>(cx, lhs);
            if (!lstr)
                return false;
        }
        if (rIsString) {
            rstr = rhs.toString();
        } else {
            // Save/restore lstr in case of GC activity under ToString.
            lhs.setString(lstr);
            rstr = ToString<CanGC>(cx, rhs);
            if (!rstr)
                return false;
            lstr = lhs.toString();
        }
        JSString *str = ConcatStrings<NoGC>(cx, lstr, rstr);
        if (!str) {
            RootedString nlstr(cx, lstr), nrstr(cx, rstr);
            str = ConcatStrings<CanGC>(cx, nlstr, nrstr);
            if (!str)
                return false;
        }
        if (lIsObject || rIsObject)
            types::TypeScript::MonitorString(cx, script, pc);
        res->setString(str);
    } else {
        double l, r;
        if (!ToNumber(cx, lhs, &l) || !ToNumber(cx, rhs, &r))
            return false;
        l += r;
        Value nres = NumberValue(l);
        if (nres.isDouble() &&
            (lIsObject || rIsObject || (!lhs.isDouble() && !rhs.isDouble()))) {
            types::TypeScript::MonitorOverflow(cx, script, pc);
        }
        *res = nres;
    }

    return true;
}

static JS_ALWAYS_INLINE bool
SubOperation(JSContext *cx, HandleScript script, jsbytecode *pc, HandleValue lhs, HandleValue rhs,
             Value *res)
{
    double d1, d2;
    if (!ToNumber(cx, lhs, &d1) || !ToNumber(cx, rhs, &d2))
        return false;
    double d = d1 - d2;
    if (!res->setNumber(d) && !(lhs.isDouble() || rhs.isDouble()))
        types::TypeScript::MonitorOverflow(cx, script, pc);
    return true;
}

static JS_ALWAYS_INLINE bool
MulOperation(JSContext *cx, HandleScript script, jsbytecode *pc, HandleValue lhs, HandleValue rhs,
             Value *res)
{
    double d1, d2;
    if (!ToNumber(cx, lhs, &d1) || !ToNumber(cx, rhs, &d2))
        return false;
    double d = d1 * d2;
    if (!res->setNumber(d) && !(lhs.isDouble() || rhs.isDouble()))
        types::TypeScript::MonitorOverflow(cx, script, pc);
    return true;
}

static JS_ALWAYS_INLINE bool
DivOperation(JSContext *cx, HandleScript script, jsbytecode *pc, HandleValue lhs, HandleValue rhs,
             Value *res)
{
    double d1, d2;
    if (!ToNumber(cx, lhs, &d1) || !ToNumber(cx, rhs, &d2))
        return false;
    res->setNumber(NumberDiv(d1, d2));

    if (d2 == 0 || (res->isDouble() && !(lhs.isDouble() || rhs.isDouble())))
        types::TypeScript::MonitorOverflow(cx, script, pc);
    return true;
}

static JS_ALWAYS_INLINE bool
ModOperation(JSContext *cx, HandleScript script, jsbytecode *pc, HandleValue lhs, HandleValue rhs,
             Value *res)
{
    int32_t l, r;
    if (lhs.isInt32() && rhs.isInt32() &&
        (l = lhs.toInt32()) >= 0 && (r = rhs.toInt32()) > 0) {
        int32_t mod = l % r;
        res->setInt32(mod);
        return true;
    }

    double d1, d2;
    if (!ToNumber(cx, lhs, &d1) || !ToNumber(cx, rhs, &d2))
        return false;

    res->setNumber(NumberMod(d1, d2));
    types::TypeScript::MonitorOverflow(cx, script, pc);
    return true;
}

static JS_ALWAYS_INLINE bool
SetObjectElementOperation(JSContext *cx, Handle<JSObject*> obj, HandleId id, const Value &value,
                          bool strict, JSScript *maybeScript = NULL, jsbytecode *pc = NULL)
{
    RootedScript script(cx, maybeScript);
    types::TypeScript::MonitorAssign(cx, obj, id);

    if (obj->isNative() && JSID_IS_INT(id)) {
        uint32_t length = obj->getDenseInitializedLength();
        int32_t i = JSID_TO_INT(id);
        if ((uint32_t)i >= length) {
            // In an Ion activation, GetPcScript won't work.  For non-baseline activations,
            // that's ok, because optimized ion doesn't generate analysis info.  However,
            // baseline must generate this information, so it passes the script and pc in
            // as arguments.
            if (script || cx->currentlyRunningInInterpreter()) {
                JS_ASSERT(!!script == !!pc);
                if (!script)
                    types::TypeScript::GetPcScript(cx, script.address(), &pc);

                if (script->hasAnalysis())
                    script->analysis()->getCode(pc).arrayWriteHole = true;
            }
        }
    }

    if (obj->isNative() && !obj->setHadElementsAccess(cx))
        return false;

    RootedValue tmp(cx, value);
    return JSObject::setGeneric(cx, obj, obj, id, &tmp, strict);
}

static JS_NEVER_INLINE bool
Interpret(JSContext *cx, RunState &state)
{
    JSAutoResolveFlags rf(cx, RESOLVE_INFER);

    gc::MaybeVerifyBarriers(cx, true);

    JS_ASSERT(!cx->compartment()->activeAnalysis);

#define CHECK_PCCOUNT_INTERRUPTS() JS_ASSERT_IF(script->hasScriptCounts, switchMask == -1)

    register int switchMask = 0;
    int switchOp;
    typedef GenericInterruptEnabler<int> InterruptEnabler;
    InterruptEnabler interrupts(&switchMask, -1);

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
# define END_CASE_LEN8      len = 8; goto advance_pc;
# define END_CASE_LEN9      len = 9; goto advance_pc;
# define END_CASE_LEN10     len = 10; goto advance_pc;
# define END_CASE_LEN11     len = 11; goto advance_pc;
# define END_CASE_LEN12     len = 12; goto advance_pc;
# define END_VARLEN_CASE    goto advance_pc;
# define ADD_EMPTY_CASE(OP) BEGIN_CASE(OP)
# define END_EMPTY_CASES    goto advance_pc_by_one;

#define LOAD_DOUBLE(PCOFF, dbl)                                               \
    (dbl = script->getConst(GET_UINT32_INDEX(regs.pc + (PCOFF))).toDouble())

    /*
     * Prepare to call a user-supplied branch handler, and abort the script
     * if it returns false.
     */
#define CHECK_BRANCH()                                                        \
    JS_BEGIN_MACRO                                                            \
        if (cx->runtime()->interrupt && !js_HandleExecutionInterrupt(cx))       \
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
        if (script->hasAnyBreakpointsOrStepMode() || script->hasScriptCounts) \
            interrupts.enable();                                              \
    JS_END_MACRO

    FrameRegs regs;
    FrameGuard fg(state, regs);

    StackFrame *entryFrame = state.pushInterpreterFrame(cx, &fg);
    if (!entryFrame)
        return false;

    if (!state.isGenerator()) {
        regs.prepareToRun(*entryFrame, state.script());
        JS_ASSERT(regs.pc == state.script()->code);
    } else {
        regs = state.asGenerator()->gen()->regs;
    }

    JS_ASSERT_IF(entryFrame->isEvalFrame(), state.script()->isActiveEval);

    InterpreterActivation activation(cx, entryFrame, regs);

    /*
     * Help Debugger find frames running scripts that it has put in
     * single-step mode.
     */
    InterpreterFrames interpreterFrame(cx, &regs, interrupts);

    /* Copy in hot values that change infrequently. */
    JSRuntime *const rt = cx->runtime();
    RootedScript script(cx);
    SET_SCRIPT(regs.fp()->script());

#if JS_TRACE_LOGGING
    AutoTraceLog logger(TraceLogging::defaultLogger(),
                        TraceLogging::INTERPRETER_START,
                        TraceLogging::INTERPRETER_STOP,
                        script);
#endif

    /*
     * Pool of rooters for use in this interpreter frame. References to these
     * are used for local variables within interpreter cases. This avoids
     * creating new rooters each time an interpreter case is entered, and also
     * correctness pitfalls due to incorrect compilation of destructor calls
     * around computed gotos.
     */
    RootedValue rootValue0(cx), rootValue1(cx);
    RootedString rootString0(cx), rootString1(cx);
    RootedObject rootObject0(cx), rootObject1(cx), rootObject2(cx);
    RootedFunction rootFunction0(cx);
    RootedTypeObject rootType0(cx);
    RootedPropertyName rootName0(cx);
    RootedId rootId0(cx);
    RootedShape rootShape0(cx);
    RootedScript rootScript0(cx);
    DebugOnly<uint32_t> blockDepth;

    if (JS_UNLIKELY(regs.fp()->isGeneratorFrame())) {
        JS_ASSERT(size_t(regs.pc - script->code) <= script->length);
        JS_ASSERT(regs.stackDepth() <= script->nslots);

        /*
         * To support generator_throw and to catch ignored exceptions,
         * fail if cx->isExceptionPending() is true.
         */
        if (cx->isExceptionPending()) {
            Probes::enterScript(cx, script, script->function(), regs.fp());
            goto error;
        }
    }

    /* State communicated between non-local jumps: */
    bool interpReturnOK;

    if (!entryFrame->isGeneratorFrame()) {
        if (!entryFrame->prologue(cx))
            goto error;
    } else {
        Probes::enterScript(cx, script, script->function(), entryFrame);
    }
    if (cx->compartment()->debugMode()) {
        JSTrapStatus status = ScriptDebugPrologue(cx, entryFrame);
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
            MOZ_ASSUME_UNREACHABLE("bad ScriptDebugPrologue status");
        }
    }

    /*
     * It is important that "op" be initialized before calling DO_OP because
     * it is possible for "op" to be specially assigned during the normal
     * processing of an opcode while looping. We rely on DO_NEXT_OP to manage
     * "op" correctly in all other cases.
     */
    JSOp op;
    int32_t len;
    len = 0;

    if (rt->profilingScripts || cx->runtime()->debugHooks.interruptHook)
        interrupts.enable();

    DO_NEXT_OP(len);

    for (;;) {
      advance_pc_by_one:
        JS_ASSERT(js_CodeSpec[op].length == 1);
        len = 1;
      advance_pc:
        js::gc::MaybeVerifyBarriers(cx);
        regs.pc += len;
        op = (JSOp) *regs.pc;

      do_op:
        CHECK_PCCOUNT_INTERRUPTS();
        switchOp = int(op) | switchMask;
      do_switch:
        switch (switchOp) {

  case -1:
    JS_ASSERT(switchMask == -1);
    {
        bool moreInterrupts = false;

        if (cx->runtime()->profilingScripts) {
            if (!script->hasScriptCounts)
                script->initScriptCounts(cx);
            moreInterrupts = true;
        }

        if (script->hasScriptCounts) {
            PCCounts counts = script->getPCCounts(regs.pc);
            counts.get(PCCounts::BASE_INTERP)++;
            moreInterrupts = true;
        }

        JSInterruptHook hook = cx->runtime()->debugHooks.interruptHook;
        if (hook || script->stepModeEnabled()) {
            RootedValue rval(cx);
            JSTrapStatus status = JSTRAP_CONTINUE;
            if (hook)
                status = hook(cx, script, regs.pc, rval.address(), cx->runtime()->debugHooks.interruptHookData);
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

        if (script->hasBreakpointsAt(regs.pc)) {
            RootedValue rval(cx);
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
            JS_ASSERT(rval.isInt32() && rval.toInt32() == op);
        }

        switchMask = moreInterrupts ? -1 : 0;
        switchOp = int(op);
        goto do_switch;
    }

/* No-ops for ease of decompilation. */
ADD_EMPTY_CASE(JSOP_NOP)
ADD_EMPTY_CASE(JSOP_UNUSED125)
ADD_EMPTY_CASE(JSOP_UNUSED126)
ADD_EMPTY_CASE(JSOP_UNUSED132)
ADD_EMPTY_CASE(JSOP_UNUSED148)
ADD_EMPTY_CASE(JSOP_UNUSED161)
ADD_EMPTY_CASE(JSOP_UNUSED162)
ADD_EMPTY_CASE(JSOP_UNUSED163)
ADD_EMPTY_CASE(JSOP_UNUSED164)
ADD_EMPTY_CASE(JSOP_UNUSED165)
ADD_EMPTY_CASE(JSOP_UNUSED166)
ADD_EMPTY_CASE(JSOP_UNUSED167)
ADD_EMPTY_CASE(JSOP_UNUSED168)
ADD_EMPTY_CASE(JSOP_UNUSED169)
ADD_EMPTY_CASE(JSOP_UNUSED170)
ADD_EMPTY_CASE(JSOP_UNUSED171)
ADD_EMPTY_CASE(JSOP_UNUSED172)
ADD_EMPTY_CASE(JSOP_UNUSED173)
ADD_EMPTY_CASE(JSOP_UNUSED174)
ADD_EMPTY_CASE(JSOP_UNUSED175)
ADD_EMPTY_CASE(JSOP_UNUSED176)
ADD_EMPTY_CASE(JSOP_UNUSED177)
ADD_EMPTY_CASE(JSOP_UNUSED178)
ADD_EMPTY_CASE(JSOP_UNUSED179)
ADD_EMPTY_CASE(JSOP_UNUSED180)
ADD_EMPTY_CASE(JSOP_UNUSED181)
ADD_EMPTY_CASE(JSOP_UNUSED182)
ADD_EMPTY_CASE(JSOP_UNUSED183)
ADD_EMPTY_CASE(JSOP_UNUSED188)
ADD_EMPTY_CASE(JSOP_UNUSED189)
ADD_EMPTY_CASE(JSOP_UNUSED190)
ADD_EMPTY_CASE(JSOP_UNUSED200)
ADD_EMPTY_CASE(JSOP_UNUSED201)
ADD_EMPTY_CASE(JSOP_UNUSED208)
ADD_EMPTY_CASE(JSOP_UNUSED209)
ADD_EMPTY_CASE(JSOP_UNUSED210)
ADD_EMPTY_CASE(JSOP_UNUSED219)
ADD_EMPTY_CASE(JSOP_UNUSED220)
ADD_EMPTY_CASE(JSOP_UNUSED221)
ADD_EMPTY_CASE(JSOP_UNUSED222)
ADD_EMPTY_CASE(JSOP_UNUSED223)
ADD_EMPTY_CASE(JSOP_CONDSWITCH)
ADD_EMPTY_CASE(JSOP_TRY)
END_EMPTY_CASES

BEGIN_CASE(JSOP_LOOPHEAD)
END_CASE(JSOP_LOOPHEAD)

BEGIN_CASE(JSOP_LABEL)
END_CASE(JSOP_LABEL)

check_backedge:
{
    CHECK_BRANCH();
    DO_OP();
}

BEGIN_CASE(JSOP_LOOPENTRY)

#ifdef JS_ION
    // Attempt on-stack replacement with Baseline code.
    if (ion::IsBaselineEnabled(cx)) {
        ion::MethodStatus status = ion::CanEnterBaselineAtBranch(cx, regs.fp(), false);
        if (status == ion::Method_Error)
            goto error;
        if (status == ion::Method_Compiled) {
            ion::IonExecStatus maybeOsr = ion::EnterBaselineAtBranch(cx, regs.fp(), regs.pc);

            // We failed to call into baseline at all, so treat as an error.
            if (maybeOsr == ion::IonExec_Aborted)
                goto error;

            interpReturnOK = (maybeOsr == ion::IonExec_Ok);

            if (entryFrame != regs.fp())
                goto jit_return_pop_frame;
            goto leave_on_safe_point;
        }
    }
#endif /* JS_ION */

END_CASE(JSOP_LOOPENTRY)

BEGIN_CASE(JSOP_NOTEARG)
END_CASE(JSOP_NOTEARG)

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
    JS_ASSERT(GET_UINT16(regs.pc) <= regs.stackDepth());
    regs.sp -= GET_UINT16(regs.pc);
#ifdef DEBUG
    if (StaticBlockObject *block = regs.fp()->maybeBlockChain())
        JS_ASSERT(regs.stackDepth() >= block->stackDepth() + block->slotCount());
#endif
END_CASE(JSOP_POPN)

BEGIN_CASE(JSOP_SETRVAL)
BEGIN_CASE(JSOP_POPV)
    POP_RETURN_VALUE();
END_CASE(JSOP_POPV)

BEGIN_CASE(JSOP_ENTERWITH)
{
    RootedValue &val = rootValue0;
    val = regs.sp[-1];

    if (!EnterWith(cx, regs.fp(), val, regs.stackDepth() - 1))
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
    regs.sp[-1].setObject(*regs.fp()->scopeChain());
}
END_CASE(JSOP_ENTERWITH)

BEGIN_CASE(JSOP_LEAVEWITH)
    JS_ASSERT(regs.sp[-1].toObject() == *regs.fp()->scopeChain());
    regs.fp()->popWith(cx);
    regs.sp--;
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
        if (cx->compartment()->debugMode())
            interpReturnOK = ScriptDebugEpilogue(cx, regs.fp(), interpReturnOK);

        if (!regs.fp()->isYielding())
            regs.fp()->epilogue(cx);
        else
            Probes::exitScript(cx, script, script->function(), regs.fp());

#if defined(JS_ION)
  jit_return_pop_frame:
#endif

        activation.popInlineFrame(regs.fp());
        SET_SCRIPT(regs.fp()->script());

#if defined(JS_ION)
  jit_return:
#endif

        JS_ASSERT(js_CodeSpec[*regs.pc].format & JOF_INVOKE);

        /* Resume execution in the calling frame. */
        if (JS_LIKELY(interpReturnOK)) {
            TypeScript::Monitor(cx, script, regs.pc, regs.sp[-1]);

            len = JSOP_CALL_LENGTH;
            DO_NEXT_OP(len);
        }

        /* Increment pc so that |sp - fp->slots == ReconstructStackDepth(pc)|. */
        regs.pc += JSOP_CALL_LENGTH;
        goto error;
    } else {
        JS_ASSERT(regs.stackDepth() == 0);
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
    bool cond = ToBooleanOp(regs);
    regs.sp--;
    if (cond == false) {
        len = GET_JUMP_OFFSET(regs.pc);
        BRANCH(len);
    }
}
END_CASE(JSOP_IFEQ)

BEGIN_CASE(JSOP_IFNE)
{
    bool cond = ToBooleanOp(regs);
    regs.sp--;
    if (cond != false) {
        len = GET_JUMP_OFFSET(regs.pc);
        BRANCH(len);
    }
}
END_CASE(JSOP_IFNE)

BEGIN_CASE(JSOP_OR)
{
    bool cond = ToBooleanOp(regs);
    if (cond == true) {
        len = GET_JUMP_OFFSET(regs.pc);
        DO_NEXT_OP(len);
    }
}
END_CASE(JSOP_OR)

BEGIN_CASE(JSOP_AND)
{
    bool cond = ToBooleanOp(regs);
    if (cond == false) {
        len = GET_JUMP_OFFSET(regs.pc);
        DO_NEXT_OP(len);
    }
}
END_CASE(JSOP_AND)

#define FETCH_ELEMENT_ID(n, id)                                               \
    JS_BEGIN_MACRO                                                            \
        if (!ValueToId<CanGC>(cx, HandleValue::fromMarkedLocation(&regs.sp[n]), &id))\
            goto error;                                                       \
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
    HandleValue rref = HandleValue::fromMarkedLocation(&regs.sp[-1]);
    if (!rref.isObject()) {
        js_ReportValueError(cx, JSMSG_IN_NOT_OBJECT, -1, rref, NullPtr());
        goto error;
    }
    RootedObject &obj = rootObject0;
    obj = &rref.toObject();
    RootedId &id = rootId0;
    FETCH_ELEMENT_ID(-2, id);
    RootedObject &obj2 = rootObject1;
    RootedShape &prop = rootShape0;
    if (!JSObject::lookupGeneric(cx, obj, id, &obj2, &prop))
        goto error;
    bool cond = prop != NULL;
    prop = NULL;
    TRY_BRANCH_AFTER_COND(cond, 2);
    regs.sp--;
    regs.sp[-1].setBoolean(cond);
}
END_CASE(JSOP_IN)

BEGIN_CASE(JSOP_ITER)
{
    JS_ASSERT(regs.stackDepth() >= 1);
    uint8_t flags = GET_UINT8(regs.pc);
    MutableHandleValue res = MutableHandleValue::fromMarkedLocation(&regs.sp[-1]);
    if (!ValueToIterator(cx, flags, res))
        goto error;
    JS_ASSERT(!res.isPrimitive());
}
END_CASE(JSOP_ITER)

BEGIN_CASE(JSOP_MOREITER)
{
    JS_ASSERT(regs.stackDepth() >= 1);
    JS_ASSERT(regs.sp[-1].isObject());
    PUSH_NULL();
    bool cond;
    MutableHandleValue res = MutableHandleValue::fromMarkedLocation(&regs.sp[-1]);
    if (!IteratorMore(cx, &regs.sp[-2].toObject(), &cond, res))
        goto error;
    regs.sp[-1].setBoolean(cond);
}
END_CASE(JSOP_MOREITER)

BEGIN_CASE(JSOP_ITERNEXT)
{
    JS_ASSERT(regs.sp[-1].isObject());
    PUSH_NULL();
    MutableHandleValue res = MutableHandleValue::fromMarkedLocation(&regs.sp[-1]);
    RootedObject &obj = rootObject0;
    obj = &regs.sp[-2].toObject();
    if (!IteratorNext(cx, obj, res))
        goto error;
}
END_CASE(JSOP_ITERNEXT)

BEGIN_CASE(JSOP_ENDITER)
{
    JS_ASSERT(regs.stackDepth() >= 1);
    RootedObject &obj = rootObject0;
    obj = &regs.sp[-1].toObject();
    bool ok = CloseIterator(cx, obj);
    regs.sp--;
    if (!ok)
        goto error;
}
END_CASE(JSOP_ENDITER)

BEGIN_CASE(JSOP_DUP)
{
    JS_ASSERT(regs.stackDepth() >= 1);
    const Value &rref = regs.sp[-1];
    PUSH_COPY(rref);
}
END_CASE(JSOP_DUP)

BEGIN_CASE(JSOP_DUP2)
{
    JS_ASSERT(regs.stackDepth() >= 2);
    const Value &lref = regs.sp[-2];
    const Value &rref = regs.sp[-1];
    PUSH_COPY(lref);
    PUSH_COPY(rref);
}
END_CASE(JSOP_DUP2)

BEGIN_CASE(JSOP_SWAP)
{
    JS_ASSERT(regs.stackDepth() >= 2);
    Value &lref = regs.sp[-2];
    Value &rref = regs.sp[-1];
    lref.swap(rref);
}
END_CASE(JSOP_SWAP)

BEGIN_CASE(JSOP_PICK)
{
    unsigned i = GET_UINT8(regs.pc);
    JS_ASSERT(regs.stackDepth() >= i + 1);
    Value lval = regs.sp[-int(i + 1)];
    memmove(regs.sp - (i + 1), regs.sp - i, sizeof(Value) * i);
    regs.sp[-1] = lval;
}
END_CASE(JSOP_PICK)

BEGIN_CASE(JSOP_SETCONST)
{
    RootedPropertyName &name = rootName0;
    name = script->getName(regs.pc);

    RootedValue &rval = rootValue0;
    rval = regs.sp[-1];

    RootedObject &obj = rootObject0;
    obj = &regs.fp()->varObj();

    if (!SetConstOperation(cx, obj, name, rval))
        goto error;
}
END_CASE(JSOP_SETCONST);

#if JS_HAS_DESTRUCTURING
BEGIN_CASE(JSOP_ENUMCONSTELEM)
{
    RootedValue &rval = rootValue0;
    rval = regs.sp[-3];

    RootedObject &obj = rootObject0;
    FETCH_OBJECT(cx, -2, obj);
    RootedId &id = rootId0;
    FETCH_ELEMENT_ID(-1, id);
    if (!JSObject::defineGeneric(cx, obj, id, rval,
                                 JS_PropertyStub, JS_StrictPropertyStub,
                                 JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY)) {
        goto error;
    }
    regs.sp -= 3;
}
END_CASE(JSOP_ENUMCONSTELEM)
#endif

BEGIN_CASE(JSOP_BINDGNAME)
    PUSH_OBJECT(regs.fp()->global());
END_CASE(JSOP_BINDGNAME)

BEGIN_CASE(JSOP_BINDINTRINSIC)
    PUSH_OBJECT(*cx->global()->intrinsicsHolder());
END_CASE(JSOP_BINDGNAME)

BEGIN_CASE(JSOP_BINDNAME)
{
    RootedObject &scopeChain = rootObject0;
    scopeChain = regs.fp()->scopeChain();

    RootedPropertyName &name = rootName0;
    name = script->getName(regs.pc);

    /* Assigning to an undeclared name adds a property to the global object. */
    RootedObject &scope = rootObject1;
    if (!LookupNameWithGlobalDefault(cx, name, scopeChain, &scope))
        goto error;

    PUSH_OBJECT(*scope);
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

BEGIN_CASE(JSOP_EQ)
    if (!LooseEqualityOp<true>(cx, regs))
        goto error;
END_CASE(JSOP_EQ)

BEGIN_CASE(JSOP_NE)
    if (!LooseEqualityOp<false>(cx, regs))
        goto error;
END_CASE(JSOP_NE)

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
    MutableHandleValue lval = MutableHandleValue::fromMarkedLocation(&regs.sp[-2]);
    MutableHandleValue rval = MutableHandleValue::fromMarkedLocation(&regs.sp[-1]);
    if (!LessThanOperation(cx, lval, rval, &cond))
        goto error;
    TRY_BRANCH_AFTER_COND(cond, 2);
    regs.sp[-2].setBoolean(cond);
    regs.sp--;
}
END_CASE(JSOP_LT)

BEGIN_CASE(JSOP_LE)
{
    bool cond;
    MutableHandleValue lval = MutableHandleValue::fromMarkedLocation(&regs.sp[-2]);
    MutableHandleValue rval = MutableHandleValue::fromMarkedLocation(&regs.sp[-1]);
    if (!LessThanOrEqualOperation(cx, lval, rval, &cond))
        goto error;
    TRY_BRANCH_AFTER_COND(cond, 2);
    regs.sp[-2].setBoolean(cond);
    regs.sp--;
}
END_CASE(JSOP_LE)

BEGIN_CASE(JSOP_GT)
{
    bool cond;
    MutableHandleValue lval = MutableHandleValue::fromMarkedLocation(&regs.sp[-2]);
    MutableHandleValue rval = MutableHandleValue::fromMarkedLocation(&regs.sp[-1]);
    if (!GreaterThanOperation(cx, lval, rval, &cond))
        goto error;
    TRY_BRANCH_AFTER_COND(cond, 2);
    regs.sp[-2].setBoolean(cond);
    regs.sp--;
}
END_CASE(JSOP_GT)

BEGIN_CASE(JSOP_GE)
{
    bool cond;
    MutableHandleValue lval = MutableHandleValue::fromMarkedLocation(&regs.sp[-2]);
    MutableHandleValue rval = MutableHandleValue::fromMarkedLocation(&regs.sp[-1]);
    if (!GreaterThanOrEqualOperation(cx, lval, rval, &cond))
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
    HandleValue lval = HandleValue::fromMarkedLocation(&regs.sp[-2]);
    HandleValue rval = HandleValue::fromMarkedLocation(&regs.sp[-1]);
    if (!UrshOperation(cx, script, regs.pc, lval, rval, &regs.sp[-2]))
        goto error;
    regs.sp--;
}
END_CASE(JSOP_URSH)

BEGIN_CASE(JSOP_ADD)
{
    MutableHandleValue lval = MutableHandleValue::fromMarkedLocation(&regs.sp[-2]);
    MutableHandleValue rval = MutableHandleValue::fromMarkedLocation(&regs.sp[-1]);
    if (!AddOperation(cx, script, regs.pc, lval, rval, &regs.sp[-2]))
        goto error;
    regs.sp--;
}
END_CASE(JSOP_ADD)

BEGIN_CASE(JSOP_SUB)
{
    RootedValue &lval = rootValue0, &rval = rootValue1;
    lval = regs.sp[-2];
    rval = regs.sp[-1];
    if (!SubOperation(cx, script, regs.pc, lval, rval, &regs.sp[-2]))
        goto error;
    regs.sp--;
}
END_CASE(JSOP_SUB)

BEGIN_CASE(JSOP_MUL)
{
    RootedValue &lval = rootValue0, &rval = rootValue1;
    lval = regs.sp[-2];
    rval = regs.sp[-1];
    if (!MulOperation(cx, script, regs.pc, lval, rval, &regs.sp[-2]))
        goto error;
    regs.sp--;
}
END_CASE(JSOP_MUL)

BEGIN_CASE(JSOP_DIV)
{
    RootedValue &lval = rootValue0, &rval = rootValue1;
    lval = regs.sp[-2];
    rval = regs.sp[-1];
    if (!DivOperation(cx, script, regs.pc, lval, rval, &regs.sp[-2]))
        goto error;
    regs.sp--;
}
END_CASE(JSOP_DIV)

BEGIN_CASE(JSOP_MOD)
{
    RootedValue &lval = rootValue0, &rval = rootValue1;
    lval = regs.sp[-2];
    rval = regs.sp[-1];
    if (!ModOperation(cx, script, regs.pc, lval, rval, &regs.sp[-2]))
        goto error;
    regs.sp--;
}
END_CASE(JSOP_MOD)

BEGIN_CASE(JSOP_NOT)
{
    bool cond = ToBooleanOp(regs);
    regs.sp--;
    PUSH_BOOLEAN(!cond);
}
END_CASE(JSOP_NOT)

BEGIN_CASE(JSOP_BITNOT)
{
    int32_t i;
    HandleValue value = HandleValue::fromMarkedLocation(&regs.sp[-1]);
    if (!BitNot(cx, value, &i))
        goto error;
    regs.sp[-1].setInt32(i);
}
END_CASE(JSOP_BITNOT)

BEGIN_CASE(JSOP_NEG)
{
    RootedValue &val = rootValue0;
    val = regs.sp[-1];
    MutableHandleValue res = MutableHandleValue::fromMarkedLocation(&regs.sp[-1]);
    if (!NegOperation(cx, script, regs.pc, val, res))
        goto error;
}
END_CASE(JSOP_NEG)

BEGIN_CASE(JSOP_POS)
    if (!ToNumber(cx, MutableHandleValue::fromMarkedLocation(&regs.sp[-1])))
        goto error;
    if (!regs.sp[-1].isInt32())
        TypeScript::MonitorOverflow(cx, script, regs.pc);
END_CASE(JSOP_POS)

BEGIN_CASE(JSOP_DELNAME)
{
    /* Strict mode code should never contain JSOP_DELNAME opcodes. */
    JS_ASSERT(!script->strict);

    RootedPropertyName &name = rootName0;
    name = script->getName(regs.pc);

    RootedObject &scopeObj = rootObject0;
    scopeObj = regs.fp()->scopeChain();

    PUSH_BOOLEAN(true);
    MutableHandleValue res = MutableHandleValue::fromMarkedLocation(&regs.sp[-1]);
    if (!DeleteNameOperation(cx, name, scopeObj, res))
        goto error;
}
END_CASE(JSOP_DELNAME)

BEGIN_CASE(JSOP_DELPROP)
{
    RootedPropertyName &name = rootName0;
    name = script->getName(regs.pc);

    RootedObject &obj = rootObject0;
    FETCH_OBJECT(cx, -1, obj);

    JSBool succeeded;
    if (!JSObject::deleteProperty(cx, obj, name, &succeeded))
        goto error;
    if (!succeeded && script->strict) {
        obj->reportNotConfigurable(cx, NameToId(name));
        goto error;
    }
    MutableHandleValue res = MutableHandleValue::fromMarkedLocation(&regs.sp[-1]);
    res.setBoolean(succeeded);
}
END_CASE(JSOP_DELPROP)

BEGIN_CASE(JSOP_DELELEM)
{
    /* Fetch the left part and resolve it to a non-null object. */
    RootedObject &obj = rootObject0;
    FETCH_OBJECT(cx, -2, obj);

    RootedValue &propval = rootValue0;
    propval = regs.sp[-1];

    JSBool succeeded;
    if (!JSObject::deleteByValue(cx, obj, propval, &succeeded))
        goto error;
    if (!succeeded && script->strict) {
        // XXX This observably calls ToString(propval).  We should convert to
        //     PropertyKey and use that to delete, and to report an error if
        //     necessary!
        RootedId id(cx);
        if (!ValueToId<CanGC>(cx, propval, &id))
            goto error;
        obj->reportNotConfigurable(cx, id);
        goto error;
    }

    MutableHandleValue res = MutableHandleValue::fromMarkedLocation(&regs.sp[-2]);
    res.setBoolean(succeeded);
    regs.sp--;
}
END_CASE(JSOP_DELELEM)

BEGIN_CASE(JSOP_TOID)
{
    /*
     * Increment or decrement requires use to lookup the same property twice,
     * but we need to avoid the observable stringification the second time.
     * There must be an object value below the id, which will not be popped.
     */
    RootedValue &objval = rootValue0, &idval = rootValue1;
    objval = regs.sp[-2];
    idval = regs.sp[-1];

    MutableHandleValue res = MutableHandleValue::fromMarkedLocation(&regs.sp[-1]);
    if (!ToIdOperation(cx, script, regs.pc, objval, idval, res))
        goto error;
}
END_CASE(JSOP_TOID)

BEGIN_CASE(JSOP_TYPEOFEXPR)
BEGIN_CASE(JSOP_TYPEOF)
{
    HandleValue ref = HandleValue::fromMarkedLocation(&regs.sp[-1]);
    regs.sp[-1].setString(TypeOfOperation(cx, ref));
}
END_CASE(JSOP_TYPEOF)

BEGIN_CASE(JSOP_VOID)
    regs.sp[-1].setUndefined();
END_CASE(JSOP_VOID)

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

    MutableHandleValue lval = MutableHandleValue::fromMarkedLocation(&regs.sp[-1]);
    if (!GetPropertyOperation(cx, regs.fp(), script, regs.pc, lval, lval))
        goto error;

    TypeScript::Monitor(cx, script, regs.pc, lval);
    assertSameCompartmentDebugOnly(cx, lval);
}
END_CASE(JSOP_GETPROP)

BEGIN_CASE(JSOP_SETINTRINSIC)
{
    HandleValue value = HandleValue::fromMarkedLocation(&regs.sp[-1]);

    if (!SetIntrinsicOperation(cx, script, regs.pc, value))
        goto error;

    regs.sp[-2] = regs.sp[-1];
    regs.sp--;
}
END_CASE(JSOP_SETINTRINSIC)

BEGIN_CASE(JSOP_SETGNAME)
BEGIN_CASE(JSOP_SETNAME)
{
    RootedObject &scope = rootObject0;
    scope = &regs.sp[-2].toObject();

    HandleValue value = HandleValue::fromMarkedLocation(&regs.sp[-1]);

    if (!SetNameOperation(cx, script, regs.pc, scope, value))
        goto error;

    regs.sp[-2] = regs.sp[-1];
    regs.sp--;
}
END_CASE(JSOP_SETNAME)

BEGIN_CASE(JSOP_SETPROP)
{
    HandleValue lval = HandleValue::fromMarkedLocation(&regs.sp[-2]);
    HandleValue rval = HandleValue::fromMarkedLocation(&regs.sp[-1]);

    if (!SetPropertyOperation(cx, script, regs.pc, lval, rval))
        goto error;

    regs.sp[-2] = regs.sp[-1];
    regs.sp--;
}
END_CASE(JSOP_SETPROP)

BEGIN_CASE(JSOP_GETELEM)
BEGIN_CASE(JSOP_CALLELEM)
{
    MutableHandleValue lval = MutableHandleValue::fromMarkedLocation(&regs.sp[-2]);
    HandleValue rval = HandleValue::fromMarkedLocation(&regs.sp[-1]);
    MutableHandleValue res = MutableHandleValue::fromMarkedLocation(&regs.sp[-2]);

    bool done = false;
    if (!GetElemOptimizedArguments(cx, regs.fp(), lval, rval, res, &done))
        goto error;

    if (!done) {
        if (!GetElementOperation(cx, op, lval, rval, res))
            goto error;
    }

    TypeScript::Monitor(cx, script, regs.pc, res);
    regs.sp--;
}
END_CASE(JSOP_GETELEM)

BEGIN_CASE(JSOP_SETELEM)
{
    RootedObject &obj = rootObject0;
    FETCH_OBJECT(cx, -3, obj);
    RootedId &id = rootId0;
    FETCH_ELEMENT_ID(-2, id);
    Value &value = regs.sp[-1];
    if (!SetObjectElementOperation(cx, obj, id, value, script->strict))
        goto error;
    regs.sp[-3] = value;
    regs.sp -= 2;
}
END_CASE(JSOP_SETELEM)

BEGIN_CASE(JSOP_ENUMELEM)
{
    RootedObject &obj = rootObject0;
    RootedValue &rval = rootValue0;

    /* Funky: the value to set is under the [obj, id] pair. */
    FETCH_OBJECT(cx, -2, obj);
    RootedId &id = rootId0;
    FETCH_ELEMENT_ID(-1, id);
    rval = regs.sp[-3];
    if (!JSObject::setGeneric(cx, obj, obj, id, &rval, script->strict))
        goto error;
    regs.sp -= 3;
}
END_CASE(JSOP_ENUMELEM)

BEGIN_CASE(JSOP_EVAL)
{
    CallArgs args = CallArgsFromSp(GET_ARGC(regs.pc), regs.sp);
    if (IsBuiltinEvalForScope(regs.fp()->scopeChain(), args.calleev())) {
        if (!DirectEval(cx, args))
            goto error;
    } else {
        if (!Invoke(cx, args))
            goto error;
    }
    regs.sp = args.spAfterCall();
    TypeScript::Monitor(cx, script, regs.pc, regs.sp[-1]);
}
END_CASE(JSOP_EVAL)

BEGIN_CASE(JSOP_FUNAPPLY)
{
    CallArgs args = CallArgsFromSp(GET_ARGC(regs.pc), regs.sp);
    if (!GuardFunApplyArgumentsOptimization(cx, regs.fp(), args.calleev(), args.array(),
                                            args.length()))
        goto error;
    /* FALL THROUGH */
}

BEGIN_CASE(JSOP_NEW)
BEGIN_CASE(JSOP_CALL)
BEGIN_CASE(JSOP_FUNCALL)
{
    if (regs.fp()->hasPushedSPSFrame())
        cx->runtime()->spsProfiler.updatePC(script, regs.pc);
    JS_ASSERT(regs.stackDepth() >= 2 + GET_ARGC(regs.pc));
    CallArgs args = CallArgsFromSp(GET_ARGC(regs.pc), regs.sp);

    bool construct = (*regs.pc == JSOP_NEW);

    RootedFunction &fun = rootFunction0;
    RootedScript &funScript = rootScript0;
    bool isFunction = IsFunctionObject(args.calleev(), fun.address());

    /*
     * Some builtins are marked as clone-at-callsite to increase precision of
     * TI and JITs.
     */
    if (isFunction && fun->isInterpreted()) {
        funScript = fun->getOrCreateScript(cx);
        if (!funScript)
            goto error;
        if (cx->typeInferenceEnabled() && funScript->shouldCloneAtCallsite) {
            fun = CloneFunctionAtCallsite(cx, fun, script, regs.pc);
            if (!fun)
                goto error;
            args.setCallee(ObjectValue(*fun));
        }
    }

    /* Don't bother trying to fast-path calls to scripted non-constructors. */
    if (!isFunction || !fun->isInterpretedConstructor()) {
        if (construct) {
            if (!InvokeConstructor(cx, args))
                goto error;
        } else {
            if (!Invoke(cx, args))
                goto error;
        }
        Value *newsp = args.spAfterCall();
        TypeScript::Monitor(cx, script, regs.pc, newsp[-1]);
        regs.sp = newsp;
        len = JSOP_CALL_LENGTH;
        DO_NEXT_OP(len);
    }

    InitialFrameFlags initial = construct ? INITIAL_CONSTRUCT : INITIAL_NONE;
    bool newType = cx->typeInferenceEnabled() && UseNewType(cx, script, regs.pc);

#ifdef JS_ION
    InvokeState state(cx, args, initial);
    if (newType)
        state.setUseNewType();

    if (!newType && ion::IsEnabled(cx)) {
        ion::MethodStatus status = ion::CanEnter(cx, state);
        if (status == ion::Method_Error)
            goto error;
        if (status == ion::Method_Compiled) {
            ion::IonExecStatus exec = ion::Cannon(cx, state);
            CHECK_BRANCH();
            regs.sp = args.spAfterCall();
            interpReturnOK = !IsErrorStatus(exec);
            goto jit_return;
        }
    }

    if (ion::IsBaselineEnabled(cx)) {
        ion::MethodStatus status = ion::CanEnterBaselineMethod(cx, state);
        if (status == ion::Method_Error)
            goto error;
        if (status == ion::Method_Compiled) {
            ion::IonExecStatus exec = ion::EnterBaselineMethod(cx, state);
            CHECK_BRANCH();
            regs.sp = args.spAfterCall();
            interpReturnOK = !IsErrorStatus(exec);
            goto jit_return;
        }
    }
#endif

    TypeMonitorCall(cx, args, construct);

    funScript = fun->nonLazyScript();
    if (!activation.pushInlineFrame(args, funScript, initial))
        goto error;

    if (newType)
        regs.fp()->setUseNewType();

    SET_SCRIPT(regs.fp()->script());

    if (!regs.fp()->prologue(cx))
        goto error;
    if (cx->compartment()->debugMode()) {
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
            MOZ_ASSUME_UNREACHABLE("bad ScriptDebugPrologue status");
        }
    }

    /* Load first op and dispatch it (safe since JSOP_STOP). */
    op = (JSOp) *regs.pc;
    DO_OP();
}

BEGIN_CASE(JSOP_SETCALL)
{
    JS_ALWAYS_FALSE(SetCallOperation(cx));
    goto error;
}
END_CASE(JSOP_SETCALL)

BEGIN_CASE(JSOP_IMPLICITTHIS)
{
    RootedPropertyName &name = rootName0;
    name = script->getName(regs.pc);

    RootedObject &scopeObj = rootObject0;
    scopeObj = regs.fp()->scopeChain();

    RootedObject &scope = rootObject1;
    if (!LookupNameWithGlobalDefault(cx, name, scopeObj, &scope))
        goto error;

    RootedValue &v = rootValue0;
    if (!ComputeImplicitThis(cx, scope, &v))
        goto error;
    PUSH_COPY(v);
}
END_CASE(JSOP_IMPLICITTHIS)

BEGIN_CASE(JSOP_GETGNAME)
BEGIN_CASE(JSOP_CALLGNAME)
BEGIN_CASE(JSOP_NAME)
BEGIN_CASE(JSOP_CALLNAME)
{
    RootedValue &rval = rootValue0;

    if (!NameOperation(cx, regs.fp(), regs.pc, &rval))
        goto error;

    PUSH_COPY(rval);
    TypeScript::Monitor(cx, script, regs.pc, rval);
}
END_CASE(JSOP_NAME)

BEGIN_CASE(JSOP_GETINTRINSIC)
BEGIN_CASE(JSOP_CALLINTRINSIC)
{
    RootedValue &rval = rootValue0;

    if (!GetIntrinsicOperation(cx, regs.pc, &rval))
        goto error;

    PUSH_COPY(rval);
    TypeScript::Monitor(cx, script, regs.pc, rval);
}
END_CASE(JSOP_GETINTRINSIC)

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
    PUSH_STRING(script->getAtom(regs.pc));
END_CASE(JSOP_STRING)

BEGIN_CASE(JSOP_OBJECT)
    PUSH_OBJECT(*script->getObject(regs.pc));
END_CASE(JSOP_OBJECT)

BEGIN_CASE(JSOP_REGEXP)
{
    /*
     * Push a regexp object cloned from the regexp literal object mapped by the
     * bytecode at pc.
     */
    uint32_t index = GET_UINT32_INDEX(regs.pc);
    JSObject *proto = regs.fp()->global().getOrCreateRegExpPrototype(cx);
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
        /* Don't use mozilla::DoubleIsInt32; treat -0 (double) as 0. */
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

BEGIN_CASE(JSOP_ARGUMENTS)
    JS_ASSERT(!regs.fp()->fun()->hasRest());
    if (!script->analyzedArgsUsage() && !script->ensureRanAnalysis(cx))
        goto error;
    if (script->needsArgsObj()) {
        ArgumentsObject *obj = ArgumentsObject::createExpected(cx, regs.fp());
        if (!obj)
            goto error;
        PUSH_COPY(ObjectValue(*obj));
    } else {
        PUSH_COPY(MagicValue(JS_OPTIMIZED_ARGUMENTS));
    }
END_CASE(JSOP_ARGUMENTS)

BEGIN_CASE(JSOP_RUNONCE)
{
    if (!RunOnceScriptPrologue(cx, script))
        goto error;
}
END_CASE(JSOP_RUNONCE)

BEGIN_CASE(JSOP_REST)
{
    RootedObject &rest = rootObject0;
    rest = regs.fp()->createRestParameter(cx);
    if (!rest)
        goto error;
    PUSH_COPY(ObjectValue(*rest));
}
END_CASE(JSOP_REST)

BEGIN_CASE(JSOP_CALLALIASEDVAR)
BEGIN_CASE(JSOP_GETALIASEDVAR)
{
    ScopeCoordinate sc = ScopeCoordinate(regs.pc);
    PUSH_COPY(regs.fp()->aliasedVarScope(sc).aliasedVar(sc));
    TypeScript::Monitor(cx, script, regs.pc, regs.sp[-1]);
}
END_CASE(JSOP_GETALIASEDVAR)

BEGIN_CASE(JSOP_SETALIASEDVAR)
{
    ScopeCoordinate sc = ScopeCoordinate(regs.pc);
    ScopeObject &obj = regs.fp()->aliasedVarScope(sc);

    // Avoid computing the name if no type updates are needed, as this may be
    // expensive on scopes with large numbers of variables.
    PropertyName *name = obj.hasSingletonType() ? ScopeCoordinateName(cx, script, regs.pc) : NULL;

    obj.setAliasedVar(cx, sc, name, regs.sp[-1]);
}
END_CASE(JSOP_SETALIASEDVAR)

BEGIN_CASE(JSOP_GETARG)
BEGIN_CASE(JSOP_CALLARG)
{
    unsigned i = GET_ARGNO(regs.pc);
    if (script->argsObjAliasesFormals())
        PUSH_COPY(regs.fp()->argsObj().arg(i));
    else
        PUSH_COPY(regs.fp()->unaliasedFormal(i));
}
END_CASE(JSOP_GETARG)

BEGIN_CASE(JSOP_SETARG)
{
    unsigned i = GET_ARGNO(regs.pc);
    if (script->argsObjAliasesFormals())
        regs.fp()->argsObj().setArg(i, regs.sp[-1]);
    else
        regs.fp()->unaliasedFormal(i) = regs.sp[-1];
}
END_CASE(JSOP_SETARG)

BEGIN_CASE(JSOP_GETLOCAL)
BEGIN_CASE(JSOP_CALLLOCAL)
{
    unsigned i = GET_SLOTNO(regs.pc);
    PUSH_COPY_SKIP_CHECK(regs.fp()->unaliasedLocal(i));

    /*
     * Skip the same-compartment assertion if the local will be immediately
     * popped. We do not guarantee sync for dead locals when coming in from the
     * method JIT, and a GETLOCAL followed by POP is not considered to be
     * a use of the variable.
     */
    if (regs.pc[JSOP_GETLOCAL_LENGTH] != JSOP_POP)
        assertSameCompartmentDebugOnly(cx, regs.sp[-1]);
}
END_CASE(JSOP_GETLOCAL)

BEGIN_CASE(JSOP_SETLOCAL)
{
    unsigned i = GET_SLOTNO(regs.pc);
    regs.fp()->unaliasedLocal(i) = regs.sp[-1];
}
END_CASE(JSOP_SETLOCAL)

BEGIN_CASE(JSOP_DEFCONST)
BEGIN_CASE(JSOP_DEFVAR)
{
    /* ES5 10.5 step 8 (with subsequent errata). */
    unsigned attrs = JSPROP_ENUMERATE;
    if (!regs.fp()->isEvalFrame())
        attrs |= JSPROP_PERMANENT;
    if (op == JSOP_DEFCONST)
        attrs |= JSPROP_READONLY;

    /* Step 8b. */
    RootedObject &obj = rootObject0;
    obj = &regs.fp()->varObj();

    RootedPropertyName &name = rootName0;
    name = script->getName(regs.pc);

    if (!DefVarOrConstOperation(cx, obj, name, attrs))
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
    RootedFunction &fun = rootFunction0;
    fun = script->getFunction(GET_UINT32_INDEX(regs.pc));

    if (!DefFunOperation(cx, script, regs.fp()->scopeChain(), fun))
        goto error;
}
END_CASE(JSOP_DEFFUN)

BEGIN_CASE(JSOP_LAMBDA)
{
    /* Load the specified function object literal. */
    RootedFunction &fun = rootFunction0;
    fun = script->getFunction(GET_UINT32_INDEX(regs.pc));

    JSObject *obj = Lambda(cx, fun, regs.fp()->scopeChain());
    if (!obj)
        goto error;
    JS_ASSERT(obj->getProto());
    PUSH_OBJECT(*obj);
}
END_CASE(JSOP_LAMBDA)

BEGIN_CASE(JSOP_CALLEE)
    JS_ASSERT(regs.fp()->isNonEvalFunctionFrame());
    PUSH_COPY(regs.fp()->calleev());
END_CASE(JSOP_CALLEE)

BEGIN_CASE(JSOP_INITPROP_GETTER)
BEGIN_CASE(JSOP_INITPROP_SETTER)
{
    RootedObject &obj = rootObject0;
    RootedPropertyName &name = rootName0;
    RootedValue &val = rootValue0;

    JS_ASSERT(regs.stackDepth() >= 2);
    obj = &regs.sp[-2].toObject();
    name = script->getName(regs.pc);
    val = regs.sp[-1];

    if (!InitGetterSetterOperation(cx, regs.pc, obj, name, val))
        goto error;

    regs.sp--;
}
END_CASE(JSOP_INITPROP_GETTER)

BEGIN_CASE(JSOP_INITELEM_GETTER)
BEGIN_CASE(JSOP_INITELEM_SETTER)
{
    RootedObject &obj = rootObject0;
    RootedValue &idval = rootValue0;
    RootedValue &val = rootValue1;

    JS_ASSERT(regs.stackDepth() >= 3);
    obj = &regs.sp[-3].toObject();
    idval = regs.sp[-2];
    val = regs.sp[-1];

    if (!InitGetterSetterOperation(cx, regs.pc, obj, idval, val))
        goto error;

    regs.sp -= 2;
}
END_CASE(JSOP_INITELEM_GETTER)

BEGIN_CASE(JSOP_HOLE)
    PUSH_HOLE();
END_CASE(JSOP_HOLE)

BEGIN_CASE(JSOP_NEWINIT)
{
    uint8_t i = GET_UINT8(regs.pc);
    JS_ASSERT(i == JSProto_Array || i == JSProto_Object);

    RootedObject &obj = rootObject0;
    NewObjectKind newKind;
    if (i == JSProto_Array) {
        newKind = UseNewTypeForInitializer(cx, script, regs.pc, &ArrayObject::class_);
        obj = NewDenseEmptyArray(cx, NULL, newKind);
    } else {
        gc::AllocKind allocKind = GuessObjectGCKind(0);
        newKind = UseNewTypeForInitializer(cx, script, regs.pc, &ObjectClass);
        obj = NewBuiltinClassInstance(cx, &ObjectClass, allocKind, newKind);
    }
    if (!obj || !SetInitializerObjectType(cx, script, regs.pc, obj, newKind))
        goto error;

    PUSH_OBJECT(*obj);
    TypeScript::Monitor(cx, script, regs.pc, regs.sp[-1]);
}
END_CASE(JSOP_NEWINIT)

BEGIN_CASE(JSOP_NEWARRAY)
{
    unsigned count = GET_UINT24(regs.pc);
    RootedObject &obj = rootObject0;
    NewObjectKind newKind = UseNewTypeForInitializer(cx, script, regs.pc, &ArrayObject::class_);
    obj = NewDenseAllocatedArray(cx, count, NULL, newKind);
    if (!obj || !SetInitializerObjectType(cx, script, regs.pc, obj, newKind))
        goto error;

    PUSH_OBJECT(*obj);
    TypeScript::Monitor(cx, script, regs.pc, regs.sp[-1]);
}
END_CASE(JSOP_NEWARRAY)

BEGIN_CASE(JSOP_NEWOBJECT)
{
    RootedObject &baseobj = rootObject0;
    baseobj = script->getObject(regs.pc);

    RootedObject &obj = rootObject1;
    NewObjectKind newKind = UseNewTypeForInitializer(cx, script, regs.pc, baseobj->getClass());
    obj = CopyInitializerObject(cx, baseobj, newKind);
    if (!obj || !SetInitializerObjectType(cx, script, regs.pc, obj, newKind))
        goto error;

    PUSH_OBJECT(*obj);
    TypeScript::Monitor(cx, script, regs.pc, regs.sp[-1]);
}
END_CASE(JSOP_NEWOBJECT)

BEGIN_CASE(JSOP_ENDINIT)
{
    /* FIXME remove JSOP_ENDINIT bug 588522 */
    JS_ASSERT(regs.stackDepth() >= 1);
    JS_ASSERT(regs.sp[-1].isObject() || regs.sp[-1].isUndefined());
}
END_CASE(JSOP_ENDINIT)

BEGIN_CASE(JSOP_INITPROP)
{
    /* Load the property's initial value into rval. */
    JS_ASSERT(regs.stackDepth() >= 2);
    RootedValue &rval = rootValue0;
    rval = regs.sp[-1];

    /* Load the object being initialized into lval/obj. */
    RootedObject &obj = rootObject0;
    obj = &regs.sp[-2].toObject();
    JS_ASSERT(obj->isObject());

    PropertyName *name = script->getName(regs.pc);

    RootedId &id = rootId0;
    id = NameToId(name);

    if (JS_UNLIKELY(name == cx->names().proto)
        ? !baseops::SetPropertyHelper(cx, obj, obj, id, 0, &rval, script->strict)
        : !DefineNativeProperty(cx, obj, id, rval, NULL, NULL,
                                JSPROP_ENUMERATE, 0, 0, 0)) {
        goto error;
    }

    regs.sp--;
}
END_CASE(JSOP_INITPROP);

BEGIN_CASE(JSOP_INITELEM)
{
    JS_ASSERT(regs.stackDepth() >= 3);
    HandleValue val = HandleValue::fromMarkedLocation(&regs.sp[-1]);
    HandleValue id = HandleValue::fromMarkedLocation(&regs.sp[-2]);

    RootedObject &obj = rootObject0;
    obj = &regs.sp[-3].toObject();

    if (!InitElemOperation(cx, obj, id, val))
        goto error;

    regs.sp -= 2;
}
END_CASE(JSOP_INITELEM)

BEGIN_CASE(JSOP_INITELEM_ARRAY)
{
    JS_ASSERT(regs.stackDepth() >= 2);
    HandleValue val = HandleValue::fromMarkedLocation(&regs.sp[-1]);

    RootedObject &obj = rootObject0;
    obj = &regs.sp[-2].toObject();

    JS_ASSERT(obj->is<ArrayObject>());

    uint32_t index = GET_UINT24(regs.pc);
    if (!InitArrayElemOperation(cx, regs.pc, obj, index, val))
        goto error;

    regs.sp--;
}
END_CASE(JSOP_INITELEM_ARRAY)

BEGIN_CASE(JSOP_INITELEM_INC)
{
    JS_ASSERT(regs.stackDepth() >= 3);
    HandleValue val = HandleValue::fromMarkedLocation(&regs.sp[-1]);

    RootedObject &obj = rootObject0;
    obj = &regs.sp[-3].toObject();

    uint32_t index = regs.sp[-2].toInt32();
    if (!InitArrayElemOperation(cx, regs.pc, obj, index, val))
        goto error;

    regs.sp[-2].setInt32(index + 1);
    regs.sp--;
}
END_CASE(JSOP_INITELEM_INC)

BEGIN_CASE(JSOP_SPREAD)
{
    int32_t count = regs.sp[-2].toInt32();
    RootedObject &arr = rootObject0;
    arr = &regs.sp[-3].toObject();
    const Value iterable = regs.sp[-1];
    ForOfIterator iter(cx, iterable);
    RootedValue &iterVal = rootValue0;
    while (iter.next()) {
        if (count == INT32_MAX) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                 JSMSG_SPREAD_TOO_LARGE);
            goto error;
        }
        iterVal = iter.value();
        if (!JSObject::defineElement(cx, arr, count++, iterVal, NULL, NULL, JSPROP_ENUMERATE))
            goto error;
    }
    if (!iter.close())
        goto error;
    regs.sp[-2].setInt32(count);
    regs.sp--;
}
END_CASE(JSOP_SPREAD)

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

    /* Increment the PC by this much. */
    len = rval.toInt32() - int32_t(regs.pc - script->code);
END_VARLEN_CASE
}

BEGIN_CASE(JSOP_EXCEPTION)
{
    PUSH_NULL();
    MutableHandleValue res = MutableHandleValue::fromMarkedLocation(&regs.sp[-1]);
    if (!GetAndClearException(cx, res))
        goto error;
}
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
    CHECK_BRANCH();
    RootedValue &v = rootValue0;
    POP_COPY_TO(v);
    JS_ALWAYS_FALSE(Throw(cx, v));
    /* let the code at error try to catch the exception. */
    goto error;
}

BEGIN_CASE(JSOP_INSTANCEOF)
{
    RootedValue &rref = rootValue0;
    rref = regs.sp[-1];
    if (rref.isPrimitive()) {
        js_ReportValueError(cx, JSMSG_BAD_INSTANCEOF_RHS, -1, rref, NullPtr());
        goto error;
    }
    RootedObject &obj = rootObject0;
    obj = &rref.toObject();
    JSBool cond = JS_FALSE;
    if (!HasInstance(cx, obj, HandleValue::fromMarkedLocation(&regs.sp[-2]), &cond))
        goto error;
    regs.sp--;
    regs.sp[-1].setBoolean(cond);
}
END_CASE(JSOP_INSTANCEOF)

BEGIN_CASE(JSOP_DEBUGGER)
{
    JSTrapStatus st = JSTRAP_CONTINUE;
    RootedValue rval(cx);
    if (JSDebuggerHandler handler = cx->runtime()->debugHooks.debuggerHandler)
        st = handler(cx, script, regs.pc, rval.address(), cx->runtime()->debugHooks.debuggerHandlerData);
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
}
END_CASE(JSOP_DEBUGGER)

BEGIN_CASE(JSOP_ENTERBLOCK)
BEGIN_CASE(JSOP_ENTERLET0)
BEGIN_CASE(JSOP_ENTERLET1)
{
    StaticBlockObject &blockObj = script->getObject(regs.pc)->as<StaticBlockObject>();

    if (op == JSOP_ENTERBLOCK) {
        JS_ASSERT(regs.stackDepth() == blockObj.stackDepth());
        JS_ASSERT(regs.stackDepth() + blockObj.slotCount() <= script->nslots);
        Value *vp = regs.sp + blockObj.slotCount();
        SetValueRangeToUndefined(regs.sp, vp);
        regs.sp = vp;
    }

    /* Clone block iff there are any closed-over variables. */
    if (!regs.fp()->pushBlock(cx, blockObj))
        goto error;
}
END_CASE(JSOP_ENTERBLOCK)

BEGIN_CASE(JSOP_LEAVEBLOCK)
BEGIN_CASE(JSOP_LEAVEFORLETIN)
BEGIN_CASE(JSOP_LEAVEBLOCKEXPR)
{
    blockDepth = regs.fp()->blockChain().stackDepth();

    regs.fp()->popBlock(cx);

    if (op == JSOP_LEAVEBLOCK) {
        /* Pop the block's slots. */
        regs.sp -= GET_UINT16(regs.pc);
        JS_ASSERT(regs.stackDepth() == blockDepth);
    } else if (op == JSOP_LEAVEBLOCKEXPR) {
        /* Pop the block's slots maintaining the topmost expr. */
        Value *vp = &regs.sp[-1];
        regs.sp -= GET_UINT16(regs.pc);
        JS_ASSERT(regs.stackDepth() == blockDepth + 1);
        regs.sp[-1] = *vp;
    } else {
        /* Another op will pop; nothing to do here. */
        len = JSOP_LEAVEFORLETIN_LENGTH;
        DO_NEXT_OP(len);
    }
}
END_CASE(JSOP_LEAVEBLOCK)

BEGIN_CASE(JSOP_GENERATOR)
{
    JS_ASSERT(!cx->isExceptionPending());
    regs.fp()->initGeneratorFrame();
    regs.pc += JSOP_GENERATOR_LENGTH;
    JSObject *obj = js_NewGenerator(cx, regs);
    if (!obj)
        goto error;
    regs.fp()->setReturnValue(ObjectValue(*obj));
    regs.fp()->setYielding();
    interpReturnOK = true;
    if (entryFrame != regs.fp())
        goto inline_return;
    goto exit;
}

BEGIN_CASE(JSOP_YIELD)
    JS_ASSERT(!cx->isExceptionPending());
    JS_ASSERT(regs.fp()->isNonEvalFunctionFrame());
    if (cx->innermostGenerator()->state == JSGEN_CLOSING) {
        RootedValue &val = rootValue0;
        val.setObject(regs.fp()->callee());
        js_ReportValueError(cx, JSMSG_BAD_GENERATOR_YIELD, JSDVG_SEARCH_STACK, val, NullPtr());
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
    RootedObject &obj = rootObject0;
    obj = &regs.fp()->unaliasedLocal(slot).toObject();
    if (!js_NewbornArrayPush(cx, obj, regs.sp[-1]))
        goto error;
    regs.sp--;
}
END_CASE(JSOP_ARRAYPUSH)

          default:
          {
            char numBuf[12];
            JS_snprintf(numBuf, sizeof numBuf, "%d", op);
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                 JSMSG_BAD_BYTECODE, numBuf);
            goto error;
          }

        } /* switch (op) */
    } /* for (;;) */

  error:
    JS_ASSERT(uint32_t(regs.pc - script->code) < script->length);

    if (cx->isExceptionPending()) {
        /* Call debugger throw hooks. */
        if (cx->compartment()->debugMode()) {
            JSTrapStatus status = DebugExceptionUnwind(cx, regs.fp(), regs.pc);
            switch (status) {
              case JSTRAP_ERROR:
                goto error;

              case JSTRAP_CONTINUE:
              case JSTRAP_THROW:
                break;

              case JSTRAP_RETURN:
                interpReturnOK = true;
                goto forced_return;

              default:
                MOZ_ASSUME_UNREACHABLE("Invalid trap status");
            }
        }

        for (TryNoteIter tni(cx, regs); !tni.done(); ++tni) {
            JSTryNote *tn = *tni;

            UnwindScope(cx, regs.fp(), tn->stackDepth);

            /*
             * Set pc to the first bytecode after the the try note to point
             * to the beginning of catch or finally or to [enditer] closing
             * the for-in loop.
             */
            regs.pc = (script)->main() + tn->start + tn->length;
            regs.sp = regs.spForStackDepth(tn->stackDepth);

            switch (tn->kind) {
              case JSTRY_CATCH:
                  JS_ASSERT(*regs.pc == JSOP_ENTERBLOCK);

                /* Catch cannot intercept the closing of a generator. */
                  if (JS_UNLIKELY(cx->getPendingException().isMagic(JS_GENERATOR_CLOSING)))
                    break;

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
                RootedObject &obj = rootObject0;
                obj = &regs.sp[-1].toObject();
                bool ok = UnwindIteratorForException(cx, obj);
                regs.sp -= 1;
                if (!ok)
                    goto error;
                break;
              }

              case JSTRY_LOOP:
                break;
           }
        }

        /*
         * Propagate the exception or error to the caller unless the exception
         * is an asynchronous return from a generator.
         */
        interpReturnOK = false;
        if (JS_UNLIKELY(cx->isExceptionPending() &&
                        cx->getPendingException().isMagic(JS_GENERATOR_CLOSING))) {
            cx->clearPendingException();
            interpReturnOK = true;
            regs.fp()->clearReturnValue();
        }
    } else {
        UnwindForUncatchableException(cx, regs);
        interpReturnOK = false;
    }

  forced_return:
    UnwindScope(cx, regs.fp(), 0);
    regs.setToEndOfScript();

    if (entryFrame != regs.fp())
        goto inline_return;

  exit:
    if (cx->compartment()->debugMode())
        interpReturnOK = ScriptDebugEpilogue(cx, regs.fp(), interpReturnOK);
    if (!regs.fp()->isYielding())
        regs.fp()->epilogue(cx);
    else
        Probes::exitScript(cx, script, script->function(), regs.fp());

    gc::MaybeVerifyBarriers(cx, true);

#ifdef JS_ION
    /*
     * This path is used when it's guaranteed the method can be finished
     * inside the JIT.
     */
  leave_on_safe_point:
#endif

    if (interpReturnOK)
        state.setReturnValue(entryFrame->returnValue());

    return interpReturnOK;
}

bool
js::Throw(JSContext *cx, HandleValue v)
{
    JS_ASSERT(!cx->isExceptionPending());
    cx->setPendingException(v);
    return false;
}

bool
js::GetProperty(JSContext *cx, HandleValue v, HandlePropertyName name, MutableHandleValue vp)
{
    if (name == cx->names().length) {
        // Fast path for strings, arrays and arguments.
        if (GetLengthProperty(v, vp))
            return true;
    }

    RootedObject obj(cx, ToObjectFromStack(cx, v));
    if (!obj)
        return false;
    return JSObject::getProperty(cx, obj, obj, name, vp);
}

bool
js::GetScopeName(JSContext *cx, HandleObject scopeChain, HandlePropertyName name, MutableHandleValue vp)
{
    RootedShape shape(cx);
    RootedObject obj(cx), pobj(cx);
    if (!LookupName(cx, name, scopeChain, &obj, &pobj, &shape))
        return false;

    if (!shape) {
        JSAutoByteString printable;
        if (AtomToPrintableString(cx, name, &printable))
            js_ReportIsNotDefined(cx, printable.ptr());
        return false;
    }

    return JSObject::getProperty(cx, obj, obj, name, vp);
}

/*
 * Alternate form for NAME opcodes followed immediately by a TYPEOF,
 * which do not report an exception on (typeof foo == "undefined") tests.
 */
bool
js::GetScopeNameForTypeOf(JSContext *cx, HandleObject scopeChain, HandlePropertyName name,
                          MutableHandleValue vp)
{
    RootedShape shape(cx);
    RootedObject obj(cx), pobj(cx);
    if (!LookupName(cx, name, scopeChain, &obj, &pobj, &shape))
        return false;

    if (!shape) {
        vp.set(UndefinedValue());
        return true;
    }

    return JSObject::getProperty(cx, obj, obj, name, vp);
}

JSObject *
js::Lambda(JSContext *cx, HandleFunction fun, HandleObject parent)
{
    RootedObject clone(cx, CloneFunctionObjectIfNotSingleton(cx, fun, parent, TenuredObject));
    if (!clone)
        return NULL;

    if (fun->isArrow()) {
        // Note that this will assert if called from Ion code. Ion can't yet
        // emit code for a bound arrow function (bug 851913).
        AbstractFramePtr frame;
        if (cx->currentlyRunningInInterpreter()) {
            frame = cx->interpreterFrame();
        } else {
#ifdef JS_ION
            JS_ASSERT(cx->currentlyRunningInJit());
            frame = ion::GetTopBaselineFrame(cx);
#endif
        }

        if (!ComputeThis(cx, frame))
            return NULL;

        RootedValue thisval(cx, frame.thisValue());
        clone = js_fun_bind(cx, clone, thisval, NULL, 0);
        if (!clone)
            return NULL;
        clone->as<JSFunction>().flags |= JSFunction::ARROW;
    }

    JS_ASSERT(clone->global() == clone->global());
    return clone;
}

bool
js::DefFunOperation(JSContext *cx, HandleScript script, HandleObject scopeChain,
                    HandleFunction funArg)
{
    /*
     * If static link is not current scope, clone fun's object to link to the
     * current scope via parent. We do this to enable sharing of compiled
     * functions among multiple equivalent scopes, amortizing the cost of
     * compilation over a number of executions.  Examples include XUL scripts
     * and event handlers shared among Firefox or other Mozilla app chrome
     * windows, and user-defined JS functions precompiled and then shared among
     * requests in server-side JS.
     */
    RootedFunction fun(cx, funArg);
    if (fun->isNative() || fun->environment() != scopeChain) {
        fun = CloneFunctionObjectIfNotSingleton(cx, fun, scopeChain, TenuredObject);
        if (!fun)
            return false;
    } else {
        JS_ASSERT(script->compileAndGo);
        JS_ASSERT(!script->function());
    }

    /*
     * We define the function as a property of the variable object and not the
     * current scope chain even for the case of function expression statements
     * and functions defined by eval inside let or with blocks.
     */
    RootedObject parent(cx, scopeChain);
    while (!parent->isVarObj())
        parent = parent->enclosingScope();

    /* ES5 10.5 (NB: with subsequent errata). */
    RootedPropertyName name(cx, fun->atom()->asPropertyName());

    RootedShape shape(cx);
    RootedObject pobj(cx);
    if (!JSObject::lookupProperty(cx, parent, name, &pobj, &shape))
        return false;

    RootedValue rval(cx, ObjectValue(*fun));

    /*
     * ECMA requires functions defined when entering Eval code to be
     * impermanent.
     */
    unsigned attrs = script->isActiveEval
                     ? JSPROP_ENUMERATE
                     : JSPROP_ENUMERATE | JSPROP_PERMANENT;

    /* Steps 5d, 5f. */
    if (!shape || pobj != parent) {
        return JSObject::defineProperty(cx, parent, name, rval, JS_PropertyStub,
                                        JS_StrictPropertyStub, attrs);
    }

    /* Step 5e. */
    JS_ASSERT(parent->isNative());
    if (parent->is<GlobalObject>()) {
        if (shape->configurable()) {
            return JSObject::defineProperty(cx, parent, name, rval, JS_PropertyStub,
                                            JS_StrictPropertyStub, attrs);
        }

        if (shape->isAccessorDescriptor() || !shape->writable() || !shape->enumerable()) {
            JSAutoByteString bytes;
            if (AtomToPrintableString(cx, name, &bytes)) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CANT_REDEFINE_PROP,
                                     bytes.ptr());
            }

            return false;
        }
    }

    /*
     * Non-global properties, and global properties which we aren't simply
     * redefining, must be set.  First, this preserves their attributes.
     * Second, this will produce warnings and/or errors as necessary if the
     * specified Call object property is not writable (const).
     */

    /* Step 5f. */
    return JSObject::setProperty(cx, parent, parent, name, &rval, script->strict);
}

bool
js::SetCallOperation(JSContext *cx)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_LEFTSIDE_OF_ASS);
    return false;
}

bool
js::GetAndClearException(JSContext *cx, MutableHandleValue res)
{
    // Check the interrupt flag to allow interrupting deeply nested exception
    // handling.
    if (cx->runtime()->interrupt && !js_HandleExecutionInterrupt(cx))
        return false;

    res.set(cx->getPendingException());
    cx->clearPendingException();
    return true;
}

template <bool strict>
bool
js::SetProperty(JSContext *cx, HandleObject obj, HandleId id, const Value &value)
{
    RootedValue v(cx, value);
    return JSObject::setGeneric(cx, obj, obj, id, &v, strict);
}

template bool js::SetProperty<true> (JSContext *cx, HandleObject obj, HandleId id, const Value &value);
template bool js::SetProperty<false>(JSContext *cx, HandleObject obj, HandleId id, const Value &value);

template <bool strict>
bool
js::DeleteProperty(JSContext *cx, HandleValue v, HandlePropertyName name, JSBool *bp)
{
    // convert value to JSObject pointer
    RootedObject obj(cx, ToObjectFromStack(cx, v));
    if (!obj)
        return false;

    if (!JSObject::deleteProperty(cx, obj, name, bp))
        return false;
    if (strict && !*bp) {
        obj->reportNotConfigurable(cx, NameToId(name));
        return false;
    }
    return true;
}

template bool js::DeleteProperty<true> (JSContext *cx, HandleValue val, HandlePropertyName name, JSBool *bp);
template bool js::DeleteProperty<false>(JSContext *cx, HandleValue val, HandlePropertyName name, JSBool *bp);

template <bool strict>
bool
js::DeleteElement(JSContext *cx, HandleValue val, HandleValue index, JSBool *bp)
{
    RootedObject obj(cx, ToObjectFromStack(cx, val));
    if (!obj)
        return false;

    if (!JSObject::deleteByValue(cx, obj, index, bp))
        return false;
    if (strict && !*bp) {
        // XXX This observably calls ToString(propval).  We should convert to
        //     PropertyKey and use that to delete, and to report an error if
        //     necessary!
        RootedId id(cx);
        if (!ValueToId<CanGC>(cx, index, &id))
            return false;
        obj->reportNotConfigurable(cx, id);
        return false;
    }
    return true;
}

template bool js::DeleteElement<true> (JSContext *, HandleValue, HandleValue, JSBool *succeeded);
template bool js::DeleteElement<false>(JSContext *, HandleValue, HandleValue, JSBool *succeeded);

bool
js::GetElement(JSContext *cx, MutableHandleValue lref, HandleValue rref, MutableHandleValue vp)
{
    return GetElementOperation(cx, JSOP_GETELEM, lref, rref, vp);
}

bool
js::CallElement(JSContext *cx, MutableHandleValue lref, HandleValue rref, MutableHandleValue res)
{
    return GetElementOperation(cx, JSOP_CALLELEM, lref, rref, res);
}

bool
js::SetObjectElement(JSContext *cx, HandleObject obj, HandleValue index, HandleValue value,
                     JSBool strict)
{
    RootedId id(cx);
    if (!ValueToId<CanGC>(cx, index, &id))
        return false;
    return SetObjectElementOperation(cx, obj, id, value, strict);
}

bool
js::SetObjectElement(JSContext *cx, HandleObject obj, HandleValue index, HandleValue value,
                     JSBool strict, HandleScript script, jsbytecode *pc)
{
    JS_ASSERT(pc);
    RootedId id(cx);
    if (!ValueToId<CanGC>(cx, index, &id))
        return false;
    return SetObjectElementOperation(cx, obj, id, value, strict, script, pc);
}

bool
js::InitElementArray(JSContext *cx, jsbytecode *pc, HandleObject obj, uint32_t index, HandleValue value)
{
    return InitArrayElemOperation(cx, pc, obj, index, value);
}

bool
js::AddValues(JSContext *cx, HandleScript script, jsbytecode *pc,
              MutableHandleValue lhs, MutableHandleValue rhs,
              Value *res)
{
    return AddOperation(cx, script, pc, lhs, rhs, res);
}

bool
js::SubValues(JSContext *cx, HandleScript script, jsbytecode *pc,
              MutableHandleValue lhs, MutableHandleValue rhs,
              Value *res)
{
    return SubOperation(cx, script, pc, lhs, rhs, res);
}

bool
js::MulValues(JSContext *cx, HandleScript script, jsbytecode *pc,
              MutableHandleValue lhs, MutableHandleValue rhs,
              Value *res)
{
    return MulOperation(cx, script, pc, lhs, rhs, res);
}

bool
js::DivValues(JSContext *cx, HandleScript script, jsbytecode *pc,
              MutableHandleValue lhs, MutableHandleValue rhs,
              Value *res)
{
    return DivOperation(cx, script, pc, lhs, rhs, res);
}

bool
js::ModValues(JSContext *cx, HandleScript script, jsbytecode *pc,
              MutableHandleValue lhs, MutableHandleValue rhs,
              Value *res)
{
    return ModOperation(cx, script, pc, lhs, rhs, res);
}

bool
js::UrshValues(JSContext *cx, HandleScript script, jsbytecode *pc,
               MutableHandleValue lhs, MutableHandleValue rhs,
               Value *res)
{
    return UrshOperation(cx, script, pc, lhs, rhs, res);
}

bool
js::DeleteNameOperation(JSContext *cx, HandlePropertyName name, HandleObject scopeObj,
                        MutableHandleValue res)
{
    RootedObject scope(cx), pobj(cx);
    RootedShape shape(cx);
    if (!LookupName(cx, name, scopeObj, &scope, &pobj, &shape))
        return false;

    if (!scope) {
        // Return true for non-existent names.
        res.setBoolean(true);
        return true;
    }

    JSBool succeeded;
    if (!JSObject::deleteProperty(cx, scope, name, &succeeded))
        return false;
    res.setBoolean(succeeded);
    return true;
}

bool
js::ImplicitThisOperation(JSContext *cx, HandleObject scopeObj, HandlePropertyName name,
                          MutableHandleValue res)
{
    RootedObject obj(cx);
    if (!LookupNameWithGlobalDefault(cx, name, scopeObj, &obj))
        return false;

    return ComputeImplicitThis(cx, obj, res);
}

bool
js::RunOnceScriptPrologue(JSContext *cx, HandleScript script)
{
    JS_ASSERT(script->treatAsRunOnce);

    if (!script->hasRunOnce) {
        script->hasRunOnce = true;
        return true;
    }

    // Force instantiation of the script's function's type to ensure the flag
    // is preserved in type information.
    if (!script->function()->getType(cx))
        return false;

    types::MarkTypeObjectFlags(cx, script->function(), types::OBJECT_FLAG_RUNONCE_INVALIDATED);
    return true;
}

bool
js::InitGetterSetterOperation(JSContext *cx, jsbytecode *pc, HandleObject obj, HandleId id,
                              HandleValue val)
{
    JS_ASSERT(js_IsCallable(val));

    /*
     * Getters and setters are just like watchpoints from an access control
     * point of view.
     */
    RootedValue scratch(cx, UndefinedValue());
    unsigned attrs = 0;
    if (!CheckAccess(cx, obj, id, JSACC_WATCH, &scratch, &attrs))
        return false;

    PropertyOp getter;
    StrictPropertyOp setter;
    attrs = JSPROP_ENUMERATE | JSPROP_SHARED;

    JSOp op = JSOp(*pc);

    if (op == JSOP_INITPROP_GETTER || op == JSOP_INITELEM_GETTER) {
        getter = CastAsPropertyOp(&val.toObject());
        setter = JS_StrictPropertyStub;
        attrs |= JSPROP_GETTER;
    } else {
        JS_ASSERT(op == JSOP_INITPROP_SETTER || op == JSOP_INITELEM_SETTER);
        getter = JS_PropertyStub;
        setter = CastAsStrictPropertyOp(&val.toObject());
        attrs |= JSPROP_SETTER;
    }

    scratch.setUndefined();
    return JSObject::defineGeneric(cx, obj, id, scratch, getter, setter, attrs);
}

bool
js::InitGetterSetterOperation(JSContext *cx, jsbytecode *pc, HandleObject obj,
                              HandlePropertyName name, HandleValue val)
{
    RootedId id(cx, NameToId(name));
    return InitGetterSetterOperation(cx, pc, obj, id, val);
}

bool
js::InitGetterSetterOperation(JSContext *cx, jsbytecode *pc, HandleObject obj, HandleValue idval,
                              HandleValue val)
{
    RootedId id(cx);
    if (!ValueToId<CanGC>(cx, idval, &id))
        return false;

    return InitGetterSetterOperation(cx, pc, obj, id, val);
}
