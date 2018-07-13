/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "builtin/Promise.h"

#include "mozilla/Atomics.h"
#include "mozilla/Maybe.h"
#include "mozilla/TimeStamp.h"

#include "jsexn.h"
#include "jsfriendapi.h"

#include "gc/Heap.h"
#include "js/Debug.h"
#include "vm/AsyncFunction.h"
#include "vm/AsyncIteration.h"
#include "vm/Debugger.h"
#include "vm/Iteration.h"
#include "vm/JSContext.h"

#include "vm/Compartment-inl.h"
#include "vm/Debugger-inl.h"
#include "vm/JSObject-inl.h"
#include "vm/NativeObject-inl.h"

using namespace js;

static double
MillisecondsSinceStartup()
{
    auto now = mozilla::TimeStamp::Now();
    return (now - mozilla::TimeStamp::ProcessCreation()).ToMilliseconds();
}

enum PromiseHandler {
    PromiseHandlerIdentity = 0,
    PromiseHandlerThrower,

    // ES 2018 draft 25.5.5.4-5.
    PromiseHandlerAsyncFunctionAwaitedFulfilled,
    PromiseHandlerAsyncFunctionAwaitedRejected,

    // Async Iteration proposal 4.1.
    PromiseHandlerAsyncGeneratorAwaitedFulfilled,
    PromiseHandlerAsyncGeneratorAwaitedRejected,

    // Async Iteration proposal 11.4.3.5.1-2.
    PromiseHandlerAsyncGeneratorResumeNextReturnFulfilled,
    PromiseHandlerAsyncGeneratorResumeNextReturnRejected,

    // Async Iteration proposal 11.4.3.7 steps 8.c-e.
    PromiseHandlerAsyncGeneratorYieldReturnAwaitedFulfilled,
    PromiseHandlerAsyncGeneratorYieldReturnAwaitedRejected,

    // Async Iteration proposal 11.1.3.2.5.
    // Async-from-Sync iterator handlers take the resolved value and create new
    // iterator objects.  To do so it needs to forward whether the iterator is
    // done. In spec, this is achieved via the [[Done]] internal slot. We
    // enumerate both true and false cases here.
    PromiseHandlerAsyncFromSyncIteratorValueUnwrapDone,
    PromiseHandlerAsyncFromSyncIteratorValueUnwrapNotDone,

    // One past the maximum allowed PromiseHandler value.
    PromiseHandlerLimit
};

enum ResolutionMode {
    ResolveMode,
    RejectMode
};

enum ResolveFunctionSlots {
    ResolveFunctionSlot_Promise = 0,
    ResolveFunctionSlot_RejectFunction,
};

enum RejectFunctionSlots {
    RejectFunctionSlot_Promise = 0,
    RejectFunctionSlot_ResolveFunction,
};

enum PromiseAllResolveElementFunctionSlots {
    PromiseAllResolveElementFunctionSlot_Data = 0,
    PromiseAllResolveElementFunctionSlot_ElementIndex,
};

enum ReactionJobSlots {
    ReactionJobSlot_ReactionRecord = 0,
};

enum ThenableJobSlots {
    // The handler to use as the Promise reaction. It is a callable object
    // that's guaranteed to be from the same compartment as the
    // PromiseReactionJob.
    ThenableJobSlot_Handler = 0,

    // JobData - a, potentially CCW-wrapped, dense list containing data
    // required for proper execution of the reaction.
    ThenableJobSlot_JobData,
};

enum ThenableJobDataIndices {
    // The Promise to resolve using the given thenable.
    ThenableJobDataIndex_Promise = 0,

    // The thenable to use as the receiver when calling the `then` function.
    ThenableJobDataIndex_Thenable,

    ThenableJobDataLength,
};

enum BuiltinThenableJobSlots {
    // The Promise to resolve using the given thenable.
    BuiltinThenableJobSlot_Promise = 0,

    // The thenable to use as the receiver when calling the built-in `then`
    // function.
    BuiltinThenableJobSlot_Thenable,
};

enum PromiseAllDataHolderSlots {
    PromiseAllDataHolderSlot_Promise = 0,
    PromiseAllDataHolderSlot_RemainingElements,
    PromiseAllDataHolderSlot_ValuesArray,
    PromiseAllDataHolderSlot_ResolveFunction,
    PromiseAllDataHolderSlots,
};

class PromiseAllDataHolder : public NativeObject
{
  public:
    static const Class class_;
    JSObject* promiseObj() { return &getFixedSlot(PromiseAllDataHolderSlot_Promise).toObject(); }
    JSObject* resolveObj() {
        return &getFixedSlot(PromiseAllDataHolderSlot_ResolveFunction).toObject();
    }
    Value valuesArray() { return getFixedSlot(PromiseAllDataHolderSlot_ValuesArray); }
    int32_t remainingCount() {
        return getFixedSlot(PromiseAllDataHolderSlot_RemainingElements).toInt32();
    }
    int32_t increaseRemainingCount() {
        int32_t remainingCount = getFixedSlot(PromiseAllDataHolderSlot_RemainingElements).toInt32();
        remainingCount++;
        setFixedSlot(PromiseAllDataHolderSlot_RemainingElements, Int32Value(remainingCount));
        return remainingCount;
    }
    int32_t decreaseRemainingCount() {
        int32_t remainingCount = getFixedSlot(PromiseAllDataHolderSlot_RemainingElements).toInt32();
        remainingCount--;
        setFixedSlot(PromiseAllDataHolderSlot_RemainingElements, Int32Value(remainingCount));
        return remainingCount;
    }
};

const Class PromiseAllDataHolder::class_ = {
    "PromiseAllDataHolder",
    JSCLASS_HAS_RESERVED_SLOTS(PromiseAllDataHolderSlots)
};

static PromiseAllDataHolder*
NewPromiseAllDataHolder(JSContext* cx, HandleObject resultPromise, HandleValue valuesArray,
                        HandleObject resolve)
{
    Rooted<PromiseAllDataHolder*> dataHolder(cx, NewObjectWithClassProto<PromiseAllDataHolder>(cx));
    if (!dataHolder)
        return nullptr;

    assertSameCompartment(cx, resultPromise);
    assertSameCompartment(cx, valuesArray);
    assertSameCompartment(cx, resolve);

    dataHolder->setFixedSlot(PromiseAllDataHolderSlot_Promise, ObjectValue(*resultPromise));
    dataHolder->setFixedSlot(PromiseAllDataHolderSlot_RemainingElements, Int32Value(1));
    dataHolder->setFixedSlot(PromiseAllDataHolderSlot_ValuesArray, valuesArray);
    dataHolder->setFixedSlot(PromiseAllDataHolderSlot_ResolveFunction, ObjectValue(*resolve));
    return dataHolder;
}

namespace {
// Generator used by PromiseObject::getID.
mozilla::Atomic<uint64_t> gIDGenerator(0);
} // namespace

static MOZ_ALWAYS_INLINE bool
ShouldCaptureDebugInfo(JSContext* cx)
{
    return cx->options().asyncStack() || cx->realm()->isDebuggee();
}

class PromiseDebugInfo : public NativeObject
{
  private:
    enum Slots {
        Slot_AllocationSite,
        Slot_ResolutionSite,
        Slot_AllocationTime,
        Slot_ResolutionTime,
        Slot_Id,
        SlotCount
    };

  public:
    static const Class class_;
    static PromiseDebugInfo* create(JSContext* cx, Handle<PromiseObject*> promise) {
        Rooted<PromiseDebugInfo*> debugInfo(cx, NewObjectWithClassProto<PromiseDebugInfo>(cx));
        if (!debugInfo)
            return nullptr;

        RootedObject stack(cx);
        if (!JS::CaptureCurrentStack(cx, &stack, JS::StackCapture(JS::AllFrames())))
            return nullptr;
        debugInfo->setFixedSlot(Slot_AllocationSite, ObjectOrNullValue(stack));
        debugInfo->setFixedSlot(Slot_ResolutionSite, NullValue());
        debugInfo->setFixedSlot(Slot_AllocationTime, DoubleValue(MillisecondsSinceStartup()));
        debugInfo->setFixedSlot(Slot_ResolutionTime, NumberValue(0));
        promise->setFixedSlot(PromiseSlot_DebugInfo, ObjectValue(*debugInfo));

        return debugInfo;
    }

    static PromiseDebugInfo* FromPromise(PromiseObject* promise) {
        Value val = promise->getFixedSlot(PromiseSlot_DebugInfo);
        if (val.isObject())
            return &val.toObject().as<PromiseDebugInfo>();
        return nullptr;
    }

    /**
     * Returns the given PromiseObject's process-unique ID.
     * The ID is lazily assigned when first queried, and then either stored
     * in the DebugInfo slot if no debug info was recorded for this Promise,
     * or in the Id slot of the DebugInfo object.
     */
    static uint64_t id(PromiseObject* promise) {
        Value idVal(promise->getFixedSlot(PromiseSlot_DebugInfo));
        if (idVal.isUndefined()) {
            idVal.setDouble(++gIDGenerator);
            promise->setFixedSlot(PromiseSlot_DebugInfo, idVal);
        } else if (idVal.isObject()) {
            PromiseDebugInfo* debugInfo = FromPromise(promise);
            idVal = debugInfo->getFixedSlot(Slot_Id);
            if (idVal.isUndefined()) {
                idVal.setDouble(++gIDGenerator);
                debugInfo->setFixedSlot(Slot_Id, idVal);
            }
        }
        return uint64_t(idVal.toNumber());
    }

    double allocationTime() { return getFixedSlot(Slot_AllocationTime).toNumber(); }
    double resolutionTime() { return getFixedSlot(Slot_ResolutionTime).toNumber(); }
    JSObject* allocationSite() { return getFixedSlot(Slot_AllocationSite).toObjectOrNull(); }
    JSObject* resolutionSite() { return getFixedSlot(Slot_ResolutionSite).toObjectOrNull(); }

    static void setResolutionInfo(JSContext* cx, Handle<PromiseObject*> promise) {
        if (!ShouldCaptureDebugInfo(cx))
            return;

        // If async stacks weren't enabled and the Promise's global wasn't a
        // debuggee when the Promise was created, we won't have a debugInfo
        // object. We still want to capture the resolution stack, so we
        // create the object now and change it's slots' values around a bit.
        Rooted<PromiseDebugInfo*> debugInfo(cx, FromPromise(promise));
        if (!debugInfo) {
            RootedValue idVal(cx, promise->getFixedSlot(PromiseSlot_DebugInfo));
            debugInfo = create(cx, promise);
            if (!debugInfo) {
                cx->clearPendingException();
                return;
            }

            // The current stack was stored in the AllocationSite slot, move
            // it to ResolutionSite as that's what it really is.
            debugInfo->setFixedSlot(Slot_ResolutionSite,
                                    debugInfo->getFixedSlot(Slot_AllocationSite));
            debugInfo->setFixedSlot(Slot_AllocationSite, NullValue());

            // There's no good default for a missing AllocationTime, so
            // instead of resetting that, ensure that it's the same as
            // ResolutionTime, so that the diff shows as 0, which isn't great,
            // but bearable.
            debugInfo->setFixedSlot(Slot_ResolutionTime,
                                    debugInfo->getFixedSlot(Slot_AllocationTime));

            // The Promise's ID might've been queried earlier, in which case
            // it's stored in the DebugInfo slot. We saved that earlier, so
            // now we can store it in the right place (or leave it as
            // undefined if it wasn't ever initialized.)
            debugInfo->setFixedSlot(Slot_Id, idVal);
            return;
        }

        RootedObject stack(cx);
        if (!JS::CaptureCurrentStack(cx, &stack, JS::StackCapture(JS::AllFrames()))) {
            cx->clearPendingException();
            return;
        }

        debugInfo->setFixedSlot(Slot_ResolutionSite, ObjectOrNullValue(stack));
        debugInfo->setFixedSlot(Slot_ResolutionTime, DoubleValue(MillisecondsSinceStartup()));
    }
};

const Class PromiseDebugInfo::class_ = {
    "PromiseDebugInfo",
    JSCLASS_HAS_RESERVED_SLOTS(SlotCount)
};

double
PromiseObject::allocationTime()
{
    auto debugInfo = PromiseDebugInfo::FromPromise(this);
    if (debugInfo)
        return debugInfo->allocationTime();
    return 0;
}

double
PromiseObject::resolutionTime()
{
    auto debugInfo = PromiseDebugInfo::FromPromise(this);
    if (debugInfo)
        return debugInfo->resolutionTime();
    return 0;
}

JSObject*
PromiseObject::allocationSite()
{
    auto debugInfo = PromiseDebugInfo::FromPromise(this);
    if (debugInfo)
        return debugInfo->allocationSite();
    return nullptr;
}

JSObject*
PromiseObject::resolutionSite()
{
    auto debugInfo = PromiseDebugInfo::FromPromise(this);
    if (debugInfo)
        return debugInfo->resolutionSite();
    return nullptr;
}

/**
 * Wrapper for GetAndClearException that handles cases where no exception is
 * pending, but an error occurred. This can be the case if an OOM was
 * encountered while throwing the error.
 */
static bool
MaybeGetAndClearException(JSContext* cx, MutableHandleValue rval)
{
    if (!cx->isExceptionPending())
        return false;

    return GetAndClearException(cx, rval);
}

static MOZ_MUST_USE bool RunResolutionFunction(JSContext *cx, HandleObject resolutionFun,
                                               HandleValue result, ResolutionMode mode,
                                               HandleObject promiseObj);

// ES2016, 25.4.1.1.1, Steps 1.a-b.
// Extracting all of this internal spec algorithm into a helper function would
// be tedious, so the check in step 1 and the entirety of step 2 aren't
// included.
static bool
AbruptRejectPromise(JSContext *cx, CallArgs& args, HandleObject promiseObj, HandleObject reject)
{
    // Step 1.a.
    RootedValue reason(cx);
    if (!MaybeGetAndClearException(cx, &reason))
        return false;

    if (!RunResolutionFunction(cx, reject, reason, RejectMode, promiseObj))
        return false;

    // Step 1.b.
    args.rval().setObject(*promiseObj);
    return true;
}

enum ReactionRecordSlots {
    // The promise for which this record provides a reaction handler.
    // Matches the [[Capability]].[[Promise]] field from the spec.
    //
    // The slot value is either an object, but not necessarily a built-in
    // Promise object, or null. The latter case is only possible for async
    // generator functions, in which case the REACTION_FLAG_ASYNC_GENERATOR
    // flag must be set.
    ReactionRecordSlot_Promise = 0,

    // The [[Handler]] field(s) of a PromiseReaction record. We create a
    // single reaction record for fulfillment and rejection, therefore our
    // PromiseReaction implementation needs two [[Handler]] fields.
    //
    // The slot value is either a callable object, an integer constant from
    // the |PromiseHandler| enum, or null. If the value is null, either the
    // REACTION_FLAG_DEBUGGER_DUMMY or the
    // REACTION_FLAG_DEFAULT_RESOLVING_HANDLER flag must be set.
    ReactionRecordSlot_OnFulfilled,
    ReactionRecordSlot_OnRejected,

    // The functions to resolve or reject the promise. Matches the
    // [[Capability]].[[Resolve]] and [[Capability]].[[Reject]] fields from
    // the spec.
    //
    // The slot values are either callable objects or null, but the latter
    // case is only allowed if the promise is either a built-in Promise object
    // or null.
    ReactionRecordSlot_Resolve,
    ReactionRecordSlot_Reject,

    // The incumbent global for this reaction record. Can be null.
    ReactionRecordSlot_IncumbentGlobalObject,

    // Bitmask of the REACTION_FLAG values.
    ReactionRecordSlot_Flags,

    // Argument when calling the function stored in OnFulfilled or OnRejected.
    ReactionRecordSlot_HandlerArg,

    // Additional slot to store extra data for specific reaction record types.
    //
    // - When the REACTION_FLAG_ASYNC_GENERATOR flag is set, this slot store
    //   the async generator function for this promise reaction.
    // - When the REACTION_FLAG_DEFAULT_RESOLVING_HANDLER flag is set, this
    //   slot stores the promise to resolve when conceptually "calling" the
    //   OnFulfilled or OnRejected handlers.
    ReactionRecordSlot_GeneratorOrPromiseToResolve,

    ReactionRecordSlots,
};

// ES2016, 25.4.1.2.
class PromiseReactionRecord : public NativeObject
{
  static constexpr size_t REACTION_FLAG_RESOLVED = 0x1;
  static constexpr size_t REACTION_FLAG_FULFILLED = 0x2;
  static constexpr size_t REACTION_FLAG_DEFAULT_RESOLVING_HANDLER = 0x4;
  static constexpr size_t REACTION_FLAG_ASYNC_FUNCTION = 0x8;
  static constexpr size_t REACTION_FLAG_ASYNC_GENERATOR = 0x10;
  static constexpr size_t REACTION_FLAG_DEBUGGER_DUMMY = 0x20;

  void setFlagOnInitialState(size_t flag) {
      int32_t flags = this->flags();
      MOZ_ASSERT(flags == 0, "Can't modify with non-default flags");
      flags |= flag;
      setFixedSlot(ReactionRecordSlot_Flags, Int32Value(flags));
  }

  public:
    static const Class class_;

    JSObject* promise() { return getFixedSlot(ReactionRecordSlot_Promise).toObjectOrNull(); }
    int32_t flags() { return getFixedSlot(ReactionRecordSlot_Flags).toInt32(); }
    JS::PromiseState targetState() {
        int32_t flags = this->flags();
        if (!(flags & REACTION_FLAG_RESOLVED))
            return JS::PromiseState::Pending;
        return flags & REACTION_FLAG_FULFILLED
               ? JS::PromiseState::Fulfilled
               : JS::PromiseState::Rejected;
    }
    void setTargetState(JS::PromiseState state) {
        int32_t flags = this->flags();
        MOZ_ASSERT(!(flags & REACTION_FLAG_RESOLVED));
        MOZ_ASSERT(state != JS::PromiseState::Pending, "Can't revert a reaction to pending.");
        flags |= REACTION_FLAG_RESOLVED;
        if (state == JS::PromiseState::Fulfilled)
            flags |= REACTION_FLAG_FULFILLED;
        setFixedSlot(ReactionRecordSlot_Flags, Int32Value(flags));
    }
    void setIsDefaultResolvingHandler(PromiseObject* promiseToResolve) {
        setFlagOnInitialState(REACTION_FLAG_DEFAULT_RESOLVING_HANDLER);
        setFixedSlot(ReactionRecordSlot_GeneratorOrPromiseToResolve, ObjectValue(*promiseToResolve));
    }
    bool isDefaultResolvingHandler() {
        int32_t flags = this->flags();
        return flags & REACTION_FLAG_DEFAULT_RESOLVING_HANDLER;
    }
    PromiseObject* defaultResolvingPromise() {
        MOZ_ASSERT(isDefaultResolvingHandler());
        const Value& promiseToResolve = getFixedSlot(ReactionRecordSlot_GeneratorOrPromiseToResolve);
        return &promiseToResolve.toObject().as<PromiseObject>();
    }
    void setIsAsyncFunction() {
        setFlagOnInitialState(REACTION_FLAG_ASYNC_FUNCTION);
    }
    bool isAsyncFunction() {
        int32_t flags = this->flags();
        return flags & REACTION_FLAG_ASYNC_FUNCTION;
    }
    void setIsAsyncGenerator(AsyncGeneratorObject* asyncGenObj) {
        setFlagOnInitialState(REACTION_FLAG_ASYNC_GENERATOR);
        setFixedSlot(ReactionRecordSlot_GeneratorOrPromiseToResolve, ObjectValue(*asyncGenObj));
    }
    bool isAsyncGenerator() {
        int32_t flags = this->flags();
        return flags & REACTION_FLAG_ASYNC_GENERATOR;
    }
    AsyncGeneratorObject* asyncGenerator() {
        MOZ_ASSERT(isAsyncGenerator());
        const Value& generator = getFixedSlot(ReactionRecordSlot_GeneratorOrPromiseToResolve);
        return &generator.toObject().as<AsyncGeneratorObject>();
    }
    void setIsDebuggerDummy() {
        setFlagOnInitialState(REACTION_FLAG_DEBUGGER_DUMMY);
    }
    bool isDebuggerDummy() {
        int32_t flags = this->flags();
        return flags & REACTION_FLAG_DEBUGGER_DUMMY;
    }
    Value handler() {
        MOZ_ASSERT(targetState() != JS::PromiseState::Pending);
        uint32_t slot = targetState() == JS::PromiseState::Fulfilled
                        ? ReactionRecordSlot_OnFulfilled
                        : ReactionRecordSlot_OnRejected;
        return getFixedSlot(slot);
    }
    Value handlerArg() {
        MOZ_ASSERT(targetState() != JS::PromiseState::Pending);
        return getFixedSlot(ReactionRecordSlot_HandlerArg);
    }
    void setHandlerArg(Value& arg) {
        MOZ_ASSERT(targetState() == JS::PromiseState::Pending);
        setFixedSlot(ReactionRecordSlot_HandlerArg, arg);
    }
    JSObject* incumbentGlobalObject() {
        return getFixedSlot(ReactionRecordSlot_IncumbentGlobalObject).toObjectOrNull();
    }
};

