/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/VMFunctions.h"

#include "builtin/ParallelArray.h"
#include "builtin/TypedObject.h"
#include "frontend/BytecodeCompiler.h"
#include "jit/BaselineIC.h"
#include "jit/IonFrames.h"
#include "jit/JitCompartment.h"
#include "vm/ArrayObject.h"
#include "vm/Debugger.h"
#include "vm/Interpreter.h"

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
            if (fun->nonLazyScript()->shouldCloneAtCallsite) {
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
NewGCThing(JSContext *cx, gc::AllocKind allocKind, size_t thingSize, gc::InitialHeap initialHeap)
{
    return gc::NewGCThing<JSObject, CanGC>(cx, allocKind, thingSize, initialHeap);
}

bool
CheckOverRecursed(JSContext *cx)
{
    // IonMonkey's stackLimit is equal to nativeStackLimit by default. When we
    // want to trigger an operation callback, we set the ionStackLimit to nullptr,
    // which causes the stack limit check to fail.
    //
    // There are two states we're concerned about here:
    //   (1) The interrupt bit is set, and we need to fire the interrupt callback.
    //   (2) The stack limit has been exceeded, and we need to throw an error.
    //
    // Note that we can reach here if ionStackLimit is MAXADDR, but interrupt
    // has not yet been set to 1. That's okay; it will be set to 1 very shortly,
    // and in the interim we might just fire a few useless calls to
    // CheckOverRecursed.
    JS_CHECK_RECURSION(cx, return false);

    if (cx->runtime()->interrupt)
        return InterruptCheck(cx);

    return true;
}

bool
CheckOverRecursedWithExtra(JSContext *cx, uint32_t extra)
{
    // See |CheckOverRecursed| above.  This is a variant of that function which
    // accepts an argument holding the extra stack space needed for the Baseline
    // frame that's about to be pushed.
    uint8_t spDummy;
    uint8_t *checkSp = (&spDummy) - extra;
    JS_CHECK_RECURSION_WITH_SP(cx, checkSp, return false);

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
InitProp(JSContext *cx, HandleObject obj, HandlePropertyName name, HandleValue value)
{
    // Copy the incoming value. This may be overwritten; the return value is discarded.
    RootedValue rval(cx, value);
    RootedId id(cx, NameToId(name));

    if (name == cx->names().proto)
        return baseops::SetPropertyHelper<SequentialExecution>(cx, obj, obj, id, 0, &rval, false);
    return DefineNativeProperty(cx, obj, id, rval, nullptr, nullptr, JSPROP_ENUMERATE, 0, 0, 0);
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

JSObject *
NewInitParallelArray(JSContext *cx, HandleObject templateObject)
{
    JS_ASSERT(templateObject->getClass() == &ParallelArrayObject::class_);
    JS_ASSERT(!templateObject->hasSingletonType());

    RootedObject obj(cx, ParallelArrayObject::newInstance(cx, TenuredObject));
    if (!obj)
        return nullptr;

    obj->setType(templateObject->type());

    return obj;
}

JSObject*
NewInitArray(JSContext *cx, uint32_t count, types::TypeObject *typeArg)
{
    RootedTypeObject type(cx, typeArg);
    NewObjectKind newKind = !type ? SingletonObject : GenericObject;
    if (type && type->isLongLivedForJITAlloc())
        newKind = TenuredObject;
    RootedObject obj(cx, NewDenseAllocatedArray(cx, count, nullptr, newKind));
    if (!obj)
        return nullptr;

    if (!type)
        types::TypeScript::Monitor(cx, ObjectValue(*obj));
    else
        obj->setType(type);

    return obj;
}

JSObject*
NewInitObject(JSContext *cx, HandleObject templateObject)
{
    NewObjectKind newKind = templateObject->hasSingletonType() ? SingletonObject : GenericObject;
    if (!templateObject->hasLazyType() && templateObject->type()->isLongLivedForJITAlloc())
        newKind = TenuredObject;
    RootedObject obj(cx, CopyInitializerObject(cx, templateObject, newKind));

    if (!obj)
        return nullptr;

    if (templateObject->hasSingletonType())
        types::TypeScript::Monitor(cx, ObjectValue(*obj));
    else
        obj->setType(templateObject->type());

    return obj;
}

JSObject *
NewInitObjectWithClassPrototype(JSContext *cx, HandleObject templateObject)
{
    JS_ASSERT(!templateObject->hasSingletonType());
    JS_ASSERT(!templateObject->hasLazyType());

    NewObjectKind newKind = templateObject->type()->isLongLivedForJITAlloc()
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
ArrayPopDense(JSContext *cx, HandleObject obj, MutableHandleValue rval)
{
    JS_ASSERT(obj->is<ArrayObject>());

    AutoDetectInvalidation adi(cx, rval.address());

    Value argv[] = { UndefinedValue(), ObjectValue(*obj) };
    AutoValueArray ava(cx, argv, 2);
    if (!js::array_pop(cx, 0, argv))
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

    Value argv[] = { UndefinedValue(), ObjectValue(*obj), v };
    AutoValueArray ava(cx, argv, 3);
    if (!js::array_push(cx, 1, argv))
        return false;

    *length = argv[0].toInt32();
    return true;
}

bool
ArrayShiftDense(JSContext *cx, HandleObject obj, MutableHandleValue rval)
{
    JS_ASSERT(obj->is<ArrayObject>());

    AutoDetectInvalidation adi(cx, rval.address());

    Value argv[] = { UndefinedValue(), ObjectValue(*obj) };
    AutoValueArray ava(cx, argv, 2);
    if (!js::array_shift(cx, 0, argv))
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

    Value argv[] = { UndefinedValue(), ObjectValue(*arr1), ObjectValue(*arr2) };
    AutoValueArray ava(cx, argv, 3);
    if (!js::array_concat(cx, 1, argv))
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
        return cx->runtime()->staticStrings.getUnit(c);

    return js_NewStringCopyN<CanGC>(cx, &c, 1);
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
        JSObject::nativeSetSlotWithType(cx, obj, shape, value);
        return true;
    }

    if (JS_LIKELY(!obj->getOps()->setProperty)) {
        unsigned defineHow = (op == JSOP_SETNAME || op == JSOP_SETGNAME) ? DNP_UNQUALIFIED : 0;
        return baseops::SetPropertyHelper<SequentialExecution>(cx, obj, obj, id, defineHow, &v,
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

    return !!js_HandleExecutionInterrupt(cx);
}

HeapSlot *
NewSlots(JSRuntime *rt, unsigned nslots)
{
    JS_STATIC_ASSERT(sizeof(Value) == sizeof(HeapSlot));

    Value *slots = reinterpret_cast<Value *>(rt->malloc_(nslots * sizeof(Value)));
    if (!slots)
        return nullptr;

    for (unsigned i = 0; i < nslots; i++)
        slots[i] = UndefinedValue();

    return reinterpret_cast<HeapSlot *>(slots);
}

JSObject *
NewCallObject(JSContext *cx, HandleScript script,
              HandleShape shape, HandleTypeObject type, HeapSlot *slots)
{
    JSObject *obj = CallObject::create(cx, script, shape, type, slots);
    if (!obj)
        return nullptr;

#ifdef JSGC_GENERATIONAL
    // The JIT creates call objects in the nursery, so elides barriers for
    // the initializing writes. The interpreter, however, may have allocated
    // the call object tenured, so barrier as needed before re-entering.
    if (!IsInsideNursery(cx->runtime(), obj))
        cx->runtime()->gcStoreBuffer.putWholeCell(obj);
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
    return cx->runtime()->spsProfiler.enter(cx, script, script->function());
}

bool
SPSExit(JSContext *cx, HandleScript script)
{
    cx->runtime()->spsProfiler.exit(cx, script, script->function());
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
    if (!cx->global()->getIntrinsicValue(cx, name, rval))
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
        atom = AtomizeString<NoGC>(cx, str);
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
    // getChars() is fallible, but cannot GC: it can only allocate a character
    // for the flattened string. If this call fails then the calling Ion code
    // will bailout, resume in the interpreter and likely fail again when
    // trying to flatten the string and unwind the stack.
    const jschar *chars = str->getChars(cx);
    if (!chars)
        return false;

    static const jschar arguments[] = {'a', 'r', 'g', 'u', 'm', 'e', 'n', 't', 's'};
    static const jschar eval[] = {'e', 'v', 'a', 'l'};

    return !StringHasPattern(chars, str->length(), arguments, mozilla::ArrayLength(arguments)) &&
        !StringHasPattern(chars, str->length(), eval, mozilla::ArrayLength(eval));
}

#ifdef JSGC_GENERATIONAL
void
PostWriteBarrier(JSRuntime *rt, JSObject *obj)
{
    JS_ASSERT(!IsInsideNursery(rt, obj));
    rt->gcStoreBuffer.putWholeCell(obj);
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
DebugPrologue(JSContext *cx, BaselineFrame *frame, bool *mustReturn)
{
    *mustReturn = false;

    JSTrapStatus status = ScriptDebugPrologue(cx, frame);
    switch (status) {
      case JSTRAP_CONTINUE:
        return true;

      case JSTRAP_RETURN:
        // The script is going to return immediately, so we have to call the
        // debug epilogue handler as well.
        JS_ASSERT(frame->hasReturnValue());
        *mustReturn = true;
        return jit::DebugEpilogue(cx, frame, true);

      case JSTRAP_THROW:
      case JSTRAP_ERROR:
        return false;

      default:
        MOZ_ASSUME_UNREACHABLE("Invalid trap status");
    }
}

bool
DebugEpilogue(JSContext *cx, BaselineFrame *frame, bool ok)
{
    // Unwind scope chain to stack depth 0.
    UnwindScope(cx, frame, 0);

    // If ScriptDebugEpilogue returns |true| we have to return the frame's
    // return value. If it returns |false|, the debugger threw an exception.
    // In both cases we have to pop debug scopes.
    ok = ScriptDebugEpilogue(cx, frame, ok);

    if (frame->isNonEvalFunctionFrame()) {
        JS_ASSERT_IF(ok, frame->hasReturnValue());
        DebugScopes::onPopCall(frame, cx);
    } else if (frame->isStrictEvalFrame()) {
        JS_ASSERT_IF(frame->hasCallObj(), frame->scopeChain()->as<CallObject>().isForEval());
        DebugScopes::onPopStrictEvalScope(frame);
    }

    // If the frame has a pushed SPS frame, make sure to pop it.
    if (frame->hasPushedSPSFrame()) {
        cx->runtime()->spsProfiler.exit(cx, frame->script(), frame->maybeFun());
        // Unset the pushedSPSFrame flag because DebugEpilogue may get called before
        // probes::ExitScript in baseline during exception handling, and we don't
        // want to double-pop SPS frames.
        frame->unsetPushedSPSFrame();
    }

    if (!ok) {
        // Pop this frame by updating ionTop, so that the exception handling
        // code will start at the previous frame.

        IonJSFrameLayout *prefix = frame->framePrefix();
        EnsureExitFrame(prefix);
        cx->mainThread().ionTop = (uint8_t *)prefix;
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

    NewObjectKind newKind = templateObj->type()->isLongLivedForJITAlloc()
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
        return jit::DebugEpilogue(cx, frame, true);

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
        return jit::DebugEpilogue(cx, frame, true);

      case JSTRAP_THROW:
        cx->setPendingException(rval);
        return false;

      default:
        MOZ_ASSUME_UNREACHABLE("Invalid trap status");
    }
}

bool
EnterBlock(JSContext *cx, BaselineFrame *frame, Handle<StaticBlockObject *> block)
{
    return frame->pushBlock(cx, block);
}

bool
LeaveBlock(JSContext *cx, BaselineFrame *frame)
{
    frame->popBlock(cx);
    return true;
}

bool
InitBaselineFrameForOsr(BaselineFrame *frame, StackFrame *interpFrame, uint32_t numStackValues)
{
    return frame->initForOsr(interpFrame, numStackValues);
}

JSObject *CreateDerivedTypedObj(JSContext *cx, HandleObject type,
                                HandleObject owner, int32_t offset)
{
    return TypedObject::createDerived(cx, type, owner, offset);
}


} // namespace jit
} // namespace js
