/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "builtin/Promise.h"

#include "mozilla/Atomics.h"
#include "mozilla/TimeStamp.h"

#include "jscntxt.h"

#include "gc/Heap.h"
#include "js/Debug.h"
#include "vm/SelfHosting.h"

#include "jsobjinlines.h"

#include "vm/NativeObject-inl.h"

using namespace js;

static double
MillisecondsSinceStartup()
{
    auto now = mozilla::TimeStamp::Now();
    bool ignored;
    return (now - mozilla::TimeStamp::ProcessCreation(ignored)).ToMilliseconds();
}

#define PROMISE_HANDLER_IDENTITY 0
#define PROMISE_HANDLER_THROWER  1

enum ResolutionMode {
    ResolveMode,
    RejectMode
};

enum ResolveFunctionSlots {
    ResolveFunctionSlot_Promise = 0,
    ResolveFunctionSlot_RejectFunction,
};

enum RejectFunctionSlots {
    RejectFunctionSlot_Promise = ResolveFunctionSlot_Promise,
    RejectFunctionSlot_ResolveFunction,
    RejectFunctionSlot_PromiseAllData = RejectFunctionSlot_ResolveFunction,
};

enum PromiseAllResolveElementFunctionSlots {
    PromiseAllResolveElementFunctionSlot_Data = 0,
    PromiseAllResolveElementFunctionSlot_ElementIndex,
};

enum ReactionJobSlots {
    ReactionJobSlot_ReactionRecord = 0,
};

enum ThenableJobSlots {
    ThenableJobSlot_Handler = 0,
    ThenableJobSlot_JobData,
};

enum ThenableJobDataIndices {
    ThenableJobDataIndex_Promise = 0,
    ThenableJobDataIndex_Thenable,
    ThenableJobDataLength,
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
        return getFixedSlot(PromiseAllDataHolderSlot_ResolveFunction).toObjectOrNull();
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
NewPromiseAllDataHolder(JSContext* cx, HandleObject resultPromise, HandleObject valuesArray,
                        HandleObject resolve)
{
    Rooted<PromiseAllDataHolder*> dataHolder(cx, NewObjectWithClassProto<PromiseAllDataHolder>(cx));
    if (!dataHolder)
        return nullptr;

    dataHolder->setFixedSlot(PromiseAllDataHolderSlot_Promise, ObjectValue(*resultPromise));
    dataHolder->setFixedSlot(PromiseAllDataHolderSlot_RemainingElements, Int32Value(1));
    dataHolder->setFixedSlot(PromiseAllDataHolderSlot_ValuesArray, ObjectValue(*valuesArray));
    dataHolder->setFixedSlot(PromiseAllDataHolderSlot_ResolveFunction, ObjectOrNullValue(resolve));
    return dataHolder;
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
    if (!GetAndClearException(cx, &reason))
        return false;

    if (!RunResolutionFunction(cx, reject, reason, RejectMode, promiseObj))
        return false;

    // Step 1.b.
    args.rval().setObject(*promiseObj);
    return true;
}

enum ReactionRecordSlots {
    ReactionRecordSlot_Promise = 0,
    ReactionRecordSlot_OnFulfilled,
    ReactionRecordSlot_OnRejected,
    ReactionRecordSlot_Resolve,
    ReactionRecordSlot_Reject,
    ReactionRecordSlot_IncumbentGlobalObject,
    ReactionRecordSlot_Flags,
    ReactionRecordSlot_HandlerArg,
    ReactionRecordSlots,
};

#define REACTION_FLAG_RESOLVED                  0x1
#define REACTION_FLAG_FULFILLED                 0x2
#define REACTION_FLAG_IGNORE_DEFAULT_RESOLUTION 0x4

// ES2016, 25.4.1.2.
class PromiseReactionRecord : public NativeObject
{
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
    int32_t flags = promise.getFixedSlot(PromiseSlot_Flags).toInt32();
    promise.setFixedSlot(PromiseSlot_Flags, Int32Value(flags | flag));
}

static bool
PromiseHasAnyFlag(PromiseObject& promise, int32_t flag)
{
    return promise.getFixedSlot(PromiseSlot_Flags).toInt32() & flag;
}

static bool ResolvePromiseFunction(JSContext* cx, unsigned argc, Value* vp);
static bool RejectPromiseFunction(JSContext* cx, unsigned argc, Value* vp);

// ES2016, 25.4.1.3.
static MOZ_MUST_USE bool
CreateResolvingFunctions(JSContext* cx, HandleValue promise,
                         MutableHandleValue resolveVal,
                         MutableHandleValue rejectVal)
{
    RootedAtom funName(cx, cx->names().empty);
    RootedFunction resolve(cx, NewNativeFunction(cx, ResolvePromiseFunction, 1, funName,
                                                 gc::AllocKind::FUNCTION_EXTENDED, GenericObject));
    if (!resolve)
        return false;

    RootedFunction reject(cx, NewNativeFunction(cx, RejectPromiseFunction, 1, funName,
                                                 gc::AllocKind::FUNCTION_EXTENDED, GenericObject));
    if (!reject)
        return false;

    // TODO: remove this once all self-hosted promise code is gone.
    // The resolving functions are trusted, so self-hosted code should be able
    // to call them using callFunction instead of callContentFunction.
    resolve->setFlags(resolve->flags() | JSFunction::SELF_HOSTED);
    reject->setFlags(reject->flags() | JSFunction::SELF_HOSTED);

    resolve->setExtendedSlot(ResolveFunctionSlot_Promise, promise);
    resolve->setExtendedSlot(ResolveFunctionSlot_RejectFunction, ObjectValue(*reject));

    reject->setExtendedSlot(RejectFunctionSlot_Promise, promise);
    reject->setExtendedSlot(RejectFunctionSlot_ResolveFunction, ObjectValue(*resolve));

    resolveVal.setObject(*resolve);
    rejectVal.setObject(*reject);

    return true;
}

static void ClearResolutionFunctionSlots(JSFunction* resolutionFun);
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

    RootedObject promise(cx, &promiseVal.toObject());

    // In some cases the Promise reference on the resolution function won't
    // have been removed during resolution, so we need to check that here,
    // too.
    if (promise->is<PromiseObject>() &&
        PromiseHasAnyFlag(promise->as<PromiseObject>(), PROMISE_FLAG_RESOLVED))
    {
        args.rval().setUndefined();
        return true;
    }

    // Step 5.
    // Here, we only remove the Promise reference from the resolution
    // functions. Actually marking it as fulfilled/rejected happens later.
    ClearResolutionFunctionSlots(reject);