const Class PromiseReactionRecord::class_ = {
    "PromiseReactionRecord",
    JSCLASS_HAS_RESERVED_SLOTS(ReactionRecordSlots)
};

static void
AddPromiseFlags(PromiseObject& promise, int32_t flag)
{
    int32_t flags = promise.flags();
    promise.setFixedSlot(PromiseSlot_Flags, Int32Value(flags | flag));
}

static bool
PromiseHasAnyFlag(PromiseObject& promise, int32_t flag)
{
    return promise.flags() & flag;
}

static bool ResolvePromiseFunction(JSContext* cx, unsigned argc, Value* vp);
static bool RejectPromiseFunction(JSContext* cx, unsigned argc, Value* vp);

// ES2016, 25.4.1.3.
static MOZ_MUST_USE MOZ_ALWAYS_INLINE bool
CreateResolvingFunctions(JSContext* cx, HandleObject promise,
                         MutableHandleObject resolveFn,
                         MutableHandleObject rejectFn)
{
    HandlePropertyName funName = cx->names().empty;
    resolveFn.set(NewNativeFunction(cx, ResolvePromiseFunction, 1, funName,
                                    gc::AllocKind::FUNCTION_EXTENDED, GenericObject));
    if (!resolveFn)
        return false;

    rejectFn.set(NewNativeFunction(cx, RejectPromiseFunction, 1, funName,
                                   gc::AllocKind::FUNCTION_EXTENDED, GenericObject));
    if (!rejectFn)
        return false;

    JSFunction* resolveFun = &resolveFn->as<JSFunction>();
    JSFunction* rejectFun = &rejectFn->as<JSFunction>();

    resolveFun->initExtendedSlot(ResolveFunctionSlot_Promise, ObjectValue(*promise));
    resolveFun->initExtendedSlot(ResolveFunctionSlot_RejectFunction, ObjectValue(*rejectFun));

    rejectFun->initExtendedSlot(RejectFunctionSlot_Promise, ObjectValue(*promise));
    rejectFun->initExtendedSlot(RejectFunctionSlot_ResolveFunction, ObjectValue(*resolveFun));

    return true;
}

static void ClearResolutionFunctionSlots(JSFunction* resolutionFun);

static bool
IsSettledMaybeWrappedPromise(JSObject* promise)
{
    if (IsProxy(promise)) {
        promise = UncheckedUnwrap(promise);

        // Caller needs to handle dead wrappers.
        if (JS_IsDeadWrapper(promise))
            return false;
    }

    return promise->as<PromiseObject>().state() != JS::PromiseState::Pending;
}

static MOZ_MUST_USE bool RejectMaybeWrappedPromise(JSContext *cx, HandleObject promiseObj,
                                                   HandleValue reason);

// ES2016, 25.4.1.3.1.
static bool
RejectPromiseFunction(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    RootedFunction reject(cx, &args.callee().as<JSFunction>());
    RootedValue reasonVal(cx, args.get(0));

    // Steps 1-2.
    RootedValue promiseVal(cx, reject->getExtendedSlot(RejectFunctionSlot_Promise));

    // Steps 3-4.
    // If the Promise isn't available anymore, it has been resolved and the
    // reference to it removed to make it eligible for collection.
    if (promiseVal.isUndefined()) {
        args.rval().setUndefined();
        return true;
    }

    // Step 5.
    // Here, we only remove the Promise reference from the resolution
    // functions. Actually marking it as fulfilled/rejected happens later.
    ClearResolutionFunctionSlots(reject);

    RootedObject promise(cx, &promiseVal.toObject());

    // In some cases the Promise reference on the resolution function won't
    // have been removed during resolution, so we need to check that here,
    // too.
    if (IsSettledMaybeWrappedPromise(promise)) {
        args.rval().setUndefined();
        return true;
    }

    // Step 6.
    if (!RejectMaybeWrappedPromise(cx, promise, reasonVal))
        return false;
    args.rval().setUndefined();
    return true;
}

static MOZ_MUST_USE bool FulfillMaybeWrappedPromise(JSContext *cx, HandleObject promiseObj,
                                                    HandleValue value_);

static MOZ_MUST_USE bool EnqueuePromiseResolveThenableJob(JSContext* cx,
                                                          HandleValue promiseToResolve,
                                                          HandleValue thenable,
                                                          HandleValue thenVal);

static MOZ_MUST_USE bool EnqueuePromiseResolveThenableBuiltinJob(JSContext* cx,
                                                                 HandleObject promiseToResolve,
                                                                 HandleObject thenable);

static bool Promise_then(JSContext* cx, unsigned argc, Value* vp);
static bool Promise_then_impl(JSContext* cx, HandleValue promiseVal, HandleValue onFulfilled,
                              HandleValue onRejected, MutableHandleValue rval, bool rvalUsed);

// ES2016, 25.4.1.3.2, steps 6-13.
static MOZ_MUST_USE bool
ResolvePromiseInternal(JSContext* cx, HandleObject promise, HandleValue resolutionVal)
{
    MOZ_ASSERT(!IsSettledMaybeWrappedPromise(promise));

    // Step 7 (reordered).
    if (!resolutionVal.isObject())
        return FulfillMaybeWrappedPromise(cx, promise, resolutionVal);

    RootedObject resolution(cx, &resolutionVal.toObject());

    // Step 6.
    if (resolution == promise) {
        // Step 6.a.
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_CANNOT_RESOLVE_PROMISE_WITH_ITSELF);
        RootedValue selfResolutionError(cx);
        if (!MaybeGetAndClearException(cx, &selfResolutionError))
            return false;

        // Step 6.b.
        return RejectMaybeWrappedPromise(cx, promise, selfResolutionError);
    }

    // Step 8.
    RootedValue thenVal(cx);
    bool status = GetProperty(cx, resolution, resolution, cx->names().then, &thenVal);

    RootedValue error(cx);
    if (!status) {
        if (!MaybeGetAndClearException(cx, &error))
            return false;
    }

    // Testing functions allow to directly settle a promise without going
    // through the resolving functions. In that case the normal bookkeeping to
    // ensure only pending promises can be resolved doesn't apply and we need
    // to manually check for already settled promises. The exception is simply
    // dropped when this case happens.
    if (IsSettledMaybeWrappedPromise(promise))
        return true;

    // Step 9.
    if (!status)
        return RejectMaybeWrappedPromise(cx, promise, error);

    // Step 10 (implicit).

    // Step 11.
    if (!IsCallable(thenVal))
        return FulfillMaybeWrappedPromise(cx, promise, resolutionVal);

    // If the resolution object is a built-in Promise object, possibly from a
    // different realm in the same compartment, and the `then` property is the
    // original Promise.prototype.then function from the current realm, we
    // skip storing/calling it.
    // And additionally require that |promise| itself is also a built-in
    // Promise object from the same compartment, so the fast path doesn't need
    // to cope with wrappers.
    bool isBuiltinThen = false;
    if (resolution->is<PromiseObject>() &&
        resolution->as<PromiseObject>().compartment() == cx->compartment() &&
        IsNativeFunction(thenVal, Promise_then) &&
        thenVal.toObject().as<JSFunction>().realm() == cx->realm() &&
        promise->is<PromiseObject>() &&
        promise->as<PromiseObject>().compartment() == cx->compartment())
    {
        isBuiltinThen = true;
    }

    // Step 12.
    if (!isBuiltinThen) {
        RootedValue promiseVal(cx, ObjectValue(*promise));
        if (!EnqueuePromiseResolveThenableJob(cx, promiseVal, resolutionVal, thenVal))
            return false;
    } else {
        if (!EnqueuePromiseResolveThenableBuiltinJob(cx, promise, resolution))
            return false;
    }

    // Step 13.
    return true;
}

// ES2016, 25.4.1.3.2.
static bool
ResolvePromiseFunction(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    RootedFunction resolve(cx, &args.callee().as<JSFunction>());
    RootedValue resolutionVal(cx, args.get(0));

    // Steps 3-4 (reordered).
    // We use the reference to the reject function as a signal for whether
    // the resolve or reject function was already called, at which point
    // the references on each of the functions are cleared.
    if (!resolve->getExtendedSlot(ResolveFunctionSlot_RejectFunction).isObject()) {
        args.rval().setUndefined();
        return true;
    }

    // Steps 1-2 (reordered).
    RootedObject promise(cx, &resolve->getExtendedSlot(ResolveFunctionSlot_Promise).toObject());

    // Step 5.
    // Here, we only remove the Promise reference from the resolution
    // functions. Actually marking it as fulfilled/rejected happens later.
    ClearResolutionFunctionSlots(resolve);

    // In some cases the Promise reference on the resolution function won't
    // have been removed during resolution, so we need to check that here,
    // too.
    if (IsSettledMaybeWrappedPromise(promise)) {
        args.rval().setUndefined();
        return true;
    }

    // Steps 6-13.
    if (!ResolvePromiseInternal(cx, promise, resolutionVal))
        return false;
    args.rval().setUndefined();
    return true;
}

static bool PromiseReactionJob(JSContext* cx, unsigned argc, Value* vp);

/**
 * Tells the embedding to enqueue a Promise reaction job, based on
 * three parameters:
 * reactionObj - The reaction record.
 * handlerArg_ - The first and only argument to pass to the handler invoked by
 *              the job. This will be stored on the reaction record.
 * targetState - The PromiseState this reaction job targets. This decides
 *               whether the onFulfilled or onRejected handler is called.
 */
MOZ_MUST_USE static bool
EnqueuePromiseReactionJob(JSContext* cx, HandleObject reactionObj,
                          HandleValue handlerArg_, JS::PromiseState targetState)
{
    MOZ_ASSERT(targetState == JS::PromiseState::Fulfilled ||
               targetState == JS::PromiseState::Rejected);

    // The reaction might have been stored on a Promise from another
    // compartment, which means it would've been wrapped in a CCW.
    // To properly handle that case here, unwrap it and enter its
    // compartment, where the job creation should take place anyway.
    Rooted<PromiseReactionRecord*> reaction(cx);
    RootedValue handlerArg(cx, handlerArg_);
    mozilla::Maybe<AutoRealm> ar;
    if (!IsProxy(reactionObj)) {
        MOZ_RELEASE_ASSERT(reactionObj->is<PromiseReactionRecord>());
        reaction = &reactionObj->as<PromiseReactionRecord>();
    } else {
        if (JS_IsDeadWrapper(UncheckedUnwrap(reactionObj))) {
            JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_DEAD_OBJECT);
            return false;
        }
        reaction = &UncheckedUnwrap(reactionObj)->as<PromiseReactionRecord>();
        MOZ_RELEASE_ASSERT(reaction->is<PromiseReactionRecord>());
        ar.emplace(cx, reaction);
        if (!reaction->compartment()->wrap(cx, &handlerArg))
            return false;
    }

    // Must not enqueue a reaction job more than once.
    MOZ_ASSERT(reaction->targetState() == JS::PromiseState::Pending);

    assertSameCompartment(cx, handlerArg);
    reaction->setHandlerArg(handlerArg.get());

    RootedValue reactionVal(cx, ObjectValue(*reaction));

    reaction->setTargetState(targetState);
    RootedValue handler(cx, reaction->handler());

    // If we have a handler callback, we enter that handler's compartment so
    // that the promise reaction job function is created in that compartment.
    // That guarantees that the embedding ends up with the right entry global.
    // This is relevant for some html APIs like fetch that derive information
    // from said global.
    mozilla::Maybe<AutoRealm> ar2;
    if (handler.isObject()) {
        RootedObject handlerObj(cx, &handler.toObject());

        // The unwrapping has to be unchecked because we specifically want to
        // be able to use handlers with wrappers that would only allow calls.
        // E.g., it's ok to have a handler from a chrome compartment in a
        // reaction to a content compartment's Promise instance.
        handlerObj = UncheckedUnwrap(handlerObj);
        MOZ_ASSERT(handlerObj);
        ar2.emplace(cx, handlerObj);

        // We need to wrap the reaction to store it on the job function.
        if (!cx->compartment()->wrap(cx, &reactionVal))
            return false;
    }

    // Create the JS function to call when the job is triggered.
    HandlePropertyName funName = cx->names().empty;
    RootedFunction job(cx, NewNativeFunction(cx, PromiseReactionJob, 0, funName,
                                             gc::AllocKind::FUNCTION_EXTENDED, GenericObject));
    if (!job)
        return false;

    // Store the reaction on the reaction job.
    job->setExtendedSlot(ReactionJobSlot_ReactionRecord, reactionVal);

    // When using JS::AddPromiseReactions, no actual promise is created, so we
    // might not have one here.
    // Additionally, we might have an object here that isn't an instance of
    // Promise. This can happen if content overrides the value of
    // Promise[@@species] (or invokes Promise#then on a Promise subclass
    // instance with a non-default @@species value on the constructor) with a
    // function that returns objects that're not Promise (subclass) instances.
    // In that case, we just pretend we didn't have an object in the first
    // place.
    // If after all this we do have an object, wrap it in case we entered the
    // handler's compartment above, because we should pass objects from a
    // single compartment to the enqueuePromiseJob callback.
    RootedObject promise(cx, reaction->promise());
    if (promise && promise->is<PromiseObject>()) {
        if (!cx->compartment()->wrap(cx, &promise))
            return false;
    }

    // Using objectFromIncumbentGlobal, we can derive the incumbent global by
    // unwrapping and then getting the global. This is very convoluted, but
    // much better than having to store the original global as a private value
    // because we couldn't wrap it to store it as a normal JS value.
    RootedObject global(cx);
    RootedObject objectFromIncumbentGlobal(cx, reaction->incumbentGlobalObject());
    if (objectFromIncumbentGlobal) {
        objectFromIncumbentGlobal = CheckedUnwrap(objectFromIncumbentGlobal);
        MOZ_ASSERT(objectFromIncumbentGlobal);
        global = &objectFromIncumbentGlobal->nonCCWGlobal();
    }

    // Note: the global we pass here might be from a different compartment
    // than job and promise. While it's somewhat unusual to pass objects
    // from multiple compartments, in this case we specifically need the
    // global to be unwrapped because wrapping and unwrapping aren't
    // necessarily symmetric for globals.
    return cx->runtime()->enqueuePromiseJob(cx, job, promise, global);
}

static MOZ_MUST_USE bool TriggerPromiseReactions(JSContext* cx, HandleValue reactionsVal,
                                                 JS::PromiseState state, HandleValue valueOrReason);

// ES2016, Commoned-out implementation of 25.4.1.4. and 25.4.1.7.
static MOZ_MUST_USE bool
ResolvePromise(JSContext* cx, Handle<PromiseObject*> promise, HandleValue valueOrReason,
               JS::PromiseState state)
{
    // Step 1.
    MOZ_ASSERT(promise->state() == JS::PromiseState::Pending);
    MOZ_ASSERT(state == JS::PromiseState::Fulfilled || state == JS::PromiseState::Rejected);

    // Step 2.
    // We only have one list of reactions for both resolution types. So
    // instead of getting the right list of reactions, we determine the
    // resolution type to retrieve the right information from the
    // reaction records.
    RootedValue reactionsVal(cx, promise->reactions());

    // Steps 3-5.
    // The same slot is used for the reactions list and the result, so setting
    // the result also removes the reactions list.
    promise->setFixedSlot(PromiseSlot_ReactionsOrResult, valueOrReason);

    // Step 6.
    int32_t flags = promise->flags();
    flags |= PROMISE_FLAG_RESOLVED;
    if (state == JS::PromiseState::Fulfilled)
        flags |= PROMISE_FLAG_FULFILLED;
    promise->setFixedSlot(PromiseSlot_Flags, Int32Value(flags));

    // Also null out the resolve/reject functions so they can be GC'd.
    promise->setFixedSlot(PromiseSlot_RejectFunction, UndefinedValue());

    // Now that everything else is done, do the things the debugger needs.
    // Step 7 of RejectPromise implemented in onSettled.
    PromiseObject::onSettled(cx, promise);

    // Step 7 of FulfillPromise.
    // Step 8 of RejectPromise.
    if (reactionsVal.isObject())
        return TriggerPromiseReactions(cx, reactionsVal, state, valueOrReason);

    return true;
}

// ES2016, 25.4.1.4.
static MOZ_MUST_USE bool
FulfillMaybeWrappedPromise(JSContext *cx, HandleObject promiseObj, HandleValue value_)
{
    Rooted<PromiseObject*> promise(cx);
    RootedValue value(cx, value_);

    mozilla::Maybe<AutoRealm> ar;
    if (!IsProxy(promiseObj)) {
        promise = &promiseObj->as<PromiseObject>();
    } else {
        if (JS_IsDeadWrapper(UncheckedUnwrap(promiseObj))) {
            JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_DEAD_OBJECT);
            return false;
        }
        promise = &UncheckedUnwrap(promiseObj)->as<PromiseObject>();
        ar.emplace(cx, promise);
        if (!promise->compartment()->wrap(cx, &value))
            return false;
    }

    return ResolvePromise(cx, promise, value, JS::PromiseState::Fulfilled);
}

static bool GetCapabilitiesExecutor(JSContext* cx, unsigned argc, Value* vp);
static bool PromiseConstructor(JSContext* cx, unsigned argc, Value* vp);
static MOZ_MUST_USE PromiseObject* CreatePromiseObjectInternal(JSContext* cx,
                                                               HandleObject proto = nullptr,
                                                               bool protoIsWrapped = false,
                                                               bool informDebugger = true);

enum GetCapabilitiesExecutorSlots {
    GetCapabilitiesExecutorSlots_Resolve,
    GetCapabilitiesExecutorSlots_Reject
};

static MOZ_MUST_USE PromiseObject*
CreatePromiseObjectWithoutResolutionFunctions(JSContext* cx)
{
    PromiseObject* promise = CreatePromiseObjectInternal(cx);
    if (!promise)
        return nullptr;

    AddPromiseFlags(*promise, PROMISE_FLAG_DEFAULT_RESOLVING_FUNCTIONS);
    return promise;
}

static MOZ_MUST_USE PromiseObject*
CreatePromiseWithDefaultResolutionFunctions(JSContext* cx, MutableHandleObject resolve,
                                            MutableHandleObject reject)
{
    // ES2016, 25.4.3.1., as if called with GetCapabilitiesExecutor as the
    // executor argument.

    // Steps 1-2 (Not applicable).

    // Steps 3-7.
    Rooted<PromiseObject*> promise(cx, CreatePromiseObjectInternal(cx));
    if (!promise)
        return nullptr;

    // Step 8.
    if (!CreateResolvingFunctions(cx, promise, resolve, reject))
        return nullptr;

    promise->setFixedSlot(PromiseSlot_RejectFunction, ObjectValue(*reject));

    // Steps 9-10 (Not applicable).

    // Step 11.
    return promise;
}

