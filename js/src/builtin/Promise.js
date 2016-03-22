/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// ES6, 25.4.1.2.
// This object is used to verify that an object is a PromiseReaction record.
var PromiseReactionRecordProto = {__proto__: null};


// ES6, 25.4.1.3.
function CreateResolvingFunctions(promise) {
    // The callbacks created here can deal with Promises wrapped in cross-
    // compartment wrappers. That's required because in some circumstances,
    // they're created in a higher-privileged compartment from the Promise,
    // so they can be invoked seamlessly by code in that compartment.
    //
    // See the comment in PromiseConstructor (in builtin/Promise.cpp) for more
    // details.
    let unwrap = false;
    if (!IsPromise(promise)) {
        assert(IsWrappedPromise(promise),
               "CreateResolvingFunctions expects arg0 to be a - maybe wrapped - promise");
        unwrap = true;
    }

    // Step 1.
    let alreadyResolved = false;

    // Steps 2-4.
    // ES6, 25.4.1.3.2. Inlined here so we can use an upvar instead of a slot to
    // store promise and alreadyResolved, and share the latter with reject below.
    function resolve(resolution) {
        // Steps 1-3 (implicit).

        // Step 4.
        if (alreadyResolved)
            return undefined;

        // Step 5.
        alreadyResolved = true;

        // Step 6.
        // We know |promise| is an object, so using strict equality instead of
        // SameValue is fine.
        if (resolution === promise) {
            // Step 6.a.
            let selfResolutionError = GetTypeError(JSMSG_CANNOT_RESOLVE_PROMISE_WITH_ITSELF);

            // Step 6.b.
            if (unwrap) {
                return RejectUnwrappedPromise(promise, selfResolutionError);
            }
            return RejectPromise(promise, selfResolutionError);
        }

        // Step 7.
        if (!IsObject(resolution)) {
            if (unwrap) {
                return callFunction(CallPromiseMethodIfWrapped, promise, resolution,
                                    "FulfillUnwrappedPromise");
            }
            return FulfillPromise(promise, resolution);
        }

        // Steps 8-9.
        let then;
        try {
            then = resolution.then;
        } catch (e) {
            if (unwrap) {
                return RejectUnwrappedPromise(promise, e);
            }
            return RejectPromise(promise, e);
        }

        // Step 10 (implicit).

        // Step 11.
        if (!IsCallable(then)) {
            if (unwrap) {
                return callFunction(CallPromiseMethodIfWrapped, promise, resolution,
                                    "FulfillUnwrappedPromise");
            }
            return FulfillPromise(promise, resolution);
        }

        // Step 12.
        EnqueuePromiseResolveThenableJob(promise, resolution, then);

        // Step 13.
        return undefined;
    }

    // Steps 5-7.
    // ES6, 25.4.1.3.2.
    function reject(reason) {
        // Steps 1-3 (implicit).

        // Step 4.
        if (alreadyResolved)
            return undefined;

        // Step 5.
        alreadyResolved = true;

        // Step 6.
        if (unwrap) {
            return RejectUnwrappedPromise(promise, reason);
        }
        return RejectPromise(promise, reason);
    }

    // Return an array instead of an object with resolve/reject properties
    // to make value extraction from C++ easier.
    return [resolve, reject];
}

// ES6, 25.4.1.4.
function FulfillPromise(promise, value) {
    return ResolvePromise(promise, value, PROMISE_FULFILL_REACTIONS_SLOT, PROMISE_STATE_FULFILLED);
}
function FulfillUnwrappedPromise(value) {
    return ResolvePromise(this, value, PROMISE_FULFILL_REACTIONS_SLOT, PROMISE_STATE_FULFILLED);
}