    // Step 6.
    bool result = RejectMaybeWrappedPromise(cx, promise, reasonVal);
    if (result)
        args.rval().setUndefined();
    return result;
}

static MOZ_MUST_USE bool FulfillMaybeWrappedPromise(JSContext *cx, HandleObject promiseObj,
                                                    HandleValue value_);

static MOZ_MUST_USE bool EnqueuePromiseResolveThenableJob(JSContext* cx,
                                                          HandleValue promiseToResolve,
                                                          HandleValue thenable,
                                                          HandleValue thenVal);

// ES2016, 25.4.1.3.2, steps 7-13.
static MOZ_MUST_USE bool
ResolvePromiseInternal(JSContext* cx, HandleObject promise, HandleValue resolutionVal)
{
    // Step 7.
    if (!resolutionVal.isObject())
        return FulfillMaybeWrappedPromise(cx, promise, resolutionVal);

    RootedObject resolution(cx, &resolutionVal.toObject());

    // Step 8.
    RootedValue thenVal(cx);
    bool status = GetProperty(cx, resolution, resolution, cx->names().then, &thenVal);

    // Step 9.
    if (!status) {
        RootedValue error(cx);
        if (!GetAndClearException(cx, &error))
            return false;

        return RejectMaybeWrappedPromise(cx, promise, error);
    }

    // Step 10 (implicit).

    // Step 11.
    if (!IsCallable(thenVal))
        return FulfillMaybeWrappedPromise(cx, promise, resolutionVal);

    // Step 12.
    RootedValue promiseVal(cx, ObjectValue(*promise));
    if (!EnqueuePromiseResolveThenableJob(cx, promiseVal, resolutionVal, thenVal))
        return false;

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

    // Steps 1-2.
    RootedValue promiseVal(cx, resolve->getExtendedSlot(ResolveFunctionSlot_Promise));

    // Steps 3-4.
    // We use the reference to the reject function as a signal for whether
    // the resolve or reject function was already called, at which point
    // the references on each of the functions are cleared.
    if (!resolve->getExtendedSlot(ResolveFunctionSlot_RejectFunction).isObject()) {
        args.rval().setUndefined();
        return true;
    }

    RootedObject promise(cx, &promiseVal.toObject());

    // In some cases the Promise reference on the resolution function won't
    // have been removed during resolution, so we need to check that here,
    // too.
    if (promise->is<PromiseObject>() &&
        PromiseHasAnyFlag(promise->as<PromiseObject>(), PROMISE_FLAG_RESOLVED))
    {
        args.rval().setUndefined();
        return true;
    }

    // Step 5.
    // Here, we only remove the Promise reference from the resolution
    // functions. Actually marking it as fulfilled/rejected happens later.
    ClearResolutionFunctionSlots(resolve);

    // Step 6.
    if (resolutionVal == promiseVal) {
        // Step 6.a.
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_CANNOT_RESOLVE_PROMISE_WITH_ITSELF);
        RootedValue selfResolutionError(cx);
        bool status = GetAndClearException(cx, &selfResolutionError);
        MOZ_ASSERT(status);

        // Step 6.b.
        status = RejectMaybeWrappedPromise(cx, promise, selfResolutionError);
        if (status)
            args.rval().setUndefined();
        return status;
    }

    bool status = ResolvePromiseInternal(cx, promise, resolutionVal);
    if (status)
        args.rval().setUndefined();
    return status;
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
    // The reaction might have been stored on a Promise from another
    // compartment, which means it would've been wrapped in a CCW.
    // To properly handle that case here, unwrap it and enter its
    // compartment, where the job creation should take place anyway.
    Rooted<PromiseReactionRecord*> reaction(cx);
    RootedValue handlerArg(cx, handlerArg_);
    mozilla::Maybe<AutoCompartment> ac;
    if (IsWrapper(reactionObj)) {
        RootedObject unwrappedReactionObj(cx, UncheckedUnwrap(reactionObj));
        if (!unwrappedReactionObj)
            return false;
        ac.emplace(cx, unwrappedReactionObj);
        reaction = &unwrappedReactionObj->as<PromiseReactionRecord>();
        if (!cx->compartment()->wrap(cx, &handlerArg))
            return false;
    } else {
        reaction = &reactionObj->as<PromiseReactionRecord>();
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
    mozilla::Maybe<AutoCompartment> ac2;
    if (handler.isObject()) {
        RootedObject handlerObj(cx, &handler.toObject());

        // The unwrapping has to be unchecked because we specifically want to
        // be able to use handlers with wrappers that would only allow calls.
        // E.g., it's ok to have a handler from a chrome compartment in a
        // reaction to a content compartment's Promise instance.
        handlerObj = UncheckedUnwrap(handlerObj);
        MOZ_ASSERT(handlerObj);
        ac2.emplace(cx, handlerObj);

        // We need to wrap the reaction to store it on the job function.
        if (!cx->compartment()->wrap(cx, &reactionVal))
            return false;
    }

    // Create the JS function to call when the job is triggered.
    RootedAtom funName(cx, cx->names().empty);
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
        global = &objectFromIncumbentGlobal->global();
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
    RootedValue reactionsVal(cx, promise->getFixedSlot(PromiseSlot_ReactionsOrResult));

    // Steps 3-5.
    // The same slot is used for the reactions list and the result, so setting
    // the result also removes the reactions list.
    promise->setFixedSlot(PromiseSlot_ReactionsOrResult, valueOrReason);

    // Step 6.
    int32_t flags = promise->getFixedSlot(PromiseSlot_Flags).toInt32();
    flags |= PROMISE_FLAG_RESOLVED;
    if (state == JS::PromiseState::Fulfilled)
        flags |= PROMISE_FLAG_FULFILLED;
    promise->setFixedSlot(PromiseSlot_Flags, Int32Value(flags));

    // Also null out the resolve/reject functions so they can be GC'd.
    promise->setFixedSlot(PromiseSlot_RejectFunction, UndefinedValue());

    // Now that everything else is done, do the things the debugger needs.
    // Step 7 of RejectPromise implemented in onSettled.
    promise->onSettled(cx);

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

    mozilla::Maybe<AutoCompartment> ac;
    if (!IsProxy(promiseObj)) {
        promise = &promiseObj->as<PromiseObject>();
    } else {
        if (JS_IsDeadWrapper(promiseObj)) {
            JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_DEAD_OBJECT);
            return false;
        }
        promise = &UncheckedUnwrap(promiseObj)->as<PromiseObject>();
        ac.emplace(cx, promise);
        if (!promise->compartment()->wrap(cx, &value))
            return false;
    }

    MOZ_ASSERT(promise->state() == JS::PromiseState::Pending);

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