// ES2016, 25.4.1.5.
static MOZ_MUST_USE bool
NewPromiseCapability(JSContext* cx, HandleObject C, MutableHandleObject promise,
                     MutableHandleObject resolve, MutableHandleObject reject,
                     bool canOmitResolutionFunctions)
{
    RootedValue cVal(cx, ObjectValue(*C));

    // Steps 1-2.
    if (!IsConstructor(C)) {
        ReportValueError(cx, JSMSG_NOT_CONSTRUCTOR, JSDVG_SEARCH_STACK, cVal, nullptr);
        return false;
    }

    // If we'd call the original Promise constructor and know that the
    // resolve/reject functions won't ever escape to content, we can skip
    // creating and calling the executor function and instead return a Promise
    // marked as having default resolve/reject functions.
    //
    // This can't be used in Promise.all and Promise.race because we have to
    // pass the reject (and resolve, in the race case) function to thenables
    // in the list passed to all/race, which (potentially) means exposing them
    // to content.
    //
    // For Promise.all and Promise.race we can only optimize away the creation
    // of the GetCapabilitiesExecutor function, and directly allocate the
    // result promise instead of invoking the Promise constructor.
    if (IsNativeFunction(cVal, PromiseConstructor)) {
        if (canOmitResolutionFunctions)
            promise.set(CreatePromiseObjectWithoutResolutionFunctions(cx));
        else
            promise.set(CreatePromiseWithDefaultResolutionFunctions(cx, resolve, reject));
        if (!promise)
            return false;
        return true;
    }

    // Step 3 (omitted).

    // Step 4.
    HandlePropertyName funName = cx->names().empty;
    RootedFunction executor(cx, NewNativeFunction(cx, GetCapabilitiesExecutor, 2, funName,
                                                  gc::AllocKind::FUNCTION_EXTENDED, GenericObject));
    if (!executor)
        return false;

    // Step 5 (omitted).

    // Step 6.
    FixedConstructArgs<1> cargs(cx);
    cargs[0].setObject(*executor);
    if (!Construct(cx, cVal, cargs, cVal, promise))
        return false;

    // Step 7.
    RootedValue resolveVal(cx, executor->getExtendedSlot(GetCapabilitiesExecutorSlots_Resolve));
    if (!IsCallable(resolveVal)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_PROMISE_RESOLVE_FUNCTION_NOT_CALLABLE);
        return false;
    }

    // Step 8.
    RootedValue rejectVal(cx, executor->getExtendedSlot(GetCapabilitiesExecutorSlots_Reject));
    if (!IsCallable(rejectVal)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_PROMISE_REJECT_FUNCTION_NOT_CALLABLE);
        return false;
    }

    // Step 9 (well, the equivalent for all of promiseCapabilities' fields.)
    resolve.set(&resolveVal.toObject());
    reject.set(&rejectVal.toObject());

    // Step 10.
    return true;
}

// ES2016, 25.4.1.5.1.
static bool
GetCapabilitiesExecutor(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedFunction F(cx, &args.callee().as<JSFunction>());

    // Steps 1-2 (implicit).

    // Steps 3-4.
    if (!F->getExtendedSlot(GetCapabilitiesExecutorSlots_Resolve).isUndefined() ||
        !F->getExtendedSlot(GetCapabilitiesExecutorSlots_Reject).isUndefined())
    {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_PROMISE_CAPABILITY_HAS_SOMETHING_ALREADY);
        return false;
    }

    // Step 5.
    F->setExtendedSlot(GetCapabilitiesExecutorSlots_Resolve, args.get(0));

    // Step 6.
    F->setExtendedSlot(GetCapabilitiesExecutorSlots_Reject, args.get(1));

    // Step 7.
    args.rval().setUndefined();
    return true;
}

// ES2016, 25.4.1.7.
static MOZ_MUST_USE bool
RejectMaybeWrappedPromise(JSContext *cx, HandleObject promiseObj, HandleValue reason_)
{
    Rooted<PromiseObject*> promise(cx);
    RootedValue reason(cx, reason_);

    mozilla::Maybe<AutoRealm> ar;
    if (!IsProxy(promiseObj)) {
        promise = &promiseObj->as<PromiseObject>();
    } else {
        if (JS_IsDeadWrapper(UncheckedUnwrap(promiseObj))) {
            JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_DEAD_OBJECT);
            return false;
        }
        promise = &UncheckedUnwrap(promiseObj)->as<PromiseObject>();
        ar.emplace(cx, promise);

        // The rejection reason might've been created in a compartment with higher
        // privileges than the Promise's. In that case, object-type rejection
        // values might be wrapped into a wrapper that throws whenever the
        // Promise's reaction handler wants to do anything useful with it. To
        // avoid that situation, we synthesize a generic error that doesn't
        // expose any privileged information but can safely be used in the
        // rejection handler.
        if (!promise->compartment()->wrap(cx, &reason))
            return false;
        if (reason.isObject() && !CheckedUnwrap(&reason.toObject())) {
            // Report the existing reason, so we don't just drop it on the
            // floor.
            RootedObject realReason(cx, UncheckedUnwrap(&reason.toObject()));
            RootedValue realReasonVal(cx, ObjectValue(*realReason));
            Rooted<GlobalObject*> realGlobal(cx, &realReason->nonCCWGlobal());
            ReportErrorToGlobal(cx, realGlobal, realReasonVal);

            // Async stacks are only properly adopted if there's at least one
            // interpreter frame active right now. If a thenable job with a
            // throwing `then` function got us here, that'll not be the case,
            // so we add one by throwing the error from self-hosted code.
            if (!GetInternalError(cx, JSMSG_PROMISE_ERROR_IN_WRAPPED_REJECTION_REASON, &reason))
                return false;
        }
    }

    return ResolvePromise(cx, promise, reason, JS::PromiseState::Rejected);
}

// ES2016, 25.4.1.8.
static MOZ_MUST_USE bool
TriggerPromiseReactions(JSContext* cx, HandleValue reactionsVal, JS::PromiseState state,
                        HandleValue valueOrReason)
{
    MOZ_ASSERT(state == JS::PromiseState::Fulfilled || state == JS::PromiseState::Rejected);

    RootedObject reactions(cx, &reactionsVal.toObject());
    RootedObject reaction(cx);

    if (reactions->is<PromiseReactionRecord>() ||
        IsWrapper(reactions) ||
        JS_IsDeadWrapper(reactions))
    {
        return EnqueuePromiseReactionJob(cx, reactions, valueOrReason, state);
    }

    RootedNativeObject reactionsList(cx, &reactions->as<NativeObject>());
    size_t reactionsCount = reactionsList->getDenseInitializedLength();
    MOZ_ASSERT(reactionsCount > 1, "Reactions list should be created lazily");

    RootedValue reactionVal(cx);
    for (size_t i = 0; i < reactionsCount; i++) {
        reactionVal = reactionsList->getDenseElement(i);
        MOZ_RELEASE_ASSERT(reactionVal.isObject());
        reaction = &reactionVal.toObject();
        if (!EnqueuePromiseReactionJob(cx, reaction, valueOrReason, state))
            return false;
    }

    return true;
}

// Implements PromiseReactionJob optimized for the case when the reaction
// handler is one of the default resolving functions as created by the
// CreateResolvingFunctions abstract operation.
static MOZ_MUST_USE bool
DefaultResolvingPromiseReactionJob(JSContext* cx, Handle<PromiseReactionRecord*> reaction,
                                   MutableHandleValue rval)
{
    MOZ_ASSERT(reaction->targetState() != JS::PromiseState::Pending);

    Rooted<PromiseObject*> promiseToResolve(cx, reaction->defaultResolvingPromise());

    // Testing functions allow to directly settle a promise without going
    // through the resolving functions. In that case the normal bookkeeping to
    // ensure only pending promises can be resolved doesn't apply and we need
    // to manually check for already settled promises. We still call
    // RunResolutionFunction for consistency with PromiseReactionJob.
    ResolutionMode resolutionMode = ResolveMode;
    RootedValue handlerResult(cx, UndefinedValue());
    if (promiseToResolve->state() == JS::PromiseState::Pending) {
        RootedValue argument(cx, reaction->handlerArg());

        // Step 6.
        bool ok;
        if (reaction->targetState() == JS::PromiseState::Fulfilled)
            ok = ResolvePromiseInternal(cx, promiseToResolve, argument);
        else
            ok = RejectMaybeWrappedPromise(cx, promiseToResolve, argument);

        if (!ok) {
            resolutionMode = RejectMode;
            if (!MaybeGetAndClearException(cx, &handlerResult))
                return false;
        }
    }

    // Steps 7-9.
    size_t hookSlot = resolutionMode == RejectMode
                      ? ReactionRecordSlot_Reject
                      : ReactionRecordSlot_Resolve;
    RootedObject callee(cx, reaction->getFixedSlot(hookSlot).toObjectOrNull());
    RootedObject promiseObj(cx, reaction->promise());
    if (!RunResolutionFunction(cx, callee, handlerResult, resolutionMode, promiseObj))
        return false;

    rval.setUndefined();
    return true;
}

static MOZ_MUST_USE bool
AsyncFunctionPromiseReactionJob(JSContext* cx, Handle<PromiseReactionRecord*> reaction,
                                MutableHandleValue rval)
{
    MOZ_ASSERT(reaction->isAsyncFunction());

    RootedValue handlerVal(cx, reaction->handler());
    RootedValue argument(cx, reaction->handlerArg());
    Rooted<PromiseObject*> resultPromise(cx, &reaction->promise()->as<PromiseObject>());
    RootedValue generatorVal(cx, resultPromise->getFixedSlot(PromiseSlot_AwaitGenerator));

    int32_t handlerNum = handlerVal.toInt32();

    // Await's handlers don't return a value, nor throw exception.
    // They fail only on OOM.
    if (handlerNum == PromiseHandlerAsyncFunctionAwaitedFulfilled) {
        if (!AsyncFunctionAwaitedFulfilled(cx, resultPromise, generatorVal, argument))
            return false;
    } else {
        MOZ_ASSERT(handlerNum == PromiseHandlerAsyncFunctionAwaitedRejected);
        if (!AsyncFunctionAwaitedRejected(cx, resultPromise, generatorVal, argument))
            return false;
    }

    rval.setUndefined();
    return true;
}

static MOZ_MUST_USE bool
AsyncGeneratorPromiseReactionJob(JSContext* cx, Handle<PromiseReactionRecord*> reaction,
                                 MutableHandleValue rval)
{
    MOZ_ASSERT(reaction->isAsyncGenerator());

    RootedValue handlerVal(cx, reaction->handler());
    RootedValue argument(cx, reaction->handlerArg());
    Rooted<AsyncGeneratorObject*> asyncGenObj(cx, reaction->asyncGenerator());

    int32_t handlerNum = handlerVal.toInt32();

    // Await's handlers don't return a value, nor throw exception.
    // They fail only on OOM.
    if (handlerNum == PromiseHandlerAsyncGeneratorAwaitedFulfilled) {
        // 4.1.1.
        if (!AsyncGeneratorAwaitedFulfilled(cx, asyncGenObj, argument))
            return false;
    } else if (handlerNum == PromiseHandlerAsyncGeneratorAwaitedRejected) {
        // 4.1.2.
        if (!AsyncGeneratorAwaitedRejected(cx, asyncGenObj, argument))
            return false;
    } else if (handlerNum == PromiseHandlerAsyncGeneratorResumeNextReturnFulfilled) {
        asyncGenObj->setCompleted();
        // 11.4.3.5.1 step 1.
        if (!AsyncGeneratorResolve(cx, asyncGenObj, argument, true))
            return false;
    } else if (handlerNum == PromiseHandlerAsyncGeneratorResumeNextReturnRejected) {
        asyncGenObj->setCompleted();
        // 11.4.3.5.2 step 1.
        if (!AsyncGeneratorReject(cx, asyncGenObj, argument))
            return false;
    } else if (handlerNum == PromiseHandlerAsyncGeneratorYieldReturnAwaitedFulfilled) {
        asyncGenObj->setExecuting();
        // 11.4.3.7 steps 8.d-e.
        if (!AsyncGeneratorYieldReturnAwaitedFulfilled(cx, asyncGenObj, argument))
            return false;
    } else {
        MOZ_ASSERT(handlerNum == PromiseHandlerAsyncGeneratorYieldReturnAwaitedRejected);
        asyncGenObj->setExecuting();
        // 11.4.3.7 step 8.c.
        if (!AsyncGeneratorYieldReturnAwaitedRejected(cx, asyncGenObj, argument))
            return false;
    }

    rval.setUndefined();
    return true;
}

// ES2016, 25.4.2.1.
/**
 * Callback triggering the fulfill/reject reaction for a resolved Promise,
 * to be invoked by the embedding during its processing of the Promise job
 * queue.
 *
 * See http://www.ecma-international.org/ecma-262/7.0/index.html#sec-jobs-and-job-queues
 *
 * A PromiseReactionJob is set as the native function of an extended
 * JSFunction object, with all information required for the job's
 * execution stored in in a reaction record in its first extended slot.
 */
static bool
PromiseReactionJob(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    RootedFunction job(cx, &args.callee().as<JSFunction>());

    RootedObject reactionObj(cx, &job->getExtendedSlot(ReactionJobSlot_ReactionRecord).toObject());

    // To ensure that the embedding ends up with the right entry global, we're
    // guaranteeing that the reaction job function gets created in the same
    // compartment as the handler function. That's not necessarily the global
    // that the job was triggered from, though.
    // We can find the triggering global via the job's reaction record. To go
    // back, we check if the reaction is a wrapper and if so, unwrap it and
    // enter its compartment.
    mozilla::Maybe<AutoRealm> ar;
    if (!IsProxy(reactionObj)) {
        MOZ_RELEASE_ASSERT(reactionObj->is<PromiseReactionRecord>());
    } else {
        reactionObj = UncheckedUnwrap(reactionObj);
        if (JS_IsDeadWrapper(reactionObj)) {
            JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_DEAD_OBJECT);
            return false;
        }
        MOZ_RELEASE_ASSERT(reactionObj->is<PromiseReactionRecord>());
        ar.emplace(cx, reactionObj);
    }

    // Steps 1-2.
    Rooted<PromiseReactionRecord*> reaction(cx, &reactionObj->as<PromiseReactionRecord>());
    if (reaction->isDefaultResolvingHandler())
        return DefaultResolvingPromiseReactionJob(cx, reaction, args.rval());
    if (reaction->isAsyncFunction())
        return AsyncFunctionPromiseReactionJob(cx, reaction, args.rval());
    if (reaction->isAsyncGenerator())
        return AsyncGeneratorPromiseReactionJob(cx, reaction, args.rval());
    if (reaction->isDebuggerDummy())
        return true;

    // Step 3.
    RootedValue handlerVal(cx, reaction->handler());

    RootedValue argument(cx, reaction->handlerArg());

    RootedValue handlerResult(cx);
    ResolutionMode resolutionMode = ResolveMode;

    // Steps 4-6.
    if (handlerVal.isInt32()) {
        int32_t handlerNum = handlerVal.toInt32();

        // Step 4.
        if (handlerNum == PromiseHandlerIdentity) {
            handlerResult = argument;
        } else if (handlerNum == PromiseHandlerThrower) {
            // Step 5.
            resolutionMode = RejectMode;
            handlerResult = argument;
        } else {
            MOZ_ASSERT(handlerNum == PromiseHandlerAsyncFromSyncIteratorValueUnwrapDone ||
                       handlerNum == PromiseHandlerAsyncFromSyncIteratorValueUnwrapNotDone);

            bool done = handlerNum == PromiseHandlerAsyncFromSyncIteratorValueUnwrapDone;
            // Async Iteration proposal 11.1.3.2.5 step 1.
            RootedObject resultObj(cx, CreateIterResultObject(cx, argument, done));
            if (!resultObj)
                return false;

            handlerResult = ObjectValue(*resultObj);
        }
    } else {
        MOZ_ASSERT(handlerVal.isObject());
        MOZ_ASSERT(IsCallable(handlerVal));

        // Step 6.
        FixedInvokeArgs<1> args2(cx);
        args2[0].set(argument);
        if (!Call(cx, handlerVal, UndefinedHandleValue, args2, &handlerResult)) {
            resolutionMode = RejectMode;
            if (!MaybeGetAndClearException(cx, &handlerResult))
                return false;
        }
    }

    // Steps 7-9.
    size_t hookSlot = resolutionMode == RejectMode
                      ? ReactionRecordSlot_Reject
                      : ReactionRecordSlot_Resolve;
    RootedObject callee(cx, reaction->getFixedSlot(hookSlot).toObjectOrNull());
    RootedObject promiseObj(cx, reaction->promise());
    if (!RunResolutionFunction(cx, callee, handlerResult, resolutionMode, promiseObj))
        return false;

    args.rval().setUndefined();
    return true;
}

// ES2016, 25.4.2.2.
/**
 * Callback for resolving a thenable, to be invoked by the embedding during
 * its processing of the Promise job queue.
 *
 * See https://tc39.github.io/ecma262/#sec-jobs-and-job-queues
 *
 * A PromiseResolveThenableJob is set as the native function of an extended
 * JSFunction object, with all information required for the job's
 * execution stored in the function's extended slots.
 *
 * Usage of the function's extended slots is described in the ThenableJobSlots
 * enum.
 */
static bool
PromiseResolveThenableJob(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    RootedFunction job(cx, &args.callee().as<JSFunction>());
    RootedValue then(cx, job->getExtendedSlot(ThenableJobSlot_Handler));
    MOZ_ASSERT(then.isObject());
    MOZ_ASSERT(!IsWrapper(&then.toObject()));
    RootedNativeObject jobArgs(cx, &job->getExtendedSlot(ThenableJobSlot_JobData)
                                    .toObject().as<NativeObject>());

    RootedObject promise(cx, &jobArgs->getDenseElement(ThenableJobDataIndex_Promise).toObject());
    RootedValue thenable(cx, jobArgs->getDenseElement(ThenableJobDataIndex_Thenable));

    // Step 1.
    RootedObject resolveFn(cx);
    RootedObject rejectFn(cx);
    if (!CreateResolvingFunctions(cx, promise, &resolveFn, &rejectFn))
        return false;

    // Step 2.
    FixedInvokeArgs<2> args2(cx);
    args2[0].setObject(*resolveFn);
    args2[1].setObject(*rejectFn);

    // In difference to the usual pattern, we return immediately on success.
    RootedValue rval(cx);
    if (Call(cx, then, thenable, args2, &rval))
        return true;

    // Steps 3-4.
    if (!MaybeGetAndClearException(cx, &rval))
        return false;

    FixedInvokeArgs<1> rejectArgs(cx);
    rejectArgs[0].set(rval);

    RootedValue rejectVal(cx, ObjectValue(*rejectFn));
    return Call(cx, rejectVal, UndefinedHandleValue, rejectArgs, &rval);
}

static MOZ_MUST_USE bool
OriginalPromiseThenWithoutSettleHandlers(JSContext* cx, Handle<PromiseObject*> promise,
                                         Handle<PromiseObject*> promiseToResolve);

/**
 * Specialization of PromiseResolveThenableJob when the `thenable` is a
 * built-in Promise object and the `then` property is the built-in
 * `Promise.prototype.then` function.
 *
 * A PromiseResolveBuiltinThenableJob is set as the native function of an
 * extended JSFunction object, with all information required for the job's
 * execution stored in the function's extended slots.
 *
 * Usage of the function's extended slots is described in the
 * BuiltinThenableJobSlots enum.
 */
