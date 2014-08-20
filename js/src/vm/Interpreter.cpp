/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * JavaScript bytecode interpreter.
 */

#include "vm/Interpreter-inl.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/PodOperations.h"

#include <string.h>

#include "jsarray.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jsiter.h"
#include "jslibmath.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsopcode.h"
#include "jsprf.h"
#include "jsscript.h"
#include "jsstr.h"

#include "builtin/Eval.h"
#include "jit/BaselineJIT.h"
#include "jit/Ion.h"
#include "jit/IonAnalysis.h"
#include "js/OldDebugAPI.h"
#include "vm/Debugger.h"
#include "vm/Opcodes.h"
#include "vm/Shape.h"
#include "vm/TraceLogging.h"

#include "jsatominlines.h"
#include "jsboolinlines.h"
#include "jsfuninlines.h"
#include "jsinferinlines.h"
#include "jsscriptinlines.h"

#include "jit/IonFrames-inl.h"
#include "vm/Probes-inl.h"
#include "vm/ScopeObject-inl.h"
#include "vm/Stack-inl.h"

using namespace js;
using namespace js::gc;
using namespace js::types;

using mozilla::DebugOnly;
using mozilla::NumberEqualsInt32;
using mozilla::PodCopy;
using JS::ForOfIterator;

template <bool Eq>
static MOZ_ALWAYS_INLINE bool
LooseEqualityOp(JSContext *cx, InterpreterRegs &regs)
{
    HandleValue rval = regs.stackHandleAt(-1);
    HandleValue lval = regs.stackHandleAt(-2);
    bool cond;
    if (!LooselyEqual(cx, lval, rval, &cond))
        return false;
    cond = (cond == Eq);
    regs.sp--;
    regs.sp[-1].setBoolean(cond);
    return true;
}

JSObject *
js::BoxNonStrictThis(JSContext *cx, HandleValue thisv)
{
    /*
     * Check for SynthesizeFrame poisoning and fast constructors which
     * didn't check their callee properly.
     */
    JS_ASSERT(!thisv.isMagic());

    if (thisv.isNullOrUndefined()) {
        Rooted<GlobalObject*> global(cx, cx->global());
        return JSObject::thisObject(cx, global);
    }

    if (thisv.isObject())
        return &thisv.toObject();

    return PrimitiveToObject(cx, thisv);
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
    JS_ASSERT(!call.thisv().isMagic());

#ifdef DEBUG
    JSFunction *fun = call.callee().is<JSFunction>() ? &call.callee().as<JSFunction>() : nullptr;
    JS_ASSERT_IF(fun && fun->isInterpreted(), !fun->strict());
#endif

    JSObject *thisObj = BoxNonStrictThis(cx, call.thisv());
    if (!thisObj)
        return false;

    call.setThis(ObjectValue(*thisObj));
    return true;
}

#if JS_HAS_NO_SUCH_METHOD

static const uint32_t JSSLOT_FOUND_FUNCTION = 0;
static const uint32_t JSSLOT_SAVED_ID = 1;

static const Class js_NoSuchMethodClass = {
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

    if (value.isObject()) {
        JSObject *obj = NewObjectWithClassProto(cx, &js_NoSuchMethodClass, nullptr, nullptr);
        if (!obj)
            return false;

        obj->setSlot(JSSLOT_FOUND_FUNCTION, value);
        obj->setSlot(JSSLOT_SAVED_ID, idval);
        vp.setObject(*obj);
    }
    return true;
}

static bool
NoSuchMethod(JSContext *cx, unsigned argc, Value *vp)
{
    InvokeArgs args(cx);
    if (!args.init(2))
        return false;

    JS_ASSERT(vp[0].isObject());
    JS_ASSERT(vp[1].isObject());
    JSObject *obj = &vp[0].toObject();
    JS_ASSERT(obj->getClass() == &js_NoSuchMethodClass);

    args.setCallee(obj->getReservedSlot(JSSLOT_FOUND_FUNCTION));
    args.setThis(vp[1]);
    args[0].set(obj->getReservedSlot(JSSLOT_SAVED_ID));
    JSObject *argsobj = NewDenseCopiedArray(cx, argc, vp + 2);
    if (!argsobj)
        return false;
    args[1].setObject(*argsobj);
    bool ok = Invoke(cx, args);
    vp[0] = args.rval();
    return ok;
}

#endif /* JS_HAS_NO_SUCH_METHOD */

static inline bool
GetPropertyOperation(JSContext *cx, InterpreterFrame *fp, HandleScript script, jsbytecode *pc,
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

    RootedId id(cx, NameToId(script->getName(pc)));

    if (id == NameToId(cx->names().callee) && IsOptimizedArguments(fp, lval.address())) {
        vp.setObject(fp->callee());
        return true;
    }

    Rooted<GlobalObject*> global(cx, &fp->global());
    RootedObject obj(cx);

    /* Optimize (.1).toString(). */
    if (lval.isNumber() && id == NameToId(cx->names().toString)) {
        JSObject *proto = GlobalObject::getOrCreateNumberPrototype(cx, global);
        if (!proto)
            return false;
        if (ClassMethodIsNative(cx, proto, &NumberObject::class_, id, js_num_toString))
            obj = proto;
    }

    if (!obj) {
        obj = ToObjectFromStack(cx, lval);
        if (!obj)
            return false;
    }

    bool wasObject = lval.isObject();

    if (!JSObject::getGeneric(cx, obj, obj, id, vp))
        return false;

#if JS_HAS_NO_SUCH_METHOD
    if (op == JSOP_CALLPROP &&
        MOZ_UNLIKELY(vp.isUndefined()) &&
        wasObject)
    {
        if (!OnUnknownMethod(cx, obj, IdToValue(id), vp))
            return false;
    }
#endif

    return true;
}

static inline bool
NameOperation(JSContext *cx, InterpreterFrame *fp, jsbytecode *pc, MutableHandleValue vp)
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

    Shape *shape = nullptr;
    JSObject *scope = nullptr, *pobj = nullptr;
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

static inline bool
SetPropertyOperation(JSContext *cx, HandleScript script, jsbytecode *pc, HandleValue lval,
                     HandleValue rval)
{
    JS_ASSERT(*pc == JSOP_SETPROP);

    RootedObject obj(cx, ToObjectFromStack(cx, lval));
    if (!obj)
        return false;

    RootedValue rref(cx, rval);

    RootedId id(cx, NameToId(script->getName(pc)));
    if (MOZ_LIKELY(!obj->getOps()->setProperty)) {
        if (!baseops::SetPropertyHelper<SequentialExecution>(cx, obj, obj, id, baseops::Qualified,
                                                             &rref, script->strict()))
        {
            return false;
        }
    } else {
        if (!JSObject::setGeneric(cx, obj, obj, id, &rref, script->strict()))
            return false;
    }

    return true;
}

bool
js::ReportIsNotFunction(JSContext *cx, HandleValue v, int numToSkip, MaybeConstruct construct)
{
    unsigned error = construct ? JSMSG_NOT_CONSTRUCTOR : JSMSG_NOT_FUNCTION;
    int spIndex = numToSkip >= 0 ? -(numToSkip + 1) : JSDVG_SEARCH_STACK;

    js_ReportValueError3(cx, error, spIndex, v, NullPtr(), nullptr, nullptr);
    return false;
}

JSObject *
js::ValueToCallable(JSContext *cx, HandleValue v, int numToSkip, MaybeConstruct construct)
{
    if (v.isObject()) {
        JSObject *callable = &v.toObject();
        if (callable->isCallable())
            return callable;
    }

    ReportIsNotFunction(cx, v, numToSkip, construct);
    return nullptr;
}

static MOZ_NEVER_INLINE bool
Interpret(JSContext *cx, RunState &state);

InterpreterFrame *
InvokeState::pushInterpreterFrame(JSContext *cx)
{
    return cx->runtime()->interpreterStack().pushInvokeFrame(cx, args_, initial_);
}

InterpreterFrame *
ExecuteState::pushInterpreterFrame(JSContext *cx)
{
    return cx->runtime()->interpreterStack().pushExecuteFrame(cx, script_, thisv_, scopeChain_,
                                                              type_, evalInFrame_);
}

bool
js::RunScript(JSContext *cx, RunState &state)
{
    JS_CHECK_RECURSION(cx, return false);

#ifdef NIGHTLY_BUILD
    if (AssertOnScriptEntryHook hook = cx->runtime()->assertOnScriptEntryHook_)
        (*hook)(cx, state.script());
#endif

    SPSEntryMarker marker(cx->runtime(), state.script());

    state.script()->ensureNonLazyCanonicalFunction(cx);

    if (jit::IsIonEnabled(cx)) {
        jit::MethodStatus status = jit::CanEnter(cx, state);
        if (status == jit::Method_Error)
            return false;
        if (status == jit::Method_Compiled) {
            jit::IonExecStatus status = jit::IonCannon(cx, state);
            return !IsErrorStatus(status);
        }
    }

    if (jit::IsBaselineEnabled(cx)) {
        jit::MethodStatus status = jit::CanEnterBaselineMethod(cx, state);
        if (status == jit::Method_Error)
            return false;
        if (status == jit::Method_Compiled) {
            jit::IonExecStatus status = jit::EnterBaselineMethod(cx, state);
            return !IsErrorStatus(status);
        }
    }

    if (state.isInvoke()) {
        InvokeState &invoke = *state.asInvoke();
        TypeMonitorCall(cx, invoke.args(), invoke.constructing());
    }

    return Interpret(cx, state);
}

struct AutoGCIfNeeded
{
    JSContext *cx_;
    explicit AutoGCIfNeeded(JSContext *cx) : cx_(cx) {}
    ~AutoGCIfNeeded() { cx_->gcIfNeeded(); }
};

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

    /* Perform GC if necessary on exit from the function. */
    AutoGCIfNeeded gcIfNeeded(cx);

    /* MaybeConstruct is a subset of InitialFrameFlags */
    InitialFrameFlags initial = (InitialFrameFlags) construct;

    if (args.calleev().isPrimitive())
        return ReportIsNotFunction(cx, args.calleev(), args.length() + 1, construct);

    JSObject &callee = args.callee();
    const Class *clasp = callee.getClass();

    /* Invoke non-functions. */
    if (MOZ_UNLIKELY(clasp != &JSFunction::class_)) {
#if JS_HAS_NO_SUCH_METHOD
        if (MOZ_UNLIKELY(clasp == &js_NoSuchMethodClass))
            return NoSuchMethod(cx, args.length(), args.base());
#endif
        JS_ASSERT_IF(construct, !clasp->construct);
        if (!clasp->call)
            return ReportIsNotFunction(cx, args.calleev(), args.length() + 1, construct);
        return CallJSNative(cx, clasp->call, args);
    }

    /* Invoke native functions. */
    JSFunction *fun = &callee.as<JSFunction>();
    JS_ASSERT_IF(construct, !fun->isNativeConstructor());
    if (fun->isNative())
        return CallJSNative(cx, fun->native(), args);

    if (!fun->getOrCreateScript(cx))
        return false;

    /* Run function until JSOP_RETRVAL, JSOP_RETURN or error. */
    InvokeState state(cx, args, initial);

    // Check to see if useNewType flag should be set for this frame.
    if (construct) {
        FrameIter iter(cx);
        if (!iter.done() && iter.hasScript()) {
            JSScript *script = iter.script();
            jsbytecode *pc = iter.pc();
            if (UseNewType(cx, script, pc))
                state.setUseNewType();
        }
    }

    bool ok = RunScript(cx, state);

    JS_ASSERT_IF(ok && construct, args.rval().isObject());
    return ok;
}

bool
js::Invoke(JSContext *cx, const Value &thisv, const Value &fval, unsigned argc, const Value *argv,
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
         * |this| already.  But don't do that if fval is a DOM function.
         */
        if (!fval.isObject() || !fval.toObject().is<JSFunction>() ||
            !fval.toObject().as<JSFunction>().isNative() ||
            !fval.toObject().as<JSFunction>().jitInfo() ||
            fval.toObject().as<JSFunction>().jitInfo()->needsOuterizedThisObject())
        {
            RootedObject thisObj(cx, &args.thisv().toObject());
            JSObject *thisp = JSObject::thisObject(cx, thisObj);
            if (!thisp)
                return false;
            args.setThis(ObjectValue(*thisp));
        }
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
        return ReportIsNotFunction(cx, args.calleev(), args.length() + 1, CONSTRUCT);

    JSObject &callee = args.callee();
    if (callee.is<JSFunction>()) {
        RootedFunction fun(cx, &callee.as<JSFunction>());

        if (fun->isNativeConstructor()) {
            bool ok = CallJSNativeConstructor(cx, fun->native(), args);
            return ok;
        }

        if (!fun->isInterpretedConstructor()) {
            RootedValue orig(cx, ObjectValue(*fun->originalFunction()));
            return ReportIsNotFunction(cx, orig, args.length() + 1, CONSTRUCT);
        }
        if (!Invoke(cx, args, CONSTRUCT))
            return false;

        JS_ASSERT(args.rval().isObject());
        return true;
    }

    const Class *clasp = callee.getClass();
    if (!clasp->construct)
        return ReportIsNotFunction(cx, args.calleev(), args.length() + 1, CONSTRUCT);

    return CallJSNativeConstructor(cx, clasp->construct, args);
}

bool
js::InvokeConstructor(JSContext *cx, Value fval, unsigned argc, const Value *argv, Value *rval)
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
#ifdef DEBUG
    if (thisv.isObject()) {
        RootedObject thisObj(cx, &thisv.toObject());
        AutoSuppressGC nogc(cx);
        JS_ASSERT(GetOuterObject(cx, thisObj) == thisObj);
    }