// ES2016, 25.4.1.5.
static MOZ_MUST_USE bool
NewPromiseCapability(JSContext* cx, HandleObject C, MutableHandleObject promise,
                     MutableHandleObject resolve, MutableHandleObject reject,
                     bool canOmitResolutionFunctions)
{
    RootedValue cVal(cx, ObjectValue(*C));

    // Steps 1-2.
    if (!IsConstructor(C)) {
        ReportValueError(cx, JSMSG_NOT_CONSTRUCTOR, -1, cVal, nullptr);
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
    if (canOmitResolutionFunctions && IsNativeFunction(cVal, PromiseConstructor)) {
        promise.set(CreatePromiseObjectInternal(cx));
        if (!promise)
            return false;
        AddPromiseFlags(promise->as<PromiseObject>(), PROMISE_FLAG_DEFAULT_RESOLVE_FUNCTION |
                                                      PROMISE_FLAG_DEFAULT_REJECT_FUNCTION);

        return true;
    }

    // Step 3 (omitted).

    // Step 4.
    RootedAtom funName(cx, cx->names().empty);
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

    mozilla::Maybe<AutoCompartment> ac;
    if (!IsProxy(promiseObj)) {
        promise = &promiseObj->as<PromiseObject>();
    } else {
        if (JS_IsDeadWrapper(promiseObj)) {
            JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_DEAD_OBJECT);
            return false;
        }
        promise = &UncheckedUnwrap(promiseObj)->as<PromiseObject>();
        ac.emplace(cx, promise);

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
            // Async stacks are only properly adopted if there's at least one
            // interpreter frame active right now. If a thenable job with a
            // throwing `then` function got us here, that'll not be the case,
            // so we add one by throwing the error from self-hosted code.
            FixedInvokeArgs<1> getErrorArgs(cx);
            getErrorArgs[0].set(Int32Value(JSMSG_PROMISE_ERROR_IN_WRAPPED_REJECTION_REASON));
            if (!CallSelfHostedFunction(cx, "GetInternalError", reason, getErrorArgs, &reason))
                return false;
        }
    }

    MOZ_ASSERT(promise->state() == JS::PromiseState::Pending);

    return ResolvePromise(cx, promise, reason, JS::PromiseState::Rejected);
}