static bool
PromiseResolveBuiltinThenableJob(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    RootedFunction job(cx, &args.callee().as<JSFunction>());
    RootedObject promise(cx, &job->getExtendedSlot(BuiltinThenableJobSlot_Promise).toObject());
    RootedObject thenable(cx, &job->getExtendedSlot(BuiltinThenableJobSlot_Thenable).toObject());

    assertSameCompartment(cx, promise, thenable);
    MOZ_ASSERT(promise->is<PromiseObject>());
    MOZ_ASSERT(thenable->is<PromiseObject>());

    // Step 1 (Skipped).

    // Step 2.
    // In difference to the usual pattern, we return immediately on success.
    if (OriginalPromiseThenWithoutSettleHandlers(cx, thenable.as<PromiseObject>(),
                                                 promise.as<PromiseObject>()))
    {
        return true;
    }

    // Steps 3-4.
    RootedValue exception(cx);
    if (!MaybeGetAndClearException(cx, &exception))
        return false;

    // Testing functions allow to directly settle a promise without going
    // through the resolving functions. In that case the normal bookkeeping to
    // ensure only pending promises can be resolved doesn't apply and we need
    // to manually check for already settled promises. The exception is simply
    // dropped when this case happens.
    if (promise->as<PromiseObject>().state() != JS::PromiseState::Pending)
        return true;

    return RejectMaybeWrappedPromise(cx, promise, exception);
}

/**
 * Tells the embedding to enqueue a Promise resolve thenable job, based on
 * three parameters:
 * promiseToResolve_ - The promise to resolve, obviously.
 * thenable_ - The thenable to resolve the Promise with.
 * thenVal - The `then` function to invoke with the `thenable` as the receiver.
 */
static MOZ_MUST_USE bool
EnqueuePromiseResolveThenableJob(JSContext* cx, HandleValue promiseToResolve_,
                                 HandleValue thenable_, HandleValue thenVal)
{
    // Need to re-root these to enable wrapping them below.
    RootedValue promiseToResolve(cx, promiseToResolve_);
    RootedValue thenable(cx, thenable_);

    // We enter the `then` callable's compartment so that the job function is
    // created in that compartment.
    // That guarantees that the embedding ends up with the right entry global.
    // This is relevant for some html APIs like fetch that derive information
    // from said global.
    RootedObject then(cx, CheckedUnwrap(&thenVal.toObject()));
    AutoRealm ar(cx, then);

    HandlePropertyName funName = cx->names().empty;
    RootedFunction job(cx, NewNativeFunction(cx, PromiseResolveThenableJob, 0, funName,
                                             gc::AllocKind::FUNCTION_EXTENDED, GenericObject));
    if (!job)
        return false;

    // Store the `then` function on the callback.
    job->setExtendedSlot(ThenableJobSlot_Handler, ObjectValue(*then));

    // Create a dense array to hold the data needed for the reaction job to
    // work.
    // The layout is described in the ThenableJobDataIndices enum.
    RootedArrayObject data(cx, NewDenseFullyAllocatedArray(cx, ThenableJobDataLength));
    if (!data ||
        data->ensureDenseElements(cx, 0, ThenableJobDataLength) != DenseElementResult::Success)
    {
        return false;
    }

    // Wrap and set the `promiseToResolve` argument.
    if (!cx->compartment()->wrap(cx, &promiseToResolve))
        return false;
    data->setDenseElement(ThenableJobDataIndex_Promise, promiseToResolve);
    // At this point the promise is guaranteed to be wrapped into the job's
    // compartment.
    RootedObject promise(cx, &promiseToResolve.toObject());

    // Wrap and set the `thenable` argument.
    MOZ_ASSERT(thenable.isObject());
    if (!cx->compartment()->wrap(cx, &thenable))
        return false;
    data->setDenseElement(ThenableJobDataIndex_Thenable, thenable);

    // Store the data array on the reaction job.
    job->setExtendedSlot(ThenableJobSlot_JobData, ObjectValue(*data));

    RootedObject incumbentGlobal(cx, cx->runtime()->getIncumbentGlobal(cx));
    return cx->runtime()->enqueuePromiseJob(cx, job, promise, incumbentGlobal);
}

/**
 * Tells the embedding to enqueue a Promise resolve thenable built-in job,
 * based on two parameters:
 * promiseToResolve - The promise to resolve, obviously.
 * thenable - The thenable to resolve the Promise with.
 */
static MOZ_MUST_USE bool
EnqueuePromiseResolveThenableBuiltinJob(JSContext* cx, HandleObject promiseToResolve,
                                        HandleObject thenable)
{
    assertSameCompartment(cx, promiseToResolve, thenable);
    MOZ_ASSERT(promiseToResolve->is<PromiseObject>());
    MOZ_ASSERT(thenable->is<PromiseObject>());

    HandlePropertyName funName = cx->names().empty;
    RootedFunction job(cx, NewNativeFunction(cx, PromiseResolveBuiltinThenableJob, 0, funName,
                                             gc::AllocKind::FUNCTION_EXTENDED, GenericObject));
    if (!job)
        return false;

    // Store the promise and the thenable on the reaction job.
    job->setExtendedSlot(BuiltinThenableJobSlot_Promise, ObjectValue(*promiseToResolve));
    job->setExtendedSlot(BuiltinThenableJobSlot_Thenable, ObjectValue(*thenable));

    RootedObject incumbentGlobal(cx, cx->runtime()->getIncumbentGlobal(cx));
    return cx->runtime()->enqueuePromiseJob(cx, job, promiseToResolve, incumbentGlobal);
}

static MOZ_MUST_USE bool
AddDummyPromiseReactionForDebugger(JSContext* cx, Handle<PromiseObject*> promise,
                                   HandleObject dependentPromise);

static MOZ_MUST_USE bool
AddPromiseReaction(JSContext* cx, Handle<PromiseObject*> promise,
                   Handle<PromiseReactionRecord*> reaction);

static MOZ_MUST_USE bool
BlockOnPromise(JSContext* cx, HandleValue promise, HandleObject blockedPromise,
               HandleValue onFulfilled, HandleValue onRejected, bool onFulfilledReturnsUndefined);

static JSFunction*
GetResolveFunctionFromReject(JSFunction* reject)
{
    MOZ_ASSERT(reject->maybeNative() == RejectPromiseFunction);
    Value resolveFunVal = reject->getExtendedSlot(RejectFunctionSlot_ResolveFunction);
    MOZ_ASSERT(IsNativeFunction(resolveFunVal, ResolvePromiseFunction));
    return &resolveFunVal.toObject().as<JSFunction>();
}

static JSFunction*
GetRejectFunctionFromResolve(JSFunction* resolve)
{
    MOZ_ASSERT(resolve->maybeNative() == ResolvePromiseFunction);
    Value rejectFunVal = resolve->getExtendedSlot(ResolveFunctionSlot_RejectFunction);
    MOZ_ASSERT(IsNativeFunction(rejectFunVal, RejectPromiseFunction));
    return &rejectFunVal.toObject().as<JSFunction>();
}

static JSFunction*
GetResolveFunctionFromPromise(PromiseObject* promise)
{
    Value rejectFunVal = promise->getFixedSlot(PromiseSlot_RejectFunction);
    if (rejectFunVal.isUndefined())
        return nullptr;
    JSObject* rejectFunObj = &rejectFunVal.toObject();

    // We can safely unwrap it because all we want is to get the resolve
    // function.
    if (IsWrapper(rejectFunObj))
        rejectFunObj = UncheckedUnwrap(rejectFunObj);

    if (!rejectFunObj->is<JSFunction>())
        return nullptr;

    JSFunction* rejectFun = &rejectFunObj->as<JSFunction>();

    // Only the original RejectPromiseFunction has a reference to the resolve
    // function.
    if (rejectFun->maybeNative() != &RejectPromiseFunction)
        return nullptr;

    // The reject function was already called and cleared its resolve-function
    // extended slot.
    if (rejectFun->getExtendedSlot(RejectFunctionSlot_ResolveFunction).isUndefined())
        return nullptr;

    return GetResolveFunctionFromReject(rejectFun);
}

static void
ClearResolutionFunctionSlots(JSFunction* resolutionFun)
{
    JSFunction* resolve;
    JSFunction* reject;
    if (resolutionFun->maybeNative() == ResolvePromiseFunction) {
        resolve = resolutionFun;
        reject = GetRejectFunctionFromResolve(resolutionFun);
    } else {
        resolve = GetResolveFunctionFromReject(resolutionFun);
        reject = resolutionFun;
    }

    resolve->setExtendedSlot(ResolveFunctionSlot_Promise, UndefinedValue());
    resolve->setExtendedSlot(ResolveFunctionSlot_RejectFunction, UndefinedValue());

    reject->setExtendedSlot(RejectFunctionSlot_Promise, UndefinedValue());
    reject->setExtendedSlot(RejectFunctionSlot_ResolveFunction, UndefinedValue());
}

// ES2016, 25.4.3.1. steps 3-7.
static MOZ_MUST_USE MOZ_ALWAYS_INLINE PromiseObject*
CreatePromiseObjectInternal(JSContext* cx, HandleObject proto /* = nullptr */,
                            bool protoIsWrapped /* = false */, bool informDebugger /* = true */)
{
    // Step 3.
    // Enter the unwrapped proto's compartment, if that's different from
    // the current one.
    // All state stored in a Promise's fixed slots must be created in the
    // same compartment, so we get all of that out of the way here.
    // (Except for the resolution functions, which are created below.)
    mozilla::Maybe<AutoRealm> ar;
    if (protoIsWrapped)
        ar.emplace(cx, proto);

    PromiseObject* promise = NewObjectWithClassProto<PromiseObject>(cx, proto);
    if (!promise)
        return nullptr;

    // Step 4.
    promise->initFixedSlot(PromiseSlot_Flags, Int32Value(0));

    // Steps 5-6.
    // Omitted, we allocate our single list of reaction records lazily.

    // Step 7.
    // Implicit, the handled flag is unset by default.

    if (MOZ_LIKELY(!ShouldCaptureDebugInfo(cx)))
        return promise;

    // Store an allocation stack so we can later figure out what the
    // control flow was for some unexpected results. Frightfully expensive,
    // but oh well.

    Rooted<PromiseObject*> promiseRoot(cx, promise);

    PromiseDebugInfo* debugInfo = PromiseDebugInfo::create(cx, promiseRoot);
    if (!debugInfo)
        return nullptr;

    // Let the Debugger know about this Promise.
    if (informDebugger)
        Debugger::onNewPromise(cx, promiseRoot);

    return promiseRoot;
}

// ES2016, 25.4.3.1.
static bool
PromiseConstructor(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    // Step 1.
    if (!ThrowIfNotConstructing(cx, args, "Promise"))
        return false;

    // Step 2.
    RootedValue executorVal(cx, args.get(0));
    if (!IsCallable(executorVal))
        return ReportIsNotFunction(cx, executorVal);
    RootedObject executor(cx, &executorVal.toObject());

    // Steps 3-10.
    RootedObject newTarget(cx, &args.newTarget().toObject());

    // If the constructor is called via an Xray wrapper, then the newTarget
    // hasn't been unwrapped. We want that because, while the actual instance
    // should be created in the target compartment, the constructor's code
    // should run in the wrapper's compartment.
    //
    // This is so that the resolve and reject callbacks get created in the
    // wrapper's compartment, which is required for code in that compartment
    // to freely interact with it, and, e.g., pass objects as arguments, which
    // it wouldn't be able to if the callbacks were themselves wrapped in Xray
    // wrappers.
    //
    // At the same time, just creating the Promise itself in the wrapper's
    // compartment wouldn't be helpful: if the wrapper forbids interactions
    // with objects except for specific actions, such as calling them, then
    // the code we want to expose it to can't actually treat it as a Promise:
    // calling .then on it would throw, for example.
    //
    // Another scenario where it's important to create the Promise in a
    // different compartment from the resolution functions is when we want to
    // give non-privileged code a Promise resolved with the result of a
    // Promise from privileged code; as a return value of a JS-implemented
    // API, say. If the resolution functions were unprivileged, then resolving
    // with a privileged Promise would cause `resolve` to attempt accessing
    // .then on the passed Promise, which would throw an exception, so we'd
    // just end up with a rejected Promise. Really, we want to chain the two
    // Promises, with the unprivileged one resolved with the resolution of the
    // privileged one.

    bool needsWrapping = false;
    RootedObject proto(cx);
    if (IsWrapper(newTarget)) {
        JSObject* unwrappedNewTarget = CheckedUnwrap(newTarget);
        MOZ_ASSERT(unwrappedNewTarget);
        MOZ_ASSERT(unwrappedNewTarget != newTarget);

        newTarget = unwrappedNewTarget;
        {
            AutoRealm ar(cx, newTarget);
            Handle<GlobalObject*> global = cx->global();
            RootedObject promiseCtor(cx, GlobalObject::getOrCreatePromiseConstructor(cx, global));
            if (!promiseCtor)
                return false;

            // Promise subclasses don't get the special Xray treatment, so
            // we only need to do the complex wrapping and unwrapping scheme
            // described above for instances of Promise itself.
            if (newTarget == promiseCtor) {
                needsWrapping = true;
                proto = GlobalObject::getOrCreatePromisePrototype(cx, cx->global());
                if (!proto)
                    return false;
            }
        }
    }

    if (needsWrapping) {
        if (!cx->compartment()->wrap(cx, &proto))
            return false;
    } else {
        if (!GetPrototypeFromBuiltinConstructor(cx, args, &proto))
            return false;
    }
    PromiseObject* promise = PromiseObject::create(cx, executor, proto, needsWrapping);
    if (!promise)
        return false;

    // Step 11.
    args.rval().setObject(*promise);
    if (needsWrapping)
        return cx->compartment()->wrap(cx, args.rval());
    return true;
}

// ES2016, 25.4.3.1. steps 3-11.
/* static */ PromiseObject*
PromiseObject::create(JSContext* cx, HandleObject executor, HandleObject proto /* = nullptr */,
                      bool needsWrapping /* = false */)
{
    MOZ_ASSERT(executor->isCallable());

    RootedObject usedProto(cx, proto);
    // If the proto is wrapped, that means the current function is running
    // with a different compartment active from the one the Promise instance
    // is to be created in.
    // See the comment in PromiseConstructor for details.
    if (needsWrapping) {
        MOZ_ASSERT(proto);
        usedProto = CheckedUnwrap(proto);
        if (!usedProto)
            return nullptr;
    }


    // Steps 3-7.
    Rooted<PromiseObject*> promise(cx, CreatePromiseObjectInternal(cx, usedProto, needsWrapping,
                                                                   false));
    if (!promise)
        return nullptr;

    RootedObject promiseObj(cx, promise);
    if (needsWrapping && !cx->compartment()->wrap(cx, &promiseObj))
        return nullptr;

    // Step 8.
    // The resolving functions are created in the compartment active when the
    // (maybe wrapped) Promise constructor was called. They contain checks and
    // can unwrap the Promise if required.
    RootedObject resolveFn(cx);
    RootedObject rejectFn(cx);
    if (!CreateResolvingFunctions(cx, promiseObj, &resolveFn, &rejectFn))
        return nullptr;

    // Need to wrap the resolution functions before storing them on the Promise.
    MOZ_ASSERT(promise->getFixedSlot(PromiseSlot_RejectFunction).isUndefined(),
               "Slot must be undefined so initFixedSlot can be used");
    if (needsWrapping) {
        AutoRealm ar(cx, promise);
        RootedObject wrappedRejectFn(cx, rejectFn);
        if (!cx->compartment()->wrap(cx, &wrappedRejectFn))
            return nullptr;
        promise->initFixedSlot(PromiseSlot_RejectFunction, ObjectValue(*wrappedRejectFn));
    } else {
        promise->initFixedSlot(PromiseSlot_RejectFunction, ObjectValue(*rejectFn));
    }

    // Step 9.
    bool success;
    {
        FixedInvokeArgs<2> args(cx);

        args[0].setObject(*resolveFn);
        args[1].setObject(*rejectFn);

        RootedValue calleeOrRval(cx, ObjectValue(*executor));
        success = Call(cx, calleeOrRval, UndefinedHandleValue, args, &calleeOrRval);
    }

    // Step 10.
    if (!success) {
        RootedValue exceptionVal(cx);
        if (!MaybeGetAndClearException(cx, &exceptionVal))
            return nullptr;

        FixedInvokeArgs<1> args(cx);

        args[0].set(exceptionVal);

        RootedValue calleeOrRval(cx, ObjectValue(*rejectFn));
        if (!Call(cx, calleeOrRval, UndefinedHandleValue, args, &calleeOrRval))
            return nullptr;
    }

    // Let the Debugger know about this Promise.
    Debugger::onNewPromise(cx, promise);

    // Step 11.
    return promise;
}

// ES2016, 25.4.3.1. skipping creation of resolution functions and executor
// function invocation.
/* static */ PromiseObject*
PromiseObject::createSkippingExecutor(JSContext* cx)
{
    return CreatePromiseObjectWithoutResolutionFunctions(cx);
}

static MOZ_MUST_USE bool PerformPromiseAll(JSContext *cx, JS::ForOfIterator& iterator,
                                           HandleObject C, HandleObject promiseObj,
                                           HandleObject resolve, HandleObject reject,
                                           bool* done);

// ES2016, 25.4.4.1.
static bool
Promise_static_all(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedValue iterable(cx, args.get(0));

    // Step 2 (reordered).
    RootedValue CVal(cx, args.thisv());
    if (!CVal.isObject()) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_NOT_NONNULL_OBJECT,
                                  "Receiver of Promise.all call");
        return false;
    }

    // Step 1.
    RootedObject C(cx, &CVal.toObject());

    // Step 3.
    RootedObject resultPromise(cx);
    RootedObject resolve(cx);
    RootedObject reject(cx);
    if (!NewPromiseCapability(cx, C, &resultPromise, &resolve, &reject, false))
        return false;

    // Steps 4-5.
    JS::ForOfIterator iter(cx);
    if (!iter.init(iterable, JS::ForOfIterator::AllowNonIterable))
        return AbruptRejectPromise(cx, args, resultPromise, reject);

    if (!iter.valueIsIterable()) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_NOT_ITERABLE,
                                  "Argument of Promise.all");
        return AbruptRejectPromise(cx, args, resultPromise, reject);
    }

    // Step 6 (implicit).

    // Step 7.
    bool done;
    bool result = PerformPromiseAll(cx, iter, C, resultPromise, resolve, reject, &done);

    // Step 8.
    if (!result) {
        // Step 8.a.
        if (!done)
            iter.closeThrow();

        // Step 8.b.
        return AbruptRejectPromise(cx, args, resultPromise, reject);
    }

    // Step 9.
    args.rval().setObject(*resultPromise);
    return true;
}

static MOZ_MUST_USE bool PerformPromiseThen(JSContext* cx, Handle<PromiseObject*> promise,
                                            HandleValue onFulfilled_, HandleValue onRejected_,
                                            HandleObject resultPromise,
                                            HandleObject resolve, HandleObject reject);

static MOZ_MUST_USE bool
PerformPromiseThenWithoutSettleHandlers(JSContext* cx, Handle<PromiseObject*> promise,
                                        Handle<PromiseObject*> promiseToResolve,
                                        HandleObject resultPromise, HandleObject resolve,
                                        HandleObject reject);

static bool PromiseAllResolveElementFunction(JSContext* cx, unsigned argc, Value* vp);