// Commoned-out implementation of 25.4.1.4. and 25.4.1.7.
// ES2016 February 12 draft.
function ResolvePromise(promise, valueOrReason, reactionsSlot, state) {
    // Step 1.
    assert(GetPromiseState(promise) === PROMISE_STATE_PENDING,
           "Can't resolve non-pending promise");
    assert(state >= PROMISE_STATE_PENDING && state <= PROMISE_STATE_REJECTED,
           `Invalid Promise state <${state}>`);

    // Step 2.
    var reactions = UnsafeGetObjectFromReservedSlot(promise, reactionsSlot);

    // Step 3.
    UnsafeSetReservedSlot(promise, PROMISE_RESULT_SLOT, valueOrReason);

    // Step 4.
    UnsafeSetReservedSlot(promise, PROMISE_FULFILL_REACTIONS_SLOT, null);

    // Step 5.
    UnsafeSetReservedSlot(promise, PROMISE_REJECT_REACTIONS_SLOT, null);

    // Step 6.
    UnsafeSetReservedSlot(promise, PROMISE_STATE_SLOT, state);

    // Also null out the resolve/reject functions so they can be GC'd.
    UnsafeSetReservedSlot(promise, PROMISE_RESOLVE_FUNCTION_SLOT, null);
    UnsafeSetReservedSlot(promise, PROMISE_REJECT_FUNCTION_SLOT, null);

    // Now that everything else is done, do the things the debugger needs.
    // Step 7 of RejectPromise implemented in the debugger intrinsic.
    _dbg_onPromiseSettled(promise);

    // Step 7 of FulfillPromise.
    // Step 8 of RejectPromise.
    return TriggerPromiseReactions(reactions, valueOrReason);
}

// Used to verify that an object is a PromiseCapability record.
var PromiseCapabilityRecordProto = {__proto__: null};

// ES6, 25.4.1.5.
// Creates PromiseCapability records, see 25.4.1.1.
function NewPromiseCapability(C) {
    // Steps 1-2.
    if (!IsConstructor(C))
        ThrowTypeError(JSMSG_NOT_CONSTRUCTOR, 0);

    // Step 3. Replaced by individual fields, combined in step 11.
    let resolve;
    let reject;

    // Steps 4-5.
    // ES6, 25.4.1.5.1. Inlined here so we can use an upvar instead of a slot to
    // store promiseCapability.
    function GetCapabilitiesExecutor(resolve_, reject_) {
        // Steps 1-2 (implicit).

        // Steps 3-4.
        if (resolve !== undefined || reject !== undefined)
            ThrowTypeError(JSMSG_PROMISE_CAPABILITY_HAS_SOMETHING_ALREADY);
        resolve = resolve_;
        reject = reject_;
    }

    // Steps 6-7.
    let promise = new C(GetCapabilitiesExecutor);

    // Step 8.
    if (!IsCallable(resolve))
        ThrowTypeError(JSMSG_PROMISE_RESOLVE_FUNCTION_NOT_CALLABLE);

    // Step 9.
    if (!IsCallable(reject))
        ThrowTypeError(JSMSG_PROMISE_REJECT_FUNCTION_NOT_CALLABLE);

    // Steps 10-11.
    return {
        __proto__: PromiseCapabilityRecordProto,
        promise,
        resolve,
        reject
    };
}

// ES6, 25.4.1.6. is implemented as an intrinsic in SelfHosting.cpp.

// ES2016, February 12 draft, 25.4.1.7.
function RejectPromise(promise, reason) {
    return ResolvePromise(promise, reason, PROMISE_REJECT_REACTIONS_SLOT, PROMISE_STATE_REJECTED);
}

// ES6, 25.4.1.8.
function TriggerPromiseReactions(reactions, argument) {
    // Step 1.
    for (var i = 0, len = reactions.length; i < len; i++)
        EnqueuePromiseReactionJob(reactions[i], argument);
    // Step 2 (implicit).
}

// ES2016, February 12 draft 25.4.1.9, implemented in SelfHosting.cpp.

// ES6, 25.4.2.1.
function EnqueuePromiseReactionJob(reaction, argument) {
    let capabilities = reaction.capabilities;
    _EnqueuePromiseReactionJob([reaction.handler,
                                argument,
                                capabilities.resolve,
                                capabilities.reject
                               ],
                               capabilities.promise);
}