// ES2016, 25.4.1.8.
static MOZ_MUST_USE bool
TriggerPromiseReactions(JSContext* cx, HandleValue reactionsVal, JS::PromiseState state,
                        HandleValue valueOrReason)
{
    RootedObject reactions(cx, &reactionsVal.toObject());
    RootedObject reaction(cx);

    if (reactions->is<PromiseReactionRecord>() || IsWrapper(reactions))
        return EnqueuePromiseReactionJob(cx, reactions, valueOrReason, state);

    RootedNativeObject reactionsList(cx, &reactions->as<NativeObject>());
    size_t reactionsCount = reactionsList->getDenseInitializedLength();
    MOZ_ASSERT(reactionsCount > 1, "Reactions list should be created lazily");

    for (size_t i = 0; i < reactionsCount; i++) {
        reaction = &reactionsList->getDenseElement(i).toObject();
        if (!EnqueuePromiseReactionJob(cx, reaction, valueOrReason, state))
            return false;
    }

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
    mozilla::Maybe<AutoCompartment> ac;
    if (IsWrapper(reactionObj)) {
        reactionObj = UncheckedUnwrap(reactionObj);
        ac.emplace(cx, reactionObj);
    }

    // Steps 1-2.
    Rooted<PromiseReactionRecord*> reaction(cx, &reactionObj->as<PromiseReactionRecord>());

    // Step 3.
    RootedValue handlerVal(cx, reaction->handler());

    RootedValue argument(cx, reaction->handlerArg());

    RootedValue handlerResult(cx);
    ResolutionMode resolutionMode = ResolveMode;

    // Steps 4-6.
    if (handlerVal.isNumber()) {
        int32_t handlerNum = int32_t(handlerVal.toNumber());

        // Step 4.
        if (handlerNum == PROMISE_HANDLER_IDENTITY) {
            handlerResult = argument;
        } else {
            // Step 5.
            MOZ_ASSERT(handlerNum == PROMISE_HANDLER_THROWER);
            resolutionMode = RejectMode;
            handlerResult = argument;
        }
    } else {
        // Step 6.
        FixedInvokeArgs<1> args2(cx);
        args2[0].set(argument);
        if (!Call(cx, handlerVal, UndefinedHandleValue, args2, &handlerResult)) {
            resolutionMode = RejectMode;
            // Not much we can do about uncatchable exceptions, so just bail
            // for those.
            if (!cx->isExceptionPending() || !GetAndClearException(cx, &handlerResult))
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
 * See http://www.ecma-international.org/ecma-262/7.0/index.html#sec-jobs-and-job-queues
 *
 * A PromiseResolveThenableJob is set as the native function of an extended
 * JSFunction object, with all information required for the job's
 * execution stored in the function's extended slots.
 *
 * Usage of the function's extended slots is as follows:
 * ThenableJobSlot_Handler: The handler to use as the Promise reaction.
 *                          This can be PROMISE_HANDLER_IDENTITY,
 *                          PROMISE_HANDLER_THROWER, or a callable. In the
 *                          latter case, it's guaranteed to be an object
 *                          from the same compartment as the
 *                          PromiseReactionJob.
 * ThenableJobSlot_JobData: JobData - a, potentially CCW-wrapped, dense list
 *                          containing data required for proper execution of
 *                          the reaction.
 *
 * The JobData list has the following entries:
 * ThenableJobDataSlot_Promise: The Promise to resolve using the given
 *                              thenable.
 * ThenableJobDataSlot_Thenable: The thenable to use as the receiver when
 *                               calling the `then` function.
 */
static bool
PromiseResolveThenableJob(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    RootedFunction job(cx, &args.callee().as<JSFunction>());
    RootedValue then(cx, job->getExtendedSlot(ThenableJobSlot_Handler));
    MOZ_ASSERT(!IsWrapper(&then.toObject()));
    RootedNativeObject jobArgs(cx, &job->getExtendedSlot(ThenableJobSlot_JobData)
                                    .toObject().as<NativeObject>());

    RootedValue promise(cx, jobArgs->getDenseElement(ThenableJobDataIndex_Promise));
    RootedValue thenable(cx, jobArgs->getDenseElement(ThenableJobDataIndex_Thenable));

    // Step 1.
    RootedValue resolveVal(cx);
    RootedValue rejectVal(cx);
    if (!CreateResolvingFunctions(cx, promise, &resolveVal, &rejectVal))
        return false;

    // Step 2.
    FixedInvokeArgs<2> args2(cx);
    args2[0].set(resolveVal);
    args2[1].set(rejectVal);

    RootedValue rval(cx);

    // In difference to the usual pattern, we return immediately on success.
    if (Call(cx, then, thenable, args2, &rval))
        return true;

    if (!GetAndClearException(cx, &rval))
        return false;

    FixedInvokeArgs<1> rejectArgs(cx);
    rejectArgs[0].set(rval);

    return Call(cx, rejectVal, UndefinedHandleValue, rejectArgs, &rval);
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
    AutoCompartment ac(cx, then);

    RootedAtom funName(cx, cx->names().empty);
    RootedFunction job(cx, NewNativeFunction(cx, PromiseResolveThenableJob, 0, funName,
                                             gc::AllocKind::FUNCTION_EXTENDED, GenericObject));
    if (!job)
        return false;

    // Store the `then` function on the callback.
    job->setExtendedSlot(ThenableJobSlot_Handler, ObjectValue(*then));

    // Create a dense array to hold the data needed for the reaction job to
    // work.
    // See the doc comment for PromiseResolveThenableJob for the layout.
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

static MOZ_MUST_USE bool
AddPromiseReaction(JSContext* cx, Handle<PromiseObject*> promise, HandleValue onFulfilled,
                   HandleValue onRejected, HandleObject dependentPromise,
                   HandleObject resolve, HandleObject reject, HandleObject incumbentGlobal);

static MOZ_MUST_USE bool
AddPromiseReaction(JSContext* cx, Handle<PromiseObject*> promise,
                   Handle<PromiseReactionRecord*> reaction);

static MOZ_MUST_USE bool BlockOnPromise(JSContext* cx, HandleObject promise,
                                        HandleObject blockedPromise,
                                        HandleValue onFulfilled, HandleValue onRejected);

static JSFunction*
GetResolveFunctionFromReject(JSFunction* reject)
{
    MOZ_ASSERT(reject->maybeNative() == RejectPromiseFunction);
    Value resolveFunVal = reject->getExtendedSlot(RejectFunctionSlot_ResolveFunction);
    if (IsNativeFunction(resolveFunVal, ResolvePromiseFunction))
        return &resolveFunVal.toObject().as<JSFunction>();

    PromiseAllDataHolder* resolveFunObj = &resolveFunVal.toObject().as<PromiseAllDataHolder>();
    return &resolveFunObj->resolveObj()->as<JSFunction>();
}

static JSFunction*
GetResolveFunctionFromPromise(PromiseObject* promise)
{
    Value rejectFunVal = promise->getFixedSlot(PromiseSlot_RejectFunction);
    if (rejectFunVal.isUndefined())
        return nullptr;
    JSObject* rejectFunObj = &rejectFunVal.toObject();
    if (IsWrapper(rejectFunObj))
        rejectFunObj = CheckedUnwrap(rejectFunObj);

    if (!rejectFunObj->is<JSFunction>())
        return nullptr;

    JSFunction* rejectFun = &rejectFunObj->as<JSFunction>();

    // Only the original RejectPromiseFunction has a reference to the resolve
    // function.
    if (rejectFun->maybeNative() != &RejectPromiseFunction)
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
        reject = &resolutionFun->getExtendedSlot(ResolveFunctionSlot_RejectFunction)
                  .toObject().as<JSFunction>();
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
static MOZ_MUST_USE PromiseObject*
CreatePromiseObjectInternal(JSContext* cx, HandleObject proto /* = nullptr */,
                            bool protoIsWrapped /* = false */, bool informDebugger /* = true */)
{
    // Step 3.
    Rooted<PromiseObject*> promise(cx);
    // Enter the unwrapped proto's compartment, if that's different from
    // the current one.
    // All state stored in a Promise's fixed slots must be created in the
    // same compartment, so we get all of that out of the way here.
    // (Except for the resolution functions, which are created below.)
    mozilla::Maybe<AutoCompartment> ac;
    if (protoIsWrapped)
        ac.emplace(cx, proto);

    promise = NewObjectWithClassProto<PromiseObject>(cx, proto);
    if (!promise)
        return nullptr;

    // Step 4.
    promise->setFixedSlot(PromiseSlot_Flags, Int32Value(0));

    // Steps 5-6.
    // Omitted, we allocate our single list of reaction records lazily.

    // Step 7.
    // Implicit, the handled flag is unset by default.

    // Store an allocation stack so we can later figure out what the
    // control flow was for some unexpected results. Frightfully expensive,
    // but oh well.
    RootedObject stack(cx);
    if (cx->options().asyncStack() || cx->compartment()->isDebuggee()) {
        if (!JS::CaptureCurrentStack(cx, &stack, JS::StackCapture(JS::AllFrames())))
            return nullptr;
    }
    promise->setFixedSlot(PromiseSlot_AllocationSite, ObjectOrNullValue(stack));
    promise->setFixedSlot(PromiseSlot_AllocationTime, DoubleValue(MillisecondsSinceStartup()));

    // Let the Debugger know about this Promise.
    if (informDebugger)
        JS::dbg::onNewPromise(cx, promise);

    return promise;
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
    RootedObject originalNewTarget(cx, newTarget);
    bool needsWrapping = false;

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
    if (IsWrapper(newTarget)) {
        newTarget = CheckedUnwrap(newTarget);
        MOZ_ASSERT(newTarget);
        MOZ_ASSERT(newTarget != originalNewTarget);
        {
            AutoCompartment ac(cx, newTarget);
            RootedObject promiseCtor(cx);
            if (!GetBuiltinConstructor(cx, JSProto_Promise, &promiseCtor))
                return false;

            // Promise subclasses don't get the special Xray treatment, so
            // we only need to do the complex wrapping and unwrapping scheme
            // described above for instances of Promise itself.
            if (newTarget == promiseCtor)
                needsWrapping = true;
        }
    }

    RootedObject proto(cx);
    if (!GetPrototypeFromConstructor(cx, newTarget, &proto))
        return false;
    if (needsWrapping && !cx->compartment()->wrap(cx, &proto))
        return false;
    Rooted<PromiseObject*> promise(cx, PromiseObject::create(cx, executor, proto));
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
PromiseObject::create(JSContext* cx, HandleObject executor, HandleObject proto /* = nullptr */)
{
    MOZ_ASSERT(executor->isCallable());

    RootedObject usedProto(cx, proto);
    bool wrappedProto = false;
    // If the proto is wrapped, that means the current function is running
    // with a different compartment active from the one the Promise instance
    // is to be created in.
    // See the comment in PromiseConstructor for details.
    if (proto && IsWrapper(proto)) {
        wrappedProto = true;
        usedProto = CheckedUnwrap(proto);
        if (!usedProto)
            return nullptr;
    }


    // Steps 3-7.
    Rooted<PromiseObject*> promise(cx, CreatePromiseObjectInternal(cx, usedProto, wrappedProto,
                                                                   false));
    if (!promise)
        return nullptr;

    RootedValue promiseVal(cx, ObjectValue(*promise));
    if (wrappedProto && !cx->compartment()->wrap(cx, &promiseVal))
        return nullptr;

    // Step 8.
    // The resolving functions are created in the compartment active when the
    // (maybe wrapped) Promise constructor was called. They contain checks and
    // can unwrap the Promise if required.
    RootedValue resolveVal(cx);
    RootedValue rejectVal(cx);
    if (!CreateResolvingFunctions(cx, promiseVal, &resolveVal, &rejectVal))
        return nullptr;

    // Need to wrap the resolution functions before storing them on the Promise.
    if (wrappedProto) {
        AutoCompartment ac(cx, promise);
        RootedValue wrappedRejectVal(cx, rejectVal);
        if (!cx->compartment()->wrap(cx, &wrappedRejectVal))
            return nullptr;
        promise->setFixedSlot(PromiseSlot_RejectFunction, wrappedRejectVal);
    } else {
        promise->setFixedSlot(PromiseSlot_RejectFunction, rejectVal);
    }

    // Step 9.
    bool success;
    {
        FixedInvokeArgs<2> args(cx);

        args[0].set(resolveVal);
        args[1].set(rejectVal);

        RootedValue calleeOrRval(cx, ObjectValue(*executor));
        success = Call(cx, calleeOrRval, UndefinedHandleValue, args, &calleeOrRval);
    }

    // Step 10.
    if (!success) {
        RootedValue exceptionVal(cx);
        // Not much we can do about uncatchable exceptions, so just bail
        // for those.
        if (!cx->isExceptionPending() || !GetAndClearException(cx, &exceptionVal))
            return nullptr;

        FixedInvokeArgs<1> args(cx);

        args[0].set(exceptionVal);

        // |rejectVal| is unused after this, so we can safely write to it.
        if (!Call(cx, rejectVal, UndefinedHandleValue, args, &rejectVal))
            return nullptr;
    }

    // Let the Debugger know about this Promise.
    JS::dbg::onNewPromise(cx, promise);

    // Step 11.
    return promise;
}

static MOZ_MUST_USE bool PerformPromiseAll(JSContext *cx, JS::ForOfIterator& iterator,
                                           HandleObject C, HandleObject promiseObj,
                                           HandleObject resolve, HandleObject reject);

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
    bool result = PerformPromiseAll(cx, iter, C, resultPromise, resolve, reject);

    // Step 8.
    if (!result) {
        // Step 8.a.
        // TODO: implement iterator closing.

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
        if (!valuesArray ||
            valuesArray->ensureDenseElements(cx, 0, promiseCount) != DenseElementResult::Success)
        {
            return nullptr;
        }

        // Sub-step 4.
        // Create our data holder that holds all the things shared across
        // every step of the iterator.  In particular, this holds the
        // remainingElementsCount (as an integer reserved slot), the array of
        // values, and the resolve function from our PromiseCapability.
        Rooted<PromiseAllDataHolder*> dataHolder(cx, NewPromiseAllDataHolder(cx, resultPromise,
                                                                             valuesArray, resolve));
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
    // the Promise object. There are also reactions where the Promise
    // itself is missing. For those, there's nothing left to do here.
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


    if (mode == ResolveMode) {
        if (!PromiseHasAnyFlag(*promise, PROMISE_FLAG_DEFAULT_RESOLVE_FUNCTION))
            return true;
        return ResolvePromiseInternal(cx, promise, result);
    }

    if (!PromiseHasAnyFlag(*promise, PROMISE_FLAG_DEFAULT_REJECT_FUNCTION))
        return true;
    return RejectMaybeWrappedPromise(cx, promiseObj, result);

}

// ES2016, 25.4.4.1.1.
static MOZ_MUST_USE bool
PerformPromiseAll(JSContext *cx, JS::ForOfIterator& iterator, HandleObject C,
                  HandleObject promiseObj, HandleObject resolve, HandleObject reject)
{
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
        JSAutoCompartment ac(cx, unwrappedPromiseObj);
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
                                                                         valuesArray, resolve));
    if (!dataHolder)
        return false;
    RootedValue dataHolderVal(cx, ObjectValue(*dataHolder));

    // Step 5.
    uint32_t index = 0;

    // Step 6.
    RootedValue nextValue(cx);
    RootedId indexId(cx);
    RootedValue rejectFunVal(cx, ObjectOrNullValue(reject));

    while (true) {
        bool done;
        // Steps a, b, c, e, f, g.
        if (!iterator.next(&nextValue, &done))
            return false;

        // Step d.
        if (done) {
            // Step d.i (implicit).
            // Step d.ii.
            int32_t remainingCount = dataHolder->decreaseRemainingCount();

            // Steps d.iii-iv.
            if (remainingCount == 0) {
                if (resolve) {
                    return RunResolutionFunction(cx, resolve, valuesArrayVal, ResolveMode,
                                                 promiseObj);
                }
                return ResolvePromiseInternal(cx, promiseObj, valuesArrayVal);
            }

            // We're all set for now!
            return true;
        }

        // Step h.
        { // Scope for the JSAutoCompartment we need to work with valuesArray.  We
            // mostly do this for performance; we could go ahead and do the define via
            // a cross-compartment proxy instead...
            JSAutoCompartment ac(cx, valuesArray);
            indexId = INT_TO_JSID(index);
            if (!DefineProperty(cx, valuesArray, indexId, UndefinedHandleValue))
                return false;
        }

        // Step i.
        // Sadly, because someone could have overridden
        // "resolve" on the canonical Promise constructor.
        RootedValue nextPromise(cx);
        RootedValue staticResolve(cx);
        if (!GetProperty(cx, CVal, cx->names().resolve, &staticResolve))
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
        RootedObject nextPromiseObj(cx, &nextPromise.toObject());
        RootedValue resolveFunVal(cx, ObjectValue(*resolveFunc));
        if (!BlockOnPromise(cx, nextPromiseObj, promiseObj, resolveFunVal, rejectFunVal))
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
    RootedNativeObject values(cx, &valuesObj->as<NativeObject>());

    // Step 6 (moved under step 10).
    // Step 7 (moved to step 9).

    // Step 8.
    // The index is guaranteed to be initialized to `undefined`.
    if (valuesListIsWrapped) {
        AutoCompartment ac(cx, values);
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
        if (!resolveAllFun) {
            if (!FulfillMaybeWrappedPromise(cx, promiseObj, valuesVal))
                return false;
        } else {
            if (!RunResolutionFunction(cx, resolveAllFun, valuesVal, ResolveMode, promiseObj))
                return false;
        }
    }

    // Step 11.
    args.rval().setUndefined();
    return true;
}

static MOZ_MUST_USE bool PerformPromiseRace(JSContext *cx, JS::ForOfIterator& iterator,
                                            HandleObject C, HandleObject promiseObj,
                                            HandleObject resolve, HandleObject reject);

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
    bool result = PerformPromiseRace(cx, iter, C, resultPromise, resolve, reject);

    // Step 8.
    if (!result) {
        // Step 8.a.
        // TODO: implement iterator closing.

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
                   HandleObject promiseObj, HandleObject resolve, HandleObject reject)
{
    MOZ_ASSERT(C->isConstructor());
    RootedValue CVal(cx, ObjectValue(*C));

    RootedValue nextValue(cx);
    RootedValue resolveFunVal(cx, ObjectOrNullValue(resolve));
    RootedValue rejectFunVal(cx, ObjectOrNullValue(reject));
    bool done;

    while (true) {
        // Steps a-c, e-g.
        if (!iterator.next(&nextValue, &done))
            return false;

        // Step d.
        if (done) {
            // Step d.i.
            // TODO: implement iterator closing.

            // Step d.ii.
            return true;
        }

        // Step h.
        // Sadly, because someone could have overridden
        // "resolve" on the canonical Promise constructor.
        RootedValue nextPromise(cx);
        RootedValue staticResolve(cx);
        if (!GetProperty(cx, CVal, cx->names().resolve, &staticResolve))
            return false;

        FixedInvokeArgs<1> resolveArgs(cx);
        resolveArgs[0].set(nextValue);
        if (!Call(cx, staticResolve, CVal, resolveArgs, &nextPromise))
            return false;

        // Step i.
        RootedObject nextPromiseObj(cx, &nextPromise.toObject());
        if (!BlockOnPromise(cx, nextPromiseObj, promiseObj, resolveFunVal, rejectFunVal))
            return false;
    }

    MOZ_ASSERT_UNREACHABLE("Shouldn't reach the end of PerformPromiseRace");
}

// ES2016, Sub-steps of 25.4.4.4 and 25.4.4.5.
static MOZ_MUST_USE bool
CommonStaticResolveRejectImpl(JSContext* cx, unsigned argc, Value* vp, ResolutionMode mode)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedValue x(cx, args.get(0));

    // Steps 1-2.
    if (!args.thisv().isObject()) {
        const char* msg = mode == ResolveMode
                          ? "Receiver of Promise.resolve call"
                          : "Receiver of Promise.reject call";
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_NOT_NONNULL_OBJECT, msg);
        return false;
    }
    RootedValue cVal(cx, args.thisv());
    RootedObject C(cx, &cVal.toObject());

    // Step 3 of Resolve.
    if (mode == ResolveMode && x.isObject()) {
        RootedObject xObj(cx, &x.toObject());
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
                return false;
            if (ctorVal == cVal) {
                args.rval().set(x);
                return true;
            }
        }
    }

    // Step 4 of Resolve, 3 of Reject.
    RootedObject promise(cx);
    RootedObject resolveFun(cx);
    RootedObject rejectFun(cx);
    if (!NewPromiseCapability(cx, C, &promise, &resolveFun, &rejectFun, true))
        return false;

    // Step 5 of Resolve, 4 of Reject.
    if (!RunResolutionFunction(cx, mode == ResolveMode ? resolveFun : rejectFun, x, mode, promise))
        return false;

    // Step 6 of Resolve, 4 of Reject.
    args.rval().setObject(*promise);
    return true;
}

/**
 * ES2016, 25.4.4.4, Promise.reject.
 */
static bool
Promise_reject(JSContext* cx, unsigned argc, Value* vp)
{
    return CommonStaticResolveRejectImpl(cx, argc, vp, RejectMode);
}

/**
 * Unforgeable version of ES2016, 25.4.4.4, Promise.reject.
 */
/* static */ JSObject*
PromiseObject::unforgeableReject(JSContext* cx, HandleValue value)
{
    // Steps 1-2 (omitted).

    // Roughly step 3.
    Rooted<PromiseObject*> promise(cx, CreatePromiseObjectInternal(cx));
    if (!promise)
        return nullptr;

    // Roughly step 4.
    if (!ResolvePromise(cx, promise, value, JS::PromiseState::Rejected))
        return nullptr;

    // Step 5.
    return promise;
}

/**
 * ES2016, 25.4.4.5, Promise.resolve.
 */
static bool
Promise_static_resolve(JSContext* cx, unsigned argc, Value* vp)
{
    return CommonStaticResolveRejectImpl(cx, argc, vp, ResolveMode);
}

/**
 * Unforgeable version of ES2016, 25.4.4.5, Promise.resolve.
 */
/* static */ JSObject*
PromiseObject::unforgeableResolve(JSContext* cx, HandleValue value)
{
    // Steps 1-2 (omitted).

    // Step 3.
    if (value.isObject()) {
        JSObject* obj = &value.toObject();
        if (IsWrapper(obj))
            obj = CheckedUnwrap(obj);
        // Instead of getting the `constructor` property, do an unforgeable
        // check.
        if (obj && obj->is<PromiseObject>())
            return obj;
    }

    // Step 4.
    Rooted<PromiseObject*> promise(cx, CreatePromiseObjectInternal(cx));
    if (!promise)
        return nullptr;

    // Steps 5.
    if (!ResolvePromiseInternal(cx, promise, value))
        return nullptr;

    // Step 6.
    return promise;
}

// ES2016, 25.4.4.6, implemented in Promise.js.

// ES2016, 25.4.5.1, implemented in Promise.js.

static PromiseReactionRecord*
NewReactionRecord(JSContext* cx, HandleObject resultPromise, HandleValue onFulfilled,
                  HandleValue onRejected, HandleObject resolve, HandleObject reject,
                  HandleObject incumbentGlobalObject)
{
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

// ES2016, 25.4.5.3., steps 3-5.
MOZ_MUST_USE JSObject*
js::OriginalPromiseThen(JSContext* cx, Handle<PromiseObject*> promise, HandleValue onFulfilled,
                        HandleValue onRejected)
{
    RootedObject promiseObj(cx, promise);
    if (promise->compartment() != cx->compartment()) {
        if (!cx->compartment()->wrap(cx, &promiseObj))
            return nullptr;
    }

    // Step 3.
    RootedValue ctorVal(cx);
    if (!SpeciesConstructor(cx, promiseObj, JSProto_Promise, &ctorVal))
        return nullptr;
    RootedObject C(cx, &ctorVal.toObject());

    // Step 4.
    RootedObject resultPromise(cx);
    RootedObject resolve(cx);
    RootedObject reject(cx);
    if (!NewPromiseCapability(cx, C, &resultPromise, &resolve, &reject, true))
        return nullptr;

    // Step 5.
    if (!PerformPromiseThen(cx, promise, onFulfilled, onRejected, resultPromise, resolve, reject))
        return nullptr;

    return resultPromise;
}

// ES2016, 25.4.5.3.
static bool
Promise_then(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    // Step 1.
    RootedValue promiseVal(cx, args.thisv());

    RootedValue onFulfilled(cx, args.get(0));
    RootedValue onRejected(cx, args.get(1));

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
            JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_UNWRAP_DENIED);
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
    RootedObject resultPromise(cx, OriginalPromiseThen(cx, promise, onFulfilled, onRejected));
    if (!resultPromise)
        return false;

    args.rval().setObject(*resultPromise);
    return true;
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
        onFulfilled = Int32Value(PROMISE_HANDLER_IDENTITY);

    // Step 4.
    RootedValue onRejected(cx, onRejected_);
    if (!IsCallable(onRejected))
        onRejected = Int32Value(PROMISE_HANDLER_THROWER);

    RootedObject incumbentGlobal(cx);
    if (!GetObjectFromIncumbentGlobal(cx, &incumbentGlobal))
        return false;

    // Step 7.
    Rooted<PromiseReactionRecord*> reaction(cx, NewReactionRecord(cx, resultPromise,
                                                                  onFulfilled, onRejected,
                                                                  resolve, reject,
                                                                  incumbentGlobal));

    JS::PromiseState state = promise->state();
    int32_t flags = promise->getFixedSlot(PromiseSlot_Flags).toInt32();
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
        RootedValue valueOrReason(cx, promise->getFixedSlot(PromiseSlot_ReactionsOrResult));

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
BlockOnPromise(JSContext* cx, HandleObject promiseObj, HandleObject blockedPromise_,
               HandleValue onFulfilled, HandleValue onRejected)
{
    RootedValue thenVal(cx);
    if (!GetProperty(cx, promiseObj, promiseObj, cx->names().then, &thenVal))
        return false;

    if (promiseObj->is<PromiseObject>() && IsNativeFunction(thenVal, Promise_then)) {
        // |promise| is an unwrapped Promise, and |then| is the original
        // |Promise.prototype.then|, inline it here.
        // 25.4.5.3., step 3.
        RootedObject PromiseCtor(cx);
        if (!GetBuiltinConstructor(cx, JSProto_Promise, &PromiseCtor))
            return false;
        RootedValue PromiseCtorVal(cx, ObjectValue(*PromiseCtor));
        RootedValue CVal(cx);
        if (!SpeciesConstructor(cx, promiseObj, PromiseCtorVal, &CVal))
            return false;
        RootedObject C(cx, &CVal.toObject());

        RootedObject resultPromise(cx, blockedPromise_);
        RootedObject resolveFun(cx);
        RootedObject rejectFun(cx);

        // By default, the blocked promise is added as an extra entry to the
        // rejected promises list.
        bool addToDependent = true;

        if (C == PromiseCtor) {
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
        RootedValue promiseVal(cx, ObjectValue(*promiseObj));
        RootedValue rval(cx);
        if (!Call(cx, thenVal, promiseVal, onFulfilled, onRejected, &rval))
            return false;
    }

    // The object created by the |promise.then| call or the inlined version
    // of it above is visible to content (either because |promise.then| was
    // overridden by content and could leak it, or because a constructor
    // other than the original value of |Promise| was used to create it).
    // To have both that object and |blockedPromise| show up as dependent
    // promises in the debugger, add a dummy reaction to the list of reject
    // reactions that contains |blockedPromise|, but otherwise does nothing.
    RootedObject unwrappedPromiseObj(cx, promiseObj);
    RootedObject blockedPromise(cx, blockedPromise_);

    mozilla::Maybe<AutoCompartment> ac;
    if (IsWrapper(promiseObj)) {
        unwrappedPromiseObj = CheckedUnwrap(promiseObj);
        if (!unwrappedPromiseObj)
            return false;
        ac.emplace(cx, unwrappedPromiseObj);
        if (!cx->compartment()->wrap(cx, &blockedPromise))
            return false;
    }

    // If the object to depend on isn't a, maybe-wrapped, Promise instance,
    // we ignore it. All this does is lose some small amount of debug
    // information in scenarios that are highly unlikely to occur in useful
    // code.
    if (!unwrappedPromiseObj->is<PromiseObject>())
        return true;

    Rooted<PromiseObject*> promise(cx, &unwrappedPromiseObj->as<PromiseObject>());
    return AddPromiseReaction(cx, promise, UndefinedHandleValue, UndefinedHandleValue,
                              blockedPromise, nullptr, nullptr, nullptr);
}

static MOZ_MUST_USE bool
AddPromiseReaction(JSContext* cx, Handle<PromiseObject*> promise,
                   Handle<PromiseReactionRecord*> reaction)
{
    RootedValue reactionVal(cx, ObjectValue(*reaction));

    // The code that creates Promise reactions can handle wrapped Promises,
    // unwrapping them as needed. That means that the `promise` and `reaction`
    // objects we have here aren't necessarily from the same compartment. In
    // order to store the reaction on the promise, we have to ensure that it
    // is properly wrapped.
    mozilla::Maybe<AutoCompartment> ac;
    if (promise->compartment() != cx->compartment()) {
        ac.emplace(cx, promise);
        if (!cx->compartment()->wrap(cx, &reactionVal))
            return false;
    }

    // 25.4.5.3.1 steps 7.a,b.
    RootedValue reactionsVal(cx, promise->getFixedSlot(PromiseSlot_ReactionsOrResult));
    RootedNativeObject reactions(cx);

    if (reactionsVal.isUndefined()) {
        // If no reactions existed so far, just store the reaction record directly.
        promise->setFixedSlot(PromiseSlot_ReactionsOrResult, reactionVal);
        return true;
    }

    RootedObject reactionsObj(cx, &reactionsVal.toObject());

    // If only a single reaction exists, it's stored directly instead of in a
    // list. In that case, `reactionsObj` might be a wrapper, which we can
    // always safely unwrap. It is always safe to unwrap it in that case.
    if (IsWrapper(reactionsObj)) {
        reactionsObj = UncheckedUnwrap(reactionsObj);
        MOZ_ASSERT(reactionsObj->is<PromiseReactionRecord>());
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
        reactions = &reactionsObj->as<NativeObject>();
        uint32_t len = reactions->getDenseInitializedLength();
        if (reactions->ensureDenseElements(cx, 0, len + 1) != DenseElementResult::Success)
            return false;
        reactions->setDenseElement(len, reactionVal);
    }

    return true;
}

static MOZ_MUST_USE bool
AddPromiseReaction(JSContext* cx, Handle<PromiseObject*> promise, HandleValue onFulfilled,
                   HandleValue onRejected, HandleObject dependentPromise,
                   HandleObject resolve, HandleObject reject, HandleObject incumbentGlobal)
{
    if (promise->state() != JS::PromiseState::Pending)
        return true;

    Rooted<PromiseReactionRecord*> reaction(cx, NewReactionRecord(cx, dependentPromise,
                                                                  onFulfilled, onRejected,
                                                                  resolve, reject,
                                                                  incumbentGlobal));
    if (!reaction)
        return false;
    return AddPromiseReaction(cx, promise, reaction);
}

namespace {
// Generator used by PromiseObject::getID.
mozilla::Atomic<uint64_t> gIDGenerator(0);
} // namespace

double
PromiseObject::lifetime()
{
    return MillisecondsSinceStartup() - allocationTime();
}

uint64_t
PromiseObject::getID()
{
    Value idVal(getFixedSlot(PromiseSlot_Id));
    if (idVal.isUndefined()) {
        idVal.setDouble(++gIDGenerator);
        setFixedSlot(PromiseSlot_Id, idVal);
    }
    return uint64_t(idVal.toNumber());
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

    RootedValue reactionsVal(cx, getFixedSlot(PromiseSlot_ReactionsOrResult));

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

bool
PromiseObject::resolve(JSContext* cx, HandleValue resolutionValue)
{
    if (state() != JS::PromiseState::Pending)
        return true;

    RootedObject resolveFun(cx, GetResolveFunctionFromPromise(this));
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

bool
PromiseObject::reject(JSContext* cx, HandleValue rejectionValue)
{
    if (state() != JS::PromiseState::Pending)
        return true;

    RootedValue funVal(cx, this->getFixedSlot(PromiseSlot_RejectFunction));
    MOZ_ASSERT(IsCallable(funVal));

    FixedInvokeArgs<1> args(cx);
    args[0].set(rejectionValue);

    RootedValue dummy(cx);
    return Call(cx, funVal, UndefinedHandleValue, args, &dummy);
}

void
PromiseObject::onSettled(JSContext* cx)
{
    Rooted<PromiseObject*> promise(cx, this);
    RootedObject stack(cx);
    if (cx->options().asyncStack() || cx->compartment()->isDebuggee()) {
        if (!JS::CaptureCurrentStack(cx, &stack, JS::StackCapture(JS::AllFrames()))) {
            cx->clearPendingException();
            return;
        }
    }
    promise->setFixedSlot(PromiseSlot_ResolutionSite, ObjectOrNullValue(stack));
    promise->setFixedSlot(PromiseSlot_ResolutionTime, DoubleValue(MillisecondsSinceStartup()));

    if (promise->state() == JS::PromiseState::Rejected && promise->isUnhandled())
        cx->runtime()->addUnhandledRejectedPromise(cx, promise);

    JS::dbg::onPromiseSettled(cx, promise);
}

MOZ_MUST_USE bool
js::EnqueuePromiseReactions(JSContext* cx, Handle<PromiseObject*> promise,
                            HandleObject dependentPromise,
                            HandleValue onFulfilled, HandleValue onRejected)
{
    MOZ_ASSERT_IF(dependentPromise, dependentPromise->is<PromiseObject>());
    return PerformPromiseThen(cx, promise, onFulfilled, onRejected, dependentPromise,
                              nullptr, nullptr);
}

PromiseTask::PromiseTask(JSContext* cx, Handle<PromiseObject*> promise)
  : runtime_(cx),
    promise_(cx, promise)
{}

PromiseTask::~PromiseTask()
{
    MOZ_ASSERT(CurrentThreadCanAccessZone(promise_->zone()));
}

void
PromiseTask::finish(JSContext* cx)
{
    MOZ_ASSERT(cx == runtime_);
    {
        // We can't leave a pending exception when returning to the caller so do
        // the same thing as Gecko, which is to ignore the error. This should
        // only happen due to OOM or interruption.
        AutoCompartment ac(cx, promise_);
        if (!finishPromise(cx, promise_))
            cx->clearPendingException();
    }
    js_delete(this);
}

void
PromiseTask::cancel(JSContext* cx)
{
    MOZ_ASSERT(cx == runtime_);
    js_delete(this);
}

static JSObject*
CreatePromisePrototype(JSContext* cx, JSProtoKey key)
{
    return cx->global()->createBlankPrototype(cx, &PromiseObject::protoClass_);
}

static const JSFunctionSpec promise_methods[] = {
    JS_SELF_HOSTED_FN("catch", "Promise_catch", 1, 0),
    JS_FN("then", Promise_then, 2, 0),
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
    JS_SELF_HOSTED_SYM_GET(species, "Promise_static_get_species", 0),
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

static const ClassSpec PromiseObjectProtoClassSpec = {
    DELEGATED_CLASSSPEC(PromiseObject::class_.spec),
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    ClassSpec::IsDelegated
};

const Class PromiseObject::protoClass_ = {
    "PromiseProto",
    JSCLASS_HAS_CACHED_PROTO(JSProto_Promise),
    JS_NULL_CLASS_OPS,
    &PromiseObjectProtoClassSpec
};