// Unforgeable version of ES2016, 25.4.4.1.
MOZ_MUST_USE JSObject*
js::GetWaitForAllPromise(JSContext* cx, const JS::AutoObjectVector& promises)
{
#ifdef DEBUG
    for (size_t i = 0, len = promises.length(); i < len; i++) {
        JSObject* obj = promises[i];
        assertSameCompartment(cx, obj);
        MOZ_ASSERT(UncheckedUnwrap(obj)->is<PromiseObject>());
    }
#endif

    // Step 1.
    RootedObject C(cx, GlobalObject::getOrCreatePromiseConstructor(cx, cx->global()));
    if (!C)
        return nullptr;

    // Step 2 (omitted).

    // Step 3.
    RootedObject resultPromise(cx);
    RootedObject resolve(cx);
    RootedObject reject(cx);
    if (!NewPromiseCapability(cx, C, &resultPromise, &resolve, &reject, false))
        return nullptr;

    // Steps 4-6 (omitted).

    // Step 7.
    // Implemented as an inlined, simplied version of ES2016 25.4.4.1.1, PerformPromiseAll.
    {
        uint32_t promiseCount = promises.length();
        // Sub-steps 1-2 (omitted).

        // Sub-step 3.
        RootedNativeObject valuesArray(cx, NewDenseFullyAllocatedArray(cx, promiseCount));
        if (!valuesArray)
            return nullptr;
        if (valuesArray->ensureDenseElements(cx, 0, promiseCount) != DenseElementResult::Success)
            return nullptr;

        // Sub-step 4.
        // Create our data holder that holds all the things shared across
        // every step of the iterator.  In particular, this holds the
        // remainingElementsCount (as an integer reserved slot), the array of
        // values, and the resolve function from our PromiseCapability.
        RootedValue valuesArrayVal(cx, ObjectValue(*valuesArray));
        Rooted<PromiseAllDataHolder*> dataHolder(cx, NewPromiseAllDataHolder(cx, resultPromise,
                                                                             valuesArrayVal,
                                                                             resolve));
        if (!dataHolder)
            return nullptr;
        RootedValue dataHolderVal(cx, ObjectValue(*dataHolder));

        // Sub-step 5 (inline in loop-header below).

        // Sub-step 6.
        for (uint32_t index = 0; index < promiseCount; index++) {
            // Steps a-c (omitted).
            // Step d (implemented after the loop).
            // Steps e-g (omitted).

            // Step h.
            valuesArray->setDenseElement(index, UndefinedHandleValue);

            // Step i, vastly simplified.
            RootedObject nextPromiseObj(cx, promises[index]);

            // Step j.
            RootedFunction resolveFunc(cx, NewNativeFunction(cx, PromiseAllResolveElementFunction,
                                                             1, nullptr,
                                                             gc::AllocKind::FUNCTION_EXTENDED,
                                                             GenericObject));
            if (!resolveFunc)
                return nullptr;

            // Steps k-o.
            resolveFunc->setExtendedSlot(PromiseAllResolveElementFunctionSlot_Data, dataHolderVal);
            resolveFunc->setExtendedSlot(PromiseAllResolveElementFunctionSlot_ElementIndex,
                                         Int32Value(index));

            // Step p.
            dataHolder->increaseRemainingCount();

            // Step q, very roughly.
            RootedValue resolveFunVal(cx, ObjectValue(*resolveFunc));
            RootedValue rejectFunVal(cx, ObjectValue(*reject));
            Rooted<PromiseObject*> nextPromise(cx);

            // GetWaitForAllPromise is used internally only and must not
            // trigger content-observable effects when registering a reaction.
            // It's also meant to work on wrapped Promises, potentially from
            // compartments with principals inaccessible from the current
            // compartment. To make that work, it unwraps promises with
            // UncheckedUnwrap,
            nextPromise = &UncheckedUnwrap(nextPromiseObj)->as<PromiseObject>();

            if (!PerformPromiseThen(cx, nextPromise, resolveFunVal, rejectFunVal,
                                    resultPromise, nullptr, nullptr))
            {
                return nullptr;
            }

            // Step r (inline in loop-header).
        }

        // Sub-step d.i (implicit).
        // Sub-step d.ii.
        int32_t remainingCount = dataHolder->decreaseRemainingCount();

        // Sub-step d.iii-iv.
        if (remainingCount == 0) {
            RootedValue valuesArrayVal(cx, ObjectValue(*valuesArray));
            if (!ResolvePromiseInternal(cx, resultPromise, valuesArrayVal))
                return nullptr;
        }
    }

    // Step 8 (omitted).

    // Step 9.
    return resultPromise;
}

static MOZ_MUST_USE bool
RunResolutionFunction(JSContext *cx, HandleObject resolutionFun, HandleValue result,
                      ResolutionMode mode, HandleObject promiseObj)
{
    // The absence of a resolve/reject function can mean that, as an
    // optimization, those weren't created. In that case, a flag is set on
    // the Promise object. (It's also possible to not have a resolution
    // function without that flag being set. This can occur if a Promise
    // subclass constructor passes null/undefined to `super()`.)
    // There are also reactions where the Promise itself is missing. For
    // those, there's nothing left to do here.
    assertSameCompartment(cx, resolutionFun);
    assertSameCompartment(cx, result);
    assertSameCompartment(cx, promiseObj);
    if (resolutionFun) {
        RootedValue calleeOrRval(cx, ObjectValue(*resolutionFun));
        FixedInvokeArgs<1> resolveArgs(cx);
        resolveArgs[0].set(result);
        return Call(cx, calleeOrRval, UndefinedHandleValue, resolveArgs, &calleeOrRval);
    }

    if (!promiseObj)
        return true;

    Rooted<PromiseObject*> promise(cx, &promiseObj->as<PromiseObject>());
    if (promise->state() != JS::PromiseState::Pending)
        return true;

    if (!PromiseHasAnyFlag(*promise, PROMISE_FLAG_DEFAULT_RESOLVING_FUNCTIONS))
        return true;

    if (mode == ResolveMode)
        return ResolvePromiseInternal(cx, promise, result);

    return RejectMaybeWrappedPromise(cx, promiseObj, result);
}

// ES2016, 25.4.4.1.1.
static MOZ_MUST_USE bool
PerformPromiseAll(JSContext *cx, JS::ForOfIterator& iterator, HandleObject C,
                  HandleObject promiseObj, HandleObject resolve, HandleObject reject,
                  bool* done)
{
    *done = false;

    RootedObject unwrappedPromiseObj(cx);
    if (IsWrapper(promiseObj)) {
        unwrappedPromiseObj = CheckedUnwrap(promiseObj);
        MOZ_ASSERT(unwrappedPromiseObj);
    }

    // Step 1.
    MOZ_ASSERT(C->isConstructor());
    RootedValue CVal(cx, ObjectValue(*C));

    // Step 2 (omitted).

    // Step 3.
    // We have to be very careful about which compartments we create things in
    // here.  In particular, we have to maintain the invariant that anything
    // stored in a reserved slot is same-compartment with the object whose
    // reserved slot it's in.  But we want to create the values array in the
    // Promise's compartment, because that array can get exposed to
    // code that has access to the Promise (in particular code from
    // that compartment), and that should work, even if the Promise
    // compartment is less-privileged than our caller compartment.
    //
    // So the plan is as follows: Create the values array in the promise
    // compartment.  Create the PromiseAllResolveElement function
    // and the data holder in our current compartment.  Store a
    // cross-compartment wrapper to the values array in the holder.  This
    // should be OK because the only things we hand the
    // PromiseAllResolveElement function to are the "then" calls we do and in
    // the case when the Promise's compartment is not the current compartment
    // those are happening over Xrays anyway, which means they get the
    // canonical "then" function and content can't see our
    // PromiseAllResolveElement.
    RootedObject valuesArray(cx);
    if (unwrappedPromiseObj) {
        JSAutoRealm ar(cx, unwrappedPromiseObj);
        valuesArray = NewDenseFullyAllocatedArray(cx, 0);
    } else {
        valuesArray = NewDenseFullyAllocatedArray(cx, 0);
    }
    if (!valuesArray)
        return false;

    RootedValue valuesArrayVal(cx, ObjectValue(*valuesArray));
    if (!cx->compartment()->wrap(cx, &valuesArrayVal))
        return false;

    // Step 4.
    // Create our data holder that holds all the things shared across
    // every step of the iterator.  In particular, this holds the
    // remainingElementsCount (as an integer reserved slot), the array of
    // values, and the resolve function from our PromiseCapability.
    Rooted<PromiseAllDataHolder*> dataHolder(cx, NewPromiseAllDataHolder(cx, promiseObj,
                                                                         valuesArrayVal, resolve));
    if (!dataHolder)
        return false;
    RootedValue dataHolderVal(cx, ObjectValue(*dataHolder));

    // Step 5.
    uint32_t index = 0;

    // Step 6.
    RootedValue nextValue(cx);
    RootedId indexId(cx);
    RootedValue rejectFunVal(cx, ObjectValue(*reject));

    while (true) {
        // Steps a-c, e-g.
        if (!iterator.next(&nextValue, done)) {
            // Steps b, f.
            *done = true;

            // Steps c, g.
            return false;
        }

        // Step d.
        if (*done) {
            // Step d.i (implicit).

            // Step d.ii.
            int32_t remainingCount = dataHolder->decreaseRemainingCount();

            // Steps d.iii-iv.
            if (remainingCount == 0) {
                return RunResolutionFunction(cx, resolve, valuesArrayVal, ResolveMode,
                                             promiseObj);
            }

            // We're all set for now!
            return true;
        }

        // Step h.
        { // Scope for the JSAutoRealm we need to work with valuesArray.  We
            // mostly do this for performance; we could go ahead and do the define via
            // a cross-compartment proxy instead...
            JSAutoRealm ar(cx, valuesArray);
            indexId = INT_TO_JSID(index);
            if (!DefineDataProperty(cx, valuesArray, indexId, UndefinedHandleValue))
                return false;
        }

        // Step i.
        // Sadly, because someone could have overridden
        // "resolve" on the canonical Promise constructor.
        RootedValue nextPromise(cx);
        RootedValue staticResolve(cx);
        if (!GetProperty(cx, C, CVal, cx->names().resolve, &staticResolve))
            return false;

        FixedInvokeArgs<1> resolveArgs(cx);
        resolveArgs[0].set(nextValue);
        if (!Call(cx, staticResolve, CVal, resolveArgs, &nextPromise))
            return false;

        // Step j.
        RootedFunction resolveFunc(cx, NewNativeFunction(cx, PromiseAllResolveElementFunction,
                                                         1, nullptr,
                                                         gc::AllocKind::FUNCTION_EXTENDED,
                                                         GenericObject));
        if (!resolveFunc)
            return false;

        // Steps k,m,n.
        resolveFunc->setExtendedSlot(PromiseAllResolveElementFunctionSlot_Data, dataHolderVal);

        // Step l.
        resolveFunc->setExtendedSlot(PromiseAllResolveElementFunctionSlot_ElementIndex,
                                     Int32Value(index));

        // Steps o-p.
        dataHolder->increaseRemainingCount();

        // Step q.
        RootedValue resolveFunVal(cx, ObjectValue(*resolveFunc));
        if (!BlockOnPromise(cx, nextPromise, promiseObj, resolveFunVal, rejectFunVal, true))
            return false;

        // Step r.
        index++;
        MOZ_ASSERT(index > 0);
    }
}

// ES2016, 25.4.4.1.2.
static bool
PromiseAllResolveElementFunction(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    RootedFunction resolve(cx, &args.callee().as<JSFunction>());
    RootedValue xVal(cx, args.get(0));

    // Step 1.
    RootedValue dataVal(cx, resolve->getExtendedSlot(PromiseAllResolveElementFunctionSlot_Data));

    // Step 2.
    // We use the existence of the data holder as a signal for whether the
    // Promise was already resolved. Upon resolution, it's reset to
    // `undefined`.
    if (dataVal.isUndefined()) {
        args.rval().setUndefined();
        return true;
    }

    Rooted<PromiseAllDataHolder*> data(cx, &dataVal.toObject().as<PromiseAllDataHolder>());

    // Step 3.
    resolve->setExtendedSlot(PromiseAllResolveElementFunctionSlot_Data, UndefinedValue());

    // Step 4.
    int32_t index = resolve->getExtendedSlot(PromiseAllResolveElementFunctionSlot_ElementIndex)
                    .toInt32();

    // Step 5.
    RootedValue valuesVal(cx, data->valuesArray());
    RootedObject valuesObj(cx, &valuesVal.toObject());
    bool valuesListIsWrapped = false;
    if (IsWrapper(valuesObj)) {
        valuesListIsWrapped = true;
        // See comment for PerformPromiseAll, step 3 for why we unwrap here.
        valuesObj = UncheckedUnwrap(valuesObj);
    }
    if (JS_IsDeadWrapper(valuesObj)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_DEAD_OBJECT);
        return false;
    }
    RootedNativeObject values(cx, &valuesObj->as<NativeObject>());

    // Step 6 (moved under step 10).
    // Step 7 (moved to step 9).

    // Step 8.
    // The index is guaranteed to be initialized to `undefined`.
    if (valuesListIsWrapped) {
        AutoRealm ar(cx, values);
        if (!cx->compartment()->wrap(cx, &xVal))
            return false;
    }
    values->setDenseElement(index, xVal);

    // Steps 7,9.
    uint32_t remainingCount = data->decreaseRemainingCount();

    // Step 10.
    if (remainingCount == 0) {
        // Step 10.a. (Omitted, happened in PerformPromiseAll.)
        // Step 10.b.

        // Step 6 (Adapted to work with PromiseAllDataHolder's layout).
        RootedObject resolveAllFun(cx, data->resolveObj());
        RootedObject promiseObj(cx, data->promiseObj());
        if (!RunResolutionFunction(cx, resolveAllFun, valuesVal, ResolveMode, promiseObj))
            return false;
    }

    // Step 11.
    args.rval().setUndefined();
    return true;
}

static MOZ_MUST_USE bool PerformPromiseRace(JSContext *cx, JS::ForOfIterator& iterator,
                                            HandleObject C, HandleObject promiseObj,
                                            HandleObject resolve, HandleObject reject,
                                            bool* done);

// ES2016, 25.4.4.3.
static bool
Promise_static_race(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedValue iterable(cx, args.get(0));

    // Step 2 (reordered).
    RootedValue CVal(cx, args.thisv());
    if (!CVal.isObject()) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_NOT_NONNULL_OBJECT,
                                  "Receiver of Promise.race call");
        return false;
    }

    // Step 1.
    RootedObject C(cx, &CVal.toObject());

    // Step 3.
    RootedObject resultPromise(cx);
    RootedObject resolve(cx);
    RootedObject reject(cx);
    if (!NewPromiseCapability(cx, C, &resultPromise, &resolve, &reject, false))
        return false;

    // Steps 4-5.
    JS::ForOfIterator iter(cx);
    if (!iter.init(iterable, JS::ForOfIterator::AllowNonIterable))
        return AbruptRejectPromise(cx, args, resultPromise, reject);

    if (!iter.valueIsIterable()) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_NOT_ITERABLE,
                                  "Argument of Promise.race");
        return AbruptRejectPromise(cx, args, resultPromise, reject);
    }

    // Step 6 (implicit).

    // Step 7.
    bool done;
    bool result = PerformPromiseRace(cx, iter, C, resultPromise, resolve, reject, &done);

    // Step 8.
    if (!result) {
        // Step 8.a.
        if (!done)
            iter.closeThrow();

        // Step 8.b.
        return AbruptRejectPromise(cx, args, resultPromise, reject);
    }

    // Step 9.
    args.rval().setObject(*resultPromise);
    return true;
}

// ES2016, 25.4.4.3.1.
static MOZ_MUST_USE bool
PerformPromiseRace(JSContext *cx, JS::ForOfIterator& iterator, HandleObject C,
                   HandleObject promiseObj, HandleObject resolve, HandleObject reject,
                   bool* done)
{
    *done = false;
    MOZ_ASSERT(C->isConstructor());
    RootedValue CVal(cx, ObjectValue(*C));

    // BlockOnPromise fast path requires the passed onFulfilled function
    // doesn't return an object value, because otherwise the skipped promise
    // creation is detectable due to missing property lookups.
    bool isDefaultResolveFn = IsNativeFunction(resolve, ResolvePromiseFunction);

    RootedValue nextValue(cx);
    RootedValue resolveFunVal(cx, ObjectValue(*resolve));
    RootedValue rejectFunVal(cx, ObjectValue(*reject));

    while (true) {
        // Steps a-c, e-g.
        if (!iterator.next(&nextValue, done)) {
            // Steps b, f.
            *done = true;

            // Steps c, g.
            return false;
        }

        // Step d.
        if (*done) {
            // Step d.i (implicit).

            // Step d.ii.
            return true;
        }

        // Step h.
        // Sadly, because someone could have overridden
        // "resolve" on the canonical Promise constructor.
        RootedValue nextPromise(cx);
        RootedValue staticResolve(cx);
        if (!GetProperty(cx, C, CVal, cx->names().resolve, &staticResolve))
            return false;

        FixedInvokeArgs<1> resolveArgs(cx);
        resolveArgs[0].set(nextValue);
        if (!Call(cx, staticResolve, CVal, resolveArgs, &nextPromise))
            return false;

        // Step i.
        if (!BlockOnPromise(cx, nextPromise, promiseObj, resolveFunVal, rejectFunVal,
                            isDefaultResolveFn))
        {
            return false;
        }
    }

    MOZ_ASSERT_UNREACHABLE("Shouldn't reach the end of PerformPromiseRace");
}


// ES2016, Sub-steps of 25.4.4.4 and 25.4.4.5.
static MOZ_MUST_USE JSObject*
CommonStaticResolveRejectImpl(JSContext* cx, HandleValue thisVal, HandleValue argVal,
                              ResolutionMode mode)
{
    // Steps 1-2.
    if (!thisVal.isObject()) {
        const char* msg = mode == ResolveMode
                          ? "Receiver of Promise.resolve call"
                          : "Receiver of Promise.reject call";
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_NOT_NONNULL_OBJECT, msg);
        return nullptr;
    }
    RootedObject C(cx, &thisVal.toObject());

    // Step 3 of Resolve.
    if (mode == ResolveMode && argVal.isObject()) {
        RootedObject xObj(cx, &argVal.toObject());
        bool isPromise = false;
        if (xObj->is<PromiseObject>()) {
            isPromise = true;
        } else if (IsWrapper(xObj)) {
            // Treat instances of Promise from other compartments as Promises
            // here, too.
            // It's important to do the GetProperty for the `constructor`
            // below through the wrapper, because wrappers can change the
            // outcome, so instead of unwrapping and then performing the
            // GetProperty, just check here and then operate on the original
            // object again.
            RootedObject unwrappedObject(cx, CheckedUnwrap(xObj));
            if (unwrappedObject && unwrappedObject->is<PromiseObject>())
                isPromise = true;
        }
        if (isPromise) {
            RootedValue ctorVal(cx);
            if (!GetProperty(cx, xObj, xObj, cx->names().constructor, &ctorVal))
                return nullptr;
            if (ctorVal == thisVal)
                return xObj;
        }
    }

    // Step 4 of Resolve, 3 of Reject.
    RootedObject promise(cx);
    RootedObject resolveFun(cx);
    RootedObject rejectFun(cx);
    if (!NewPromiseCapability(cx, C, &promise, &resolveFun, &rejectFun, true))
        return nullptr;

    // Step 5 of Resolve, 4 of Reject.
    if (!RunResolutionFunction(cx, mode == ResolveMode ? resolveFun : rejectFun, argVal, mode,
                               promise))
    {
        return nullptr;
    }

    // Step 6 of Resolve, 4 of Reject.
    return promise;
}

MOZ_MUST_USE JSObject*
js::PromiseResolve(JSContext* cx, HandleObject constructor, HandleValue value)
{
    RootedValue C(cx, ObjectValue(*constructor));
    return CommonStaticResolveRejectImpl(cx, C, value, ResolveMode);
}

/**
 * ES2016, 25.4.4.4, Promise.reject.
 */
static bool
Promise_reject(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedValue thisVal(cx, args.thisv());
    RootedValue argVal(cx, args.get(0));
    JSObject* result = CommonStaticResolveRejectImpl(cx, thisVal, argVal, RejectMode);
    if (!result)
        return false;
    args.rval().setObject(*result);
    return true;
}

/**
 * Unforgeable version of ES2016, 25.4.4.4, Promise.reject.
 */