// ES6, 25.4.2.2.
function EnqueuePromiseResolveThenableJob(promiseToResolve, thenable, then) {
    _EnqueuePromiseResolveThenableJob([then,
                                       thenable,
                                       promiseToResolve
                                      ],
                                      promiseToResolve);
}

// ES6, 25.4.3.1. (Implemented in C++).

// ES7 2016-01-21 draft, 25.4.4.1.
function Promise_static_all(iterable) {
    // Step 1.
    let C = this;

    // Step 2.
    if (!IsObject(C))
        ThrowTypeError(JSMSG_NOT_NONNULL_OBJECT, "Receiver of Promise.all call");

    // Step 3.
    let promiseCapability = NewPromiseCapability(C);

    // Steps 4-5.
    let iterator;
    try {
        iterator = GetIterator(iterable);
    } catch (e) {
        callContentFunction(promiseCapability.reject, undefined, e);
        return promiseCapability.promise;
    }

    // Step 6.
    let iteratorRecord = {__proto__: null, iterator, done: false};

    // Steps 7-9.
    try {
        // Steps 7,9.
        return PerformPromiseAll(iteratorRecord, C, promiseCapability);
    } catch (e) {
        // Step 8.a.
        // TODO: implement iterator closing.

        // Step 8.b.
        callContentFunction(promiseCapability.reject, undefined, e);
        return promiseCapability.promise;
    }
}

// ES6, 25.4.4.1.1.
function PerformPromiseAll(iteratorRecord, constructor, resultCapability) {
    // Step 1.
    assert(IsConstructor(constructor), "PerformPromiseAll called with non-constructor");

    // Step 2.
    assert(IsPromiseCapability(resultCapability), "Invalid promise capability record");

    // Step 3.
    // Immediately create an Array instead of a List, so we can skip step 6.d.iii.1.
    //
    // We might be dealing with a wrapped instance from another Realm. In that
    // case, we want to create the `values` array in that other Realm so if
    // it's less-privileged than the current one, code in that Realm can still
    // work with the array.
    let values = IsPromise(resultCapability.promise) || !IsWrappedPromise(resultCapability.promise)
                 ? []
                 : NewArrayInCompartment(constructor);
    let valuesCount = 0;

    // Step 4.
    let remainingElementsCount = {value: 1};

    // Step 5.
    let index = 0;

    // Step 6.
    let iterator = iteratorRecord.iterator;
    let next;
    let nextValue;
    let allPromise = resultCapability.promise;
    while (true) {
        try {
            // Step 6.a.
            next = callContentFunction(iterator.next, iterator);
            if (!IsObject(next))
                ThrowTypeError(JSMSG_NEXT_RETURNED_PRIMITIVE);
        } catch (e) {
            // Step 6.b.
            iteratorRecord.done = true;

            // Step 6.c.
            throw (e);
        }

        // Step 6.d.
        if (next.done) {
            // Step 6.d.i.
            iteratorRecord.done = true;

            // Step 6.d.ii.
            remainingElementsCount.value--;
            assert(remainingElementsCount.value >= 0,
                   "remainingElementsCount mustn't be negative.");

            // Step 6.d.iii.
            if (remainingElementsCount.value === 0)
                callContentFunction(resultCapability.resolve, undefined, values);

            // Step 6.d.iv.
            return allPromise;
        }
        try {
            // Step 6.e.
            nextValue = next.value;
        } catch (e) {
            // Step 6.f.
            iteratorRecord.done = true;

            // Step 6.g.
            throw e;
        }

        // Step 6.h.
        _DefineDataProperty(values, valuesCount++, undefined);

        // Steps 6.i-j.
        let nextPromise = callContentFunction(constructor.resolve, constructor, nextValue);

        // Steps 6.k-p.
        let resolveElement = CreatePromiseAllResolveElementFunction(index, values,
                                                                    resultCapability,
                                                                    remainingElementsCount);

        // Step 6.q.
        remainingElementsCount.value++;

        // Steps 6.r-s.
        BlockOnPromise(nextPromise, allPromise, resolveElement, resultCapability.reject);

        // Step 6.t.
        index++;
    }
}

