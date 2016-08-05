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

static const JSFunctionSpec promise_methods[] = {
    JS_SELF_HOSTED_FN("catch", "Promise_catch", 1, 0),
    JS_SELF_HOSTED_FN("then", "Promise_then", 2, 0),
    JS_FS_END
};

static const JSFunctionSpec promise_static_methods[] = {
    JS_SELF_HOSTED_FN("all", "Promise_static_all", 1, 0),
    JS_SELF_HOSTED_FN("race", "Promise_static_race", 1, 0),
    JS_SELF_HOSTED_FN("reject", "Promise_static_reject", 1, 0),
    JS_SELF_HOSTED_FN("resolve", "Promise_static_resolve", 1, 0),
    JS_FS_END
};

static const JSPropertySpec promise_static_properties[] = {
    JS_SELF_HOSTED_SYM_GET(species, "Promise_static_get_species", 0),
    JS_PS_END
};

static double
MillisecondsSinceStartup()
{
    auto now = mozilla::TimeStamp::Now();
    bool ignored;
    return (now - mozilla::TimeStamp::ProcessCreation(ignored)).ToMilliseconds();
}

enum ResolutionFunctionSlots {
    ResolutionFunctionSlot_Promise = 0,
    ResolutionFunctionSlot_OtherFunction,
};

// ES2016, 25.4.1.8.
static MOZ_MUST_USE bool
TriggerPromiseReactions(JSContext* cx, HandleValue reactionsVal, JS::PromiseState state,
                        HandleValue valueOrReason)
{
    RootedObject reactions(cx, &reactionsVal.toObject());

    AutoIdVector keys(cx);
    if (!GetPropertyKeys(cx, reactions, JSITER_OWNONLY, &keys))
        return false;
    MOZ_ASSERT(keys.length() > 0, "Reactions list should be created lazily");

    RootedPropertyName handlerName(cx);
    handlerName = state == JS::PromiseState::Fulfilled
                  ? cx->names().fulfillHandler
                  : cx->names().rejectHandler;

    // Each reaction is an internally-created object with the structure:
    // {
    //   promise: [the promise this reaction resolves],
    //   resolve: [the `resolve` callback content code provided],
    //   reject:  [the `reject` callback content code provided],
    //   fulfillHandler: [the internal handler that fulfills the promise]
    //   rejectHandler: [the internal handler that rejects the promise]
    //   incumbentGlobal: [an object from the global that was incumbent when
    //                     the reaction was created]
    // }
    RootedValue val(cx);
    RootedObject reaction(cx);
    RootedValue handler(cx);
    RootedObject promise(cx);
    RootedObject resolve(cx);
    RootedObject reject(cx);
    RootedObject objectFromIncumbentGlobal(cx);

    for (size_t i = 0; i < keys.length(); i++) {
        if (!GetProperty(cx, reactions, reactions, keys[i], &val))
            return false;
        reaction = &val.toObject();

        if (!GetProperty(cx, reaction, reaction, cx->names().promise, &val))
            return false;

        // The jsapi function AddPromiseReactions can add reaction records
        // without a `promise` object. See comment for EnqueuePromiseReactions
        // in Promise.js.
        promise = val.toObjectOrNull();

        if (!GetProperty(cx, reaction, reaction, handlerName, &handler))
            return false;

#ifdef DEBUG
        if (handler.isNumber()) {
            MOZ_ASSERT(handler.toNumber() == PROMISE_HANDLER_IDENTITY ||
                       handler.toNumber() == PROMISE_HANDLER_THROWER);
        } else {
            MOZ_ASSERT(handler.toObject().isCallable());
        }
#endif

        if (!GetProperty(cx, reaction, reaction, cx->names().resolve, &val))
            return false;
        resolve = &val.toObject();
        MOZ_ASSERT(IsCallable(resolve));

        if (!GetProperty(cx, reaction, reaction, cx->names().reject, &val))
            return false;
        reject = &val.toObject();
        MOZ_ASSERT(IsCallable(reject));

        if (!GetProperty(cx, reaction, reaction, cx->names().incumbentGlobal, &val))
            return false;
        objectFromIncumbentGlobal = val.toObjectOrNull();

        if (!EnqueuePromiseReactionJob(cx, handler, valueOrReason, resolve, reject, promise,
                                       objectFromIncumbentGlobal))
        {
            return false;
        }
    }

    return true;
}

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
    RootedValue reactionsVal(cx, promise->getFixedSlot(PROMISE_REACTIONS_SLOT));

    // Step 3.
    promise->setFixedSlot(PROMISE_RESULT_SLOT, valueOrReason);

    // Steps 4-5.
    promise->setFixedSlot(PROMISE_REACTIONS_SLOT, UndefinedValue());

    // Step 6.
    promise->setFixedSlot(PROMISE_STATE_SLOT, Int32Value(int32_t(state)));

    // Also null out the resolve/reject functions so they can be GC'd.
    promise->setFixedSlot(PROMISE_RESOLVE_FUNCTION_SLOT, UndefinedValue());
    promise->setFixedSlot(PROMISE_REJECT_FUNCTION_SLOT, UndefinedValue());

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
FulfillMaybeWrappedPromise(JSContext *cx, HandleObject promiseObj, HandleValue value_) {
    Rooted<PromiseObject*> promise(cx);
    RootedValue value(cx, value_);

    mozilla::Maybe<AutoCompartment> ac;
    if (!IsWrapper(promiseObj)) {
        promise = &promiseObj->as<PromiseObject>();
    } else {
        promise = &UncheckedUnwrap(promiseObj)->as<PromiseObject>();
        ac.emplace(cx, promise);
        if (!promise->compartment()->wrap(cx, &value))
            return false;
    }

    MOZ_ASSERT(promise->state() == JS::PromiseState::Pending);

    return ResolvePromise(cx, promise, value, JS::PromiseState::Fulfilled);
}