/* static */ JSObject*
PromiseObject::unforgeableReject(JSContext* cx, HandleValue value)
{
    RootedObject promiseCtor(cx, JS::GetPromiseConstructor(cx));
    if (!promiseCtor)
        return nullptr;
    RootedValue cVal(cx, ObjectValue(*promiseCtor));
    return CommonStaticResolveRejectImpl(cx, cVal, value, RejectMode);
}

/**
 * ES2016, 25.4.4.5, Promise.resolve.
 */
static bool
Promise_static_resolve(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedValue thisVal(cx, args.thisv());
    RootedValue argVal(cx, args.get(0));
    JSObject* result = CommonStaticResolveRejectImpl(cx, thisVal, argVal, ResolveMode);
    if (!result)
        return false;
    args.rval().setObject(*result);
    return true;
}

/**
 * Unforgeable version of ES2016, 25.4.4.5, Promise.resolve.
 */
/* static */ JSObject*
PromiseObject::unforgeableResolve(JSContext* cx, HandleValue value)
{
    RootedObject promiseCtor(cx, JS::GetPromiseConstructor(cx));
    if (!promiseCtor)
        return nullptr;
    RootedValue cVal(cx, ObjectValue(*promiseCtor));
    return CommonStaticResolveRejectImpl(cx, cVal, value, ResolveMode);
}

/**
 * ES2016, 25.4.4.6 get Promise [ @@species ]
 */
static bool
Promise_static_species(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    // Step 1: Return the this value.
    args.rval().set(args.thisv());
    return true;
}

// ES2016, 25.4.5.1, implemented in Promise.js.

enum class IncumbentGlobalObject {
    Yes, No
};

static PromiseReactionRecord*
NewReactionRecord(JSContext* cx, HandleObject resultPromise, HandleValue onFulfilled,
                  HandleValue onRejected, HandleObject resolve, HandleObject reject,
                  IncumbentGlobalObject incumbentGlobalObjectOption)
{
    // Either of the following conditions must be met:
    //   * resultPromise is a PromiseObject
    //   * resolve and reject are callable
    // except for Async Generator, there resultPromise can be nullptr.
    MOZ_ASSERT_IF(resultPromise && !resultPromise->is<PromiseObject>(), resolve);
    MOZ_ASSERT_IF(resultPromise && !resultPromise->is<PromiseObject>(), IsCallable(resolve));
    MOZ_ASSERT_IF(resultPromise && !resultPromise->is<PromiseObject>(), reject);
    MOZ_ASSERT_IF(resultPromise && !resultPromise->is<PromiseObject>(), IsCallable(reject));

    // Ensure the onFulfilled handler has the expected type.
    MOZ_ASSERT(onFulfilled.isInt32() || onFulfilled.isObjectOrNull());
    MOZ_ASSERT_IF(onFulfilled.isObject(), IsCallable(onFulfilled));
    MOZ_ASSERT_IF(onFulfilled.isInt32(),
                  0 <= onFulfilled.toInt32() && onFulfilled.toInt32() < PromiseHandlerLimit);

    // Ensure the onRejected handler has the expected type.
    MOZ_ASSERT(onRejected.isInt32() || onRejected.isObjectOrNull());
    MOZ_ASSERT_IF(onRejected.isObject(), IsCallable(onRejected));
    MOZ_ASSERT_IF(onRejected.isInt32(),
                  0 <= onRejected.toInt32() && onRejected.toInt32() < PromiseHandlerLimit);

    // Handlers must either both be present or both be absent.
    MOZ_ASSERT(onFulfilled.isNull() == onRejected.isNull());

    RootedObject incumbentGlobalObject(cx);
    if (incumbentGlobalObjectOption == IncumbentGlobalObject::Yes) {
        if (!GetObjectFromIncumbentGlobal(cx, &incumbentGlobalObject))
            return nullptr;
    }

    Rooted<PromiseReactionRecord*> reaction(cx, NewObjectWithClassProto<PromiseReactionRecord>(cx));
    if (!reaction)
        return nullptr;

    assertSameCompartment(cx, resultPromise);
    assertSameCompartment(cx, onFulfilled);
    assertSameCompartment(cx, onRejected);
    assertSameCompartment(cx, resolve);
    assertSameCompartment(cx, reject);
    assertSameCompartment(cx, incumbentGlobalObject);

    reaction->setFixedSlot(ReactionRecordSlot_Promise, ObjectOrNullValue(resultPromise));
    reaction->setFixedSlot(ReactionRecordSlot_Flags, Int32Value(0));
    reaction->setFixedSlot(ReactionRecordSlot_OnFulfilled, onFulfilled);
    reaction->setFixedSlot(ReactionRecordSlot_OnRejected, onRejected);
    reaction->setFixedSlot(ReactionRecordSlot_Resolve, ObjectOrNullValue(resolve));
    reaction->setFixedSlot(ReactionRecordSlot_Reject, ObjectOrNullValue(reject));
    reaction->setFixedSlot(ReactionRecordSlot_IncumbentGlobalObject,
                           ObjectOrNullValue(incumbentGlobalObject));

    return reaction;
}

static bool
IsPromiseSpecies(JSContext* cx, JSFunction* species)
{
    return species->maybeNative() == Promise_static_species;
}

static bool
PromiseThenNewPromiseCapability(JSContext* cx, HandleObject promiseObj,
                                CreateDependentPromise createDependent,
                                MutableHandleObject resultPromise,
                                MutableHandleObject resolve, MutableHandleObject reject)
{
    if (createDependent != CreateDependentPromise::Never) {
        // Step 3.
        RootedObject C(cx, SpeciesConstructor(cx, promiseObj, JSProto_Promise, IsPromiseSpecies));
        if (!C)
            return false;

        if (createDependent == CreateDependentPromise::Always ||
            !IsNativeFunction(C, PromiseConstructor))
        {
            // Step 4.
            if (!NewPromiseCapability(cx, C, resultPromise, resolve, reject, true))
                return false;
        }
    }

    return true;
}

// ES2016, 25.4.5.3., steps 3-5.
MOZ_MUST_USE bool
js::OriginalPromiseThen(JSContext* cx, Handle<PromiseObject*> promise,
                        HandleValue onFulfilled, HandleValue onRejected,
                        MutableHandleObject dependent, CreateDependentPromise createDependent)
{
    RootedObject promiseObj(cx, promise);
    if (promise->compartment() != cx->compartment()) {
        if (!cx->compartment()->wrap(cx, &promiseObj))
            return false;
    }

    // Steps 3-4.
    RootedObject resultPromise(cx);
    RootedObject resolve(cx);
    RootedObject reject(cx);
    if (!PromiseThenNewPromiseCapability(cx, promiseObj, createDependent, &resultPromise,
                                         &resolve, &reject))
    {
        return false;
    }

    // Step 5.
    if (!PerformPromiseThen(cx, promise, onFulfilled, onRejected, resultPromise, resolve, reject))
        return false;

    dependent.set(resultPromise);
    return true;
}

static MOZ_MUST_USE bool
OriginalPromiseThenWithoutSettleHandlers(JSContext* cx, Handle<PromiseObject*> promise,
                                         Handle<PromiseObject*> promiseToResolve)
{
    assertSameCompartment(cx, promise);

    // Steps 3-4.
    RootedObject resultPromise(cx);
    RootedObject resolve(cx);
    RootedObject reject(cx);
    if (!PromiseThenNewPromiseCapability(cx, promise, CreateDependentPromise::SkipIfCtorUnobservable,
                                         &resultPromise, &resolve, &reject))
    {
        return false;
    }

    // Step 5.
    return PerformPromiseThenWithoutSettleHandlers(cx, promise, promiseToResolve, resultPromise,
                                                   resolve, reject);
}

static MOZ_MUST_USE bool PerformPromiseThenWithReaction(JSContext* cx,
                                                        Handle<PromiseObject*> promise,
                                                        Handle<PromiseReactionRecord*> reaction);

// Some async/await functions are implemented here instead of
// js/src/builtin/AsyncFunction.cpp, to call Promise internal functions.

// ES 2018 draft 14.6.11 and 14.7.14 step 1.
MOZ_MUST_USE PromiseObject*
js::CreatePromiseObjectForAsync(JSContext* cx, HandleValue generatorVal)
{
    // Step 1.
    PromiseObject* promise = CreatePromiseObjectWithoutResolutionFunctions(cx);
    if (!promise)
        return nullptr;

    AddPromiseFlags(*promise, PROMISE_FLAG_ASYNC);
    promise->setFixedSlot(PromiseSlot_AwaitGenerator, generatorVal);
    return promise;
}

bool
js::IsPromiseForAsync(JSObject* promise)
{
    return promise->is<PromiseObject>() &&
           PromiseHasAnyFlag(promise->as<PromiseObject>(), PROMISE_FLAG_ASYNC);
}

// ES 2018 draft 25.5.5.2 steps 3.f, 3.g.
MOZ_MUST_USE bool
js::AsyncFunctionThrown(JSContext* cx, Handle<PromiseObject*> resultPromise)
{
    // Step 3.f.
    RootedValue exc(cx);
    if (!MaybeGetAndClearException(cx, &exc))
        return false;

    if (!RejectMaybeWrappedPromise(cx, resultPromise, exc))
        return false;

    // Step 3.g.
    return true;
}

// ES 2018 draft 25.5.5.2 steps 3.d-e, 3.g.
MOZ_MUST_USE bool
js::AsyncFunctionReturned(JSContext* cx, Handle<PromiseObject*> resultPromise, HandleValue value)
{
    // Steps 3.d-e.
    if (!ResolvePromiseInternal(cx, resultPromise, value))
        return false;

    // Step 3.g.
    return true;
}

// Helper function that performs the equivalent steps as
// Async Iteration proposal 4.1 Await steps 2-3, 6-9 or similar.
template <typename T>
static MOZ_MUST_USE bool
InternalAwait(JSContext* cx, HandleValue value, HandleObject resultPromise,
              HandleValue onFulfilled, HandleValue onRejected, T extraStep)
{
    MOZ_ASSERT(onFulfilled.isInt32());
    MOZ_ASSERT(onRejected.isInt32());

    // Step 2.
    Rooted<PromiseObject*> promise(cx, CreatePromiseObjectWithoutResolutionFunctions(cx));
    if (!promise)
        return false;

    // Step 3.
    if (!ResolvePromiseInternal(cx, promise, value))
        return false;

    // Step 7-8.
    Rooted<PromiseReactionRecord*> reaction(cx, NewReactionRecord(cx, resultPromise,
                                                                  onFulfilled, onRejected,
                                                                  nullptr, nullptr,
                                                                  IncumbentGlobalObject::Yes));
    if (!reaction)
        return false;

    // Step 6.
    extraStep(reaction);

    // Step 9.
    return PerformPromiseThenWithReaction(cx, promise, reaction);
}

// ES 2018 draft 25.5.5.3 steps 2-10.
MOZ_MUST_USE bool
js::AsyncFunctionAwait(JSContext* cx, Handle<PromiseObject*> resultPromise, HandleValue value)
{
    // Steps 4-5.
    RootedValue onFulfilled(cx, Int32Value(PromiseHandlerAsyncFunctionAwaitedFulfilled));
    RootedValue onRejected(cx, Int32Value(PromiseHandlerAsyncFunctionAwaitedRejected));

    // Steps 2-3, 6-10.
    auto extra = [](Handle<PromiseReactionRecord*> reaction) {
        reaction->setIsAsyncFunction();
    };
    return InternalAwait(cx, value, resultPromise, onFulfilled, onRejected, extra);
}

// Async Iteration proposal 4.1 Await steps 2-9.
MOZ_MUST_USE bool
js::AsyncGeneratorAwait(JSContext* cx, Handle<AsyncGeneratorObject*> asyncGenObj,
                        HandleValue value)
{
    // Steps 4-5.
    RootedValue onFulfilled(cx, Int32Value(PromiseHandlerAsyncGeneratorAwaitedFulfilled));
    RootedValue onRejected(cx, Int32Value(PromiseHandlerAsyncGeneratorAwaitedRejected));

    // Steps 2-3, 6-9.
    auto extra = [&](Handle<PromiseReactionRecord*> reaction) {
        reaction->setIsAsyncGenerator(asyncGenObj);
    };
    return InternalAwait(cx, value, nullptr, onFulfilled, onRejected, extra);
}

// Async Iteration proposal 11.1.3.2.1 %AsyncFromSyncIteratorPrototype%.next
// Async Iteration proposal 11.1.3.2.2 %AsyncFromSyncIteratorPrototype%.return
// Async Iteration proposal 11.1.3.2.3 %AsyncFromSyncIteratorPrototype%.throw
bool
js::AsyncFromSyncIteratorMethod(JSContext* cx, CallArgs& args, CompletionKind completionKind)
{
    // Step 1.
    RootedValue thisVal(cx, args.thisv());

    // Step 2.
    Rooted<PromiseObject*> resultPromise(cx, CreatePromiseObjectWithoutResolutionFunctions(cx));
    if (!resultPromise)
        return false;

    // Step 3.
    if (!thisVal.isObject() || !thisVal.toObject().is<AsyncFromSyncIteratorObject>()) {
        // NB: See https://github.com/tc39/proposal-async-iteration/issues/105
        // for why this check shouldn't be necessary as long as we can ensure
        // the Async-from-Sync iterator can't be accessed directly by user
        // code.

        // Step 3.a.
        RootedValue badGeneratorError(cx);
        if (!GetTypeError(cx, JSMSG_NOT_AN_ASYNC_ITERATOR, &badGeneratorError))
            return false;

        // Step 3.b.
        if (!RejectMaybeWrappedPromise(cx, resultPromise, badGeneratorError))
            return false;

        // Step 3.c.
        args.rval().setObject(*resultPromise);
        return true;
    }

    Rooted<AsyncFromSyncIteratorObject*> asyncIter(
        cx, &thisVal.toObject().as<AsyncFromSyncIteratorObject>());

    // Step 4.
    RootedObject iter(cx, asyncIter->iterator());

    RootedValue resultVal(cx);
    RootedValue func(cx);
    if (completionKind == CompletionKind::Normal) {
        // 11.1.3.2.1 steps 5-6 (partially).
        func.set(asyncIter->nextMethod());
    } else if (completionKind == CompletionKind::Return) {
        // 11.1.3.2.2 steps 5-6.
        if (!GetProperty(cx, iter, iter, cx->names().return_, &func))
            return AbruptRejectPromise(cx, args, resultPromise, nullptr);

        // Step 7.
        if (func.isNullOrUndefined()) {
            // Step 7.a.
            RootedObject resultObj(cx, CreateIterResultObject(cx, args.get(0), true));
            if (!resultObj)
                return AbruptRejectPromise(cx, args, resultPromise, nullptr);

            RootedValue resultVal(cx, ObjectValue(*resultObj));

            // Step 7.b.
            if (!ResolvePromiseInternal(cx, resultPromise, resultVal))
                return AbruptRejectPromise(cx, args, resultPromise, nullptr);

            // Step 7.c.
            args.rval().setObject(*resultPromise);
            return true;
        }
    } else {
        // 11.1.3.2.3 steps 5-6.
        MOZ_ASSERT(completionKind == CompletionKind::Throw);
        if (!GetProperty(cx, iter, iter, cx->names().throw_, &func))
            return AbruptRejectPromise(cx, args, resultPromise, nullptr);

        // Step 7.
        if (func.isNullOrUndefined()) {
            // Step 7.a.
            if (!RejectMaybeWrappedPromise(cx, resultPromise, args.get(0)))
                return AbruptRejectPromise(cx, args, resultPromise, nullptr);

            // Step 7.b.
            args.rval().setObject(*resultPromise);
            return true;
        }
    }

    // 11.1.3.2.1 steps 5-6 (partially).
    // 11.1.3.2.2, 11.1.3.2.3 steps 8-9.
    RootedValue iterVal(cx, ObjectValue(*iter));
    FixedInvokeArgs<1> args2(cx);
    args2[0].set(args.get(0));
    if (!js::Call(cx, func, iterVal, args2, &resultVal))
        return AbruptRejectPromise(cx, args, resultPromise, nullptr);

    // 11.1.3.2.1 steps 5-6 (partially).
    // 11.1.3.2.2, 11.1.3.2.3 steps 10.
    if (!resultVal.isObject()) {
        CheckIsObjectKind kind;
        switch (completionKind) {
          case CompletionKind::Normal:
            kind = CheckIsObjectKind::IteratorNext;
            break;
          case CompletionKind::Throw:
            kind = CheckIsObjectKind::IteratorThrow;
            break;
          case CompletionKind::Return:
            kind = CheckIsObjectKind::IteratorReturn;
            break;
        }
        MOZ_ALWAYS_FALSE(ThrowCheckIsObject(cx, kind));
        return AbruptRejectPromise(cx, args, resultPromise, nullptr);
    }

    RootedObject resultObj(cx, &resultVal.toObject());

    // Following step numbers are for 11.1.3.2.1.
    // For 11.1.3.2.2 and 11.1.3.2.3, steps 7-16 corresponds to steps 11-20.

    // Steps 7-8.
    RootedValue doneVal(cx);
    if (!GetProperty(cx, resultObj, resultObj, cx->names().done, &doneVal))
        return AbruptRejectPromise(cx, args, resultPromise, nullptr);
    bool done = ToBoolean(doneVal);

    // Steps 9-10.
    RootedValue value(cx);
    if (!GetProperty(cx, resultObj, resultObj, cx->names().value, &value))
        return AbruptRejectPromise(cx, args, resultPromise, nullptr);

    // Steps 13-14.
    RootedValue onFulfilled(cx, Int32Value(done
                                           ? PromiseHandlerAsyncFromSyncIteratorValueUnwrapDone
                                           : PromiseHandlerAsyncFromSyncIteratorValueUnwrapNotDone));
    RootedValue onRejected(cx, Int32Value(PromiseHandlerThrower));

    // Steps 11-12, 15.
    auto extra = [](Handle<PromiseReactionRecord*> reaction) {
    };
    if (!InternalAwait(cx, value, resultPromise, onFulfilled, onRejected, extra))
        return false;

    // Step 16.
    args.rval().setObject(*resultPromise);
    return true;
}

enum class ResumeNextKind {
    Enqueue, Reject, Resolve
};

static MOZ_MUST_USE bool
AsyncGeneratorResumeNext(JSContext* cx, Handle<AsyncGeneratorObject*> asyncGenObj,
                         ResumeNextKind kind, HandleValue valueOrException = UndefinedHandleValue,
                         bool done = false);

// Async Iteration proposal 11.4.3.3.
MOZ_MUST_USE bool
js::AsyncGeneratorResolve(JSContext* cx, Handle<AsyncGeneratorObject*> asyncGenObj,
                          HandleValue value, bool done)
{
    return AsyncGeneratorResumeNext(cx, asyncGenObj, ResumeNextKind::Resolve, value, done);
}

// Async Iteration proposal 11.4.3.4.
MOZ_MUST_USE bool
js::AsyncGeneratorReject(JSContext* cx, Handle<AsyncGeneratorObject*> asyncGenObj,
                         HandleValue exception)
{
    return AsyncGeneratorResumeNext(cx, asyncGenObj, ResumeNextKind::Reject, exception);
}