/**
 * Unforgeable version of Promise.all for internal use.
 *
 * Takes a dense array of Promise objects and returns a promise that's
 * resolved with an array of resolution values when all those promises ahve
 * been resolved, or rejected with the rejection value of the first rejected
 * promise.
 *
 * Asserts if the array isn't dense or one of the entries isn't a Promise.
 */
function GetWaitForAllPromise(promises) {
    let resultCapability = NewPromiseCapability(GetBuiltinConstructor('Promise'));
    let allPromise = resultCapability.promise;

    // Step 3.
    // Immediately create an Array instead of a List, so we can skip step 6.d.iii.1.
    let values = [];
    let valuesCount = 0;

    // Step 4.
    let remainingElementsCount = {value: 0};

    // Step 6.
    for (let i = 0; i < promises.length; i++) {
        // Parts of step 6 for deriving next promise, vastly simplified.
        assert(callFunction(std_Object_hasOwnProperty, promises, i),
               "GetWaitForAllPromise must be called with a dense array of promises");
        let nextPromise = promises[i];
        assert(IsPromise(nextPromise) || IsWrappedPromise(nextPromise),
               "promises list must only contain possibly wrapped promises");

        // Step 6.h.
        _DefineDataProperty(values, valuesCount++, undefined);

        // Steps 6.k-p.
        let resolveElement = CreatePromiseAllResolveElementFunction(i, values,
                                                                    resultCapability,
                                                                    remainingElementsCount);

        // Step 6.q.
        remainingElementsCount.value++;

        // Steps 6.r-s, very roughly.
        EnqueuePromiseReactions(nextPromise, allPromise, resolveElement, resultCapability.reject);
    }

    if (remainingElementsCount.value === 0)
        callFunction(resultCapability.resolve, undefined, values);

    return allPromise;
}

// ES6, 25.4.4.1.2.
function CreatePromiseAllResolveElementFunction(index, values, promiseCapability,
                                                remainingElementsCount)
{
    var alreadyCalled = false;
    return function PromiseAllResolveElementFunction(x) {
        // Steps 1-2.
        if (alreadyCalled)
            return undefined;

        // Step 3.
        alreadyCalled = true;

        // Steps 4-7 (implicit).

        // Step 8.
        // Note: this can't throw because the slot was initialized to `undefined` earlier.
        values[index] = x;

        // Step 9.
        remainingElementsCount.value--;
        assert(remainingElementsCount.value >= 0, "remainingElementsCount mustn't be negative.");

        // Step 10.
        if (remainingElementsCount.value === 0) {
            // Step 10.a (implicit).

            // Step 10.b.
            return callContentFunction(promiseCapability.resolve, undefined, values);
        }

        // Step 11 (implicit).
    };
}

// ES7, 2016-01-21 draft, 25.4.4.3.
function Promise_static_race(iterable) {
    // Step 1.
    let C = this;

    // Step 2.
    if (!IsObject(C))
        ThrowTypeError(JSMSG_NOT_NONNULL_OBJECT, "Receiver of Promise.race call");

    // step 3.
    let promiseCapability = NewPromiseCapability(C);

    // Steps 4-5.
    let iterator;
    try {
        iterator = GetIterator(iterable);
    } catch (e) {
        callContentFunction(promiseCapability.reject, undefined, e);
        return promiseCapability.promise;
    }

    // Step 6.
    let iteratorRecord = {__proto__: null, iterator, done: false};

    // Steps 7-9.
    try {
        // Steps 7,9.
        return PerformPromiseRace(iteratorRecord, promiseCapability, C);
    } catch (e) {
        // Step 8.a.
        // TODO: implement iterator closing.

        // Step 8.b.
        callContentFunction(promiseCapability.reject, undefined, e);
        return promiseCapability.promise;
    }
}

