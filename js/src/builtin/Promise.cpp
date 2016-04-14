/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "builtin/Promise.h"

#include "mozilla/Atomics.h"

#include "jscntxt.h"

#include "gc/Heap.h"
#include "js/Date.h"
#include "js/Debug.h"

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

// ES6, 25.4.3.1. steps 3-11.
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

        promise = &NewObjectWithClassProto(cx, &class_, usedProto)->as<PromiseObject>();

        // Step 4.
        if (!promise)
            return nullptr;

        // Step 5.
        promise->setFixedSlot(PROMISE_STATE_SLOT, Int32Value(PROMISE_STATE_PENDING));

        // Step 6.
        RootedArrayObject reactions(cx, NewDenseEmptyArray(cx));
        if (!reactions)
            return nullptr;
        promise->setFixedSlot(PROMISE_FULFILL_REACTIONS_SLOT, ObjectValue(*reactions));

        // Step 7.
        reactions = NewDenseEmptyArray(cx);
        if (!reactions)
            return nullptr;
        promise->setFixedSlot(PROMISE_REJECT_REACTIONS_SLOT, ObjectValue(*reactions));

        RootedObject stack(cx);
        if (!JS::CaptureCurrentStack(cx, &stack, 0))
            return nullptr;
        promise->setFixedSlot(PROMISE_ALLOCATION_SITE_SLOT, ObjectValue(*stack));
        Value now = JS::TimeValue(JS::TimeClip(static_cast<double>(PRMJ_Now()) /
                                               PRMJ_USEC_PER_MSEC));
        promise->setFixedSlot(PROMISE_ALLOCATION_TIME_SLOT, now);
    }

    RootedValue promiseVal(cx, ObjectValue(*promise));
    if (wrappedProto && !cx->compartment()->wrap(cx, &promiseVal))
        return nullptr;

    // Step 8.
    // The resolving functions are created in the compartment active when the
    // (maybe wrapped) Promise constructor was called. They contain checks and
    // can unwrap the Promise if required.
    RootedValue resolvingFunctionsVal(cx);
    if (!GlobalObject::getIntrinsicValue(cx, cx->global(), cx->names().CreateResolvingFunctions,
                                         &resolvingFunctionsVal))
    {
        return nullptr;
    }

    RootedArrayObject resolvingFunctions(cx);
    {
        FixedInvokeArgs<1> args(cx);

        args[0].set(promiseVal);

        RootedValue rval(cx);
        if (!Call(cx, resolvingFunctionsVal, UndefinedHandleValue, args, &rval))
            return nullptr;

        resolvingFunctions = &rval.toObject().as<ArrayObject>();
    }

    RootedValue resolveVal(cx, resolvingFunctions->getDenseElement(0));
    MOZ_ASSERT(IsCallable(resolveVal));
    RootedValue rejectVal(cx, resolvingFunctions->getDenseElement(1));
    MOZ_ASSERT(IsCallable(rejectVal));

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

    JS::dbg::onNewPromise(cx, promise);

    // Step 11.
    return promise;
}

namespace {
// Generator used by PromiseObject::getID.
mozilla::Atomic<uint64_t> gIDGenerator(0);
} // namespace

double
PromiseObject::getID()
{
    Value idVal(getReservedSlot(PROMISE_ID_SLOT));
    if (idVal.isUndefined()) {
        idVal.setDouble(++gIDGenerator);
        setReservedSlot(PROMISE_ID_SLOT, idVal);
    }
    return idVal.toNumber();
}

/**
 * Returns all promises that directly depend on this one. That means those
 * created by calling `then` on this promise, or the promise returned by
 * `Promise.all(iterable)` or `Promise.race(iterable)`, with this promise
 * being a member of the passed-in `iterable`.
 *
 *
 * For the then() case, we have both resolve and reject callbacks that know
 * what the next promise is.
 *
 * For the race() case, likewise.
 *
 * For the all() case, our reject callback knows what the next promise is, but
 * our resolve callback doesn't.
 *
 * So we walk over our _reject_ callbacks and ask each of them what promise
 * its dependent promise is.
 */
bool
PromiseObject::dependentPromises(JSContext* cx, AutoValueVector& values)
{
    RootedValue rejectReactionsVal(cx, getReservedSlot(PROMISE_REJECT_REACTIONS_SLOT));
    RootedObject rejectReactions(cx, rejectReactionsVal.toObjectOrNull());
    if (!rejectReactions)
        return true;

    AutoIdVector keys(cx);
    if (!GetPropertyKeys(cx, rejectReactions, JSITER_OWNONLY, &keys))
        return false;

    if (keys.length() == 0)
        return true;

    if (!values.growBy(keys.length()))
        return false;

    RootedAtom capabilitiesAtom(cx, Atomize(cx, "capabilities", strlen("capabilities")));
    if (!capabilitiesAtom)
        return false;
    RootedId capabilitiesId(cx, AtomToId(capabilitiesAtom));

    // Each reaction is an internally-created object with the structure:
    // {
    //   capabilities: {
    //     promise: [the promise this reaction resolves],
    //     resolve: [the `resolve` callback content code provided],
    //     reject:  [the `reject` callback content code provided],
    //    },
    //    handler: [the internal handler that fulfills/rejects the promise]
    //  }
    //
    // In the following loop we collect the `capabilities.promise` values for
    // each reaction.
    for (size_t i = 0; i < keys.length(); i++) {
        MutableHandleValue val = values[i];
        if (!GetProperty(cx, rejectReactions, rejectReactions, keys[i], val))
            return false;
        RootedObject reaction(cx, &val.toObject());
        if (!GetProperty(cx, reaction, reaction, capabilitiesId, val))
            return false;
        RootedObject capabilities(cx, &val.toObject());
        if (!GetProperty(cx, capabilities, capabilities, cx->runtime()->commonNames->promise, val))
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