// Async Iteration proposal 11.4.3.5.
static MOZ_MUST_USE bool
AsyncGeneratorResumeNext(JSContext* cx, Handle<AsyncGeneratorObject*> asyncGenObj,
                         ResumeNextKind kind,
                         HandleValue valueOrException_ /* = UndefinedHandleValue */,
                         bool done /* = false */)
{
    RootedValue valueOrException(cx, valueOrException_);

    while (true) {
        switch (kind) {
          case ResumeNextKind::Enqueue:
            // No further action required.
            break;
          case ResumeNextKind::Reject: {
            // 11.4.3.4 AsyncGeneratorReject ( generator, exception )
            HandleValue exception = valueOrException;

            // Step 1 (implicit).

            // Steps 2-3.
            MOZ_ASSERT(!asyncGenObj->isQueueEmpty());

            // Step 4.
            Rooted<AsyncGeneratorRequest*> request(
                cx, AsyncGeneratorObject::dequeueRequest(cx, asyncGenObj));
            if (!request)
                return false;

            // Step 5.
            Rooted<PromiseObject*> resultPromise(cx, request->promise());

            asyncGenObj->cacheRequest(request);

            // Step 6.
            if (!RejectMaybeWrappedPromise(cx, resultPromise, exception))
                return false;

            // Steps 7-8.
            break;
          }
          case ResumeNextKind::Resolve: {
            // 11.4.3.3 AsyncGeneratorResolve ( generator, value, done )
            HandleValue value = valueOrException;

            // Step 1 (implicit).

            // Steps 2-3.
            MOZ_ASSERT(!asyncGenObj->isQueueEmpty());

            // Step 4.
            Rooted<AsyncGeneratorRequest*> request(
                cx, AsyncGeneratorObject::dequeueRequest(cx, asyncGenObj));
            if (!request)
                return false;

            // Step 5.
            Rooted<PromiseObject*> resultPromise(cx, request->promise());

            asyncGenObj->cacheRequest(request);

            // Step 6.
            RootedObject resultObj(cx, CreateIterResultObject(cx, value, done));
            if (!resultObj)
                return false;

            RootedValue resultValue(cx, ObjectValue(*resultObj));

            // Step 7.
            if (!ResolvePromiseInternal(cx, resultPromise, resultValue))
                return false;

            // Steps 8-9.
            break;
          }
        }

        // Step 1 (implicit).

        // Steps 2-3.
        MOZ_ASSERT(!asyncGenObj->isExecuting());

        // Step 4.
        if (asyncGenObj->isAwaitingYieldReturn() || asyncGenObj->isAwaitingReturn())
            return true;

        // Steps 5-6.
        if (asyncGenObj->isQueueEmpty())
            return true;

        // Steps 7-8.
        Rooted<AsyncGeneratorRequest*> request(
            cx, AsyncGeneratorObject::peekRequest(asyncGenObj));
        if (!request)
            return false;

        // Step 9.
        CompletionKind completionKind = request->completionKind();

        // Step 10.
        if (completionKind != CompletionKind::Normal) {
            // Step 10.a.
            if (asyncGenObj->isSuspendedStart())
                asyncGenObj->setCompleted();

            // Step 10.b.
            if (asyncGenObj->isCompleted()) {
                RootedValue value(cx, request->completionValue());

                // Step 10.b.i.
                if (completionKind == CompletionKind::Return) {
                    // Steps 10.b.i.1.
                    asyncGenObj->setAwaitingReturn();

                    // Steps 10.b.i.4-6 (reordered).
                    static constexpr int32_t ResumeNextReturnFulfilled =
                            PromiseHandlerAsyncGeneratorResumeNextReturnFulfilled;
                    static constexpr int32_t ResumeNextReturnRejected =
                            PromiseHandlerAsyncGeneratorResumeNextReturnRejected;

                    RootedValue onFulfilled(cx, Int32Value(ResumeNextReturnFulfilled));
                    RootedValue onRejected(cx, Int32Value(ResumeNextReturnRejected));

                    // Steps 10.b.i.2-3, 7-10.
                    auto extra = [&](Handle<PromiseReactionRecord*> reaction) {
                        reaction->setIsAsyncGenerator(asyncGenObj);
                    };
                    return InternalAwait(cx, value, nullptr, onFulfilled, onRejected, extra);
                }

                // Step 10.b.ii.1.
                MOZ_ASSERT(completionKind == CompletionKind::Throw);

                // Steps 10.b.ii.2-3.
                kind = ResumeNextKind::Reject;
                valueOrException.set(value);
                // |done| is unused for ResumeNextKind::Reject.
                continue;
            }
        } else if (asyncGenObj->isCompleted()) {
            // Step 11.
            kind = ResumeNextKind::Resolve;
            valueOrException.setUndefined();
            done = true;
            continue;
        }

        // Step 12.
        MOZ_ASSERT(asyncGenObj->isSuspendedStart() || asyncGenObj->isSuspendedYield());

        // Step 16 (reordered).
        asyncGenObj->setExecuting();

        RootedValue argument(cx, request->completionValue());

        if (completionKind == CompletionKind::Return) {
            // 11.4.3.7 AsyncGeneratorYield step 8.b-e.
            // Since we don't have the place that handles return from yield
            // inside the generator, handle the case here, with extra state
            // State_AwaitingYieldReturn.
            asyncGenObj->setAwaitingYieldReturn();

            static constexpr int32_t YieldReturnAwaitedFulfilled =
                    PromiseHandlerAsyncGeneratorYieldReturnAwaitedFulfilled;
            static constexpr int32_t YieldReturnAwaitedRejected =
                    PromiseHandlerAsyncGeneratorYieldReturnAwaitedRejected;

            RootedValue onFulfilled(cx, Int32Value(YieldReturnAwaitedFulfilled));
            RootedValue onRejected(cx, Int32Value(YieldReturnAwaitedRejected));

            auto extra = [&](Handle<PromiseReactionRecord*> reaction) {
                reaction->setIsAsyncGenerator(asyncGenObj);
            };
            return InternalAwait(cx, argument, nullptr, onFulfilled, onRejected, extra);
        }

        // Steps 13-15, 17-21.
        return AsyncGeneratorResume(cx, asyncGenObj, completionKind, argument);
    }
}

// Async Iteration proposal 11.4.3.6.
MOZ_MUST_USE bool
js::AsyncGeneratorEnqueue(JSContext* cx, HandleValue asyncGenVal,
                          CompletionKind completionKind, HandleValue completionValue,
                          MutableHandleValue result)
{
    // Step 1 (implicit).

    // Step 2.
    Rooted<PromiseObject*> resultPromise(cx, CreatePromiseObjectWithoutResolutionFunctions(cx));
    if (!resultPromise)
        return false;

    // Step 3.
    if (!asyncGenVal.isObject() || !asyncGenVal.toObject().is<AsyncGeneratorObject>()) {
        // Step 3.a.
        RootedValue badGeneratorError(cx);
        if (!GetTypeError(cx, JSMSG_NOT_AN_ASYNC_GENERATOR, &badGeneratorError))
            return false;

        // Step 3.b.
        if (!RejectMaybeWrappedPromise(cx, resultPromise, badGeneratorError))
            return false;

        // Step 3.c.
        result.setObject(*resultPromise);
        return true;
    }

    Rooted<AsyncGeneratorObject*> asyncGenObj(
        cx, &asyncGenVal.toObject().as<AsyncGeneratorObject>());

    // Step 5 (reordered).
    Rooted<AsyncGeneratorRequest*> request(
        cx, AsyncGeneratorObject::createRequest(cx, asyncGenObj, completionKind, completionValue,
                                                resultPromise));
    if (!request)
        return false;

    // Steps 4, 6.
    if (!AsyncGeneratorObject::enqueueRequest(cx, asyncGenObj, request))
        return false;

    // Step 7.
    if (!asyncGenObj->isExecuting()) {
        // Step 8.
        if (!AsyncGeneratorResumeNext(cx, asyncGenObj, ResumeNextKind::Enqueue))
            return false;
    }

    // Step 9.
    result.setObject(*resultPromise);
    return true;
}

static bool
Promise_catch_impl(JSContext* cx, unsigned argc, Value* vp, bool rvalUsed)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    // Step 1.
    RootedValue thenVal(cx);
    if (!GetProperty(cx, args.thisv(), cx->names().then, &thenVal))
        return false;

    if (IsNativeFunction(thenVal, &Promise_then)) {
        return Promise_then_impl(cx, args.thisv(), UndefinedHandleValue, args.get(0),
                                 args.rval(), rvalUsed);
    }

    FixedInvokeArgs<2> iargs(cx);
    iargs[0].setUndefined();
    iargs[1].set(args.get(0));

    return Call(cx, thenVal, args.thisv(), iargs, args.rval());
}

static MOZ_ALWAYS_INLINE bool
IsPromiseThenOrCatchRetValImplicitlyUsed(JSContext* cx)
{
    // The returned promise of Promise#then and Promise#catch contains
    // stack info if async stack is enabled.  Even if their return value is not
    // used explicitly in the script, the stack info is observable in devtools
    // and profilers.  We shouldn't apply the optimization not to allocate the
    // returned Promise object if the it's implicitly used by them.
    //
    // FIXME: Once bug 1280819 gets fixed, we can use ShouldCaptureDebugInfo.
    if (!cx->options().asyncStack())
        return false;

    // If devtools is opened, the current realm will become debuggee.
    if (cx->realm()->isDebuggee())
        return true;

    // There are 2 profilers, and they can be independently enabled.
    if (cx->runtime()->geckoProfiler().enabled())
        return true;
    if (JS::IsProfileTimelineRecordingEnabled())
        return true;

    // The stack is also observable from Error#stack, but we don't care since
    // it's nonstandard feature.
    return false;
}

// ES2016, 25.4.5.3.
static bool
Promise_catch_noRetVal(JSContext* cx, unsigned argc, Value* vp)
{
    return Promise_catch_impl(cx, argc, vp, IsPromiseThenOrCatchRetValImplicitlyUsed(cx));
}

// ES2016, 25.4.5.3.
static bool
Promise_catch(JSContext* cx, unsigned argc, Value* vp)
{
    return Promise_catch_impl(cx, argc, vp, true);
}

static bool
Promise_then_impl(JSContext* cx, HandleValue promiseVal, HandleValue onFulfilled,
                  HandleValue onRejected, MutableHandleValue rval, bool rvalUsed)
{
    // Step 1 (implicit).
    // Step 2.
    if (!promiseVal.isObject()) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_NOT_NONNULL_OBJECT,
                                  "Receiver of Promise.prototype.then call");
        return false;
    }
    RootedObject promiseObj(cx, &promiseVal.toObject());
    Rooted<PromiseObject*> promise(cx);

    bool isPromise = promiseObj->is<PromiseObject>();
    if (isPromise) {
        promise = &promiseObj->as<PromiseObject>();
    } else {
        RootedObject unwrappedPromiseObj(cx, CheckedUnwrap(promiseObj));
        if (!unwrappedPromiseObj) {
            ReportAccessDenied(cx);
            return false;
        }
        if (!unwrappedPromiseObj->is<PromiseObject>()) {
            JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_INCOMPATIBLE_PROTO,
                                      "Promise", "then", "value");
            return false;
        }
        promise = &unwrappedPromiseObj->as<PromiseObject>();
    }

    // Steps 3-5.
    CreateDependentPromise createDependent = rvalUsed
                                             ? CreateDependentPromise::Always
                                             : CreateDependentPromise::SkipIfCtorUnobservable;
    RootedObject resultPromise(cx);
    if (!OriginalPromiseThen(cx, promise, onFulfilled, onRejected, &resultPromise,
                             createDependent))
    {
        return false;
    }

    if (rvalUsed)
        rval.setObject(*resultPromise);
    else
        rval.setUndefined();
    return true;
}

// ES2016, 25.4.5.3.
bool
Promise_then_noRetVal(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return Promise_then_impl(cx, args.thisv(), args.get(0), args.get(1), args.rval(),
                             IsPromiseThenOrCatchRetValImplicitlyUsed(cx));
}

// ES2016, 25.4.5.3.
static bool
Promise_then(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return Promise_then_impl(cx, args.thisv(), args.get(0), args.get(1), args.rval(), true);
}

// ES2016, 25.4.5.3.1.
static MOZ_MUST_USE bool
PerformPromiseThen(JSContext* cx, Handle<PromiseObject*> promise, HandleValue onFulfilled_,
                   HandleValue onRejected_, HandleObject resultPromise,
                   HandleObject resolve, HandleObject reject)
{
    // Step 1 (implicit).
    // Step 2 (implicit).

    // Step 3.
    RootedValue onFulfilled(cx, onFulfilled_);
    if (!IsCallable(onFulfilled))
        onFulfilled = Int32Value(PromiseHandlerIdentity);

    // Step 4.
    RootedValue onRejected(cx, onRejected_);
    if (!IsCallable(onRejected))
        onRejected = Int32Value(PromiseHandlerThrower);

    // Step 7.
    Rooted<PromiseReactionRecord*> reaction(cx, NewReactionRecord(cx, resultPromise,
                                                                  onFulfilled, onRejected,
                                                                  resolve, reject,
                                                                  IncumbentGlobalObject::Yes));
    if (!reaction)
        return false;

    return PerformPromiseThenWithReaction(cx, promise, reaction);
}

static MOZ_MUST_USE bool
PerformPromiseThenWithoutSettleHandlers(JSContext* cx, Handle<PromiseObject*> promise,
                                        Handle<PromiseObject*> promiseToResolve,
                                        HandleObject resultPromise, HandleObject resolve,
                                        HandleObject reject)
{
    // Step 1 (implicit).
    // Step 2 (implicit).

    // Step 3.
    HandleValue onFulfilled = NullHandleValue;

    // Step 4.
    HandleValue onRejected = NullHandleValue;

    // Step 7.
    Rooted<PromiseReactionRecord*> reaction(cx, NewReactionRecord(cx, resultPromise,
                                                                  onFulfilled, onRejected,
                                                                  resolve, reject,
                                                                  IncumbentGlobalObject::Yes));
    if (!reaction)
        return false;

    reaction->setIsDefaultResolvingHandler(promiseToResolve);

    return PerformPromiseThenWithReaction(cx, promise, reaction);
}

static MOZ_MUST_USE bool
PerformPromiseThenWithReaction(JSContext* cx, Handle<PromiseObject*> promise,
                               Handle<PromiseReactionRecord*> reaction)
{
    JS::PromiseState state = promise->state();
    int32_t flags = promise->flags();
    if (state == JS::PromiseState::Pending) {
        // Steps 5,6 (reordered).
        // Instead of creating separate reaction records for fulfillment and
        // rejection, we create a combined record. All places we use the record
        // can handle that.
        if (!AddPromiseReaction(cx, promise, reaction))
            return false;
    }

    // Steps 8,9.
    else {
        // Step 9.a.
        MOZ_ASSERT_IF(state != JS::PromiseState::Fulfilled, state == JS::PromiseState::Rejected);

        // Step 8.a. / 9.b.
        RootedValue valueOrReason(cx, promise->valueOrReason());

        // We might be operating on a promise from another compartment. In
        // that case, we need to wrap the result/reason value before using it.
        if (!cx->compartment()->wrap(cx, &valueOrReason))
            return false;

        // Step 9.c.
        if (state == JS::PromiseState::Rejected && !(flags & PROMISE_FLAG_HANDLED))
            cx->runtime()->removeUnhandledRejectedPromise(cx, promise);

        // Step 8.b. / 9.d.
        if (!EnqueuePromiseReactionJob(cx, reaction, valueOrReason, state))
            return false;
    }

    // Step 10.
    promise->setFixedSlot(PromiseSlot_Flags, Int32Value(flags | PROMISE_FLAG_HANDLED));

    // Step 11.
    return true;
}

/**
 * Calls |promise.then| with the provided hooks and adds |blockedPromise| to
 * its list of dependent promises. Used by |Promise.all| and |Promise.race|.
 *
 * If |promise.then| is the original |Promise.prototype.then| function and
 * the call to |promise.then| would use the original |Promise| constructor to
 * create the resulting promise, this function skips the call to |promise.then|
 * and thus creating a new promise that would not be observable by content.
 */
static MOZ_MUST_USE bool
BlockOnPromise(JSContext* cx, HandleValue promiseVal, HandleObject blockedPromise_,
               HandleValue onFulfilled, HandleValue onRejected, bool onFulfilledReturnsUndefined)
{
    RootedObject promiseObj(cx, ToObject(cx, promiseVal));
    if (!promiseObj)
        return false;

    RootedValue thenVal(cx);
    if (!GetProperty(cx, promiseObj, promiseVal, cx->names().then, &thenVal))
        return false;

    if (promiseObj->is<PromiseObject>() && IsNativeFunction(thenVal, Promise_then)) {
        // |promise| is an unwrapped Promise, and |then| is the original
        // |Promise.prototype.then|, inline it here.
        // 25.4.5.3., step 3.
        RootedObject PromiseCtor(cx,
                                 GlobalObject::getOrCreatePromiseConstructor(cx, cx->global()));
        if (!PromiseCtor)
            return false;

        RootedObject C(cx, SpeciesConstructor(cx, promiseObj, JSProto_Promise, IsPromiseSpecies));
        if (!C)
            return false;

        RootedObject resultPromise(cx);
        RootedObject resolveFun(cx);
        RootedObject rejectFun(cx);

        // By default, the blocked promise is added as an extra entry to the
        // rejected promises list.
        bool addToDependent = true;

        // Skip the creation of a built-in Promise object if:
        // 1. `C` is the built-in Promise constructor.
        // 2. The `onFulfilled` handler doesn't return an object, which
        //    ensures no side-effects take place in ResolvePromiseInternal.
        // 3. The blocked promise is a built-in Promise object.
        // 4. The blocked promise doesn't use the default resolving functions,
        //    which in turn means RunResolutionFunction when called from
        //    PromiseRectionJob won't try to resolve the promise.
        if (C == PromiseCtor &&
            onFulfilledReturnsUndefined &&
            blockedPromise_->is<PromiseObject>() &&
            !PromiseHasAnyFlag(blockedPromise_->as<PromiseObject>(),
                               PROMISE_FLAG_DEFAULT_RESOLVING_FUNCTIONS))
        {
            resultPromise.set(blockedPromise_);
            addToDependent = false;
        } else {
            // 25.4.5.3., step 4.
            if (!NewPromiseCapability(cx, C, &resultPromise, &resolveFun, &rejectFun, true))
                return false;
        }

        // 25.4.5.3., step 5.
        Rooted<PromiseObject*> promise(cx, &promiseObj->as<PromiseObject>());
        if (!PerformPromiseThen(cx, promise, onFulfilled, onRejected, resultPromise,
                                resolveFun, rejectFun))
        {
            return false;
        }

        if (!addToDependent)
            return true;
    } else {
        // Optimization failed, do the normal call.
        RootedValue rval(cx);
        if (!Call(cx, thenVal, promiseVal, onFulfilled, onRejected, &rval))
            return false;
    }

    // In case the value to depend on isn't an object at all, there's nothing
    // more to do here: we can only add reactions to Promise objects
    // (potentially after unwrapping them), and non-object values can't be
    // Promise objects. This can happen if Promise.all is called on an object
    // with a `resolve` method that returns primitives.
    if (!promiseVal.isObject())
        return true;

    // The object created by the |promise.then| call or the inlined version
    // of it above is visible to content (either because |promise.then| was
    // overridden by content and could leak it, or because a constructor
    // other than the original value of |Promise| was used to create it).
    // To have both that object and |blockedPromise| show up as dependent
    // promises in the debugger, add a dummy reaction to the list of reject
    // reactions that contains |blockedPromise|, but otherwise does nothing.
    RootedObject unwrappedPromiseObj(cx, promiseObj);
    RootedObject blockedPromise(cx, blockedPromise_);

    mozilla::Maybe<AutoRealm> ar;
    if (IsProxy(promiseObj)) {
        unwrappedPromiseObj = CheckedUnwrap(promiseObj);
        if (!unwrappedPromiseObj) {
            ReportAccessDenied(cx);
            return false;
        }
        if (JS_IsDeadWrapper(unwrappedPromiseObj)) {
            JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_DEAD_OBJECT);
            return false;
        }
        ar.emplace(cx, unwrappedPromiseObj);
        if (!cx->compartment()->wrap(cx, &blockedPromise))
            return false;
    }

    // If either the object to depend on or the object that gets blocked isn't
    // a, maybe-wrapped, Promise instance, we ignore it. All this does is lose
    // some small amount of debug information in scenarios that are highly
    // unlikely to occur in useful code.
    if (!unwrappedPromiseObj->is<PromiseObject>())
        return true;
    if (!blockedPromise_->is<PromiseObject>())
        return true;

    Rooted<PromiseObject*> promise(cx, &unwrappedPromiseObj->as<PromiseObject>());
    return AddDummyPromiseReactionForDebugger(cx, promise, blockedPromise);
}