// ES7, 2016-01-21 draft, 25.4.4.3.1.
function PerformPromiseRace(iteratorRecord, resultCapability, C) {
    assert(IsConstructor(C), "PerformPromiseRace called with non-constructor");
    assert(IsPromiseCapability(resultCapability), "Invalid promise capability record");

    // Step 1.
    let iterator = iteratorRecord.iterator;
    let racePromise = resultCapability.promise;
    let next;
    let nextValue;
    while (true) {
        try {
            // Step 1.a.
            next = callContentFunction(iterator.next, iterator);
            if (!IsObject(next))
                ThrowTypeError(JSMSG_NEXT_RETURNED_PRIMITIVE);
        } catch (e) {
            // Step 1.b.
            iteratorRecord.done = true;

            // Step 1.c.
            throw (e);
        }

        // Step 1.d.
        if (next.done) {
            // Step 1.d.i.
            iteratorRecord.done = true;

            // Step 1.d.ii.
            return racePromise;
        }
        try {
            // Step 1.e.
            nextValue = next.value;
        } catch (e) {
            // Step 1.f.
            iteratorRecord.done = true;

            // Step 1.g.
            throw e;
        }

        // Step 1.h.
        let nextPromise = callContentFunction(C.resolve, C, nextValue);

        // Steps 1.i.
        BlockOnPromise(nextPromise, racePromise, resultCapability.resolve,
                       resultCapability.reject);
    }
    assert(false, "Shouldn't reach the end of PerformPromiseRace");
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
function BlockOnPromise(promise, blockedPromise, onResolve, onReject) {
    let then = promise.then;

    // By default, the blocked promise is added as an extra entry to the
    // rejected promises list.
    let addToDependent = true;
    if (then === Promise_then && IsObject(promise) && IsPromise(promise)) {
        // |then| is the original |Promise.prototype.then|, inline it here.
        // 25.4.5.3., steps 3-4.
        let PromiseCtor = GetBuiltinConstructor('Promise');
        let C = SpeciesConstructor(promise, PromiseCtor);
        let resultCapability;

        if (C === PromiseCtor) {
            resultCapability = {
                __proto__: PromiseCapabilityRecordProto,
                promise: blockedPromise,
                reject: NullFunction,
                resolve: NullFunction
            };
            addToDependent = false;
        } else {
            // 25.4.5.3., steps 5-6.
            resultCapability = NewPromiseCapability(C);
        }

        // 25.4.5.3., step 7.
        PerformPromiseThen(promise, onResolve, onReject, resultCapability);
    } else {
        // Optimization failed, do the normal call.
        callContentFunction(then, promise, onResolve, onReject);
    }
    if (!addToDependent)
        return;

    // The promise created by the |promise.then| call or the inlined version
    // of it above is visible to content (either because |promise.then| was
    // overridden by content and could leak it, or because a constructor
    // other than the original value of |Promise| was used to create it).
    // To have both that promise and |blockedPromise| show up as dependent
    // promises in the debugger, add a dummy reaction to the list of reject
    // reactions that contains |blockedPromise|, but otherwise does nothing.
    if (IsPromise(promise))
        return callFunction(AddPromiseReaction, promise, PROMISE_REJECT_REACTIONS_SLOT,
                            blockedPromise);

    assert(IsWrappedPromise(promise), "Can only block on, maybe wrapped, Promise objects");
    callFunction(CallPromiseMethodIfWrapped, promise, PROMISE_REJECT_REACTIONS_SLOT,
                 blockedPromise, "AddPromiseReaction");
}

/**
 * Invoked with a Promise as the receiver, AddPromiseReaction adds an entry to
 * the reactions list in `slot`, using the other parameters as values for that
 * reaction.
 *
 * If any of the callback functions aren't specified, they're set to
 * NullFunction. Doing that here is useful in case the call is performed on an
 * unwrapped Promise. Passing in NullFunctions would cause useless compartment
 * switches.
 *
 * The reason for the target Promise to be passed as the receiver is so the
 * same function can be used for wrapped and unwrapped Promise objects.
 */
function AddPromiseReaction(slot, dependentPromise, onResolve, onReject, handler) {
    assert(IsPromise(this), "AddPromiseReaction expects an unwrapped Promise as the receiver");
    assert(slot === PROMISE_FULFILL_REACTIONS_SLOT || slot === PROMISE_REJECT_REACTIONS_SLOT,
           "Invalid slot");

    if (!onResolve)
        onResolve = NullFunction;
    if (!onReject)
        onReject = NullFunction;
    if (!handler)
        handler = NullFunction;

    let reactions = UnsafeGetReservedSlot(this, slot);

    // The reactions slot might've been reset because the Promise was resolved.
    if (!reactions) {
        assert(GetPromiseState(this) !== PROMISE_STATE_PENDING,
               "Pending promises must have reactions lists.");
        return;
    }
    _DefineDataProperty(reactions, reactions.length, {
                            __proto__: PromiseReactionRecordProto,
                            capabilities: {
                                __proto__: PromiseCapabilityRecordProto,
                                promise: dependentPromise,
                                reject: onReject,
                                resolve: onResolve
                            },
                            handler: handler
                        });
}

// ES6, 25.4.4.4.
function Promise_static_reject(r) {
    // Step 1.
    let C = this;

    // Step 2.
    if (!IsObject(C))
        ThrowTypeError(JSMSG_NOT_NONNULL_OBJECT, "Receiver of Promise.reject call");

    // Steps 3-4.
    let promiseCapability = NewPromiseCapability(C);

    // Steps 5-6.
    callContentFunction(promiseCapability.reject, undefined, r);

    // Step 7.
    return promiseCapability.promise;
}

// ES6, 25.4.4.5.
function Promise_static_resolve(x) {
    // Step 1.
    let C = this;

    // Step 2.
    if (!IsObject(C))
        ThrowTypeError(JSMSG_NOT_NONNULL_OBJECT, "Receiver of Promise.resolve call");

    // Step 3.
    if (IsObject(x) && (IsPromise(x) || IsWrappedPromise(x)) && x.constructor === C)
        return x;

    // Steps 4-5.
    let promiseCapability = NewPromiseCapability(C);

    // Steps 6-7.
    callContentFunction(promiseCapability.resolve, undefined, x);

    // Step 8.
    return promiseCapability.promise;
}

//ES6, 25.4.4.6.
function Promise_static_get_species() {
    // Step 1.
    return this;
}
_SetCanonicalName(Promise_static_get_species, "get [Symbol.species]");

// ES6, 25.4.5.1.
function Promise_catch(onRejected) {
    // Steps 1-2.
    return callContentFunction(this.then, this, undefined, onRejected);
}

// ES6, 25.4.5.3.
function Promise_then(onFulfilled, onRejected) {
    // Step 1.
    let promise = this;

    // Step 2.
    if (!IsObject(promise))
        ThrowTypeError(JSMSG_NOT_NONNULL_OBJECT, "Receiver of Promise.prototype.then call");

    let isPromise = IsPromise(promise);
    let isWrappedPromise = isPromise ? false : IsWrappedPromise(promise);
    if (!(isPromise || isWrappedPromise))
        ThrowTypeError(JSMSG_INCOMPATIBLE_PROTO, "Promise", "then", "value");

    // Steps 3-4.
    let C = SpeciesConstructor(promise, GetBuiltinConstructor('Promise'));

    // Steps 5-6.
    let resultCapability = NewPromiseCapability(C);

    // Step 7.
    if (isWrappedPromise) {
        // See comment above GetPromiseHandlerForwarders for why this is needed.
        let handlerForwarders = GetPromiseHandlerForwarders(onFulfilled, onRejected);
        return callFunction(CallPromiseMethodIfWrapped, promise,
                            handlerForwarders[0], handlerForwarders[1],
                            resultCapability.promise, resultCapability.resolve,
                            resultCapability.reject, "UnwrappedPerformPromiseThen");
    }
    return PerformPromiseThen(promise, onFulfilled, onRejected, resultCapability);
}

/**
 * Enqueues resolve/reject reactions in the given Promise's reactions lists
 * in a content-invisible way.
 *
 * Used internally to implement DOM functionality.
 *
 * Note: the reactions pushed using this function contain a `capabilities`
 * object whose `promise` field can contain null. That field is only ever used
 * by devtools, which have to treat these reactions specially.
 */
function EnqueuePromiseReactions(promise, dependentPromise, onFulfilled, onRejected) {
    let isWrappedPromise = false;
    if (!IsPromise(promise)) {
        assert(IsWrappedPromise(promise),
               "EnqueuePromiseReactions must be provided with a possibly wrapped promise");
        isWrappedPromise = true;
    }

    assert(dependentPromise === null || IsPromise(dependentPromise),
           "EnqueuePromiseReactions's dependentPromise argument must be a Promise or null");

    if (isWrappedPromise) {
        // See comment above GetPromiseHandlerForwarders for why this is needed.
        let handlerForwarders = GetPromiseHandlerForwarders(onFulfilled, onRejected);
        return callFunction(CallPromiseMethodIfWrapped, promise, handlerForwarders[0],
                            handlerForwarders[1], dependentPromise, NullFunction, NullFunction,
                            "UnwrappedPerformPromiseThen");
    }
    let capability = {
        __proto__: PromiseCapabilityRecordProto,
        promise: dependentPromise,
        resolve: NullFunction,
        reject: NullFunction
    };
    return PerformPromiseThen(promise, onFulfilled, onRejected, capability);
}

/**
 * Returns a set of functions that are (1) self-hosted, and (2) exact
 * forwarders of the passed-in functions, for use by
 * UnwrappedPerformPromiseThen.
 *
 * When calling `then` on an xray-wrapped promise, the receiver isn't
 * unwrapped. Instead, Promise_then operates on the wrapped Promise. Just
 * calling PerformPromiseThen from Promise_then as we normally would doesn't
 * work in this case: PerformPromiseThen can only deal with unwrapped
 * Promises. Instead, we use the CallPromiseMethodIfWrapped intrinsic to
 * switch compartments before calling PerformPromiseThen, via
 * UnwrappedPerformPromiseThen.
 *
 * This is almost enough, but there's an additional wrinkle: when calling the
 * fulfillment and rejection handlers, we might pass in Object-type arguments
 * from within the xray-ed, lower-privileged compartment. By default, this
 * doesn't work, because they're wrapped into wrappers that disallow passing
 * in Object-typed arguments (so the higher-privileged code doesn't
 * accidentally operate on objects assuming they're higher-privileged, too.)
 * So instead UnwrappedPerformPromiseThen adds another level of indirection:
 * it closes over the, by now cross-compartment-wrapped, handler forwarders
 * created by GetPromiseHandlerForwarders and creates a second set of
 * forwarders around them, which use UnsafeCallWrappedFunction to call the
 * initial forwarders.

 * Note that both above-mentioned guarantees are required: while it may seem
 * as though the original handlers would always be wrappers once they reach
 * UnwrappedPerformPromiseThen (because the call to `then` originated in the
 * higher-privileged compartment, and after unwrapping we end up in the
 * lower-privileged one), that's not necessarily the case. One or both of the
 * handlers can originate from the lower-privileged compartment, so they'd
 * actually be unwrapped functions when they reach
 * UnwrappedPerformPromiseThen.
 */
function GetPromiseHandlerForwarders(fulfilledHandler, rejectedHandler) {
    // If non-callable values are passed, we have to preserve them so steps
    // 3 and 4 of PerformPromiseThen work as expected.
    return [
        IsCallable(fulfilledHandler) ? function onFulfilled(argument) {
            return callContentFunction(fulfilledHandler, this, argument);
        } : fulfilledHandler,
        IsCallable(rejectedHandler) ? function onRejected(argument) {
            return callContentFunction(rejectedHandler, this, argument);
        } : rejectedHandler
    ];
}

/**
 * Forwarder used to invoke PerformPromiseThen on an unwrapped Promise, while
 * wrapping the resolve/reject callbacks into functions that invoke them in
 * their original compartment. See the comment for GetPromiseHandlerForwarders
 * for details.
 */
function UnwrappedPerformPromiseThen(fulfilledHandler, rejectedHandler, promise, resolve, reject) {
    let resultCapability = {
        __proto__: PromiseCapabilityRecordProto,
        promise,
        resolve(resolution) {
            return UnsafeCallWrappedFunction(resolve, undefined, resolution);
        },
        reject(reason) {
            return UnsafeCallWrappedFunction(reject, undefined, reason);
        }
    };
    function onFulfilled(argument) {
        return UnsafeCallWrappedFunction(fulfilledHandler, undefined, argument);
    }
    function onRejected(argument) {
        return UnsafeCallWrappedFunction(rejectedHandler, undefined, argument);
    }
    return PerformPromiseThen(this, IsCallable(fulfilledHandler) ? onFulfilled : fulfilledHandler,
                              IsCallable(rejectedHandler) ? onRejected : rejectedHandler,
                              resultCapability);
}

// ES2016, March 1, 2016 draft, 25.4.5.3.1.
function PerformPromiseThen(promise, onFulfilled, onRejected, resultCapability) {
    // Step 1.
    assert(IsPromise(promise), "Can't call PerformPromiseThen on non-Promise objects");

    // Step 2.
    assert(IsPromiseCapability(resultCapability), "Invalid promise capability record");

    // Step 3.
    if (!IsCallable(onFulfilled))
        onFulfilled = PROMISE_HANDLER_IDENTITY;

    // Step 4.
    if (!IsCallable(onRejected))
        onRejected = PROMISE_HANDLER_THROWER;

    // Step 5.
    let fulfillReaction = {
        __proto__: PromiseReactionRecordProto,
        capabilities: resultCapability,
        handler: onFulfilled
    };

    // Step 6.
    let rejectReaction = {
        __proto__: PromiseReactionRecordProto,
        capabilities: resultCapability,
        handler: onRejected
    };

    // Step 7.
    let state = GetPromiseState(promise);
    if (state === PROMISE_STATE_PENDING) {
        // Step 7.a.
        let fulfillReactions = UnsafeGetObjectFromReservedSlot(promise,
                                                               PROMISE_FULFILL_REACTIONS_SLOT);
        _DefineDataProperty(fulfillReactions, fulfillReactions.length, fulfillReaction);

        // Step 7.b.
        let rejectReactions = UnsafeGetObjectFromReservedSlot(promise,
                                                              PROMISE_REJECT_REACTIONS_SLOT);
        _DefineDataProperty(rejectReactions, rejectReactions.length, rejectReaction);
    }

    // Step 8.
    else if (state === PROMISE_STATE_FULFILLED) {
        // Step 8.a.
        let value = UnsafeGetReservedSlot(promise, PROMISE_RESULT_SLOT);

        // Step 8.b.
        EnqueuePromiseReactionJob(fulfillReaction, value);
    }

    // Step 9.
    else {
        // Step 9.a.
        assert(state === PROMISE_STATE_REJECTED, "Invalid Promise state " + state);

        // Step 9.b.
        let reason = UnsafeGetReservedSlot(promise, PROMISE_RESULT_SLOT);

        // Step 9.c.
        if (UnsafeGetInt32FromReservedSlot(promise, PROMISE_IS_HANDLED_SLOT) !==
            PROMISE_IS_HANDLED_STATE_HANDLED)
        {
            HostPromiseRejectionTracker(promise, PROMISE_REJECTION_TRACKER_OPERATION_HANDLE);
        }

        // Step 9.d.
        EnqueuePromiseReactionJob(rejectReaction, reason);
    }

    // Step 10.
    UnsafeSetReservedSlot(promise, PROMISE_IS_HANDLED_SLOT, PROMISE_IS_HANDLED_STATE_HANDLED);

    // Step 11.
    return resultCapability.promise;
}

/// Utility functions below.
function IsPromiseReaction(record) {
    return std_Reflect_getPrototypeOf(record) === PromiseReactionRecordProto;
}

function IsPromiseCapability(capability) {
    return std_Reflect_getPrototypeOf(capability) === PromiseCapabilityRecordProto;
}

function GetPromiseState(promise) {
    return UnsafeGetInt32FromReservedSlot(promise, PROMISE_STATE_SLOT);
}
