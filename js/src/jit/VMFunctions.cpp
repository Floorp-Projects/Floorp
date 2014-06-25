/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/VMFunctions.h"

#include "builtin/TypedObject.h"
#include "frontend/BytecodeCompiler.h"
#include "jit/arm/Simulator-arm.h"
#include "jit/BaselineIC.h"
#include "jit/IonFrames.h"
#include "jit/JitCompartment.h"
#include "jit/mips/Simulator-mips.h"
#include "vm/ArrayObject.h"
#include "vm/Debugger.h"
#include "vm/Interpreter.h"
#include "vm/TraceLogging.h"

#include "jsinferinlines.h"

#include "jit/BaselineFrame-inl.h"
#include "jit/IonFrames-inl.h"
#include "vm/Interpreter-inl.h"
#include "vm/StringObject-inl.h"

using namespace js;
using namespace js::jit;

namespace js {
namespace jit {

// Don't explicitly initialize, it's not guaranteed that this initializer will
// run before the constructors for static VMFunctions.
/* static */ VMFunction *VMFunction::functions;

AutoDetectInvalidation::AutoDetectInvalidation(JSContext *cx, Value *rval, IonScript *ionScript)
  : cx_(cx),
    ionScript_(ionScript ? ionScript : GetTopIonJSScript(cx)->ionScript()),
    rval_(rval),
    disabled_(false)
{ }

void
VMFunction::addToFunctions()
{
    static bool initialized = false;
    if (!initialized) {
        initialized = true;
        functions = nullptr;
    }
    this->next = functions;
    functions = this;
}

bool
InvokeFunction(JSContext *cx, HandleObject obj0, uint32_t argc, Value *argv, Value *rval)
{
    RootedObject obj(cx, obj0);
    if (obj->is<JSFunction>()) {
        RootedFunction fun(cx, &obj->as<JSFunction>());
        if (fun->isInterpreted()) {
            if (fun->isInterpretedLazy() && !fun->getOrCreateScript(cx))
                return false;

            // Clone function at call site if needed.
            if (fun->nonLazyScript()->shouldCloneAtCallsite()) {
                jsbytecode *pc;
                RootedScript script(cx, cx->currentScript(&pc));
                fun = CloneFunctionAtCallsite(cx, fun, script, pc);
                if (!fun)
                    return false;
            }
        }
    }

    // Data in the argument vector is arranged for a JIT -> JIT call.
    Value thisv = argv[0];
    Value *argvWithoutThis = argv + 1;

    // For constructing functions, |this| is constructed at caller side and we can just call Invoke.
    // When creating this failed / is impossible at caller site, i.e. MagicValue(JS_IS_CONSTRUCTING),
    // we use InvokeConstructor that creates it at the callee side.
    RootedValue rv(cx);
    if (thisv.isMagic(JS_IS_CONSTRUCTING)) {
        if (!InvokeConstructor(cx, ObjectValue(*obj), argc, argvWithoutThis, rv.address()))
            return false;
    } else {
        if (!Invoke(cx, thisv, ObjectValue(*obj), argc, argvWithoutThis, &rv))
            return false;
    }

    if (obj->is<JSFunction>()) {
        jsbytecode *pc;
        RootedScript script(cx, cx->currentScript(&pc));
        types::TypeScript::Monitor(cx, script, pc, rv.get());
    }

    *rval = rv;
    return true;
}

JSObject *
NewGCObject(JSContext *cx, gc::AllocKind allocKind, gc::InitialHeap initialHeap)
{
    return js::NewGCObject<CanGC>(cx, allocKind, 0, initialHeap);
}

bool
CheckOverRecursed(JSContext *cx)
{
    // IonMonkey's stackLimit is equal to nativeStackLimit by default. When we
    // request an interrupt, we set the jitStackLimit to nullptr, which causes
    // the stack limit check to fail.
    //
    // There are two states we're concerned about here:
    //   (1) The interrupt bit is set, and we need to fire the interrupt callback.
    //   (2) The stack limit has been exceeded, and we need to throw an error.
    //
    // Note that we can reach here if jitStackLimit is MAXADDR, but interrupt
    // has not yet been set to 1. That's okay; it will be set to 1 very shortly,
    // and in the interim we might just fire a few useless calls to
    // CheckOverRecursed.
#if defined(JS_ARM_SIMULATOR) || defined(JS_MIPS_SIMULATOR)
    JS_CHECK_SIMULATOR_RECURSION_WITH_EXTRA(cx, 0, return false);
#else
    JS_CHECK_RECURSION(cx, return false);
#endif

    if (cx->runtime()->interrupt)
        return InterruptCheck(cx);

    return true;
}

// This function can get called in two contexts.  In the usual context, it's
// called with ealyCheck=false, after the scope chain has been initialized on
// a baseline frame.  In this case, it's ok to throw an exception, so a failed
// stack check returns false, and a successful stack check promps a check for
// an interrupt from the runtime, which may also cause a false return.
//
// In the second case, it's called with earlyCheck=true, prior to frame
// initialization.  An exception cannot be thrown in this instance, so instead
// an error flag is set on the frame and true returned.
bool
CheckOverRecursedWithExtra(JSContext *cx, BaselineFrame *frame,
                           uint32_t extra, uint32_t earlyCheck)
{
    JS_ASSERT_IF(earlyCheck, !frame->overRecursed());

    // See |CheckOverRecursed| above.  This is a variant of that function which
    // accepts an argument holding the extra stack space needed for the Baseline
    // frame that's about to be pushed.
    uint8_t spDummy;
    uint8_t *checkSp = (&spDummy) - extra;
    if (earlyCheck) {
#if defined(JS_ARM_SIMULATOR) || defined(JS_MIPS_SIMULATOR)
        (void)checkSp;
        JS_CHECK_SIMULATOR_RECURSION_WITH_EXTRA(cx, extra, frame->setOverRecursed());
#else
        JS_CHECK_RECURSION_WITH_SP(cx, checkSp, frame->setOverRecursed());
#endif
        return true;
    }

    // The OVERRECURSED flag may have already been set on the frame by an
    // early over-recursed check.  If so, throw immediately.
    if (frame->overRecursed())
        return false;

#if defined(JS_ARM_SIMULATOR) || defined(JS_MIPS_SIMULATOR)
    JS_CHECK_SIMULATOR_RECURSION_WITH_EXTRA(cx, extra, return false);
#else
    JS_CHECK_RECURSION_WITH_SP(cx, checkSp, return false);
#endif

    if (cx->runtime()->interrupt)
        return InterruptCheck(cx);

    return true;
}

bool
DefVarOrConst(JSContext *cx, HandlePropertyName dn, unsigned attrs, HandleObject scopeChain)
{
    // Given the ScopeChain, extract the VarObj.
    RootedObject obj(cx, scopeChain);
    while (!obj->isVarObj())
        obj = obj->enclosingScope();

    return DefVarOrConstOperation(cx, obj, dn, attrs);
}

bool
SetConst(JSContext *cx, HandlePropertyName name, HandleObject scopeChain, HandleValue rval)
{
    // Given the ScopeChain, extract the VarObj.
    RootedObject obj(cx, scopeChain);
    while (!obj->isVarObj())
        obj = obj->enclosingScope();

    return SetConstOperation(cx, obj, name, rval);
}

bool
MutatePrototype(JSContext *cx, HandleObject obj, HandleValue value)
{
    MOZ_ASSERT(obj->is<JSObject>(), "must only be used with object literals");
    if (!value.isObjectOrNull())
        return true;

    RootedObject newProto(cx, value.toObjectOrNull());

    bool succeeded;
    if (!JSObject::setProto(cx, obj, newProto, &succeeded))
        return false;
    MOZ_ASSERT(succeeded);
    return true;
}

bool
InitProp(JSContext *cx, HandleObject obj, HandlePropertyName name, HandleValue value)
{
    // Copy the incoming value. This may be overwritten; the return value is discarded.
    RootedValue rval(cx, value);
    RootedId id(cx, NameToId(name));

    MOZ_ASSERT(name != cx->names().proto,
               "__proto__ should have been handled by JSOP_MUTATEPROTO");
    return DefineNativeProperty(cx, obj, id, rval, nullptr, nullptr, JSPROP_ENUMERATE);
}

template<bool Equal>
bool
LooselyEqual(JSContext *cx, MutableHandleValue lhs, MutableHandleValue rhs, bool *res)
{
    if (!js::LooselyEqual(cx, lhs, rhs, res))
        return false;
    if (!Equal)
        *res = !*res;
    return true;
}

template bool LooselyEqual<true>(JSContext *cx, MutableHandleValue lhs, MutableHandleValue rhs, bool *res);
template bool LooselyEqual<false>(JSContext *cx, MutableHandleValue lhs, MutableHandleValue rhs, bool *res);

template<bool Equal>
bool
StrictlyEqual(JSContext *cx, MutableHandleValue lhs, MutableHandleValue rhs, bool *res)
{
    if (!js::StrictlyEqual(cx, lhs, rhs, res))
        return false;
    if (!Equal)
        *res = !*res;
    return true;
}

template bool StrictlyEqual<true>(JSContext *cx, MutableHandleValue lhs, MutableHandleValue rhs, bool *res);
template bool StrictlyEqual<false>(JSContext *cx, MutableHandleValue lhs, MutableHandleValue rhs, bool *res);

bool
LessThan(JSContext *cx, MutableHandleValue lhs, MutableHandleValue rhs, bool *res)
{
    return LessThanOperation(cx, lhs, rhs, res);
}

bool
LessThanOrEqual(JSContext *cx, MutableHandleValue lhs, MutableHandleValue rhs, bool *res)
{
    return LessThanOrEqualOperation(cx, lhs, rhs, res);
}

bool
GreaterThan(JSContext *cx, MutableHandleValue lhs, MutableHandleValue rhs, bool *res)
{
    return GreaterThanOperation(cx, lhs, rhs, res);
}

bool
GreaterThanOrEqual(JSContext *cx, MutableHandleValue lhs, MutableHandleValue rhs, bool *res)
{
    return GreaterThanOrEqualOperation(cx, lhs, rhs, res);
}

template<bool Equal>
bool
StringsEqual(JSContext *cx, HandleString lhs, HandleString rhs, bool *res)
{
    if (!js::EqualStrings(cx, lhs, rhs, res))
        return false;
    if (!Equal)
        *res = !*res;
    return true;
}

template bool StringsEqual<true>(JSContext *cx, HandleString lhs, HandleString rhs, bool *res);
template bool StringsEqual<false>(JSContext *cx, HandleString lhs, HandleString rhs, bool *res);

bool
IteratorMore(JSContext *cx, HandleObject obj, bool *res)
{
    RootedValue tmp(cx);
    if (!js_IteratorMore(cx, obj, &tmp))
        return false;

    *res = tmp.toBoolean();
    return true;
}

JSObject*
NewInitArray(JSContext *cx, uint32_t count, types::TypeObject *typeArg)
{
    RootedTypeObject type(cx, typeArg);
    NewObjectKind newKind = !type ? SingletonObject : GenericObject;
    if (type && type->shouldPreTenure())
        newKind = TenuredObject;
    RootedObject obj(cx, NewDenseAllocatedArray(cx, count, nullptr, newKind));
    if (!obj)
        return nullptr;

    if (type)
        obj->setType(type);

    return obj;
}

JSObject*
NewInitObject(JSContext *cx, HandleObject templateObject)
{
    NewObjectKind newKind = templateObject->hasSingletonType() ? SingletonObject : GenericObject;
    if (!templateObject->hasLazyType() && templateObject->type()->shouldPreTenure())
        newKind = TenuredObject;
    RootedObject obj(cx, CopyInitializerObject(cx, templateObject, newKind));

    if (!obj)
        return nullptr;

    if (!templateObject->hasSingletonType())
        obj->setType(templateObject->type());

    return obj;
}

JSObject *
NewInitObjectWithClassPrototype(JSContext *cx, HandleObject templateObject)
{
    JS_ASSERT(!templateObject->hasSingletonType());
    JS_ASSERT(!templateObject->hasLazyType());

    NewObjectKind newKind = templateObject->type()->shouldPreTenure()
                            ? TenuredObject
                            : GenericObject;
    JSObject *obj = NewObjectWithGivenProto(cx,
                                            templateObject->getClass(),
                                            templateObject->getProto(),
                                            cx->global(),
                                            newKind);
    if (!obj)
        return nullptr;

    obj->setType(templateObject->type());

    return obj;
}

bool
ArraySpliceDense(JSContext *cx, HandleObject obj, uint32_t start, uint32_t deleteCount)
{
    JS::AutoValueArray<4> argv(cx);
    argv[0].setUndefined();
    argv[1].setObject(*obj);
    argv[2].set(Int32Value(start));
    argv[3].set(Int32Value(deleteCount));

    return js::array_splice_impl(cx, 2, argv.begin(), false);
}

bool
ArrayPopDense(JSContext *cx, HandleObject obj, MutableHandleValue rval)
{
    JS_ASSERT(obj->is<ArrayObject>());

    AutoDetectInvalidation adi(cx, rval.address());

    JS::AutoValueArray<2> argv(cx);
    argv[0].setUndefined();
    argv[1].setObject(*obj);
    if (!js::array_pop(cx, 0, argv.begin()))
        return false;

    // If the result is |undefined|, the array was probably empty and we
    // have to monitor the return value.
    rval.set(argv[0]);
    if (rval.isUndefined())
        types::TypeScript::Monitor(cx, rval);
    return true;
}

bool
ArrayPushDense(JSContext *cx, HandleObject obj, HandleValue v, uint32_t *length)
{
    JS_ASSERT(obj->is<ArrayObject>());

    JS::AutoValueArray<3> argv(cx);
    argv[0].setUndefined();
    argv[1].setObject(*obj);
    argv[2].set(v);
    if (!js::array_push(cx, 1, argv.begin()))
        return false;

    *length = argv[0].toInt32();
    return true;
}

bool
ArrayShiftDense(JSContext *cx, HandleObject obj, MutableHandleValue rval)
{
    JS_ASSERT(obj->is<ArrayObject>());

    AutoDetectInvalidation adi(cx, rval.address());

    JS::AutoValueArray<2> argv(cx);
    argv[0].setUndefined();
    argv[1].setObject(*obj);
    if (!js::array_shift(cx, 0, argv.begin()))
        return false;

    // If the result is |undefined|, the array was probably empty and we
    // have to monitor the return value.
    rval.set(argv[0]);
    if (rval.isUndefined())
        types::TypeScript::Monitor(cx, rval);
    return true;
}

JSObject *
ArrayConcatDense(JSContext *cx, HandleObject obj1, HandleObject obj2, HandleObject objRes)
{
    Rooted<ArrayObject*> arr1(cx, &obj1->as<ArrayObject>());
    Rooted<ArrayObject*> arr2(cx, &obj2->as<ArrayObject>());
    Rooted<ArrayObject*> arrRes(cx, objRes ? &objRes->as<ArrayObject>() : nullptr);

    if (arrRes) {
        // Fast path if we managed to allocate an object inline.
        if (!js::array_concat_dense(cx, arr1, arr2, arrRes))
            return nullptr;
        return arrRes;
    }

    JS::AutoValueArray<3> argv(cx);
    argv[0].setUndefined();
    argv[1].setObject(*arr1);
    argv[2].setObject(*arr2);
    if (!js::array_concat(cx, 1, argv.begin()))
        return nullptr;
    return &argv[0].toObject();
}

bool
CharCodeAt(JSContext *cx, HandleString str, int32_t index, uint32_t *code)
{
    jschar c;
    if (!str->getChar(cx, index, &c))
        return false;
    *code = c;
    return true;
}

JSFlatString *
StringFromCharCode(JSContext *cx, int32_t code)
{
    jschar c = jschar(code);

    if (StaticStrings::hasUnit(c))
        return cx->staticStrings().getUnit(c);

    return NewStringCopyN<CanGC>(cx, &c, 1);
}

bool
SetProperty(JSContext *cx, HandleObject obj, HandlePropertyName name, HandleValue value,
            bool strict, jsbytecode *pc)
{
    RootedValue v(cx, value);
    RootedId id(cx, NameToId(name));

    JSOp op = JSOp(*pc);

    if (op == JSOP_SETALIASEDVAR) {
        // Aliased var assigns ignore readonly attributes on the property, as
        // required for initializing 'const' closure variables.
        Shape *shape = obj->nativeLookup(cx, name);
        JS_ASSERT(shape && shape->hasSlot());
        obj->nativeSetSlotWithType(cx, shape, value);
        return true;
    }

    if (MOZ_LIKELY(!obj->getOps()->setProperty)) {
        return baseops::SetPropertyHelper<SequentialExecution>(
            cx, obj, obj, id,
            (op == JSOP_SETNAME || op == JSOP_SETGNAME)
            ? baseops::Unqualified
            : baseops::Qualified,
            &v,
            strict);
    }

    return JSObject::setGeneric(cx, obj, obj, id, &v, strict);
}

bool
InterruptCheck(JSContext *cx)
{
    gc::MaybeVerifyBarriers(cx);

    // Fix loop backedges so that they do not invoke the interrupt again.
    // No lock is held here and it's possible we could segv in the middle here
    // and end up with a state where some fraction of the backedges point to
    // the interrupt handler and some don't. This is ok since the interrupt
    // is definitely about to be handled; if there are still backedges
    // afterwards which point to the interrupt handler, the next time they are
    // taken the backedges will just be reset again.
    cx->runtime()->jitRuntime()->patchIonBackedges(cx->runtime(),
                                                   JitRuntime::BackedgeLoopHeader);

    return CheckForInterrupt(cx);
}

void *
MallocWrapper(JSRuntime *rt, size_t nbytes)
{
    return rt->pod_malloc<uint8_t>(nbytes);
}

JSObject *
NewCallObject(JSContext *cx, HandleShape shape, HandleTypeObject type)
{
    JSObject *obj = CallObject::create(cx, shape, type);
    if (!obj)
        return nullptr;

#ifdef JSGC_GENERATIONAL
    // The JIT creates call objects in the nursery, so elides barriers for
    // the initializing writes. The interpreter, however, may have allocated
    // the call object tenured, so barrier as needed before re-entering.
    if (!IsInsideNursery(obj))
        cx->runtime()->gc.storeBuffer.putWholeCellFromMainThread(obj);
#endif

    return obj;
}

JSObject *
NewSingletonCallObject(JSContext *cx, HandleShape shape)
{
    JSObject *obj = CallObject::createSingleton(cx, shape);
    if (!obj)
        return nullptr;

#ifdef JSGC_GENERATIONAL
    // The JIT creates call objects in the nursery, so elides barriers for
    // the initializing writes. The interpreter, however, may have allocated
    // the call object tenured, so barrier as needed before re-entering.
    MOZ_ASSERT(!IsInsideNursery(obj),
               "singletons are created in the tenured heap");
    cx->runtime()->gc.storeBuffer.putWholeCellFromMainThread(obj);
#endif

    return obj;
}

JSObject *
NewStringObject(JSContext *cx, HandleString str)
{
    return StringObject::create(cx, str);
}

bool
SPSEnter(JSContext *cx, HandleScript script)
{
    return cx->runtime()->spsProfiler.enter(script, script->functionNonDelazifying());
}

bool
SPSExit(JSContext *cx, HandleScript script)
{
    cx->runtime()->spsProfiler.exit(script, script->functionNonDelazifying());
    return true;
}

bool
OperatorIn(JSContext *cx, HandleValue key, HandleObject obj, bool *out)
{
    RootedId id(cx);
    if (!ValueToId<CanGC>(cx, key, &id))
        return false;

    RootedObject obj2(cx);
    RootedShape prop(cx);
    if (!JSObject::lookupGeneric(cx, obj, id, &obj2, &prop))
        return false;

    *out = !!prop;
    return true;
}

bool
OperatorInI(JSContext *cx, uint32_t index, HandleObject obj, bool *out)
{
    RootedValue key(cx, Int32Value(index));
    return OperatorIn(cx, key, obj, out);
}

bool
GetIntrinsicValue(JSContext *cx, HandlePropertyName name, MutableHandleValue rval)
{
    if (!GlobalObject::getIntrinsicValue(cx, cx->global(), name, rval))
        return false;

    // This function is called when we try to compile a cold getintrinsic
    // op. MCallGetIntrinsicValue has an AliasSet of None for optimization
    // purposes, as its side effect is not observable from JS. We are
    // guaranteed to bail out after this function, but because of its AliasSet,
    // type info will not be reflowed. Manually monitor here.
    types::TypeScript::Monitor(cx, rval);

    return true;
}

bool
CreateThis(JSContext *cx, HandleObject callee, MutableHandleValue rval)
{
    rval.set(MagicValue(JS_IS_CONSTRUCTING));

    if (callee->is<JSFunction>()) {
        JSFunction *fun = &callee->as<JSFunction>();
        if (fun->isInterpretedConstructor()) {
            JSScript *script = fun->getOrCreateScript(cx);
            if (!script || !script->ensureHasTypes(cx))
                return false;
            JSObject *thisObj = CreateThisForFunction(cx, callee, GenericObject);
            if (!thisObj)
                return false;
            rval.set(ObjectValue(*thisObj));
        }
    }

    return true;
}

void
GetDynamicName(JSContext *cx, JSObject *scopeChain, JSString *str, Value *vp)
{
    // Lookup a string on the scope chain, returning either the value found or
    // undefined through rval. This function is infallible, and cannot GC or
    // invalidate.

    JSAtom *atom;
    if (str->isAtom()) {
        atom = &str->asAtom();
    } else {
        atom = AtomizeString(cx, str);
        if (!atom) {
            vp->setUndefined();
            return;
        }
    }

    if (!frontend::IsIdentifier(atom) || frontend::IsKeyword(atom)) {
        vp->setUndefined();
        return;
    }

    Shape *shape = nullptr;
    JSObject *scope = nullptr, *pobj = nullptr;
    if (LookupNameNoGC(cx, atom->asPropertyName(), scopeChain, &scope, &pobj, &shape)) {
        if (FetchNameNoGC(pobj, shape, MutableHandleValue::fromMarkedLocation(vp)))
            return;
    }

    vp->setUndefined();
}

bool
FilterArgumentsOrEval(JSContext *cx, JSString *str)
{
    // ensureLinear() is fallible, but cannot GC: it can only allocate a
    // character buffer for the flattened string. If this call fails then the
    // calling Ion code will bailout, resume in Baseline and likely fail again
    // when trying to flatten the string and unwind the stack.
    JS::AutoCheckCannotGC nogc;
    JSLinearString *linear = str->ensureLinear(cx);
    if (!linear)
        return false;

    static const jschar arguments[] = {'a', 'r', 'g', 'u', 'm', 'e', 'n', 't', 's'};
    static const jschar eval[] = {'e', 'v', 'a', 'l'};

    return !StringHasPattern(linear, arguments, mozilla::ArrayLength(arguments)) &&
        !StringHasPattern(linear, eval, mozilla::ArrayLength(eval));
}

#ifdef JSGC_GENERATIONAL
void
PostWriteBarrier(JSRuntime *rt, JSObject *obj)
{
    JS_ASSERT(!IsInsideNursery(obj));
    rt->gc.storeBuffer.putWholeCellFromMainThread(obj);
}

void
PostGlobalWriteBarrier(JSRuntime *rt, JSObject *obj)
{
    JS_ASSERT(obj->is<GlobalObject>());
    if (!obj->compartment()->globalWriteBarriered) {
        PostWriteBarrier(rt, obj);
        obj->compartment()->globalWriteBarriered = true;
    }
}
#endif

uint32_t
GetIndexFromString(JSString *str)
{
    // Masks the return value UINT32_MAX as failure to get the index.
    // I.e. it is impossible to distinguish between failing to get the index
    // or the actual index UINT32_MAX.

    if (!str->isAtom())
        return UINT32_MAX;

    uint32_t index;
    JSAtom *atom = &str->asAtom();
    if (!atom->isIndex(&index))
        return UINT32_MAX;

    return index;
}

bool
DebugPrologue(JSContext *cx, BaselineFrame *frame, jsbytecode *pc, bool *mustReturn)
{
    *mustReturn = false;

    JSTrapStatus status = ScriptDebugPrologue(cx, frame, pc);
    switch (status) {
      case JSTRAP_CONTINUE:
        return true;

      case JSTRAP_RETURN:
        // The script is going to return immediately, so we have to call the
        // debug epilogue handler as well.
        JS_ASSERT(frame->hasReturnValue());
        *mustReturn = true;
        return jit::DebugEpilogue(cx, frame, pc, true);

      case JSTRAP_THROW:
      case JSTRAP_ERROR:
        return false;

      default:
        MOZ_ASSUME_UNREACHABLE("Invalid trap status");
    }
}

bool
DebugEpilogueOnBaselineReturn(JSContext *cx, BaselineFrame *frame, jsbytecode *pc)
{
    if (!DebugEpilogue(cx, frame, pc, true)) {
        // DebugEpilogue popped the frame by updating jitTop, so run the stop event
        // here before we enter the exception handler.
        TraceLogger *logger = TraceLoggerForMainThread(cx->runtime());
        TraceLogStopEvent(logger, TraceLogger::Baseline);
        TraceLogStopEvent(logger); // Leave script.
        return false;
    }

    return true;
}

bool
DebugEpilogue(JSContext *cx, BaselineFrame *frame, jsbytecode *pc, bool ok)
{
    // Unwind scope chain to stack depth 0.
    ScopeIter si(frame, pc, cx);
    jsbytecode *unwindPc = frame->script()->main();
    UnwindScope(cx, si, unwindPc);
    frame->setUnwoundScopeOverridePc(unwindPc);

    // If ScriptDebugEpilogue returns |true| we have to return the frame's
    // return value. If it returns |false|, the debugger threw an exception.
    // In both cases we have to pop debug scopes.
    ok = ScriptDebugEpilogue(cx, frame, pc, ok);

    if (frame->isNonEvalFunctionFrame()) {
        JS_ASSERT_IF(ok, frame->hasReturnValue());
        DebugScopes::onPopCall(frame, cx);
    } else if (frame->isStrictEvalFrame()) {
        JS_ASSERT_IF(frame->hasCallObj(), frame->scopeChain()->as<CallObject>().isForEval());
        DebugScopes::onPopStrictEvalScope(frame);
    }

    // If the frame has a pushed SPS frame, make sure to pop it.
    if (frame->hasPushedSPSFrame()) {
        cx->runtime()->spsProfiler.exit(frame->script(), frame->maybeFun());
        // Unset the pushedSPSFrame flag because DebugEpilogue may get called before
        // probes::ExitScript in baseline during exception handling, and we don't
        // want to double-pop SPS frames.
        frame->unsetPushedSPSFrame();
    }

    if (!ok) {
        // Pop this frame by updating jitTop, so that the exception handling
        // code will start at the previous frame.

        IonJSFrameLayout *prefix = frame->framePrefix();
        EnsureExitFrame(prefix);
        cx->mainThread().jitTop = (uint8_t *)prefix;
    }

    return ok;
}

bool
StrictEvalPrologue(JSContext *cx, BaselineFrame *frame)
{
    return frame->strictEvalPrologue(cx);
}

bool
HeavyweightFunPrologue(JSContext *cx, BaselineFrame *frame)
{
    return frame->heavyweightFunPrologue(cx);
}

bool
NewArgumentsObject(JSContext *cx, BaselineFrame *frame, MutableHandleValue res)
{
    ArgumentsObject *obj = ArgumentsObject::createExpected(cx, frame);
    if (!obj)
        return false;
    res.setObject(*obj);
    return true;
}

JSObject *
InitRestParameter(JSContext *cx, uint32_t length, Value *rest, HandleObject templateObj,
                  HandleObject objRes)
{
    if (objRes) {
        Rooted<ArrayObject*> arrRes(cx, &objRes->as<ArrayObject>());

        JS_ASSERT(!arrRes->getDenseInitializedLength());
        JS_ASSERT(arrRes->type() == templateObj->type());

        // Fast path: we managed to allocate the array inline; initialize the
        // slots.
        if (length > 0) {
            if (!arrRes->ensureElements(cx, length))
                return nullptr;
            arrRes->setDenseInitializedLength(length);
            arrRes->initDenseElements(0, rest, length);
            arrRes->setLengthInt32(length);
        }
        return arrRes;
    }

    NewObjectKind newKind = templateObj->type()->shouldPreTenure()
                            ? TenuredObject
                            : GenericObject;
    ArrayObject *arrRes = NewDenseCopiedArray(cx, length, rest, nullptr, newKind);
    if (arrRes)
        arrRes->setType(templateObj->type());
    return arrRes;
}

bool
HandleDebugTrap(JSContext *cx, BaselineFrame *frame, uint8_t *retAddr, bool *mustReturn)
{
    *mustReturn = false;

    RootedScript script(cx, frame->script());
    jsbytecode *pc = script->baselineScript()->icEntryFromReturnAddress(retAddr).pc(script);

    JS_ASSERT(cx->compartment()->debugMode());
    JS_ASSERT(script->stepModeEnabled() || script->hasBreakpointsAt(pc));

    RootedValue rval(cx);
    JSTrapStatus status = JSTRAP_CONTINUE;
    JSInterruptHook hook = cx->runtime()->debugHooks.interruptHook;

    if (hook || script->stepModeEnabled()) {
        if (hook)
            status = hook(cx, script, pc, rval.address(), cx->runtime()->debugHooks.interruptHookData);
        if (status == JSTRAP_CONTINUE && script->stepModeEnabled())
            status = Debugger::onSingleStep(cx, &rval);
    }

    if (status == JSTRAP_CONTINUE && script->hasBreakpointsAt(pc))
        status = Debugger::onTrap(cx, &rval);

    switch (status) {
      case JSTRAP_CONTINUE:
        break;

      case JSTRAP_ERROR:
        return false;

      case JSTRAP_RETURN:
        *mustReturn = true;
        frame->setReturnValue(rval);
        return jit::DebugEpilogue(cx, frame, pc, true);

      case JSTRAP_THROW:
        cx->setPendingException(rval);
        return false;

      default:
        MOZ_ASSUME_UNREACHABLE("Invalid trap status");
    }

    return true;
}

bool
OnDebuggerStatement(JSContext *cx, BaselineFrame *frame, jsbytecode *pc, bool *mustReturn)
{
    *mustReturn = false;

    RootedScript script(cx, frame->script());
    JSTrapStatus status = JSTRAP_CONTINUE;
    RootedValue rval(cx);

    if (JSDebuggerHandler handler = cx->runtime()->debugHooks.debuggerHandler)
        status = handler(cx, script, pc, rval.address(), cx->runtime()->debugHooks.debuggerHandlerData);

    if (status == JSTRAP_CONTINUE)
        status = Debugger::onDebuggerStatement(cx, &rval);

    switch (status) {
      case JSTRAP_ERROR:
        return false;

      case JSTRAP_CONTINUE:
        return true;

      case JSTRAP_RETURN:
        frame->setReturnValue(rval);
        *mustReturn = true;
        return jit::DebugEpilogue(cx, frame, pc, true);

      case JSTRAP_THROW:
        cx->setPendingException(rval);
        return false;

      default:
        MOZ_ASSUME_UNREACHABLE("Invalid trap status");
    }
}

bool
PushBlockScope(JSContext *cx, BaselineFrame *frame, Handle<StaticBlockObject *> block)
{
    return frame->pushBlock(cx, block);
}

bool
PopBlockScope(JSContext *cx, BaselineFrame *frame)
{
    frame->popBlock(cx);
    return true;
}

bool
DebugLeaveBlock(JSContext *cx, BaselineFrame *frame, jsbytecode *pc)
{
    JS_ASSERT(frame->script()->baselineScript()->debugMode());

    DebugScopes::onPopBlock(cx, frame, pc);

    return true;
}

bool
EnterWith(JSContext *cx, BaselineFrame *frame, HandleValue val, Handle<StaticWithObject *> templ)
{
    return EnterWithOperation(cx, frame, val, templ);
}

bool
LeaveWith(JSContext *cx, BaselineFrame *frame)
{
    frame->popWith(cx);
    return true;
}

bool
InitBaselineFrameForOsr(BaselineFrame *frame, InterpreterFrame *interpFrame,
                        uint32_t numStackValues)
{
    return frame->initForOsr(interpFrame, numStackValues);
}

JSObject *
CreateDerivedTypedObj(JSContext *cx, HandleObject descr,
                      HandleObject owner, int32_t offset)
{
    JS_ASSERT(descr->is<SizedTypeDescr>());
    JS_ASSERT(owner->is<TypedObject>());
    Rooted<SizedTypeDescr*> descr1(cx, &descr->as<SizedTypeDescr>());
    Rooted<TypedObject*> owner1(cx, &owner->as<TypedObject>());
    return TypedObject::createDerived(cx, descr1, owner1, offset);
}

JSString *
RegExpReplace(JSContext *cx, HandleString string, HandleObject regexp, HandleString repl)
{
    JS_ASSERT(string);
    JS_ASSERT(repl);

    RootedValue rval(cx);
    if (!str_replace_regexp_raw(cx, string, regexp, repl, &rval))
        return nullptr;

    return rval.toString();
}

JSString *
StringReplace(JSContext *cx, HandleString string, HandleString pattern, HandleString repl)
{
    JS_ASSERT(string);
    JS_ASSERT(pattern);
    JS_ASSERT(repl);

    RootedValue rval(cx);
    if (!str_replace_string_raw(cx, string, pattern, repl, &rval))
        return nullptr;

    return rval.toString();
}

bool
Recompile(JSContext *cx)
{
    JS_ASSERT(cx->currentlyRunningInJit());
    JitActivationIterator activations(cx->runtime());
    JitFrameIterator iter(activations);

    JS_ASSERT(iter.type() == JitFrame_Exit);
    ++iter;

    bool isConstructing = iter.isConstructing();
    RootedScript script(cx, iter.script());
    JS_ASSERT(script->hasIonScript());

    if (!IsIonEnabled(cx))
        return true;

    MethodStatus status = Recompile(cx, script, nullptr, nullptr, isConstructing);
    if (status == Method_Error)
        return false;

    return true;
}

bool
SetDenseElement(JSContext *cx, HandleObject obj, int32_t index, HandleValue value,
                bool strict)
{
    // This function is called from Ion code for StoreElementHole's OOL path.
    // In this case we know the object is native, has no indexed properties
    // and we can use setDenseElement instead of setDenseElementWithType.

    MOZ_ASSERT(obj->isNative());
    MOZ_ASSERT(!obj->isIndexed());

    JSObject::EnsureDenseResult result = JSObject::ED_SPARSE;
    do {
        if (index < 0)
            break;
        bool isArray = obj->is<ArrayObject>();
        if (isArray && !obj->as<ArrayObject>().lengthIsWritable())
            break;
        uint32_t idx = uint32_t(index);
        result = obj->ensureDenseElements(cx, idx, 1);
        if (result != JSObject::ED_OK)
            break;
        if (isArray) {
            ArrayObject &arr = obj->as<ArrayObject>();
            if (idx >= arr.length())
                arr.setLengthInt32(idx + 1);
        }
        obj->setDenseElement(idx, value);
        return true;
    } while (false);

    if (result == JSObject::ED_FAILED)
        return false;
    MOZ_ASSERT(result == JSObject::ED_SPARSE);

    RootedValue indexVal(cx, Int32Value(index));
    return SetObjectElement(cx, obj, indexVal, value, strict);
}

void
AutoDetectInvalidation::setReturnOverride()
{
    cx_->runtime()->jitRuntime()->setIonReturnOverride(*rval_);
}

#ifdef DEBUG
void
AssertValidObjectPtr(JSContext *cx, JSObject *obj)
{
    // Check what we can, so that we'll hopefully assert/crash if we get a
    // bogus object (pointer).
    JS_ASSERT(obj->compartment() == cx->compartment());
    JS_ASSERT(obj->runtimeFromMainThread() == cx->runtime());

    JS_ASSERT_IF(!obj->hasLazyType(),
                 obj->type()->clasp() == obj->lastProperty()->getObjectClass());

    if (obj->isTenured()) {
        JS_ASSERT(obj->isAligned());
        gc::AllocKind kind = obj->tenuredGetAllocKind();
        JS_ASSERT(kind >= js::gc::FINALIZE_OBJECT0 && kind <= js::gc::FINALIZE_OBJECT_LAST);
        JS_ASSERT(obj->tenuredZone() == cx->zone());
    }
}

void
AssertValidStringPtr(JSContext *cx, JSString *str)
{
    // We can't closely inspect strings from another runtime.
    if (str->runtimeFromAnyThread() != cx->runtime()) {
        JS_ASSERT(str->isPermanentAtom());
        return;
    }

    if (str->isAtom())
        JS_ASSERT(cx->runtime()->isAtomsZone(str->tenuredZone()));
    else
        JS_ASSERT(str->tenuredZone() == cx->zone());

    JS_ASSERT(str->runtimeFromMainThread() == cx->runtime());
    JS_ASSERT(str->isAligned());
    JS_ASSERT(str->length() <= JSString::MAX_LENGTH);

    gc::AllocKind kind = str->tenuredGetAllocKind();
    if (str->isFatInline())
        JS_ASSERT(kind == gc::FINALIZE_FAT_INLINE_STRING);
    else if (str->isExternal())
        JS_ASSERT(kind == gc::FINALIZE_EXTERNAL_STRING);
    else if (str->isAtom() || str->isFlat())
        JS_ASSERT(kind == gc::FINALIZE_STRING || kind == gc::FINALIZE_FAT_INLINE_STRING);
    else
        JS_ASSERT(kind == gc::FINALIZE_STRING);
}

void
AssertValidSymbolPtr(JSContext *cx, JS::Symbol *sym)
{
    // We can't closely inspect symbols from another runtime.
    if (sym->runtimeFromAnyThread() != cx->runtime())
        return;

    JS_ASSERT(cx->runtime()->isAtomsZone(sym->tenuredZone()));

    JS_ASSERT(sym->runtimeFromMainThread() == cx->runtime());
    JS_ASSERT(sym->isAligned());
    if (JSString *desc = sym->description()) {
        JS_ASSERT(desc->isAtom());
        AssertValidStringPtr(cx, desc);
    }

    JS_ASSERT(sym->tenuredGetAllocKind() == gc::FINALIZE_SYMBOL);
}

void
AssertValidValue(JSContext *cx, Value *v)
{
    if (v->isObject())
        AssertValidObjectPtr(cx, &v->toObject());
    else if (v->isString())
        AssertValidStringPtr(cx, v->toString());
    else if (v->isSymbol())
        AssertValidSymbolPtr(cx, v->toSymbol());
}
#endif

// Definition of the MTypedObjectProto MIR.
JSObject *
TypedObjectProto(JSObject *obj)
{
    JS_ASSERT(obj->is<TypedObject>());
    TypedObject &typedObj = obj->as<TypedObject>();
    return &typedObj.typedProto();
}

} // namespace jit
} // namespace js