static MOZ_MUST_USE bool
AddPromiseReaction(JSContext* cx, Handle<PromiseObject*> promise,
                   Handle<PromiseReactionRecord*> reaction)
{
    MOZ_RELEASE_ASSERT(reaction->is<PromiseReactionRecord>());
    RootedValue reactionVal(cx, ObjectValue(*reaction));

    // The code that creates Promise reactions can handle wrapped Promises,
    // unwrapping them as needed. That means that the `promise` and `reaction`
    // objects we have here aren't necessarily from the same compartment. In
    // order to store the reaction on the promise, we have to ensure that it
    // is properly wrapped.
    mozilla::Maybe<AutoRealm> ar;
    if (promise->compartment() != cx->compartment()) {
        ar.emplace(cx, promise);
        if (!cx->compartment()->wrap(cx, &reactionVal))
            return false;
    }

    // 25.4.5.3.1 steps 7.a,b.
    RootedValue reactionsVal(cx, promise->reactions());
    RootedNativeObject reactions(cx);

    if (reactionsVal.isUndefined()) {
        // If no reactions existed so far, just store the reaction record directly.
        promise->setFixedSlot(PromiseSlot_ReactionsOrResult, reactionVal);
        return true;
    }

    RootedObject reactionsObj(cx, &reactionsVal.toObject());

    // If only a single reaction exists, it's stored directly instead of in a
    // list. In that case, `reactionsObj` might be a wrapper, which we can
    // always safely unwrap.
    if (IsProxy(reactionsObj)) {
        reactionsObj = UncheckedUnwrap(reactionsObj);
        if (JS_IsDeadWrapper(reactionsObj)) {
            JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_DEAD_OBJECT);
            return false;
        }
        MOZ_RELEASE_ASSERT(reactionsObj->is<PromiseReactionRecord>());
    }

    if (reactionsObj->is<PromiseReactionRecord>()) {
        // If a single reaction existed so far, create a list and store the
        // old and the new reaction in it.
        reactions = NewDenseFullyAllocatedArray(cx, 2);
        if (!reactions)
            return false;
        if (reactions->ensureDenseElements(cx, 0, 2) != DenseElementResult::Success)
            return false;

        reactions->setDenseElement(0, reactionsVal);
        reactions->setDenseElement(1, reactionVal);

        promise->setFixedSlot(PromiseSlot_ReactionsOrResult, ObjectValue(*reactions));
    } else {
        // Otherwise, just store the new reaction.
        MOZ_RELEASE_ASSERT(reactionsObj->is<NativeObject>());
        reactions = &reactionsObj->as<NativeObject>();
        uint32_t len = reactions->getDenseInitializedLength();
        if (reactions->ensureDenseElements(cx, 0, len + 1) != DenseElementResult::Success)
            return false;
        reactions->setDenseElement(len, reactionVal);
    }

    return true;
}

static MOZ_MUST_USE bool
AddDummyPromiseReactionForDebugger(JSContext* cx, Handle<PromiseObject*> promise,
                                   HandleObject dependentPromise)
{
    if (promise->state() != JS::PromiseState::Pending)
        return true;

    Rooted<PromiseReactionRecord*> reaction(cx, NewReactionRecord(cx, dependentPromise,
                                                                  NullHandleValue, NullHandleValue,
                                                                  nullptr, nullptr,
                                                                  IncumbentGlobalObject::No));
    if (!reaction)
        return false;

    reaction->setIsDebuggerDummy();

    return AddPromiseReaction(cx, promise, reaction);
}

uint64_t
PromiseObject::getID()
{
    return PromiseDebugInfo::id(this);
}

double
PromiseObject::lifetime()
{
    return MillisecondsSinceStartup() - allocationTime();
}

/**
 * Returns all promises that directly depend on this one. That means those
 * created by calling `then` on this promise, or the promise returned by
 * `Promise.all(iterable)` or `Promise.race(iterable)`, with this promise
 * being a member of the passed-in `iterable`.
 *
 * Per spec, we should have separate lists of reaction records for the
 * fulfill and reject cases. As an optimization, we have only one of those,
 * containing the required data for both cases. So we just walk that list
 * and extract the dependent promises from all reaction records.
 */
bool
PromiseObject::dependentPromises(JSContext* cx, MutableHandle<GCVector<Value>> values)
{
    if (state() != JS::PromiseState::Pending)
        return true;

    RootedValue reactionsVal(cx, reactions());

    // If no reactions are pending, we don't have list and are done.
    if (reactionsVal.isNullOrUndefined())
        return true;

    RootedNativeObject reactions(cx, &reactionsVal.toObject().as<NativeObject>());

    // If only a single reaction is pending, it's stored directly.
    if (reactions->is<PromiseReactionRecord>()) {
        // Not all reactions have a Promise on them.
        RootedObject promiseObj(cx, reactions->as<PromiseReactionRecord>().promise());
        if (!promiseObj)
            return true;

        if (!values.growBy(1))
            return false;

        values[0].setObject(*promiseObj);
        return true;
    }

    uint32_t len = reactions->getDenseInitializedLength();
    MOZ_ASSERT(len >= 2);

    size_t valuesIndex = 0;
    Rooted<PromiseReactionRecord*> reaction(cx);
    for (size_t i = 0; i < len; i++) {
        reaction = &reactions->getDenseElement(i).toObject().as<PromiseReactionRecord>();

        // Not all reactions have a Promise on them.
        RootedObject promiseObj(cx, reaction->promise());
        if (!promiseObj)
            continue;
        if (!values.growBy(1))
            return false;

        values[valuesIndex++].setObject(*promiseObj);
    }

    return true;
}

/* static */ bool
PromiseObject::resolve(JSContext* cx, Handle<PromiseObject*> promise, HandleValue resolutionValue)
{
    MOZ_ASSERT(!PromiseHasAnyFlag(*promise, PROMISE_FLAG_ASYNC));
    if (promise->state() != JS::PromiseState::Pending)
        return true;

    if (PromiseHasAnyFlag(*promise, PROMISE_FLAG_DEFAULT_RESOLVING_FUNCTIONS))
        return ResolvePromiseInternal(cx, promise, resolutionValue);

    RootedObject resolveFun(cx, GetResolveFunctionFromPromise(promise));
    if (!resolveFun)
        return true;

    RootedValue funVal(cx, ObjectValue(*resolveFun));

    // For xray'd Promises, the resolve fun may have been created in another
    // compartment. For the call below to work in that case, wrap the
    // function into the current compartment.
    if (!cx->compartment()->wrap(cx, &funVal))
        return false;

    FixedInvokeArgs<1> args(cx);
    args[0].set(resolutionValue);

    RootedValue dummy(cx);
    return Call(cx, funVal, UndefinedHandleValue, args, &dummy);
}

/* static */ bool
PromiseObject::reject(JSContext* cx, Handle<PromiseObject*> promise, HandleValue rejectionValue)
{
    MOZ_ASSERT(!PromiseHasAnyFlag(*promise, PROMISE_FLAG_ASYNC));
    if (promise->state() != JS::PromiseState::Pending)
        return true;

    if (PromiseHasAnyFlag(*promise, PROMISE_FLAG_DEFAULT_RESOLVING_FUNCTIONS))
        return ResolvePromise(cx, promise, rejectionValue, JS::PromiseState::Rejected);

    RootedValue funVal(cx, promise->getFixedSlot(PromiseSlot_RejectFunction));
    MOZ_ASSERT(IsCallable(funVal));

    FixedInvokeArgs<1> args(cx);
    args[0].set(rejectionValue);

    RootedValue dummy(cx);
    return Call(cx, funVal, UndefinedHandleValue, args, &dummy);
}

/* static */ void
PromiseObject::onSettled(JSContext* cx, Handle<PromiseObject*> promise)
{
    PromiseDebugInfo::setResolutionInfo(cx, promise);

    if (promise->state() == JS::PromiseState::Rejected && promise->isUnhandled())
        cx->runtime()->addUnhandledRejectedPromise(cx, promise);

    Debugger::onPromiseSettled(cx, promise);
}

OffThreadPromiseTask::OffThreadPromiseTask(JSContext* cx, Handle<PromiseObject*> promise)
  : runtime_(cx->runtime()),
    promise_(cx, promise),
    registered_(false)
{
    MOZ_ASSERT(runtime_ == promise_->zone()->runtimeFromMainThread());
    MOZ_ASSERT(CurrentThreadCanAccessRuntime(runtime_));
    MOZ_ASSERT(cx->runtime()->offThreadPromiseState.ref().initialized());
}

OffThreadPromiseTask::~OffThreadPromiseTask()
{
    MOZ_ASSERT(CurrentThreadCanAccessRuntime(runtime_));

    OffThreadPromiseRuntimeState& state = runtime_->offThreadPromiseState.ref();
    MOZ_ASSERT(state.initialized());

    if (registered_) {
        LockGuard<Mutex> lock(state.mutex_);
        state.live_.remove(this);
    }
}

bool
OffThreadPromiseTask::init(JSContext* cx)
{
    MOZ_ASSERT(cx->runtime() == runtime_);
    MOZ_ASSERT(CurrentThreadCanAccessRuntime(runtime_));

    OffThreadPromiseRuntimeState& state = runtime_->offThreadPromiseState.ref();
    MOZ_ASSERT(state.initialized());

    LockGuard<Mutex> lock(state.mutex_);

    if (!state.live_.putNew(this)) {
        ReportOutOfMemory(cx);
        return false;
    }

    registered_ = true;
    return true;
}

void
OffThreadPromiseTask::run(JSContext* cx, MaybeShuttingDown maybeShuttingDown)
{
    MOZ_ASSERT(cx->runtime() == runtime_);
    MOZ_ASSERT(CurrentThreadCanAccessRuntime(runtime_));
    MOZ_ASSERT(registered_);
    MOZ_ASSERT(runtime_->offThreadPromiseState.ref().initialized());

    if (maybeShuttingDown == JS::Dispatchable::NotShuttingDown) {
        // We can't leave a pending exception when returning to the caller so do
        // the same thing as Gecko, which is to ignore the error. This should
        // only happen due to OOM or interruption.
        AutoRealm ar(cx, promise_);
        if (!resolve(cx, promise_))
            cx->clearPendingException();
    }

    js_delete(this);
}

void
OffThreadPromiseTask::dispatchResolveAndDestroy()
{
    MOZ_ASSERT(registered_);

    OffThreadPromiseRuntimeState& state = runtime_->offThreadPromiseState.ref();
    MOZ_ASSERT(state.initialized());
    MOZ_ASSERT((LockGuard<Mutex>(state.mutex_), state.live_.has(this)));

    // If the dispatch succeeds, then we are guaranteed that run() will be
    // called on an active JSContext of runtime_.
    if (state.dispatchToEventLoopCallback_(state.dispatchToEventLoopClosure_, this))
        return;

    // We assume, by interface contract, that if the dispatch fails, it's
    // because the embedding is in the process of shutting down the JSRuntime.
    // Since JSRuntime destruction calls shutdown(), we can rely on shutdown()
    // to delete the task on its active JSContext thread. shutdown() waits for
    // numCanceled_ == live_.length, so we notify when this condition is
    // reached.
    LockGuard<Mutex> lock(state.mutex_);
    state.numCanceled_++;
    if (state.numCanceled_ == state.live_.count())
        state.allCanceled_.notify_one();
}

OffThreadPromiseRuntimeState::OffThreadPromiseRuntimeState()
  : dispatchToEventLoopCallback_(nullptr),
    dispatchToEventLoopClosure_(nullptr),
    mutex_(mutexid::OffThreadPromiseState),
    numCanceled_(0),
    internalDispatchQueueClosed_(false)
{
    AutoEnterOOMUnsafeRegion noOOM;
    if (!live_.init())
        noOOM.crash("OffThreadPromiseRuntimeState");
}

OffThreadPromiseRuntimeState::~OffThreadPromiseRuntimeState()
{
    MOZ_ASSERT(live_.empty());
    MOZ_ASSERT(numCanceled_ == 0);
    MOZ_ASSERT(internalDispatchQueue_.empty());
    MOZ_ASSERT(!initialized());
}

void
OffThreadPromiseRuntimeState::init(JS::DispatchToEventLoopCallback callback, void* closure)
{
    MOZ_ASSERT(!initialized());

    dispatchToEventLoopCallback_ = callback;
    dispatchToEventLoopClosure_ = closure;

    MOZ_ASSERT(initialized());
}

/* static */ bool
OffThreadPromiseRuntimeState::internalDispatchToEventLoop(void* closure, JS::Dispatchable* d)
{
    OffThreadPromiseRuntimeState& state = *reinterpret_cast<OffThreadPromiseRuntimeState*>(closure);
    MOZ_ASSERT(state.usingInternalDispatchQueue());

    LockGuard<Mutex> lock(state.mutex_);

    if (state.internalDispatchQueueClosed_)
        return false;

    // The JS API contract is that 'false' means shutdown, so be infallible
    // here (like Gecko).
    AutoEnterOOMUnsafeRegion noOOM;
    if (!state.internalDispatchQueue_.append(d))
        noOOM.crash("internalDispatchToEventLoop");

    // Wake up internalDrain() if it is waiting for a job to finish.
    state.internalDispatchQueueAppended_.notify_one();
    return true;
}

bool
OffThreadPromiseRuntimeState::usingInternalDispatchQueue() const
{
    return dispatchToEventLoopCallback_ == internalDispatchToEventLoop;
}

void
OffThreadPromiseRuntimeState::initInternalDispatchQueue()
{
    init(internalDispatchToEventLoop, this);
    MOZ_ASSERT(usingInternalDispatchQueue());
}

bool
OffThreadPromiseRuntimeState::initialized() const
{
    return !!dispatchToEventLoopCallback_;
}

void
OffThreadPromiseRuntimeState::internalDrain(JSContext* cx)
{
    MOZ_ASSERT(usingInternalDispatchQueue());
    MOZ_ASSERT(!internalDispatchQueueClosed_);

    while (true) {
        DispatchableVector dispatchQueue;
        {
            LockGuard<Mutex> lock(mutex_);

            MOZ_ASSERT_IF(!internalDispatchQueue_.empty(), !live_.empty());
            if (live_.empty())
                return;

            while (internalDispatchQueue_.empty())
                internalDispatchQueueAppended_.wait(lock);

            Swap(dispatchQueue, internalDispatchQueue_);
            MOZ_ASSERT(internalDispatchQueue_.empty());
        }

        // Don't call run() with mutex_ held to avoid deadlock.
        for (JS::Dispatchable* d : dispatchQueue)
            d->run(cx, JS::Dispatchable::NotShuttingDown);
    }
}

bool
OffThreadPromiseRuntimeState::internalHasPending()
{
    MOZ_ASSERT(usingInternalDispatchQueue());
    MOZ_ASSERT(!internalDispatchQueueClosed_);

    LockGuard<Mutex> lock(mutex_);
    MOZ_ASSERT_IF(!internalDispatchQueue_.empty(), !live_.empty());
    return !live_.empty();
}

void
OffThreadPromiseRuntimeState::shutdown(JSContext* cx)
{
    if (!initialized())
        return;

    // When the shell is using the internal event loop, we must simulate our
    // requirement of the embedding that, before shutdown, all successfully-
    // dispatched-to-event-loop tasks have been run.
    if (usingInternalDispatchQueue()) {
        DispatchableVector dispatchQueue;
        {
            LockGuard<Mutex> lock(mutex_);
            Swap(dispatchQueue, internalDispatchQueue_);
            MOZ_ASSERT(internalDispatchQueue_.empty());
            internalDispatchQueueClosed_ = true;
        }

        // Don't call run() with mutex_ held to avoid deadlock.
        for (JS::Dispatchable* d : dispatchQueue)
            d->run(cx, JS::Dispatchable::ShuttingDown);
    }

    {
        // Wait until all live OffThreadPromiseRuntimeState have been confirmed
        // canceled by OffThreadPromiseTask::dispatchResolve().
        LockGuard<Mutex> lock(mutex_);
        while (live_.count() != numCanceled_) {
            MOZ_ASSERT(numCanceled_ < live_.count());
            allCanceled_.wait(lock);
        }
    }

    // Now that all the tasks have stopped concurrent execution, we can just
    // delete everything. We don't want each OffThreadPromiseTask to unregister
    // itself (which would mutate live_ while we are iterating over it) so reset
    // the tasks' internal registered_ flag.
    for (OffThreadPromiseTaskSet::Range r = live_.all(); !r.empty(); r.popFront()) {
        OffThreadPromiseTask* task = r.front();
        MOZ_ASSERT(task->registered_);
        task->registered_ = false;
        js_delete(task);
    }
    live_.clear();
    numCanceled_ = 0;

    // After shutdown, there should be no OffThreadPromiseTask activity in this
    // JSRuntime. Revert to the !initialized() state to catch bugs.
    dispatchToEventLoopCallback_ = nullptr;
    MOZ_ASSERT(!initialized());
}

static JSObject*
CreatePromisePrototype(JSContext* cx, JSProtoKey key)
{
    return GlobalObject::createBlankPrototype(cx, cx->global(), &PromiseObject::protoClass_);
}

const JSJitInfo promise_then_info = {
  { (JSJitGetterOp)Promise_then_noRetVal },
  { 0 }, /* unused */
  { 0 }, /* unused */
  JSJitInfo::IgnoresReturnValueNative,
  JSJitInfo::AliasEverything,
  JSVAL_TYPE_UNDEFINED,
};

const JSJitInfo promise_catch_info = {
  { (JSJitGetterOp)Promise_catch_noRetVal },
  { 0 }, /* unused */
  { 0 }, /* unused */
  JSJitInfo::IgnoresReturnValueNative,
  JSJitInfo::AliasEverything,
  JSVAL_TYPE_UNDEFINED,
};

static const JSFunctionSpec promise_methods[] = {
    JS_FNINFO("then", Promise_then, &promise_then_info, 2, 0),
    JS_FNINFO("catch", Promise_catch, &promise_catch_info, 1, 0),
    JS_SELF_HOSTED_FN("finally", "Promise_finally", 1, 0),
    JS_FS_END
};

static const JSPropertySpec promise_properties[] = {
    JS_STRING_SYM_PS(toStringTag, "Promise", JSPROP_READONLY),
    JS_PS_END
};

static const JSFunctionSpec promise_static_methods[] = {
    JS_FN("all", Promise_static_all, 1, 0),
    JS_FN("race", Promise_static_race, 1, 0),
    JS_FN("reject", Promise_reject, 1, 0),
    JS_FN("resolve", Promise_static_resolve, 1, 0),
    JS_FS_END
};

static const JSPropertySpec promise_static_properties[] = {
    JS_SYM_GET(species, Promise_static_species, 0),
    JS_PS_END
};

static const ClassSpec PromiseObjectClassSpec = {
    GenericCreateConstructor<PromiseConstructor, 1, gc::AllocKind::FUNCTION>,
    CreatePromisePrototype,
    promise_static_methods,
    promise_static_properties,
    promise_methods,
    promise_properties
};

const Class PromiseObject::class_ = {
    "Promise",
    JSCLASS_HAS_RESERVED_SLOTS(RESERVED_SLOTS) | JSCLASS_HAS_CACHED_PROTO(JSProto_Promise) |
    JSCLASS_HAS_XRAYED_CONSTRUCTOR,
    JS_NULL_CLASS_OPS,
    &PromiseObjectClassSpec
};

const Class PromiseObject::protoClass_ = {
    "PromiseProto",
    JSCLASS_HAS_CACHED_PROTO(JSProto_Promise),
    JS_NULL_CLASS_OPS,
    &PromiseObjectClassSpec
};