// ES2016, 25.4.1.7.
static MOZ_MUST_USE bool
RejectMaybeWrappedPromise(JSContext *cx, HandleObject promiseObj, HandleValue reason_) {
    Rooted<PromiseObject*> promise(cx);
    RootedValue reason(cx, reason_);

    mozilla::Maybe<AutoCompartment> ac;
    if (!IsWrapper(promiseObj)) {
        promise = &promiseObj->as<PromiseObject>();
    } else {
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

static void
ClearResolutionFunctionSlots(JSFunction* resolutionFun)
{
    JSFunction* otherFun = &resolutionFun->getExtendedSlot(ResolutionFunctionSlot_OtherFunction)
                           .toObject().as<JSFunction>();
    resolutionFun->setExtendedSlot(ResolutionFunctionSlot_Promise, UndefinedValue());
    otherFun->setExtendedSlot(ResolutionFunctionSlot_Promise, UndefinedValue());

    // Also reset the reference to the resolve function so it can be collected.
    resolutionFun->setExtendedSlot(ResolutionFunctionSlot_OtherFunction, UndefinedValue());
    otherFun->setExtendedSlot(ResolutionFunctionSlot_OtherFunction, UndefinedValue());
}
// ES2016, 25.4.1.3.1.
static bool
RejectPromiseFunction(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    RootedFunction reject(cx, &args.callee().as<JSFunction>());
    RootedValue reasonVal(cx, args.get(0));

    // Steps 1-2.
    RootedValue promiseVal(cx, reject->getExtendedSlot(ResolutionFunctionSlot_Promise));

    // Steps 3-4.
    // We use the existence of the Promise a a signal for whether it was
    // already resolved. Upon resolution, it's reset to `undefined`.
    if (promiseVal.isUndefined()) {
        args.rval().setUndefined();
        return true;
    }

    RootedObject promise(cx, &promiseVal.toObject());

    // Step 5.
    ClearResolutionFunctionSlots(reject);

    // Step 6.
    bool result = RejectMaybeWrappedPromise(cx, promise, reasonVal);
    if (result)
        args.rval().setUndefined();
    return result;
}

// ES2016, 25.4.1.3.2.
static bool
ResolvePromiseFunction(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    RootedFunction resolve(cx, &args.callee().as<JSFunction>());
    RootedValue resolutionVal(cx, args.get(0));

    // Steps 1-2.
    RootedValue promiseVal(cx, resolve->getExtendedSlot(ResolutionFunctionSlot_Promise));

    // Steps 3-4.
    // We use the existence of the Promise a a signal for whether it was
    // already resolved. Upon resolution, it's reset to `undefined`.
    if (promiseVal.isUndefined()) {
        args.rval().setUndefined();
        return true;
    }

    RootedObject promise(cx, &promiseVal.toObject());

    // Step 5.
    ClearResolutionFunctionSlots(resolve);

    // Step 6.
    if (resolutionVal == promiseVal) {
        // Step 6.a.
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr,
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

    // Step 7.
    if (!resolutionVal.isObject()) {
        bool status = FulfillMaybeWrappedPromise(cx, promise, resolutionVal);
        if (status)
            args.rval().setUndefined();
        return status;
    }

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
    if (!IsCallable(thenVal)) {
        bool status = FulfillMaybeWrappedPromise(cx, promise, resolutionVal);
        if (status)
            args.rval().setUndefined();
        return status;
    }

    // Step 12.
    if (!EnqueuePromiseResolveThenableJob(cx, promiseVal, resolutionVal, thenVal))
        return false;

    // Step 13.
    args.rval().setUndefined();
    return true;
}

// ES2016, 25.4.1.3.
static MOZ_MUST_USE bool
CreateResolvingFunctions(JSContext* cx, HandleValue promise,
                         MutableHandleValue resolveVal,
                         MutableHandleValue rejectVal)
{
    RootedAtom funName(cx, cx->names().empty);
    RootedFunction resolve(cx, NewNativeFunction(cx, ResolvePromiseFunction, 1, funName,
                                                 gc::AllocKind::FUNCTION_EXTENDED));
    if (!resolve)
        return false;

    RootedFunction reject(cx, NewNativeFunction(cx, RejectPromiseFunction, 1, funName,
                                                 gc::AllocKind::FUNCTION_EXTENDED));
    if (!reject)
        return false;

    // The resolving functions are trusted, so self-hosted code should be able
    // to call them using callFunction instead of callContentFunction.
    resolve->setFlags(resolve->flags() | JSFunction::SELF_HOSTED);
    reject->setFlags(reject->flags() | JSFunction::SELF_HOSTED);

    resolve->setExtendedSlot(ResolutionFunctionSlot_Promise, promise);
    resolve->setExtendedSlot(ResolutionFunctionSlot_OtherFunction, ObjectValue(*reject));

    reject->setExtendedSlot(ResolutionFunctionSlot_Promise, promise);
    reject->setExtendedSlot(ResolutionFunctionSlot_OtherFunction, ObjectValue(*resolve));

    resolveVal.setObject(*resolve);
    rejectVal.setObject(*reject);

    return true;
}

// ES2016, February 12 draft, 25.4.3.1. steps 3-11.
PromiseObject*
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


    // Step 3.
    Rooted<PromiseObject*> promise(cx);
    {
        // Enter the unwrapped proto's compartment, if that's different from
        // the current one.
        // All state stored in a Promise's fixed slots must be created in the
        // same compartment, so we get all of that out of the way here.
        // (Except for the resolution functions, which are created below.)
        mozilla::Maybe<AutoCompartment> ac;
        if (wrappedProto)
            ac.emplace(cx, usedProto);

        promise = NewObjectWithClassProto<PromiseObject>(cx, usedProto);
        if (!promise)
            return nullptr;

        // Step 4.
        promise->setFixedSlot(PROMISE_STATE_SLOT, Int32Value(PROMISE_STATE_PENDING));

        // Steps 5-6.
        // Omitted, we allocate our single list of reaction records lazily.

        // Step 7.
        promise->setFixedSlot(PROMISE_IS_HANDLED_SLOT,
                              Int32Value(PROMISE_IS_HANDLED_STATE_UNHANDLED));

        // Store an allocation stack so we can later figure out what the
        // control flow was for some unexpected results. Frightfully expensive,
        // but oh well.
        RootedObject stack(cx);
        if (cx->options().asyncStack() || cx->compartment()->isDebuggee()) {
            if (!JS::CaptureCurrentStack(cx, &stack, JS::StackCapture(JS::AllFrames())))
                return nullptr;
        }
        promise->setFixedSlot(PROMISE_ALLOCATION_SITE_SLOT, ObjectOrNullValue(stack));
        promise->setFixedSlot(PROMISE_ALLOCATION_TIME_SLOT,
                              DoubleValue(MillisecondsSinceStartup()));
    }

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
        RootedValue wrappedResolveVal(cx, resolveVal);
        RootedValue wrappedRejectVal(cx, rejectVal);
        if (!cx->compartment()->wrap(cx, &wrappedResolveVal) ||
            !cx->compartment()->wrap(cx, &wrappedRejectVal))
        {
            return nullptr;
        }
        promise->setFixedSlot(PROMISE_RESOLVE_FUNCTION_SLOT, wrappedResolveVal);
        promise->setFixedSlot(PROMISE_REJECT_FUNCTION_SLOT, wrappedRejectVal);
    } else {
        promise->setFixedSlot(PROMISE_RESOLVE_FUNCTION_SLOT, resolveVal);
        promise->setFixedSlot(PROMISE_REJECT_FUNCTION_SLOT, rejectVal);
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
    Value idVal(getReservedSlot(PROMISE_ID_SLOT));
    if (idVal.isUndefined()) {
        idVal.setDouble(++gIDGenerator);
        setReservedSlot(PROMISE_ID_SLOT, idVal);
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
    RootedValue reactionsVal(cx, getReservedSlot(PROMISE_REACTIONS_SLOT));
    if (reactionsVal.isNullOrUndefined())
        return true;
    RootedObject reactions(cx, &reactionsVal.toObject());

    AutoIdVector keys(cx);
    if (!GetPropertyKeys(cx, reactions, JSITER_OWNONLY, &keys))
        return false;

    if (keys.length() == 0)
        return true;

    if (!values.growBy(keys.length()))
        return false;

    // Each reaction is an internally-created object with the structure:
    // {
    //   promise: [the promise this reaction resolves],
    //   resolve: [the `resolve` callback content code provided],
    //   reject:  [the `reject` callback content code provided],
    //   fulfillHandler: [the internal handler that fulfills the promise]
    //   rejectHandler: [the internal handler that rejects the promise]
    //   incumbentGlobal: [an object from the global that was incumbent when
    //                     the reaction was created]
    // }
    //
    // In the following loop we collect the `capabilities.promise` values for
    // each reaction.
    for (size_t i = 0; i < keys.length(); i++) {
        MutableHandleValue val = values[i];
        if (!GetProperty(cx, reactions, reactions, keys[i], val))
            return false;
        RootedObject reaction(cx, &val.toObject());
        if (!GetProperty(cx, reaction, reaction, cx->names().promise, val))
            return false;
    }

    return true;
}

namespace js {

// ES6, 25.4.3.1.
bool
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



bool
PromiseObject::resolve(JSContext* cx, HandleValue resolutionValue)
{
    if (this->getFixedSlot(PROMISE_STATE_SLOT).toInt32() != unsigned(JS::PromiseState::Pending))
        return true;

    RootedValue funVal(cx, this->getReservedSlot(PROMISE_RESOLVE_FUNCTION_SLOT));
    MOZ_ASSERT(funVal.toObject().is<JSFunction>());

    FixedInvokeArgs<1> args(cx);

    args[0].set(resolutionValue);

    RootedValue dummy(cx);
    return Call(cx, funVal, UndefinedHandleValue, args, &dummy);
}

bool
PromiseObject::reject(JSContext* cx, HandleValue rejectionValue)
{
    if (this->getFixedSlot(PROMISE_STATE_SLOT).toInt32() != unsigned(JS::PromiseState::Pending))
        return true;

    RootedValue funVal(cx, this->getReservedSlot(PROMISE_REJECT_FUNCTION_SLOT));
    MOZ_ASSERT(funVal.toObject().is<JSFunction>());

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
    promise->setFixedSlot(PROMISE_RESOLUTION_SITE_SLOT, ObjectOrNullValue(stack));
    promise->setFixedSlot(PROMISE_RESOLUTION_TIME_SLOT, DoubleValue(MillisecondsSinceStartup()));

    if (promise->state() == JS::PromiseState::Rejected &&
        promise->getFixedSlot(PROMISE_IS_HANDLED_SLOT).toInt32() !=
            PROMISE_IS_HANDLED_STATE_HANDLED)
    {
        cx->runtime()->addUnhandledRejectedPromise(cx, promise);
    }

    JS::dbg::onPromiseSettled(cx, promise);
}

enum ReactionJobSlots {
    ReactionJobSlot_Handler = 0,
    ReactionJobSlot_JobData,
};

enum ReactionJobDataSlots {
    ReactionJobDataSlot_HandlerArg = 0,
    ReactionJobDataSlot_ResolveHook,
    ReactionJobDataSlot_RejectHook,
    ReactionJobDataSlotsCount,
};

// ES6, 25.4.2.1.
/**
 * Callback triggering the fulfill/reject reaction for a resolved Promise,
 * to be invoked by the embedding during its processing of the Promise job
 * queue.
 *
 * See http://www.ecma-international.org/ecma-262/6.0/index.html#sec-jobs-and-job-queues
 *
 * A PromiseReactionJob is set as the native function of an extended
 * JSFunction object, with all information required for the job's
 * execution stored in the function's extended slots.
 *
 * Usage of the function's extended slots is as follows:
 * ReactionJobSlot_Handler: The handler to use as the Promise reaction.
 *                          This can be PROMISE_HANDLER_IDENTITY,
 *                          PROMISE_HANDLER_THROWER, or a callable. In the
 *                          latter case, it's guaranteed to be an object from
 *                          the same compartment as the PromiseReactionJob.
 * ReactionJobSlot_JobData: JobData - a, potentially CCW-wrapped, dense list
 *                          containing data required for proper execution of
 *                          the reaction.
 *
 * The JobData list has the following entries:
 * ReactionJobDataSlot_HandlerArg: Value passed as argument when invoking the
 *                                 reaction handler.
 * ReactionJobDataSlot_ResolveHook: The Promise's resolve hook, invoked if the
 *                                  handler is PROMISE_HANDLER_IDENTITY or
 *                                  upon successful execution of a callable
 *                                  handler.
 *  ReactionJobDataSlot_RejectHook: The Promise's reject hook, invoked if the
 *                                  handler is PROMISE_HANDLER_THROWER or if
 *                                  execution of a callable handler aborts
 *                                  abnormally.
 */
static bool
PromiseReactionJob(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    RootedFunction job(cx, &args.callee().as<JSFunction>());

    RootedValue handlerVal(cx, job->getExtendedSlot(ReactionJobSlot_Handler));
    RootedObject jobDataObj(cx, &job->getExtendedSlot(ReactionJobSlot_JobData).toObject());

    // To ensure that the embedding ends up with the right entry global, we're
    // guaranteeing that the reaction job function gets created in the same
    // compartment as the handler function. That's not necessarily the global
    // that the job was triggered from, though. To be able to find the
    // triggering global, we always create the jobArgs object in that global
    // and wrap it into the handler's. So to go back, we check if jobArgsObj
    // is a wrapper and if so, unwrap it, enter its compartment, and wrap
    // the handler into that compartment.
    //
    // See the doc comment for PromiseReactionJob for how this information is
    // stored.
    mozilla::Maybe<AutoCompartment> ac;
    if (IsWrapper(jobDataObj)) {
        jobDataObj = UncheckedUnwrap(jobDataObj);
        ac.emplace(cx, jobDataObj);
        if (!cx->compartment()->wrap(cx, &handlerVal))
            return false;
    }
    RootedNativeObject jobData(cx, &jobDataObj->as<NativeObject>());
    RootedValue argument(cx, jobData->getDenseElement(ReactionJobDataSlot_HandlerArg));

    // Step 1 (omitted).

    // Steps 2-3.
    RootedValue handlerResult(cx);
    bool shouldReject = false;

    // Steps 4-7.
    if (handlerVal.isNumber()) {
        int32_t handlerNum = int32_t(handlerVal.toNumber());
        // Step 4.
        if (handlerNum == PROMISE_HANDLER_IDENTITY) {
            handlerResult = argument;
        } else {
            // Step 5.
            MOZ_ASSERT(handlerNum == PROMISE_HANDLER_THROWER);
            shouldReject = true;
            handlerResult = argument;
        }
    } else {
        // Step 6.
        FixedInvokeArgs<1> args2(cx);
        args2[0].set(argument);
        if (!Call(cx, handlerVal, UndefinedHandleValue, args2, &handlerResult)) {
            shouldReject = true;
            // Not much we can do about uncatchable exceptions, so just bail
            // for those.
            if (!cx->isExceptionPending() || !GetAndClearException(cx, &handlerResult))
                return false;
        }
    }

    // Steps 7-9.
    size_t hookSlot = shouldReject
                      ? ReactionJobDataSlot_RejectHook
                      : ReactionJobDataSlot_ResolveHook;
    RootedObject callee(cx, &jobData->getDenseElement(hookSlot).toObject());

    FixedInvokeArgs<1> args2(cx);
    args2[0].set(handlerResult);
    RootedValue calleeOrRval(cx, ObjectValue(*callee));
    bool result = Call(cx, calleeOrRval, UndefinedHandleValue, args2, &calleeOrRval);

    args.rval().set(calleeOrRval);
    return result;
}

bool
EnqueuePromiseReactionJob(JSContext* cx, HandleValue handler_, HandleValue handlerArg,
                          HandleObject resolve, HandleObject reject,
                          HandleObject promise_, HandleObject objectFromIncumbentGlobal_)
{
    // Create a dense array to hold the data needed for the reaction job to
    // work.
    // See doc comment for PromiseReactionJob for layout details.
    RootedArrayObject data(cx, NewDenseFullyAllocatedArray(cx, ReactionJobDataSlotsCount));
    if (!data ||
        data->ensureDenseElements(cx, 0, ReactionJobDataSlotsCount) != DenseElementResult::Success)
    {
        return false;
    }

    // Store the handler argument.
    data->setDenseElement(ReactionJobDataSlot_HandlerArg, handlerArg);

    // Store the resolve hook.
    data->setDenseElement(ReactionJobDataSlot_ResolveHook, ObjectValue(*resolve));

    // Store the reject hook.
    data->setDenseElement(ReactionJobDataSlot_RejectHook, ObjectValue(*reject));

    RootedValue dataVal(cx, ObjectValue(*data));

    // Re-rooting because we might need to unwrap it.
    RootedValue handler(cx, handler_);

    // If we have a handler callback, we enter that handler's compartment so
    // that the promise reaction job function is created in that compartment.
    // That guarantees that the embedding ends up with the right entry global.
    // This is relevant for some html APIs like fetch that derive information
    // from said global.
    mozilla::Maybe<AutoCompartment> ac;
    if (handler.isObject()) {
        RootedObject handlerObj(cx, &handler.toObject());

        // The unwrapping has to be unchecked because we specifically want to
        // be able to use handlers with wrappers that would only allow calls.
        // E.g., it's ok to have a handler from a chrome compartment in a
        // reaction to a content compartment's Promise instance.
        handlerObj = UncheckedUnwrap(handlerObj);
        MOZ_ASSERT(handlerObj);
        ac.emplace(cx, handlerObj);
        handler = ObjectValue(*handlerObj);

        // We need to wrap the |data| array to store it on the job function.
        if (!cx->compartment()->wrap(cx, &dataVal))
            return false;
    }

    // Create the JS function to call when the job is triggered.
    RootedAtom funName(cx, cx->names().empty);
    RootedFunction job(cx, NewNativeFunction(cx, PromiseReactionJob, 0, funName,
                                             gc::AllocKind::FUNCTION_EXTENDED));
    if (!job)
        return false;

    // Store the handler and the data array on the reaction job.
    job->setExtendedSlot(ReactionJobSlot_Handler, handler);
    job->setExtendedSlot(ReactionJobSlot_JobData, dataVal);

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
    RootedObject promise(cx);
    if (promise_ && promise_->is<PromiseObject>()) {
      promise = promise_;
      if (!cx->compartment()->wrap(cx, &promise))
          return false;
    }

    // Using objectFromIncumbentGlobal, we can derive the incumbent global by
    // unwrapping and then getting the global. This is very convoluted, but
    // much better than having to store the original global as a private value
    // because we couldn't wrap it to store it as a normal JS value.
    RootedObject global(cx);
    RootedObject objectFromIncumbentGlobal(cx, objectFromIncumbentGlobal_);
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

enum ThenableJobSlots {
    ThenableJobSlot_Handler = 0,
    ThenableJobSlot_JobData,
};

enum ThenableJobDataSlots {
    ThenableJobDataSlot_Promise = 0,
    ThenableJobDataSlot_Thenable,
    ThenableJobDataSlotsCount,
};
// ES6, 25.4.2.2.
/**
 * Callback for resolving a thenable, to be invoked by the embedding during
 * its processing of the Promise job queue.
 *
 * See http://www.ecma-international.org/ecma-262/6.0/index.html#sec-jobs-and-job-queues
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

    RootedValue promise(cx, jobArgs->getDenseElement(ThenableJobDataSlot_Promise));
    RootedValue thenable(cx, jobArgs->getDenseElement(ThenableJobDataSlot_Thenable));

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

bool
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
                                             gc::AllocKind::FUNCTION_EXTENDED));
    if (!job)
        return false;

    // Store the `then` function on the callback.
    job->setExtendedSlot(ThenableJobSlot_Handler, ObjectValue(*then));

    // Create a dense array to hold the data needed for the reaction job to
    // work.
    // See the doc comment for PromiseResolveThenableJob for the layout.
    RootedArrayObject data(cx, NewDenseFullyAllocatedArray(cx, ThenableJobDataSlotsCount));
    if (!data ||
        data->ensureDenseElements(cx, 0, ThenableJobDataSlotsCount) != DenseElementResult::Success)
    {
        return false;
    }

    // Wrap and set the `promiseToResolve` argument.
    if (!cx->compartment()->wrap(cx, &promiseToResolve))
        return false;
    data->setDenseElement(ThenableJobDataSlot_Promise, promiseToResolve);
    // At this point the promise is guaranteed to be wrapped into the job's
    // compartment.
    RootedObject promise(cx, &promiseToResolve.toObject());

    // Wrap and set the `thenable` argument.
    MOZ_ASSERT(thenable.isObject());
    if (!cx->compartment()->wrap(cx, &thenable))
        return false;
    data->setDenseElement(ThenableJobDataSlot_Thenable, thenable);

    // Store the data array on the reaction job.
    job->setExtendedSlot(ThenableJobSlot_JobData, ObjectValue(*data));

    RootedObject incumbentGlobal(cx, cx->runtime()->getIncumbentGlobal(cx));
    return cx->runtime()->enqueuePromiseJob(cx, job, promise, incumbentGlobal);
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

} // namespace js

static JSObject*
CreatePromisePrototype(JSContext* cx, JSProtoKey key)
{
    return cx->global()->createBlankPrototype(cx, &PromiseObject::protoClass_);
}

static const ClassSpec PromiseObjectClassSpec = {
    GenericCreateConstructor<PromiseConstructor, 1, gc::AllocKind::FUNCTION>,
    CreatePromisePrototype,
    promise_static_methods,
    promise_static_properties,
    promise_methods
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