#endif

    if (script->isEmpty()) {
        if (result)
            result->setUndefined();
        return true;
    }

    TypeScript::SetThis(cx, script, thisv);

    probes::StartExecution(script);
    ExecuteState state(cx, script, thisv, scopeChainArg, type, evalInFrame, result);
    bool ok = RunScript(cx, state);
    probes::StopExecution(script);

    return ok;
}

bool
js::Execute(JSContext *cx, HandleScript script, JSObject &scopeChainArg, Value *rval)
{
    /* The scope chain could be anything, so innerize just in case. */
    RootedObject scopeChain(cx, &scopeChainArg);
    scopeChain = GetInnerObject(scopeChain);
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
    if (!cx->runtime()->options().varObjFix()) {
        if (!scopeChain->setQualifiedVarObj(cx))
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
js::HasInstance(JSContext *cx, HandleObject obj, HandleValue v, bool *bp)
{
    const Class *clasp = obj->getClass();
    RootedValue local(cx, v);
    if (clasp->hasInstance)
        return clasp->hasInstance(cx, obj, &local, bp);

    RootedValue val(cx, ObjectValue(*obj));
    js_ReportValueError(cx, JSMSG_BAD_INSTANCEOF_RHS,
                        JSDVG_SEARCH_STACK, val, NullPtr());
    return false;
}

static inline bool
EqualGivenSameType(JSContext *cx, const Value &lval, const Value &rval, bool *equal)
{
    MOZ_ASSERT(SameType(lval, rval));

    if (lval.isString())
        return EqualStrings(cx, lval.toString(), rval.toString(), equal);
    if (lval.isDouble()) {
        *equal = (lval.toDouble() == rval.toDouble());
        return true;
    }
    if (lval.isGCThing()) {  // objects or symbols
        *equal = (lval.toGCThing() == rval.toGCThing());
        return true;
    }
    *equal = lval.payloadAsRawUint32() == rval.payloadAsRawUint32();
    MOZ_ASSERT_IF(lval.isUndefined(), *equal);
    return true;
}

bool
js::LooselyEqual(JSContext *cx, const Value &lval, const Value &rval, bool *result)
{
    if (SameType(lval, rval))
        return EqualGivenSameType(cx, lval, rval, result);

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

    if (SameType(lvalue, rvalue))
        return EqualGivenSameType(cx, lvalue, rvalue, result);

    if (lvalue.isSymbol() || rvalue.isSymbol()) {
        *result = false;
        return true;
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
    if (SameType(lval, rval))
        return EqualGivenSameType(cx, lval, rval, equal);

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
js::TypeOfObject(JSObject *obj)
{
    if (EmulatesUndefined(obj))
        return JSTYPE_VOID;
    if (obj->isCallable())
        return JSTYPE_FUNCTION;
    return JSTYPE_OBJECT;
}

JSType
js::TypeOfValue(const Value &v)
{
    if (v.isNumber())
        return JSTYPE_NUMBER;
    if (v.isString())
        return JSTYPE_STRING;
    if (v.isNull())
        return JSTYPE_OBJECT;
    if (v.isUndefined())
        return JSTYPE_VOID;
    if (v.isObject())
        return TypeOfObject(&v.toObject());
    if (v.isBoolean())
        return JSTYPE_BOOLEAN;
    JS_ASSERT(v.isSymbol());
    return JSTYPE_SYMBOL;
}

/*
 * Enter the new with scope using an object at sp[-1] and associate the depth
 * of the with block with sp + stackIndex.
 */
bool
js::EnterWithOperation(JSContext *cx, AbstractFramePtr frame, HandleValue val,
                       HandleObject staticWith)
{
    JS_ASSERT(staticWith->is<StaticWithObject>());
    RootedObject obj(cx);
    if (val.isObject()) {
        obj = &val.toObject();
    } else {
        obj = ToObject(cx, val);
        if (!obj)
            return false;
    }

    RootedObject scopeChain(cx, frame.scopeChain());
    DynamicWithObject *withobj = DynamicWithObject::create(cx, obj, scopeChain, staticWith);
    if (!withobj)
        return false;

    frame.pushOnScopeChain(*withobj);
    return true;
}

// Unwind scope chain and iterator to match the static scope corresponding to
// the given bytecode position.
void
js::UnwindScope(JSContext *cx, ScopeIter &si, jsbytecode *pc)
{
    if (si.done())
        return;

    Rooted<NestedScopeObject *> staticScope(cx, si.frame().script()->getStaticScope(pc));

    for (; si.staticScope() != staticScope; ++si) {
        switch (si.type()) {
          case ScopeIter::Block:
            if (cx->compartment()->debugMode())
                DebugScopes::onPopBlock(cx, si);
            if (si.staticBlock().needsClone())
                si.frame().popBlock(cx);
            break;
          case ScopeIter::With:
            si.frame().popWith(cx);
            break;
          case ScopeIter::Call:
          case ScopeIter::StrictEvalScope:
            break;
        }
    }
}

static void
ForcedReturn(JSContext *cx, ScopeIter &si, InterpreterRegs &regs)
{
    UnwindScope(cx, si, regs.fp()->script()->main());
    regs.setToEndOfScript();
}

static void
ForcedReturn(JSContext *cx, InterpreterRegs &regs)
{
    ScopeIter si(regs.fp(), regs.pc, cx);
    ForcedReturn(cx, si, regs);
}

void
js::UnwindForUncatchableException(JSContext *cx, const InterpreterRegs &regs)
{
    /* c.f. the regular (catchable) TryNoteIter loop in HandleError. */
    for (TryNoteIter tni(cx, regs); !tni.done(); ++tni) {
        JSTryNote *tn = *tni;
        if (tn->kind == JSTRY_ITER) {
            Value *sp = regs.spForStackDepth(tn->stackDepth);
            UnwindIteratorForUncatchableException(cx, &sp[-1].toObject());
        }
    }
}

TryNoteIter::TryNoteIter(JSContext *cx, const InterpreterRegs &regs)
  : regs(regs),
    script(cx, regs.fp()->script()),
    pcOffset(regs.pc - script->main())
{
    if (script->hasTrynotes()) {
        tn = script->trynotes()->vector;
        tnEnd = tn + script->trynotes()->length;
    } else {
        tn = tnEnd = nullptr;
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

enum HandleErrorContinuation
{
    SuccessfulReturnContinuation,
    ErrorReturnContinuation,
    CatchContinuation,
    FinallyContinuation
};

static HandleErrorContinuation
HandleError(JSContext *cx, InterpreterRegs &regs)
{
    JS_ASSERT(regs.fp()->script()->containsPC(regs.pc));

    ScopeIter si(regs.fp(), regs.pc, cx);
    bool ok = false;

  again:
    if (cx->isExceptionPending()) {
        /* Call debugger throw hooks. */
        if (MOZ_UNLIKELY(cx->compartment()->debugMode())) {
            JSTrapStatus status = DebugExceptionUnwind(cx, regs.fp(), regs.pc);
            switch (status) {
              case JSTRAP_ERROR:
                goto again;

              case JSTRAP_CONTINUE:
              case JSTRAP_THROW:
                break;

              case JSTRAP_RETURN:
                ForcedReturn(cx, si, regs);
                return SuccessfulReturnContinuation;

              default:
                MOZ_CRASH("Invalid trap status");
            }
        }

        RootedValue exception(cx);
        for (TryNoteIter tni(cx, regs); !tni.done(); ++tni) {
            JSTryNote *tn = *tni;

            UnwindScope(cx, si, regs.fp()->script()->main() + tn->start);

            /*
             * Set pc to the first bytecode after the the try note to point
             * to the beginning of catch or finally or to [enditer] closing
             * the for-in loop.
             */
            regs.pc = regs.fp()->script()->main() + tn->start + tn->length;
            regs.sp = regs.spForStackDepth(tn->stackDepth);

            switch (tn->kind) {
              case JSTRY_CATCH:
                /* Catch cannot intercept the closing of a generator. */
                if (!cx->getPendingException(&exception))
                    return ErrorReturnContinuation;
                if (exception.isMagic(JS_GENERATOR_CLOSING))
                    break;
                return CatchContinuation;

              case JSTRY_FINALLY:
                return FinallyContinuation;

              case JSTRY_ITER: {
                /* This is similar to JSOP_ENDITER in the interpreter loop. */
                JS_ASSERT(JSOp(*regs.pc) == JSOP_ENDITER);
                RootedObject obj(cx, &regs.sp[-1].toObject());
                bool ok = UnwindIteratorForException(cx, obj);
                regs.sp -= 1;
                if (!ok)
                    goto again;
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
        if (cx->isExceptionPending()) {
            RootedValue exception(cx);
            if (!cx->getPendingException(&exception))
                return ErrorReturnContinuation;

            if (exception.isMagic(JS_GENERATOR_CLOSING)) {
                cx->clearPendingException();
                ok = true;
                regs.fp()->clearReturnValue();
            }
        }
    } else {
        // We may be propagating a forced return from the interrupt
        // callback, which cannot easily force a return.
        if (MOZ_UNLIKELY(cx->isPropagatingForcedReturn())) {
            cx->clearPropagatingForcedReturn();
            ForcedReturn(cx, si, regs);
            return SuccessfulReturnContinuation;
        }

        UnwindForUncatchableException(cx, regs);
    }

    ForcedReturn(cx, si, regs);
    return ok ? SuccessfulReturnContinuation : ErrorReturnContinuation;
}

#define REGS                     (activation.regs())
#define PUSH_COPY(v)             do { *REGS.sp++ = (v); assertSameCompartmentDebugOnly(cx, REGS.sp[-1]); } while (0)
#define PUSH_COPY_SKIP_CHECK(v)  *REGS.sp++ = (v)
#define PUSH_NULL()              REGS.sp++->setNull()
#define PUSH_UNDEFINED()         REGS.sp++->setUndefined()
#define PUSH_BOOLEAN(b)          REGS.sp++->setBoolean(b)
#define PUSH_DOUBLE(d)           REGS.sp++->setDouble(d)
#define PUSH_INT32(i)            REGS.sp++->setInt32(i)
#define PUSH_STRING(s)           do { REGS.sp++->setString(s); assertSameCompartmentDebugOnly(cx, REGS.sp[-1]); } while (0)
#define PUSH_OBJECT(obj)         do { REGS.sp++->setObject(obj); assertSameCompartmentDebugOnly(cx, REGS.sp[-1]); } while (0)
#define PUSH_OBJECT_OR_NULL(obj) do { REGS.sp++->setObjectOrNull(obj); assertSameCompartmentDebugOnly(cx, REGS.sp[-1]); } while (0)
#define PUSH_HOLE()              REGS.sp++->setMagic(JS_ELEMENTS_HOLE)
#define POP_COPY_TO(v)           (v) = *--REGS.sp
#define POP_RETURN_VALUE()       REGS.fp()->setReturnValue(*--REGS.sp)

#define FETCH_OBJECT(cx, n, obj)                                              \
    JS_BEGIN_MACRO                                                            \
        HandleValue val = REGS.stackHandleAt(n);                              \
        obj = ToObjectFromStack((cx), (val));                                 \
        if (!obj)                                                             \
            goto error;                                                       \
    JS_END_MACRO

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
static inline bool
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

static MOZ_ALWAYS_INLINE bool
AddOperation(JSContext *cx, MutableHandleValue lhs, MutableHandleValue rhs, MutableHandleValue res)
{
    if (lhs.isInt32() && rhs.isInt32()) {
        int32_t l = lhs.toInt32(), r = rhs.toInt32();
        int32_t t;
        if (MOZ_LIKELY(SafeAdd(l, r, &t))) {
            res.setInt32(t);
            return true;
        }
    }

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
        res.setString(str);
    } else {
        double l, r;
        if (!ToNumber(cx, lhs, &l) || !ToNumber(cx, rhs, &r))
            return false;
        res.setNumber(l + r);
    }

    return true;
}

static MOZ_ALWAYS_INLINE bool
SubOperation(JSContext *cx, HandleValue lhs, HandleValue rhs, MutableHandleValue res)
{
    double d1, d2;
    if (!ToNumber(cx, lhs, &d1) || !ToNumber(cx, rhs, &d2))
        return false;
    res.setNumber(d1 - d2);
    return true;
}

static MOZ_ALWAYS_INLINE bool
MulOperation(JSContext *cx, HandleValue lhs, HandleValue rhs, MutableHandleValue res)
{
    double d1, d2;
    if (!ToNumber(cx, lhs, &d1) || !ToNumber(cx, rhs, &d2))
        return false;
    res.setNumber(d1 * d2);
    return true;
}

static MOZ_ALWAYS_INLINE bool
DivOperation(JSContext *cx, HandleValue lhs, HandleValue rhs, MutableHandleValue res)
{
    double d1, d2;
    if (!ToNumber(cx, lhs, &d1) || !ToNumber(cx, rhs, &d2))
        return false;
    res.setNumber(NumberDiv(d1, d2));
    return true;
}

static MOZ_ALWAYS_INLINE bool
ModOperation(JSContext *cx, HandleValue lhs, HandleValue rhs, MutableHandleValue res)
{
    int32_t l, r;
    if (lhs.isInt32() && rhs.isInt32() &&
        (l = lhs.toInt32()) >= 0 && (r = rhs.toInt32()) > 0) {
        int32_t mod = l % r;
        res.setInt32(mod);
        return true;
    }

    double d1, d2;
    if (!ToNumber(cx, lhs, &d1) || !ToNumber(cx, rhs, &d2))
        return false;

    res.setNumber(NumberMod(d1, d2));
    return true;
}

static MOZ_ALWAYS_INLINE bool
SetObjectElementOperation(JSContext *cx, Handle<JSObject*> obj, HandleId id, const Value &value,
                          bool strict, JSScript *script = nullptr, jsbytecode *pc = nullptr)
{
    types::TypeScript::MonitorAssign(cx, obj, id);

    if (obj->isNative() && JSID_IS_INT(id)) {
        uint32_t length = obj->getDenseInitializedLength();
        int32_t i = JSID_TO_INT(id);
        if ((uint32_t)i >= length) {
            // Annotate script if provided with information (e.g. baseline)
            if (script && script->hasBaselineScript() && *pc == JSOP_SETELEM)
                script->baselineScript()->noteArrayWriteHole(script->pcToOffset(pc));
        }
    }

    if (obj->isNative() && !JSID_IS_INT(id) && !obj->setHadElementsAccess(cx))
        return false;

    RootedValue tmp(cx, value);
    return JSObject::setGeneric(cx, obj, obj, id, &tmp, strict);
}

static MOZ_NEVER_INLINE bool
Interpret(JSContext *cx, RunState &state)
{
/*
 * Define macros for an interpreter loop. Opcode dispatch may be either by a
 * switch statement or by indirect goto (aka a threaded interpreter), depending
 * on compiler support.
 *
 * Threaded interpretation appears to be well-supported by GCC 3 and higher.
 * IBM's C compiler when run with the right options (e.g., -qlanglvl=extended)
 * also supports threading. Ditto the SunPro C compiler.
 */
#if (__GNUC__ >= 3 ||                                                         \
     (__IBMC__ >= 700 && defined __IBM_COMPUTED_GOTO) ||                      \
     __SUNPRO_C >= 0x570)
// Non-standard but faster indirect-goto-based dispatch.
# define INTERPRETER_LOOP()
# define CASE(OP)                 label_##OP:
# define DEFAULT()                label_default:
# define DISPATCH_TO(OP)          goto *addresses[(OP)]

# define LABEL(X)                 (&&label_##X)

    // Use addresses instead of offsets to optimize for runtime speed over
    // load-time relocation overhead.
    static const void *const addresses[EnableInterruptsPseudoOpcode + 1] = {
# define OPCODE_LABEL(op, ...)  LABEL(op),
        FOR_EACH_OPCODE(OPCODE_LABEL)
# undef OPCODE_LABEL
# define TRAILING_LABEL(v)                                                    \
    ((v) == EnableInterruptsPseudoOpcode                                      \
     ? LABEL(EnableInterruptsPseudoOpcode)                                    \
     : LABEL(default)),
        FOR_EACH_TRAILING_UNUSED_OPCODE(TRAILING_LABEL)
# undef TRAILING_LABEL
    };
#else
// Portable switch-based dispatch.
# define INTERPRETER_LOOP()       the_switch: switch (switchOp)
# define CASE(OP)                 case OP:
# define DEFAULT()                default:
# define DISPATCH_TO(OP)                                                      \
    JS_BEGIN_MACRO                                                            \
        switchOp = (OP);                                                      \
        goto the_switch;                                                      \
    JS_END_MACRO

    // This variable is effectively a parameter to the_switch.
    jsbytecode switchOp;
#endif

    /*
     * Increment REGS.pc by N, load the opcode at that position,
     * and jump to the code to execute it.
     *
     * When Debugger puts a script in single-step mode, all js::Interpret
     * invocations that might be presently running that script must have
     * interrupts enabled. It's not practical to simply check
     * script->stepModeEnabled() at each point some callee could have changed
     * it, because there are so many places js::Interpret could possibly cause
     * JavaScript to run: each place an object might be coerced to a primitive
     * or a number, for example. So instead, we expose a simple mechanism to
     * let Debugger tweak the affected js::Interpret frames when an onStep
     * handler is added: calling activation.enableInterruptsUnconditionally()
     * will enable interrupts, and activation.opMask() is or'd with the opcode
     * to implement a simple alternate dispatch.
     */
#define ADVANCE_AND_DISPATCH(N)                                               \
    JS_BEGIN_MACRO                                                            \
        REGS.pc += (N);                                                       \
        SANITY_CHECKS();                                                      \
        DISPATCH_TO(*REGS.pc | activation.opMask());                          \
    JS_END_MACRO

   /*
    * Shorthand for the common sequence at the end of a fixed-size opcode.
    */
#define END_CASE(OP)              ADVANCE_AND_DISPATCH(OP##_LENGTH);

    /*
     * Prepare to call a user-supplied branch handler, and abort the script
     * if it returns false.
     */
#define CHECK_BRANCH()                                                        \
    JS_BEGIN_MACRO                                                            \
        if (!CheckForInterrupt(cx))                                           \
            goto error;                                                       \
    JS_END_MACRO

    /*
     * This is a simple wrapper around ADVANCE_AND_DISPATCH which also does
     * a CHECK_BRANCH() if n is not positive, which possibly indicates that it
     * is the backedge of a loop.
     */
#define BRANCH(n)                                                             \
    JS_BEGIN_MACRO                                                            \
        int32_t nlen = (n);                                                   \
        if (nlen <= 0)                                                        \
            CHECK_BRANCH();                                                   \
        ADVANCE_AND_DISPATCH(nlen);                                           \
    JS_END_MACRO

#define LOAD_DOUBLE(PCOFF, dbl)                                               \
    ((dbl) = script->getConst(GET_UINT32_INDEX(REGS.pc + (PCOFF))).toDouble())

#define SET_SCRIPT(s)                                                         \
    JS_BEGIN_MACRO                                                            \
        script = (s);                                                         \
        if (script->hasAnyBreakpointsOrStepMode() || script->hasScriptCounts()) \
            activation.enableInterruptsUnconditionally();                     \
    JS_END_MACRO

#define SANITY_CHECKS()                                                       \
    JS_BEGIN_MACRO                                                            \
        js::gc::MaybeVerifyBarriers(cx);                                      \
        JS_ASSERT_IF(script->hasScriptCounts(),                               \
                     activation.opMask() == EnableInterruptsPseudoOpcode);    \
    JS_END_MACRO

    gc::MaybeVerifyBarriers(cx, true);
    JS_ASSERT(!cx->compartment()->activeAnalysis);

    InterpreterFrame *entryFrame = state.pushInterpreterFrame(cx);
    if (!entryFrame)
        return false;

    InterpreterActivation activation(state, cx, entryFrame);

    /* The script is used frequently, so keep a local copy. */
    RootedScript script(cx);
    SET_SCRIPT(REGS.fp()->script());

    TraceLogger *logger = TraceLoggerForMainThread(cx->runtime());
    uint32_t scriptLogId = TraceLogCreateTextId(logger, script);
    TraceLogStartEvent(logger, scriptLogId);
    TraceLogStartEvent(logger, TraceLogger::Interpreter);

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

    if (MOZ_UNLIKELY(REGS.fp()->isGeneratorFrame())) {
        JS_ASSERT(script->containsPC(REGS.pc));
        JS_ASSERT(REGS.stackDepth() <= script->nslots());

        /*
         * To support generator_throw and to catch ignored exceptions,
         * fail if cx->isExceptionPending() is true.
         */
        if (cx->isExceptionPending()) {
            probes::EnterScript(cx, script, script->functionNonDelazifying(), REGS.fp());
            goto error;
        }
    }

    /* State communicated between non-local jumps: */
    bool interpReturnOK;

    if (!activation.entryFrame()->isGeneratorFrame()) {
        if (!activation.entryFrame()->prologue(cx))
            goto error;
    } else {
        if (!probes::EnterScript(cx, script, script->functionNonDelazifying(),
                                 activation.entryFrame()))
        {
            goto error;
        }
    }
    if (MOZ_UNLIKELY(cx->compartment()->debugMode())) {
        JSTrapStatus status = ScriptDebugPrologue(cx, activation.entryFrame(), REGS.pc);
        switch (status) {
          case JSTRAP_CONTINUE:
            break;
          case JSTRAP_RETURN:
            ForcedReturn(cx, REGS);
            goto successful_return_continuation;
          case JSTRAP_THROW:
          case JSTRAP_ERROR:
            goto error;
          default:
            MOZ_CRASH("bad ScriptDebugPrologue status");
        }
    }

    if (cx->runtime()->profilingScripts)
        activation.enableInterruptsUnconditionally();

    // Enter the interpreter loop starting at the current pc.
    ADVANCE_AND_DISPATCH(0);

INTERPRETER_LOOP() {

CASE(EnableInterruptsPseudoOpcode)
{
    bool moreInterrupts = false;
    jsbytecode op = *REGS.pc;

    if (cx->runtime()->profilingScripts) {
        if (!script->hasScriptCounts())
            script->initScriptCounts(cx);
        moreInterrupts = true;
    }

    if (script->hasScriptCounts()) {
        PCCounts counts = script->getPCCounts(REGS.pc);
        counts.get(PCCounts::BASE_INTERP)++;
        moreInterrupts = true;
    }

    if (cx->compartment()->debugMode()) {
        if (script->stepModeEnabled()) {
            RootedValue rval(cx);
            JSTrapStatus status = JSTRAP_CONTINUE;
            status = Debugger::onSingleStep(cx, &rval);
            switch (status) {
              case JSTRAP_ERROR:
                goto error;
              case JSTRAP_CONTINUE:
                break;
              case JSTRAP_RETURN:
                REGS.fp()->setReturnValue(rval);
                ForcedReturn(cx, REGS);
                goto successful_return_continuation;
              case JSTRAP_THROW:
                cx->setPendingException(rval);
                goto error;
              default:;
            }
            moreInterrupts = true;
        }

        if (script->hasAnyBreakpointsOrStepMode())
            moreInterrupts = true;

        if (script->hasBreakpointsAt(REGS.pc)) {
            RootedValue rval(cx);
            JSTrapStatus status = Debugger::onTrap(cx, &rval);
            switch (status) {
              case JSTRAP_ERROR:
                goto error;
              case JSTRAP_RETURN:
                REGS.fp()->setReturnValue(rval);
                ForcedReturn(cx, REGS);
                goto successful_return_continuation;
              case JSTRAP_THROW:
                cx->setPendingException(rval);
                goto error;
              default:
                break;
            }
            JS_ASSERT(status == JSTRAP_CONTINUE);
            JS_ASSERT(rval.isInt32() && rval.toInt32() == op);
        }
    }

    JS_ASSERT(activation.opMask() == EnableInterruptsPseudoOpcode);
    if (!moreInterrupts)
        activation.clearInterruptsMask();

    /* Commence executing the actual opcode. */
    SANITY_CHECKS();
    DISPATCH_TO(op);
}

/* Various 1-byte no-ops. */
CASE(JSOP_NOP)
CASE(JSOP_UNUSED2)
CASE(JSOP_UNUSED45)
CASE(JSOP_UNUSED46)
CASE(JSOP_UNUSED47)
CASE(JSOP_UNUSED48)
CASE(JSOP_UNUSED49)
CASE(JSOP_UNUSED50)
CASE(JSOP_UNUSED51)
CASE(JSOP_UNUSED52)
CASE(JSOP_UNUSED57)
CASE(JSOP_UNUSED83)
CASE(JSOP_UNUSED103)
CASE(JSOP_UNUSED104)
CASE(JSOP_UNUSED105)
CASE(JSOP_UNUSED107)
CASE(JSOP_UNUSED124)
CASE(JSOP_UNUSED125)
CASE(JSOP_UNUSED126)
CASE(JSOP_UNUSED138)
CASE(JSOP_UNUSED139)
CASE(JSOP_UNUSED140)
CASE(JSOP_UNUSED141)
CASE(JSOP_UNUSED142)
CASE(JSOP_UNUSED146)
CASE(JSOP_UNUSED147)
CASE(JSOP_UNUSED148)
CASE(JSOP_BACKPATCH)
CASE(JSOP_UNUSED150)
CASE(JSOP_UNUSED156)
CASE(JSOP_UNUSED157)
CASE(JSOP_UNUSED158)
CASE(JSOP_UNUSED159)
CASE(JSOP_UNUSED161)
CASE(JSOP_UNUSED162)
CASE(JSOP_UNUSED163)
CASE(JSOP_UNUSED164)
CASE(JSOP_UNUSED165)
CASE(JSOP_UNUSED166)
CASE(JSOP_UNUSED167)
CASE(JSOP_UNUSED168)
CASE(JSOP_UNUSED169)
CASE(JSOP_UNUSED170)
CASE(JSOP_UNUSED171)
CASE(JSOP_UNUSED172)
CASE(JSOP_UNUSED173)
CASE(JSOP_UNUSED174)
CASE(JSOP_UNUSED175)
CASE(JSOP_UNUSED176)
CASE(JSOP_UNUSED177)
CASE(JSOP_UNUSED178)
CASE(JSOP_UNUSED179)
CASE(JSOP_UNUSED180)
CASE(JSOP_UNUSED181)
CASE(JSOP_UNUSED182)
CASE(JSOP_UNUSED183)
CASE(JSOP_UNUSED185)
CASE(JSOP_UNUSED186)
CASE(JSOP_UNUSED187)
CASE(JSOP_UNUSED189)
CASE(JSOP_UNUSED190)
CASE(JSOP_UNUSED191)
CASE(JSOP_UNUSED192)
CASE(JSOP_UNUSED196)
CASE(JSOP_UNUSED201)
CASE(JSOP_UNUSED205)
CASE(JSOP_UNUSED206)
CASE(JSOP_UNUSED207)
CASE(JSOP_UNUSED208)
CASE(JSOP_UNUSED209)
CASE(JSOP_UNUSED210)
CASE(JSOP_UNUSED211)
CASE(JSOP_UNUSED212)
CASE(JSOP_UNUSED213)
CASE(JSOP_UNUSED219)
CASE(JSOP_UNUSED220)
CASE(JSOP_UNUSED221)
CASE(JSOP_UNUSED222)
CASE(JSOP_UNUSED223)
CASE(JSOP_CONDSWITCH)
CASE(JSOP_TRY)
{
    JS_ASSERT(js_CodeSpec[*REGS.pc].length == 1);
    ADVANCE_AND_DISPATCH(1);
}

CASE(JSOP_LOOPHEAD)
END_CASE(JSOP_LOOPHEAD)

CASE(JSOP_LABEL)
END_CASE(JSOP_LABEL)

CASE(JSOP_LOOPENTRY)
    // Attempt on-stack replacement with Baseline code.
    if (jit::IsBaselineEnabled(cx)) {
        jit::MethodStatus status = jit::CanEnterBaselineAtBranch(cx, REGS.fp(), false);
        if (status == jit::Method_Error)
            goto error;
        if (status == jit::Method_Compiled) {
            bool wasSPS = REGS.fp()->hasPushedSPSFrame();
            jit::IonExecStatus maybeOsr = jit::EnterBaselineAtBranch(cx, REGS.fp(), REGS.pc);

            // We failed to call into baseline at all, so treat as an error.
            if (maybeOsr == jit::IonExec_Aborted)
                goto error;

            interpReturnOK = (maybeOsr == jit::IonExec_Ok);

            // Pop the SPS frame pushed by the interpreter.  (The compiled version of the
            // function popped a copy of the frame pushed by the OSR trampoline.)
            if (wasSPS)
                cx->runtime()->spsProfiler.exit(script, script->functionNonDelazifying());

            if (activation.entryFrame() != REGS.fp())
                goto jit_return_pop_frame;
            goto leave_on_safe_point;
        }
    }
END_CASE(JSOP_LOOPENTRY)

CASE(JSOP_LINENO)
END_CASE(JSOP_LINENO)

CASE(JSOP_UNDEFINED)
    PUSH_UNDEFINED();
END_CASE(JSOP_UNDEFINED)

CASE(JSOP_POP)
    REGS.sp--;
END_CASE(JSOP_POP)

CASE(JSOP_POPN)
    JS_ASSERT(GET_UINT16(REGS.pc) <= REGS.stackDepth());
    REGS.sp -= GET_UINT16(REGS.pc);
END_CASE(JSOP_POPN)

CASE(JSOP_DUPAT)
{
    JS_ASSERT(GET_UINT24(REGS.pc) < REGS.stackDepth());
    unsigned i = GET_UINT24(REGS.pc);
    const Value &rref = REGS.sp[-int(i + 1)];
    PUSH_COPY(rref);
}
END_CASE(JSOP_DUPAT)

CASE(JSOP_SETRVAL)
    POP_RETURN_VALUE();
END_CASE(JSOP_SETRVAL)

CASE(JSOP_ENTERWITH)
{
    RootedValue &val = rootValue0;
    RootedObject &staticWith = rootObject0;
    val = REGS.sp[-1];
    REGS.sp--;
    staticWith = script->getObject(REGS.pc);

    if (!EnterWithOperation(cx, REGS.fp(), val, staticWith))
        goto error;
}
END_CASE(JSOP_ENTERWITH)

CASE(JSOP_LEAVEWITH)
    REGS.fp()->popWith(cx);
END_CASE(JSOP_LEAVEWITH)

CASE(JSOP_RETURN)
    POP_RETURN_VALUE();
    /* FALL THROUGH */

CASE(JSOP_RETRVAL)
{
    /*
     * When the inlined frame exits with an exception or an error, ok will be
     * false after the inline_return label.
     */
    CHECK_BRANCH();

  successful_return_continuation:
    interpReturnOK = true;
  return_continuation:
    if (activation.entryFrame() != REGS.fp())
  inline_return:
    {
        // Stop the engine. (No details about which engine exactly, could be
        // interpreter, Baseline or IonMonkey.)
        TraceLogStopEvent(logger);
        // Stop the script. (Again no details about which script exactly.)
        TraceLogStopEvent(logger);

        if (MOZ_UNLIKELY(cx->compartment()->debugMode()))
            interpReturnOK = ScriptDebugEpilogue(cx, REGS.fp(), REGS.pc, interpReturnOK);

        if (!REGS.fp()->isYielding())
            REGS.fp()->epilogue(cx);
        else
            probes::ExitScript(cx, script, script->functionNonDelazifying(),
                               REGS.fp()->hasPushedSPSFrame());

  jit_return_pop_frame:

        activation.popInlineFrame(REGS.fp());
        SET_SCRIPT(REGS.fp()->script());

  jit_return:

        JS_ASSERT(js_CodeSpec[*REGS.pc].format & JOF_INVOKE);

        /* Resume execution in the calling frame. */
        if (MOZ_LIKELY(interpReturnOK)) {
            TypeScript::Monitor(cx, script, REGS.pc, REGS.sp[-1]);

            ADVANCE_AND_DISPATCH(JSOP_CALL_LENGTH);
        }

        /* Increment pc so that |sp - fp->slots == ReconstructStackDepth(pc)|. */
        REGS.pc += JSOP_CALL_LENGTH;
        goto error;
    } else {
        JS_ASSERT(REGS.stackDepth() == 0);
    }
    goto exit;
}

CASE(JSOP_DEFAULT)
    REGS.sp--;
    /* FALL THROUGH */
CASE(JSOP_GOTO)
{
    BRANCH(GET_JUMP_OFFSET(REGS.pc));
}

CASE(JSOP_IFEQ)
{
    bool cond = ToBoolean(REGS.stackHandleAt(-1));
    REGS.sp--;
    if (!cond)
        BRANCH(GET_JUMP_OFFSET(REGS.pc));
}
END_CASE(JSOP_IFEQ)

CASE(JSOP_IFNE)
{
    bool cond = ToBoolean(REGS.stackHandleAt(-1));
    REGS.sp--;
    if (cond)
        BRANCH(GET_JUMP_OFFSET(REGS.pc));
}
END_CASE(JSOP_IFNE)

CASE(JSOP_OR)
{
    bool cond = ToBoolean(REGS.stackHandleAt(-1));
    if (cond)
        ADVANCE_AND_DISPATCH(GET_JUMP_OFFSET(REGS.pc));
}
END_CASE(JSOP_OR)

CASE(JSOP_AND)
{
    bool cond = ToBoolean(REGS.stackHandleAt(-1));
    if (!cond)
        ADVANCE_AND_DISPATCH(GET_JUMP_OFFSET(REGS.pc));
}
END_CASE(JSOP_AND)

#define FETCH_ELEMENT_ID(n, id)                                               \
    JS_BEGIN_MACRO                                                            \
        if (!ValueToId<CanGC>(cx, REGS.stackHandleAt(n), &(id))) \
            goto error;                                                       \
    JS_END_MACRO

#define TRY_BRANCH_AFTER_COND(cond,spdec)                                     \
    JS_BEGIN_MACRO                                                            \
        JS_ASSERT(js_CodeSpec[*REGS.pc].length == 1);                         \
        unsigned diff_ = (unsigned) GET_UINT8(REGS.pc) - (unsigned) JSOP_IFEQ; \
        if (diff_ <= 1) {                                                     \
            REGS.sp -= (spdec);                                               \
            if ((cond) == (diff_ != 0)) {                                     \
                ++REGS.pc;                                                    \
                BRANCH(GET_JUMP_OFFSET(REGS.pc));                             \
            }                                                                 \
            ADVANCE_AND_DISPATCH(1 + JSOP_IFEQ_LENGTH);                       \
        }                                                                     \
    JS_END_MACRO

CASE(JSOP_IN)
{
    HandleValue rref = REGS.stackHandleAt(-1);
    if (!rref.isObject()) {
        js_ReportValueError(cx, JSMSG_IN_NOT_OBJECT, -1, rref, js::NullPtr());
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
    bool cond = prop != nullptr;
    prop = nullptr;
    TRY_BRANCH_AFTER_COND(cond, 2);
    REGS.sp--;
    REGS.sp[-1].setBoolean(cond);
}
END_CASE(JSOP_IN)

CASE(JSOP_ITER)
{
    JS_ASSERT(REGS.stackDepth() >= 1);
    uint8_t flags = GET_UINT8(REGS.pc);
    MutableHandleValue res = REGS.stackHandleAt(-1);
    if (!ValueToIterator(cx, flags, res))
        goto error;
    JS_ASSERT(res.isObject());
}
END_CASE(JSOP_ITER)

CASE(JSOP_MOREITER)
{
    JS_ASSERT(REGS.stackDepth() >= 1);
    JS_ASSERT(REGS.sp[-1].isObject());
    PUSH_NULL();
    bool cond;
    MutableHandleValue res = REGS.stackHandleAt(-1);
    if (!IteratorMore(cx, &REGS.sp[-2].toObject(), &cond, res))
        goto error;
    REGS.sp[-1].setBoolean(cond);
}
END_CASE(JSOP_MOREITER)

CASE(JSOP_ITERNEXT)
{
    JS_ASSERT(REGS.sp[-1].isObject());
    PUSH_NULL();
    MutableHandleValue res = REGS.stackHandleAt(-1);
    RootedObject &obj = rootObject0;
    obj = &REGS.sp[-2].toObject();
    if (!IteratorNext(cx, obj, res))
        goto error;
}
END_CASE(JSOP_ITERNEXT)

CASE(JSOP_ENDITER)
{
    JS_ASSERT(REGS.stackDepth() >= 1);
    RootedObject &obj = rootObject0;
    obj = &REGS.sp[-1].toObject();
    bool ok = CloseIterator(cx, obj);
    REGS.sp--;
    if (!ok)
        goto error;
}
END_CASE(JSOP_ENDITER)

CASE(JSOP_DUP)
{
    JS_ASSERT(REGS.stackDepth() >= 1);
    const Value &rref = REGS.sp[-1];
    PUSH_COPY(rref);
}
END_CASE(JSOP_DUP)

CASE(JSOP_DUP2)
{
    JS_ASSERT(REGS.stackDepth() >= 2);
    const Value &lref = REGS.sp[-2];
    const Value &rref = REGS.sp[-1];
    PUSH_COPY(lref);
    PUSH_COPY(rref);
}
END_CASE(JSOP_DUP2)

CASE(JSOP_SWAP)
{
    JS_ASSERT(REGS.stackDepth() >= 2);
    Value &lref = REGS.sp[-2];
    Value &rref = REGS.sp[-1];
    lref.swap(rref);
}
END_CASE(JSOP_SWAP)

CASE(JSOP_PICK)
{
    unsigned i = GET_UINT8(REGS.pc);
    JS_ASSERT(REGS.stackDepth() >= i + 1);
    Value lval = REGS.sp[-int(i + 1)];
    memmove(REGS.sp - (i + 1), REGS.sp - i, sizeof(Value) * i);
    REGS.sp[-1] = lval;
}
END_CASE(JSOP_PICK)

CASE(JSOP_SETCONST)
{
    RootedPropertyName &name = rootName0;
    name = script->getName(REGS.pc);

    RootedValue &rval = rootValue0;
    rval = REGS.sp[-1];

    RootedObject &obj = rootObject0;
    obj = &REGS.fp()->varObj();

    if (!SetConstOperation(cx, obj, name, rval))
        goto error;
}
END_CASE(JSOP_SETCONST)

CASE(JSOP_BINDGNAME)
    PUSH_OBJECT(REGS.fp()->global());
END_CASE(JSOP_BINDGNAME)

CASE(JSOP_BINDINTRINSIC)
    PUSH_OBJECT(*cx->global()->intrinsicsHolder());
END_CASE(JSOP_BINDINTRINSIC)

CASE(JSOP_BINDNAME)
{
    RootedObject &scopeChain = rootObject0;
    scopeChain = REGS.fp()->scopeChain();

    RootedPropertyName &name = rootName0;
    name = script->getName(REGS.pc);

    /* Assigning to an undeclared name adds a property to the global object. */
    RootedObject &scope = rootObject1;
    if (!LookupNameUnqualified(cx, name, scopeChain, &scope))
        goto error;

    PUSH_OBJECT(*scope);
}
END_CASE(JSOP_BINDNAME)

#define BITWISE_OP(OP)                                                        \
    JS_BEGIN_MACRO                                                            \
        int32_t i, j;                                                         \
        if (!ToInt32(cx, REGS.stackHandleAt(-2), &i))                         \
            goto error;                                                       \
        if (!ToInt32(cx, REGS.stackHandleAt(-1), &j))                         \
            goto error;                                                       \
        i = i OP j;                                                           \
        REGS.sp--;                                                            \
        REGS.sp[-1].setInt32(i);                                              \
    JS_END_MACRO

CASE(JSOP_BITOR)
    BITWISE_OP(|);
END_CASE(JSOP_BITOR)

CASE(JSOP_BITXOR)
    BITWISE_OP(^);
END_CASE(JSOP_BITXOR)

CASE(JSOP_BITAND)
    BITWISE_OP(&);
END_CASE(JSOP_BITAND)

#undef BITWISE_OP

CASE(JSOP_EQ)
    if (!LooseEqualityOp<true>(cx, REGS))
        goto error;
END_CASE(JSOP_EQ)

CASE(JSOP_NE)
    if (!LooseEqualityOp<false>(cx, REGS))
        goto error;
END_CASE(JSOP_NE)

#define STRICT_EQUALITY_OP(OP, COND)                                          \
    JS_BEGIN_MACRO                                                            \
        const Value &rref = REGS.sp[-1];                                      \
        const Value &lref = REGS.sp[-2];                                      \
        bool equal;                                                           \
        if (!StrictlyEqual(cx, lref, rref, &equal))                           \
            goto error;                                                       \
        (COND) = equal OP true;                                               \
        REGS.sp--;                                                            \
    JS_END_MACRO

CASE(JSOP_STRICTEQ)
{
    bool cond;
    STRICT_EQUALITY_OP(==, cond);
    REGS.sp[-1].setBoolean(cond);
}
END_CASE(JSOP_STRICTEQ)

CASE(JSOP_STRICTNE)
{
    bool cond;
    STRICT_EQUALITY_OP(!=, cond);
    REGS.sp[-1].setBoolean(cond);
}
END_CASE(JSOP_STRICTNE)

CASE(JSOP_CASE)
{
    bool cond;
    STRICT_EQUALITY_OP(==, cond);
    if (cond) {
        REGS.sp--;
        BRANCH(GET_JUMP_OFFSET(REGS.pc));
    }
}
END_CASE(JSOP_CASE)

#undef STRICT_EQUALITY_OP

CASE(JSOP_LT)
{
    bool cond;
    MutableHandleValue lval = REGS.stackHandleAt(-2);
    MutableHandleValue rval = REGS.stackHandleAt(-1);
    if (!LessThanOperation(cx, lval, rval, &cond))
        goto error;
    TRY_BRANCH_AFTER_COND(cond, 2);
    REGS.sp[-2].setBoolean(cond);
    REGS.sp--;
}
END_CASE(JSOP_LT)

CASE(JSOP_LE)
{
    bool cond;
    MutableHandleValue lval = REGS.stackHandleAt(-2);
    MutableHandleValue rval = REGS.stackHandleAt(-1);
    if (!LessThanOrEqualOperation(cx, lval, rval, &cond))
        goto error;
    TRY_BRANCH_AFTER_COND(cond, 2);
    REGS.sp[-2].setBoolean(cond);
    REGS.sp--;
}
END_CASE(JSOP_LE)

CASE(JSOP_GT)
{
    bool cond;
    MutableHandleValue lval = REGS.stackHandleAt(-2);
    MutableHandleValue rval = REGS.stackHandleAt(-1);
    if (!GreaterThanOperation(cx, lval, rval, &cond))
        goto error;
    TRY_BRANCH_AFTER_COND(cond, 2);
    REGS.sp[-2].setBoolean(cond);
    REGS.sp--;
}
END_CASE(JSOP_GT)

CASE(JSOP_GE)
{
    bool cond;
    MutableHandleValue lval = REGS.stackHandleAt(-2);
    MutableHandleValue rval = REGS.stackHandleAt(-1);
    if (!GreaterThanOrEqualOperation(cx, lval, rval, &cond))
        goto error;
    TRY_BRANCH_AFTER_COND(cond, 2);
    REGS.sp[-2].setBoolean(cond);
    REGS.sp--;
}
END_CASE(JSOP_GE)

#define SIGNED_SHIFT_OP(OP)                                                   \
    JS_BEGIN_MACRO                                                            \
        int32_t i, j;                                                         \
        if (!ToInt32(cx, REGS.stackHandleAt(-2), &i))                         \
            goto error;                                                       \
        if (!ToInt32(cx, REGS.stackHandleAt(-1), &j))                         \
            goto error;                                                       \
        i = i OP (j & 31);                                                    \
        REGS.sp--;                                                            \
        REGS.sp[-1].setInt32(i);                                              \
    JS_END_MACRO

CASE(JSOP_LSH)
    SIGNED_SHIFT_OP(<<);
END_CASE(JSOP_LSH)

CASE(JSOP_RSH)
    SIGNED_SHIFT_OP(>>);
END_CASE(JSOP_RSH)

#undef SIGNED_SHIFT_OP

CASE(JSOP_URSH)
{
    HandleValue lval = REGS.stackHandleAt(-2);
    HandleValue rval = REGS.stackHandleAt(-1);
    MutableHandleValue res = REGS.stackHandleAt(-2);
    if (!UrshOperation(cx, lval, rval, res))
        goto error;
    REGS.sp--;
}
END_CASE(JSOP_URSH)

CASE(JSOP_ADD)
{
    MutableHandleValue lval = REGS.stackHandleAt(-2);
    MutableHandleValue rval = REGS.stackHandleAt(-1);
    MutableHandleValue res = REGS.stackHandleAt(-2);
    if (!AddOperation(cx, lval, rval, res))
        goto error;
    REGS.sp--;
}
END_CASE(JSOP_ADD)

CASE(JSOP_SUB)
{
    RootedValue &lval = rootValue0, &rval = rootValue1;
    lval = REGS.sp[-2];
    rval = REGS.sp[-1];
    MutableHandleValue res = REGS.stackHandleAt(-2);
    if (!SubOperation(cx, lval, rval, res))
        goto error;
    REGS.sp--;
}
END_CASE(JSOP_SUB)

CASE(JSOP_MUL)
{
    RootedValue &lval = rootValue0, &rval = rootValue1;
    lval = REGS.sp[-2];
    rval = REGS.sp[-1];
    MutableHandleValue res = REGS.stackHandleAt(-2);
    if (!MulOperation(cx, lval, rval, res))
        goto error;
    REGS.sp--;
}
END_CASE(JSOP_MUL)

CASE(JSOP_DIV)
{
    RootedValue &lval = rootValue0, &rval = rootValue1;
    lval = REGS.sp[-2];
    rval = REGS.sp[-1];
    MutableHandleValue res = REGS.stackHandleAt(-2);
    if (!DivOperation(cx, lval, rval, res))
        goto error;
    REGS.sp--;
}
END_CASE(JSOP_DIV)

CASE(JSOP_MOD)
{
    RootedValue &lval = rootValue0, &rval = rootValue1;
    lval = REGS.sp[-2];
    rval = REGS.sp[-1];
    MutableHandleValue res = REGS.stackHandleAt(-2);
    if (!ModOperation(cx, lval, rval, res))
        goto error;
    REGS.sp--;
}
END_CASE(JSOP_MOD)

CASE(JSOP_NOT)
{
    bool cond = ToBoolean(REGS.stackHandleAt(-1));
    REGS.sp--;
    PUSH_BOOLEAN(!cond);
}
END_CASE(JSOP_NOT)

CASE(JSOP_BITNOT)
{
    int32_t i;
    HandleValue value = REGS.stackHandleAt(-1);
    if (!BitNot(cx, value, &i))
        goto error;
    REGS.sp[-1].setInt32(i);
}
END_CASE(JSOP_BITNOT)

CASE(JSOP_NEG)
{
    RootedValue &val = rootValue0;
    val = REGS.sp[-1];
    MutableHandleValue res = REGS.stackHandleAt(-1);
    if (!NegOperation(cx, script, REGS.pc, val, res))
        goto error;
}
END_CASE(JSOP_NEG)

CASE(JSOP_POS)
    if (!ToNumber(cx, REGS.stackHandleAt(-1)))
        goto error;
END_CASE(JSOP_POS)

CASE(JSOP_DELNAME)
{
    /* Strict mode code should never contain JSOP_DELNAME opcodes. */
    JS_ASSERT(!script->strict());

    RootedPropertyName &name = rootName0;
    name = script->getName(REGS.pc);

    RootedObject &scopeObj = rootObject0;
    scopeObj = REGS.fp()->scopeChain();

    PUSH_BOOLEAN(true);
    MutableHandleValue res = REGS.stackHandleAt(-1);
    if (!DeleteNameOperation(cx, name, scopeObj, res))
        goto error;
}
END_CASE(JSOP_DELNAME)

CASE(JSOP_DELPROP)
{
    RootedId &id = rootId0;
    id = NameToId(script->getName(REGS.pc));

    RootedObject &obj = rootObject0;
    FETCH_OBJECT(cx, -1, obj);

    bool succeeded;
    if (!JSObject::deleteGeneric(cx, obj, id, &succeeded))
        goto error;
    if (!succeeded && script->strict()) {
        obj->reportNotConfigurable(cx, id);
        goto error;
    }
    MutableHandleValue res = REGS.stackHandleAt(-1);
    res.setBoolean(succeeded);
}
END_CASE(JSOP_DELPROP)

CASE(JSOP_DELELEM)
{
    /* Fetch the left part and resolve it to a non-null object. */
    RootedObject &obj = rootObject0;
    FETCH_OBJECT(cx, -2, obj);

    RootedValue &propval = rootValue0;
    propval = REGS.sp[-1];

    bool succeeded;
    RootedId &id = rootId0;
    if (!ValueToId<CanGC>(cx, propval, &id))
        goto error;
    if (!JSObject::deleteGeneric(cx, obj, id, &succeeded))
        goto error;
    if (!succeeded && script->strict()) {
        obj->reportNotConfigurable(cx, id);
        goto error;
    }

    MutableHandleValue res = REGS.stackHandleAt(-2);
    res.setBoolean(succeeded);
    REGS.sp--;
}
END_CASE(JSOP_DELELEM)

CASE(JSOP_TOID)
{
    /*
     * Increment or decrement requires use to lookup the same property twice,
     * but we need to avoid the observable stringification the second time.
     * There must be an object value below the id, which will not be popped.
     */
    RootedValue &objval = rootValue0, &idval = rootValue1;
    objval = REGS.sp[-2];
    idval = REGS.sp[-1];

    MutableHandleValue res = REGS.stackHandleAt(-1);
    if (!ToIdOperation(cx, script, REGS.pc, objval, idval, res))
        goto error;
}
END_CASE(JSOP_TOID)

CASE(JSOP_TYPEOFEXPR)
CASE(JSOP_TYPEOF)
{
    REGS.sp[-1].setString(TypeOfOperation(REGS.sp[-1], cx->runtime()));
}
END_CASE(JSOP_TYPEOF)

CASE(JSOP_VOID)
    REGS.sp[-1].setUndefined();
END_CASE(JSOP_VOID)

CASE(JSOP_THIS)
    if (!ComputeThis(cx, REGS.fp()))
        goto error;
    PUSH_COPY(REGS.fp()->thisValue());
END_CASE(JSOP_THIS)

CASE(JSOP_GETPROP)
CASE(JSOP_GETXPROP)
CASE(JSOP_LENGTH)
CASE(JSOP_CALLPROP)
{

    MutableHandleValue lval = REGS.stackHandleAt(-1);
    if (!GetPropertyOperation(cx, REGS.fp(), script, REGS.pc, lval, lval))
        goto error;

    TypeScript::Monitor(cx, script, REGS.pc, lval);
    assertSameCompartmentDebugOnly(cx, lval);
}
END_CASE(JSOP_GETPROP)

CASE(JSOP_SETINTRINSIC)
{
    HandleValue value = REGS.stackHandleAt(-1);

    if (!SetIntrinsicOperation(cx, script, REGS.pc, value))
        goto error;

    REGS.sp[-2] = REGS.sp[-1];
    REGS.sp--;
}
END_CASE(JSOP_SETINTRINSIC)

CASE(JSOP_SETGNAME)
CASE(JSOP_SETNAME)
{
    RootedObject &scope = rootObject0;
    scope = &REGS.sp[-2].toObject();

    HandleValue value = REGS.stackHandleAt(-1);

    if (!SetNameOperation(cx, script, REGS.pc, scope, value))
        goto error;

    REGS.sp[-2] = REGS.sp[-1];
    REGS.sp--;
}
END_CASE(JSOP_SETNAME)

CASE(JSOP_SETPROP)
{
    HandleValue lval = REGS.stackHandleAt(-2);
    HandleValue rval = REGS.stackHandleAt(-1);

    if (!SetPropertyOperation(cx, script, REGS.pc, lval, rval))
        goto error;

    REGS.sp[-2] = REGS.sp[-1];
    REGS.sp--;
}
END_CASE(JSOP_SETPROP)

CASE(JSOP_GETELEM)
CASE(JSOP_CALLELEM)
{
    MutableHandleValue lval = REGS.stackHandleAt(-2);
    HandleValue rval = REGS.stackHandleAt(-1);
    MutableHandleValue res = REGS.stackHandleAt(-2);

    bool done = false;
    if (!GetElemOptimizedArguments(cx, REGS.fp(), lval, rval, res, &done))
        goto error;

    if (!done) {
        if (!GetElementOperation(cx, JSOp(*REGS.pc), lval, rval, res))
            goto error;
    }

    TypeScript::Monitor(cx, script, REGS.pc, res);
    REGS.sp--;
}
END_CASE(JSOP_GETELEM)

CASE(JSOP_SETELEM)
{
    RootedObject &obj = rootObject0;
    FETCH_OBJECT(cx, -3, obj);
    RootedId &id = rootId0;
    FETCH_ELEMENT_ID(-2, id);
    Value &value = REGS.sp[-1];
    if (!SetObjectElementOperation(cx, obj, id, value, script->strict()))
        goto error;
    REGS.sp[-3] = value;
    REGS.sp -= 2;
}
END_CASE(JSOP_SETELEM)

CASE(JSOP_EVAL)
{
    CallArgs args = CallArgsFromSp(GET_ARGC(REGS.pc), REGS.sp);
    if (REGS.fp()->scopeChain()->global().valueIsEval(args.calleev())) {
        if (!DirectEval(cx, args))
            goto error;
    } else {
        if (!Invoke(cx, args))
            goto error;
    }
    REGS.sp = args.spAfterCall();
    TypeScript::Monitor(cx, script, REGS.pc, REGS.sp[-1]);
}
END_CASE(JSOP_EVAL)

CASE(JSOP_SPREADNEW)
CASE(JSOP_SPREADCALL)
    if (REGS.fp()->hasPushedSPSFrame())
        cx->runtime()->spsProfiler.updatePC(script, REGS.pc);
    /* FALL THROUGH */

CASE(JSOP_SPREADEVAL)
{
    JS_ASSERT(REGS.stackDepth() >= 3);

    HandleValue callee = REGS.stackHandleAt(-3);
    HandleValue thisv = REGS.stackHandleAt(-2);
    HandleValue arr = REGS.stackHandleAt(-1);
    MutableHandleValue ret = REGS.stackHandleAt(-3);
    if (!SpreadCallOperation(cx, script, REGS.pc, thisv, callee, arr, ret))
        goto error;

    REGS.sp -= 2;
}
END_CASE(JSOP_SPREADCALL)

CASE(JSOP_FUNAPPLY)
{
    CallArgs args = CallArgsFromSp(GET_ARGC(REGS.pc), REGS.sp);
    if (!GuardFunApplyArgumentsOptimization(cx, REGS.fp(), args.calleev(), args.array(),
                                            args.length()))
        goto error;
    /* FALL THROUGH */
}

CASE(JSOP_NEW)
CASE(JSOP_CALL)
CASE(JSOP_FUNCALL)
{
    if (REGS.fp()->hasPushedSPSFrame())
        cx->runtime()->spsProfiler.updatePC(script, REGS.pc);
    JS_ASSERT(REGS.stackDepth() >= 2 + GET_ARGC(REGS.pc));
    CallArgs args = CallArgsFromSp(GET_ARGC(REGS.pc), REGS.sp);

    bool construct = (*REGS.pc == JSOP_NEW);

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
        if (funScript->shouldCloneAtCallsite()) {
            fun = CloneFunctionAtCallsite(cx, fun, script, REGS.pc);
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
        TypeScript::Monitor(cx, script, REGS.pc, newsp[-1]);
        REGS.sp = newsp;
        ADVANCE_AND_DISPATCH(JSOP_CALL_LENGTH);
    }

    InitialFrameFlags initial = construct ? INITIAL_CONSTRUCT : INITIAL_NONE;
    bool newType = UseNewType(cx, script, REGS.pc);

    TypeMonitorCall(cx, args, construct);

    {
        InvokeState state(cx, args, initial);
        if (newType)
            state.setUseNewType();

        if (!newType && jit::IsIonEnabled(cx)) {
            jit::MethodStatus status = jit::CanEnter(cx, state);
            if (status == jit::Method_Error)
                goto error;
            if (status == jit::Method_Compiled) {
                jit::IonExecStatus exec = jit::IonCannon(cx, state);
                CHECK_BRANCH();
                REGS.sp = args.spAfterCall();
                interpReturnOK = !IsErrorStatus(exec);
                goto jit_return;
            }
        }

        if (jit::IsBaselineEnabled(cx)) {
            jit::MethodStatus status = jit::CanEnterBaselineMethod(cx, state);
            if (status == jit::Method_Error)
                goto error;
            if (status == jit::Method_Compiled) {
                jit::IonExecStatus exec = jit::EnterBaselineMethod(cx, state);
                CHECK_BRANCH();
                REGS.sp = args.spAfterCall();
                interpReturnOK = !IsErrorStatus(exec);
                goto jit_return;
            }
        }
    }

    funScript = fun->nonLazyScript();
    if (!activation.pushInlineFrame(args, funScript, initial))
        goto error;

    if (newType)
        REGS.fp()->setUseNewType();

    SET_SCRIPT(REGS.fp()->script());

    uint32_t scriptLogId = TraceLogCreateTextId(logger, script);
    TraceLogStartEvent(logger, scriptLogId);
    TraceLogStartEvent(logger, TraceLogger::Interpreter);

    if (!REGS.fp()->prologue(cx))
        goto error;
    if (MOZ_UNLIKELY(cx->compartment()->debugMode())) {
        switch (ScriptDebugPrologue(cx, REGS.fp(), REGS.pc)) {
          case JSTRAP_CONTINUE:
            break;
          case JSTRAP_RETURN:
            ForcedReturn(cx, REGS);
            goto successful_return_continuation;
          case JSTRAP_THROW:
          case JSTRAP_ERROR:
            goto error;
          default:
            MOZ_CRASH("bad ScriptDebugPrologue status");
        }
    }

    /* Load first op and dispatch it (safe since JSOP_RETRVAL). */
    ADVANCE_AND_DISPATCH(0);
}

CASE(JSOP_SETCALL)
{
    JS_ALWAYS_FALSE(SetCallOperation(cx));
    goto error;
}
END_CASE(JSOP_SETCALL)

CASE(JSOP_IMPLICITTHIS)
{
    RootedPropertyName &name = rootName0;
    name = script->getName(REGS.pc);

    RootedObject &scopeObj = rootObject0;
    scopeObj = REGS.fp()->scopeChain();

    RootedObject &scope = rootObject1;
    if (!LookupNameWithGlobalDefault(cx, name, scopeObj, &scope))
        goto error;

    RootedValue &v = rootValue0;
    if (!ComputeImplicitThis(cx, scope, &v))
        goto error;
    PUSH_COPY(v);
}
END_CASE(JSOP_IMPLICITTHIS)

CASE(JSOP_GETGNAME)
CASE(JSOP_NAME)
{
    RootedValue &rval = rootValue0;

    if (!NameOperation(cx, REGS.fp(), REGS.pc, &rval))
        goto error;

    PUSH_COPY(rval);
    TypeScript::Monitor(cx, script, REGS.pc, rval);
}
END_CASE(JSOP_NAME)

CASE(JSOP_GETINTRINSIC)
{
    RootedValue &rval = rootValue0;

    if (!GetIntrinsicOperation(cx, REGS.pc, &rval))
        goto error;

    PUSH_COPY(rval);
    TypeScript::Monitor(cx, script, REGS.pc, rval);
}
END_CASE(JSOP_GETINTRINSIC)

CASE(JSOP_UINT16)
    PUSH_INT32((int32_t) GET_UINT16(REGS.pc));
END_CASE(JSOP_UINT16)

CASE(JSOP_UINT24)
    PUSH_INT32((int32_t) GET_UINT24(REGS.pc));
END_CASE(JSOP_UINT24)

CASE(JSOP_INT8)
    PUSH_INT32(GET_INT8(REGS.pc));
END_CASE(JSOP_INT8)

CASE(JSOP_INT32)
    PUSH_INT32(GET_INT32(REGS.pc));
END_CASE(JSOP_INT32)

CASE(JSOP_DOUBLE)
{
    double dbl;
    LOAD_DOUBLE(0, dbl);
    PUSH_DOUBLE(dbl);
}
END_CASE(JSOP_DOUBLE)

CASE(JSOP_STRING)
    PUSH_STRING(script->getAtom(REGS.pc));
END_CASE(JSOP_STRING)

CASE(JSOP_TOSTRING)
{
    MutableHandleValue oper = REGS.stackHandleAt(-1);

    if (!oper.isString()) {
        JSString *operString = ToString<CanGC>(cx, oper);
        if (!operString)
            goto error;
        oper.setString(operString);
    }
}
END_CASE(JSOP_TOSTRING)

CASE(JSOP_OBJECT)
{
    RootedObject &ref = rootObject0;
    ref = script->getObject(REGS.pc);
    if (JS::CompartmentOptionsRef(cx).cloneSingletons()) {
        JSObject *obj = js::DeepCloneObjectLiteral(cx, ref, js::MaybeSingletonObject);
        if (!obj)
            goto error;
        PUSH_OBJECT(*obj);
    } else {
        JS::CompartmentOptionsRef(cx).setSingletonsAsValues();
        PUSH_OBJECT(*ref);
    }
}
END_CASE(JSOP_OBJECT)

CASE(JSOP_CALLSITEOBJ)
{

    RootedObject &cso = rootObject0;
    cso = script->getObject(REGS.pc);
    RootedObject &raw = rootObject1;
    raw = script->getObject(GET_UINT32_INDEX(REGS.pc) + 1);
    RootedValue &rawValue = rootValue0;
    rawValue.setObject(*raw);

    if (!ProcessCallSiteObjOperation(cx, cso, raw, rawValue))
        goto error;

    PUSH_OBJECT(*cso);
}
END_CASE(JSOP_CALLSITEOBJ)

CASE(JSOP_REGEXP)
{
    /*
     * Push a regexp object cloned from the regexp literal object mapped by the
     * bytecode at pc.
     */
    JSObject *obj = CloneRegExpObject(cx, script->getRegExp(REGS.pc));
    if (!obj)
        goto error;
    PUSH_OBJECT(*obj);
}
END_CASE(JSOP_REGEXP)

CASE(JSOP_ZERO)
    PUSH_INT32(0);
END_CASE(JSOP_ZERO)

CASE(JSOP_ONE)
    PUSH_INT32(1);
END_CASE(JSOP_ONE)

CASE(JSOP_NULL)
    PUSH_NULL();
END_CASE(JSOP_NULL)

CASE(JSOP_FALSE)
    PUSH_BOOLEAN(false);
END_CASE(JSOP_FALSE)

CASE(JSOP_TRUE)
    PUSH_BOOLEAN(true);
END_CASE(JSOP_TRUE)

CASE(JSOP_TABLESWITCH)
{
    jsbytecode *pc2 = REGS.pc;
    int32_t len = GET_JUMP_OFFSET(pc2);

    /*
     * ECMAv2+ forbids conversion of discriminant, so we will skip to the
     * default case if the discriminant isn't already an int jsval.  (This
     * opcode is emitted only for dense int-domain switches.)
     */
    const Value &rref = *--REGS.sp;
    int32_t i;
    if (rref.isInt32()) {
        i = rref.toInt32();
    } else {
        /* Use mozilla::NumberEqualsInt32 to treat -0 (double) as 0. */
        if (!rref.isDouble() || !NumberEqualsInt32(rref.toDouble(), &i))
            ADVANCE_AND_DISPATCH(len);
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
    ADVANCE_AND_DISPATCH(len);
}

CASE(JSOP_ARGUMENTS)
    JS_ASSERT(!REGS.fp()->fun()->hasRest());
    if (!script->ensureHasAnalyzedArgsUsage(cx))
        goto error;
    if (script->needsArgsObj()) {
        ArgumentsObject *obj = ArgumentsObject::createExpected(cx, REGS.fp());
        if (!obj)
            goto error;
        PUSH_COPY(ObjectValue(*obj));
    } else {
        PUSH_COPY(MagicValue(JS_OPTIMIZED_ARGUMENTS));
    }
END_CASE(JSOP_ARGUMENTS)

CASE(JSOP_RUNONCE)
{
    if (!RunOnceScriptPrologue(cx, script))
        goto error;
}
END_CASE(JSOP_RUNONCE)

CASE(JSOP_REST)
{
    RootedObject &rest = rootObject0;
    rest = REGS.fp()->createRestParameter(cx);
    if (!rest)
        goto error;
    PUSH_COPY(ObjectValue(*rest));
}
END_CASE(JSOP_REST)

CASE(JSOP_GETALIASEDVAR)
{
    ScopeCoordinate sc = ScopeCoordinate(REGS.pc);
    PUSH_COPY(REGS.fp()->aliasedVarScope(sc).aliasedVar(sc));
    TypeScript::Monitor(cx, script, REGS.pc, REGS.sp[-1]);
}
END_CASE(JSOP_GETALIASEDVAR)

CASE(JSOP_SETALIASEDVAR)
{
    ScopeCoordinate sc = ScopeCoordinate(REGS.pc);
    ScopeObject &obj = REGS.fp()->aliasedVarScope(sc);

    // Avoid computing the name if no type updates are needed, as this may be
    // expensive on scopes with large numbers of variables.
    PropertyName *name = (obj.hasSingletonType() && !obj.hasLazyType())
                         ? ScopeCoordinateName(cx->runtime()->scopeCoordinateNameCache, script, REGS.pc)
                         : nullptr;

    obj.setAliasedVar(cx, sc, name, REGS.sp[-1]);
}
END_CASE(JSOP_SETALIASEDVAR)

CASE(JSOP_GETARG)
{
    unsigned i = GET_ARGNO(REGS.pc);
    if (script->argsObjAliasesFormals())
        PUSH_COPY(REGS.fp()->argsObj().arg(i));
    else
        PUSH_COPY(REGS.fp()->unaliasedFormal(i));
}
END_CASE(JSOP_GETARG)

CASE(JSOP_SETARG)
{
    unsigned i = GET_ARGNO(REGS.pc);
    if (script->argsObjAliasesFormals())
        REGS.fp()->argsObj().setArg(i, REGS.sp[-1]);
    else
        REGS.fp()->unaliasedFormal(i) = REGS.sp[-1];
}
END_CASE(JSOP_SETARG)

CASE(JSOP_GETLOCAL)
{
    uint32_t i = GET_LOCALNO(REGS.pc);
    PUSH_COPY_SKIP_CHECK(REGS.fp()->unaliasedLocal(i));

    /*
     * Skip the same-compartment assertion if the local will be immediately
     * popped. We do not guarantee sync for dead locals when coming in from the
     * method JIT, and a GETLOCAL followed by POP is not considered to be
     * a use of the variable.
     */
    if (REGS.pc[JSOP_GETLOCAL_LENGTH] != JSOP_POP)
        assertSameCompartmentDebugOnly(cx, REGS.sp[-1]);
}
END_CASE(JSOP_GETLOCAL)

CASE(JSOP_SETLOCAL)
{
    uint32_t i = GET_LOCALNO(REGS.pc);
    REGS.fp()->unaliasedLocal(i) = REGS.sp[-1];
}
END_CASE(JSOP_SETLOCAL)

CASE(JSOP_DEFCONST)
CASE(JSOP_DEFVAR)
{
    /* ES5 10.5 step 8 (with subsequent errata). */
    unsigned attrs = JSPROP_ENUMERATE;
    if (*REGS.pc == JSOP_DEFCONST)
        attrs |= JSPROP_READONLY;
    else if (!REGS.fp()->isEvalFrame())
        attrs |= JSPROP_PERMANENT;

    /* Step 8b. */
    RootedObject &obj = rootObject0;
    obj = &REGS.fp()->varObj();

    RootedPropertyName &name = rootName0;
    name = script->getName(REGS.pc);

    if (!DefVarOrConstOperation(cx, obj, name, attrs))
        goto error;
}
END_CASE(JSOP_DEFVAR)

CASE(JSOP_DEFFUN)
{
    /*
     * A top-level function defined in Global or Eval code (see ECMA-262
     * Ed. 3), or else a SpiderMonkey extension: a named function statement in
     * a compound statement (not at the top statement level of global code, or
     * at the top level of a function body).
     */
    RootedFunction &fun = rootFunction0;
    fun = script->getFunction(GET_UINT32_INDEX(REGS.pc));

    if (!DefFunOperation(cx, script, REGS.fp()->scopeChain(), fun))
        goto error;
}
END_CASE(JSOP_DEFFUN)

CASE(JSOP_LAMBDA)
{
    /* Load the specified function object literal. */
    RootedFunction &fun = rootFunction0;
    fun = script->getFunction(GET_UINT32_INDEX(REGS.pc));

    JSObject *obj = Lambda(cx, fun, REGS.fp()->scopeChain());
    if (!obj)
        goto error;
    JS_ASSERT(obj->getProto());
    PUSH_OBJECT(*obj);
}
END_CASE(JSOP_LAMBDA)

CASE(JSOP_LAMBDA_ARROW)
{
    /* Load the specified function object literal. */
    RootedFunction &fun = rootFunction0;
    fun = script->getFunction(GET_UINT32_INDEX(REGS.pc));
    RootedValue &thisv = rootValue0;
    thisv = REGS.sp[-1];
    JSObject *obj = LambdaArrow(cx, fun, REGS.fp()->scopeChain(), thisv);
    if (!obj)
        goto error;
    JS_ASSERT(obj->getProto());
    REGS.sp[-1].setObject(*obj);
}
END_CASE(JSOP_LAMBDA_ARROW)

CASE(JSOP_CALLEE)
    JS_ASSERT(REGS.fp()->isNonEvalFunctionFrame());
    PUSH_COPY(REGS.fp()->calleev());
END_CASE(JSOP_CALLEE)

CASE(JSOP_INITPROP_GETTER)
CASE(JSOP_INITPROP_SETTER)
{
    RootedObject &obj = rootObject0;
    RootedPropertyName &name = rootName0;
    RootedObject &val = rootObject1;

    JS_ASSERT(REGS.stackDepth() >= 2);
    obj = &REGS.sp[-2].toObject();
    name = script->getName(REGS.pc);
    val = &REGS.sp[-1].toObject();

    if (!InitGetterSetterOperation(cx, REGS.pc, obj, name, val))
        goto error;

    REGS.sp--;
}
END_CASE(JSOP_INITPROP_GETTER)

CASE(JSOP_INITELEM_GETTER)
CASE(JSOP_INITELEM_SETTER)
{
    RootedObject &obj = rootObject0;
    RootedValue &idval = rootValue0;
    RootedObject &val = rootObject1;

    JS_ASSERT(REGS.stackDepth() >= 3);
    obj = &REGS.sp[-3].toObject();
    idval = REGS.sp[-2];
    val = &REGS.sp[-1].toObject();

    if (!InitGetterSetterOperation(cx, REGS.pc, obj, idval, val))
        goto error;

    REGS.sp -= 2;
}
END_CASE(JSOP_INITELEM_GETTER)

CASE(JSOP_HOLE)
    PUSH_HOLE();
END_CASE(JSOP_HOLE)

CASE(JSOP_NEWINIT)
{
    uint8_t i = GET_UINT8(REGS.pc);
    JS_ASSERT(i == JSProto_Array || i == JSProto_Object);

    RootedObject &obj = rootObject0;
    NewObjectKind newKind;
    if (i == JSProto_Array) {
        newKind = UseNewTypeForInitializer(script, REGS.pc, &ArrayObject::class_);
        obj = NewDenseEmptyArray(cx, nullptr, newKind);
    } else {
        gc::AllocKind allocKind = GuessObjectGCKind(0);
        newKind = UseNewTypeForInitializer(script, REGS.pc, &JSObject::class_);
        obj = NewBuiltinClassInstance(cx, &JSObject::class_, allocKind, newKind);
    }
    if (!obj || !SetInitializerObjectType(cx, script, REGS.pc, obj, newKind))
        goto error;

    PUSH_OBJECT(*obj);
}
END_CASE(JSOP_NEWINIT)

CASE(JSOP_NEWARRAY)
{
    unsigned count = GET_UINT24(REGS.pc);
    RootedObject &obj = rootObject0;
    NewObjectKind newKind = UseNewTypeForInitializer(script, REGS.pc, &ArrayObject::class_);
    obj = NewDenseAllocatedArray(cx, count, nullptr, newKind);
    if (!obj || !SetInitializerObjectType(cx, script, REGS.pc, obj, newKind))
        goto error;

    PUSH_OBJECT(*obj);
}
END_CASE(JSOP_NEWARRAY)

CASE(JSOP_NEWARRAY_COPYONWRITE)
{
    RootedObject &baseobj = rootObject0;
    baseobj = types::GetOrFixupCopyOnWriteObject(cx, script, REGS.pc);
    if (!baseobj)
        goto error;

    RootedObject &obj = rootObject1;
    obj = NewDenseCopyOnWriteArray(cx, baseobj, gc::DefaultHeap);
    if (!obj)
        goto error;

    PUSH_OBJECT(*obj);
}
END_CASE(JSOP_NEWARRAY_COPYONWRITE)

CASE(JSOP_NEWOBJECT)
{
    RootedObject &baseobj = rootObject0;
    baseobj = script->getObject(REGS.pc);

    RootedObject &obj = rootObject1;
    NewObjectKind newKind = UseNewTypeForInitializer(script, REGS.pc, baseobj->getClass());
    obj = CopyInitializerObject(cx, baseobj, newKind);
    if (!obj || !SetInitializerObjectType(cx, script, REGS.pc, obj, newKind))
        goto error;

    PUSH_OBJECT(*obj);
}
END_CASE(JSOP_NEWOBJECT)

CASE(JSOP_ENDINIT)
{
    /* FIXME remove JSOP_ENDINIT bug 588522 */
    JS_ASSERT(REGS.stackDepth() >= 1);
    JS_ASSERT(REGS.sp[-1].isObject() || REGS.sp[-1].isUndefined());
}
END_CASE(JSOP_ENDINIT)

CASE(JSOP_MUTATEPROTO)
{
    MOZ_ASSERT(REGS.stackDepth() >= 2);

    if (REGS.sp[-1].isObjectOrNull()) {
        RootedObject &newProto = rootObject1;
        rootObject1 = REGS.sp[-1].toObjectOrNull();

        RootedObject &obj = rootObject0;
        obj = &REGS.sp[-2].toObject();
        MOZ_ASSERT(obj->is<JSObject>());

        bool succeeded;
        if (!JSObject::setProto(cx, obj, newProto, &succeeded))
            goto error;
        MOZ_ASSERT(succeeded);
    }

    REGS.sp--;
}
END_CASE(JSOP_MUTATEPROTO)

CASE(JSOP_INITPROP)
{
    /* Load the property's initial value into rval. */
    JS_ASSERT(REGS.stackDepth() >= 2);
    RootedValue &rval = rootValue0;
    rval = REGS.sp[-1];

    /* Load the object being initialized into lval/obj. */
    RootedObject &obj = rootObject0;
    obj = &REGS.sp[-2].toObject();
    JS_ASSERT(obj->is<JSObject>());

    PropertyName *name = script->getName(REGS.pc);

    RootedId &id = rootId0;
    id = NameToId(name);

    if (!DefineNativeProperty(cx, obj, id, rval, nullptr, nullptr, JSPROP_ENUMERATE))
        goto error;

    REGS.sp--;
}
END_CASE(JSOP_INITPROP)

CASE(JSOP_INITELEM)
{
    JS_ASSERT(REGS.stackDepth() >= 3);
    HandleValue val = REGS.stackHandleAt(-1);
    HandleValue id = REGS.stackHandleAt(-2);

    RootedObject &obj = rootObject0;
    obj = &REGS.sp[-3].toObject();

    if (!InitElemOperation(cx, obj, id, val))
        goto error;

    REGS.sp -= 2;
}
END_CASE(JSOP_INITELEM)

CASE(JSOP_INITELEM_ARRAY)
{
    JS_ASSERT(REGS.stackDepth() >= 2);
    HandleValue val = REGS.stackHandleAt(-1);

    RootedObject &obj = rootObject0;
    obj = &REGS.sp[-2].toObject();

    JS_ASSERT(obj->is<ArrayObject>());

    uint32_t index = GET_UINT24(REGS.pc);
    if (!InitArrayElemOperation(cx, REGS.pc, obj, index, val))
        goto error;

    REGS.sp--;
}
END_CASE(JSOP_INITELEM_ARRAY)

CASE(JSOP_INITELEM_INC)
{
    JS_ASSERT(REGS.stackDepth() >= 3);
    HandleValue val = REGS.stackHandleAt(-1);

    RootedObject &obj = rootObject0;
    obj = &REGS.sp[-3].toObject();

    uint32_t index = REGS.sp[-2].toInt32();
    if (!InitArrayElemOperation(cx, REGS.pc, obj, index, val))
        goto error;

    REGS.sp[-2].setInt32(index + 1);
    REGS.sp--;
}
END_CASE(JSOP_INITELEM_INC)

CASE(JSOP_GOSUB)
{
    PUSH_BOOLEAN(false);
    int32_t i = script->pcToOffset(REGS.pc) + JSOP_GOSUB_LENGTH;
    int32_t len = GET_JUMP_OFFSET(REGS.pc);
    PUSH_INT32(i);
    ADVANCE_AND_DISPATCH(len);
}

CASE(JSOP_RETSUB)
{
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
    int32_t len = rval.toInt32() - int32_t(script->pcToOffset(REGS.pc));
    ADVANCE_AND_DISPATCH(len);
}

CASE(JSOP_EXCEPTION)
{
    PUSH_NULL();
    MutableHandleValue res = REGS.stackHandleAt(-1);
    if (!GetAndClearException(cx, res))
        goto error;
}
END_CASE(JSOP_EXCEPTION)

CASE(JSOP_FINALLY)
    CHECK_BRANCH();
END_CASE(JSOP_FINALLY)

CASE(JSOP_THROWING)
{
    JS_ASSERT(!cx->isExceptionPending());
    Value v;
    POP_COPY_TO(v);
    cx->setPendingException(v);
}
END_CASE(JSOP_THROWING)

CASE(JSOP_THROW)
{
    CHECK_BRANCH();
    RootedValue &v = rootValue0;
    POP_COPY_TO(v);
    JS_ALWAYS_FALSE(Throw(cx, v));
    /* let the code at error try to catch the exception. */
    goto error;
}

CASE(JSOP_INSTANCEOF)
{
    RootedValue &rref = rootValue0;
    rref = REGS.sp[-1];
    if (rref.isPrimitive()) {
        js_ReportValueError(cx, JSMSG_BAD_INSTANCEOF_RHS, -1, rref, js::NullPtr());
        goto error;
    }
    RootedObject &obj = rootObject0;
    obj = &rref.toObject();
    bool cond = false;
    if (!HasInstance(cx, obj, REGS.stackHandleAt(-2), &cond))
        goto error;
    REGS.sp--;
    REGS.sp[-1].setBoolean(cond);
}
END_CASE(JSOP_INSTANCEOF)

CASE(JSOP_DEBUGGER)
{
    RootedValue rval(cx);
    switch (Debugger::onDebuggerStatement(cx, &rval)) {
      case JSTRAP_ERROR:
        goto error;
      case JSTRAP_CONTINUE:
        break;
      case JSTRAP_RETURN:
        REGS.fp()->setReturnValue(rval);
        ForcedReturn(cx, REGS);
        goto successful_return_continuation;
      case JSTRAP_THROW:
        cx->setPendingException(rval);
        goto error;
      default:;
    }
}
END_CASE(JSOP_DEBUGGER)

CASE(JSOP_PUSHBLOCKSCOPE)
{
    StaticBlockObject &blockObj = script->getObject(REGS.pc)->as<StaticBlockObject>();

    JS_ASSERT(blockObj.needsClone());
    // Clone block and push on scope chain.
    if (!REGS.fp()->pushBlock(cx, blockObj))
        goto error;
}
END_CASE(JSOP_PUSHBLOCKSCOPE)

CASE(JSOP_POPBLOCKSCOPE)
{
#ifdef DEBUG
    // Pop block from scope chain.
    JS_ASSERT(*(REGS.pc - JSOP_DEBUGLEAVEBLOCK_LENGTH) == JSOP_DEBUGLEAVEBLOCK);
    NestedScopeObject *scope = script->getStaticScope(REGS.pc - JSOP_DEBUGLEAVEBLOCK_LENGTH);
    JS_ASSERT(scope && scope->is<StaticBlockObject>());
    StaticBlockObject &blockObj = scope->as<StaticBlockObject>();
    JS_ASSERT(blockObj.needsClone());
#endif

    // Pop block from scope chain.
    REGS.fp()->popBlock(cx);
}
END_CASE(JSOP_POPBLOCKSCOPE)

CASE(JSOP_DEBUGLEAVEBLOCK)
{
    JS_ASSERT(script->getStaticScope(REGS.pc));
    JS_ASSERT(script->getStaticScope(REGS.pc)->is<StaticBlockObject>());

    // FIXME: This opcode should not be necessary.  The debugger shouldn't need
    // help from bytecode to do its job.  See bug 927782.

    if (MOZ_UNLIKELY(cx->compartment()->debugMode()))
        DebugScopes::onPopBlock(cx, REGS.fp(), REGS.pc);
}
END_CASE(JSOP_DEBUGLEAVEBLOCK)

CASE(JSOP_GENERATOR)
{
    JS_ASSERT(!cx->isExceptionPending());
    REGS.fp()->initGeneratorFrame();
    REGS.pc += JSOP_GENERATOR_LENGTH;
    JSObject *obj = js_NewGenerator(cx, REGS);
    if (!obj)
        goto error;
    REGS.fp()->setReturnValue(ObjectValue(*obj));
    REGS.fp()->setYielding();
    interpReturnOK = true;
    if (activation.entryFrame() != REGS.fp())
        goto inline_return;
    goto exit;
}

CASE(JSOP_YIELD)
    JS_ASSERT(!cx->isExceptionPending());
    JS_ASSERT(REGS.fp()->isNonEvalFunctionFrame());
    if (cx->innermostGenerator()->state == JSGEN_CLOSING) {
        RootedValue &val = rootValue0;
        val.setObject(REGS.fp()->callee());
        js_ReportValueError(cx, JSMSG_BAD_GENERATOR_YIELD, JSDVG_SEARCH_STACK, val, js::NullPtr());
        goto error;
    }
    REGS.fp()->setReturnValue(REGS.sp[-1]);
    REGS.fp()->setYielding();
    REGS.pc += JSOP_YIELD_LENGTH;
    interpReturnOK = true;
    goto exit;

CASE(JSOP_ARRAYPUSH)
{
    RootedObject &obj = rootObject0;
    obj = &REGS.sp[-1].toObject();
    if (!NewbornArrayPush(cx, obj, REGS.sp[-2]))
        goto error;
    REGS.sp -= 2;
}
END_CASE(JSOP_ARRAYPUSH)

DEFAULT()
{
    char numBuf[12];
    JS_snprintf(numBuf, sizeof numBuf, "%d", *REGS.pc);
    JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr,
                         JSMSG_BAD_BYTECODE, numBuf);
    goto error;
}

} /* interpreter loop */

    MOZ_CRASH("Interpreter loop exited via fallthrough");

  error:
    switch (HandleError(cx, REGS)) {
      case SuccessfulReturnContinuation:
        goto successful_return_continuation;

      case ErrorReturnContinuation:
        interpReturnOK = false;
        goto return_continuation;

      case CatchContinuation:
        ADVANCE_AND_DISPATCH(0);

      case FinallyContinuation:
        /*
         * Push (true, exception) pair for finally to indicate that [retsub]
         * should rethrow the exception.
         */
        RootedValue &exception = rootValue0;
        if (!cx->getPendingException(&exception)) {
            interpReturnOK = false;
            goto return_continuation;
        }
        PUSH_BOOLEAN(true);
        PUSH_COPY(exception);
        cx->clearPendingException();
        ADVANCE_AND_DISPATCH(0);
    }

    MOZ_CRASH("Invalid HandleError continuation");

  exit:
    if (MOZ_UNLIKELY(cx->compartment()->debugMode()))
        interpReturnOK = ScriptDebugEpilogue(cx, REGS.fp(), REGS.pc, interpReturnOK);
    if (!REGS.fp()->isYielding())
        REGS.fp()->epilogue(cx);
    else
        probes::ExitScript(cx, script, script->functionNonDelazifying(),
                           REGS.fp()->hasPushedSPSFrame());

    gc::MaybeVerifyBarriers(cx, true);

    TraceLogStopEvent(logger);
    TraceLogStopEvent(logger, scriptLogId);

    /*
     * This path is used when it's guaranteed the method can be finished
     * inside the JIT.
     */
  leave_on_safe_point:

    if (interpReturnOK)
        state.setReturnValue(activation.entryFrame()->returnValue());

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
js::CallProperty(JSContext *cx, HandleValue v, HandlePropertyName name, MutableHandleValue vp)
{
    if (!GetProperty(cx, v, name, vp))
        return false;

#if JS_HAS_NO_SUCH_METHOD
    if (MOZ_UNLIKELY(vp.isUndefined()) && v.isObject())
    {
        RootedObject obj(cx, &v.toObject());
        if (!OnUnknownMethod(cx, obj, StringValue(name), vp))
            return false;
    }
#endif

    return true;
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
    MOZ_ASSERT(!fun->isArrow());

    RootedObject clone(cx, CloneFunctionObjectIfNotSingleton(cx, fun, parent));
    if (!clone)
        return nullptr;

    MOZ_ASSERT(clone->global() == clone->global());
    return clone;
}

JSObject *
js::LambdaArrow(JSContext *cx, HandleFunction fun, HandleObject parent, HandleValue thisv)
{
    MOZ_ASSERT(fun->isArrow());

    RootedObject clone(cx, CloneFunctionObjectIfNotSingleton(cx, fun, parent, TenuredObject));
    if (!clone)
        return nullptr;

    MOZ_ASSERT(clone->as<JSFunction>().isArrow());
    clone->as<JSFunction>().setExtendedSlot(0, thisv);

    MOZ_ASSERT(clone->global() == clone->global());
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
        JS_ASSERT(script->compileAndGo());
        JS_ASSERT(!script->functionNonDelazifying());
    }

    /*
     * We define the function as a property of the variable object and not the
     * current scope chain even for the case of function expression statements
     * and functions defined by eval inside let or with blocks.
     */
    RootedObject parent(cx, scopeChain);
    while (!parent->isQualifiedVarObj())
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
    unsigned attrs = script->isActiveEval()
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
                JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_CANT_REDEFINE_PROP,
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
    return JSObject::setProperty(cx, parent, parent, name, &rval, script->strict());
}

bool
js::SetCallOperation(JSContext *cx)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_BAD_LEFTSIDE_OF_ASS);
    return false;
}

bool
js::GetAndClearException(JSContext *cx, MutableHandleValue res)
{
    bool status = cx->getPendingException(res);
    cx->clearPendingException();
    if (!status)
        return false;

    // Allow interrupting deeply nested exception handling.
    return CheckForInterrupt(cx);
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
js::DeleteProperty(JSContext *cx, HandleValue v, HandlePropertyName name, bool *bp)
{
    RootedObject obj(cx, ToObjectFromStack(cx, v));
    if (!obj)
        return false;

    RootedId id(cx, NameToId(name));
    if (!JSObject::deleteGeneric(cx, obj, id, bp))
        return false;

    if (strict && !*bp) {
        obj->reportNotConfigurable(cx, NameToId(name));
        return false;
    }
    return true;
}

template bool js::DeleteProperty<true> (JSContext *cx, HandleValue val, HandlePropertyName name, bool *bp);
template bool js::DeleteProperty<false>(JSContext *cx, HandleValue val, HandlePropertyName name, bool *bp);

template <bool strict>
bool
js::DeleteElement(JSContext *cx, HandleValue val, HandleValue index, bool *bp)
{
    RootedObject obj(cx, ToObjectFromStack(cx, val));
    if (!obj)
        return false;

    RootedId id(cx);
    if (!ValueToId<CanGC>(cx, index, &id))
        return false;
    if (!JSObject::deleteGeneric(cx, obj, id, bp))
        return false;

    if (strict && !*bp) {
        obj->reportNotConfigurable(cx, id);
        return false;
    }
    return true;
}

template bool js::DeleteElement<true> (JSContext *, HandleValue, HandleValue, bool *succeeded);
template bool js::DeleteElement<false>(JSContext *, HandleValue, HandleValue, bool *succeeded);

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
                     bool strict)
{
    RootedId id(cx);
    if (!ValueToId<CanGC>(cx, index, &id))
        return false;
    return SetObjectElementOperation(cx, obj, id, value, strict);
}

bool
js::SetObjectElement(JSContext *cx, HandleObject obj, HandleValue index, HandleValue value,
                     bool strict, HandleScript script, jsbytecode *pc)
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
js::AddValues(JSContext *cx, MutableHandleValue lhs, MutableHandleValue rhs, MutableHandleValue res)
{
    return AddOperation(cx, lhs, rhs, res);
}

bool
js::SubValues(JSContext *cx, MutableHandleValue lhs, MutableHandleValue rhs, MutableHandleValue res)
{
    return SubOperation(cx, lhs, rhs, res);
}

bool
js::MulValues(JSContext *cx, MutableHandleValue lhs, MutableHandleValue rhs, MutableHandleValue res)
{
    return MulOperation(cx, lhs, rhs, res);
}

bool
js::DivValues(JSContext *cx, MutableHandleValue lhs, MutableHandleValue rhs, MutableHandleValue res)
{
    return DivOperation(cx, lhs, rhs, res);
}

bool
js::ModValues(JSContext *cx, MutableHandleValue lhs, MutableHandleValue rhs, MutableHandleValue res)
{
    return ModOperation(cx, lhs, rhs, res);
}

bool
js::UrshValues(JSContext *cx, MutableHandleValue lhs, MutableHandleValue rhs, MutableHandleValue res)
{
    return UrshOperation(cx, lhs, rhs, res);
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

    bool succeeded;
    RootedId id(cx, NameToId(name));
    if (!JSObject::deleteGeneric(cx, scope, id, &succeeded))
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
    JS_ASSERT(script->treatAsRunOnce());

    if (!script->hasRunOnce()) {
        script->setHasRunOnce();
        return true;
    }

    // Force instantiation of the script's function's type to ensure the flag
    // is preserved in type information.
    if (!script->functionNonDelazifying()->getType(cx))
        return false;

    types::MarkTypeObjectFlags(cx, script->functionNonDelazifying(),
                               types::OBJECT_FLAG_RUNONCE_INVALIDATED);
    return true;
}

bool
js::InitGetterSetterOperation(JSContext *cx, jsbytecode *pc, HandleObject obj, HandleId id,
                              HandleObject val)
{
    JS_ASSERT(val->isCallable());
    PropertyOp getter;
    StrictPropertyOp setter;
    unsigned attrs = JSPROP_ENUMERATE | JSPROP_SHARED;

    JSOp op = JSOp(*pc);

    if (op == JSOP_INITPROP_GETTER || op == JSOP_INITELEM_GETTER) {
        getter = CastAsPropertyOp(val);
        setter = JS_StrictPropertyStub;
        attrs |= JSPROP_GETTER;
    } else {
        JS_ASSERT(op == JSOP_INITPROP_SETTER || op == JSOP_INITELEM_SETTER);
        getter = JS_PropertyStub;
        setter = CastAsStrictPropertyOp(val);
        attrs |= JSPROP_SETTER;
    }

    RootedValue scratch(cx);
    return JSObject::defineGeneric(cx, obj, id, scratch, getter, setter, attrs);
}

bool
js::InitGetterSetterOperation(JSContext *cx, jsbytecode *pc, HandleObject obj,
                              HandlePropertyName name, HandleObject val)
{
    RootedId id(cx, NameToId(name));
    return InitGetterSetterOperation(cx, pc, obj, id, val);
}

bool
js::InitGetterSetterOperation(JSContext *cx, jsbytecode *pc, HandleObject obj, HandleValue idval,
                              HandleObject val)
{
    RootedId id(cx);
    if (!ValueToId<CanGC>(cx, idval, &id))
        return false;

    return InitGetterSetterOperation(cx, pc, obj, id, val);
}

bool
js::SpreadCallOperation(JSContext *cx, HandleScript script, jsbytecode *pc, HandleValue thisv,
                        HandleValue callee, HandleValue arr, MutableHandleValue res)
{
    RootedObject aobj(cx, &arr.toObject());
    uint32_t length = aobj->as<ArrayObject>().length();
    JSOp op = JSOp(*pc);

    if (length > ARGS_LENGTH_MAX) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr,
                             op == JSOP_SPREADNEW ? JSMSG_TOO_MANY_CON_SPREADARGS
                                                  : JSMSG_TOO_MANY_FUN_SPREADARGS);
        return false;
    }

#ifdef DEBUG
    // The object must be an array with dense elements and no holes. Baseline's
    // optimized spread call stubs rely on this.
    JS_ASSERT(aobj->getDenseInitializedLength() == length);
    JS_ASSERT(!aobj->isIndexed());
    for (uint32_t i = 0; i < length; i++)
        JS_ASSERT(!aobj->getDenseElement(i).isMagic());
#endif

    InvokeArgs args(cx);

    if (!args.init(length))
        return false;

    args.setCallee(callee);
    args.setThis(thisv);

    if (!GetElements(cx, aobj, length, args.array()))
        return false;

    switch (op) {
      case JSOP_SPREADNEW:
        if (!InvokeConstructor(cx, args))
            return false;
        break;
      case JSOP_SPREADCALL:
        if (!Invoke(cx, args))
            return false;
        break;
      case JSOP_SPREADEVAL:
        if (cx->global()->valueIsEval(args.calleev())) {
            if (!DirectEval(cx, args))
                return false;
        } else {
            if (!Invoke(cx, args))
                return false;
        }
        break;
      default:
        MOZ_CRASH("bad spread opcode");
    }

    res.set(args.rval());
    TypeScript::Monitor(cx, script, pc, res);
    return true;
}
