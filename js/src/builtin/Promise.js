/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This implementation is by no means complete and is using a polyfill.
 * In particular, it doesn't fully comply to ES6 Promises spec.
 * The list of incompatibilities may not be complete and includes:
 *
 * - no [Symbol.species] implementation
 * - implementation is not really async at all, but all is executed
 *   in correct order
 * - Promise.race is not implemented (not necessary for async/await)
 * - Promise.all implementation currently only handles arrays, no other
 *   iterables.
 */

 /**
  * This polyfill implements the Promise specification in the following way:
  * At first, Promise is initialized with the intrinsic Promise constructor,
  * with its initialization completed by calling Promise_init. The goal is to
  * completely resolve/reject the promise. There are certain helper functions:
  * - resolveOneStep() executes "one step" of the promise - which means
  *   getting either the final value or next promise towards complete execution.
  * - resolveCompletely() executes the entire promise, which merely means
  *   resolving one step at a time, until the final step no longer resolves
  *   to a promise (which means either resolving to a value or rejecting).
  *
  * Once resolution is finished, resolveCompletely() is called in order to
  * update state of the promise. It also spawns callbacks that may have been
  * deferred with a then() - they are NOT normally taken into consideration,
  * because resolveCompletely() just runs one path.
  */

/**
 * Below is a simple simulation of an event loop. Events enqueued using
 * this setTimeout implementation must be manually triggered by calling
 * runEvents().
 */
var eventQueue = new List();

function runEvents() {
  while (eventQueue.length > 0) {
    var evt = callFunction(std_Array_pop, eventQueue);
    evt();
  }
}

function setTimeout(cb, interval) {
  if (!IsCallable(cb))
    ThrowTypeError(JSMSG_NOT_CALLABLE, "Argument 0");
  if (interval !== 0)
    ThrowTypeError(JSMSG_SETTIMEOUT_INTERVAL_NONZERO);
  callFunction(std_Array_push, eventQueue, cb);
}

function Handler(onFulfilled, onRejected, resolve, reject) {
  this.onFulfilled = IsCallable(onFulfilled) ? onFulfilled : null;
  this.onRejected = IsCallable(onRejected) ? onRejected : null;
  this.resolve = resolve;
  this.reject = reject;
}

MakeConstructible(Handler, std_Object_create(null));

function Promise_setState(promise, state) {
  UnsafeSetReservedSlot(promise, PROMISE_STATE_SLOT, state);
}

function Promise_getState(promise) {
  return UnsafeGetReservedSlot(promise, PROMISE_STATE_SLOT);
}

function Promise_setDeferreds(promise, deferreds) {
  UnsafeSetReservedSlot(promise, PROMISE_DEFERREDS_SLOT, deferreds);
}

function Promise_getDeferreds(promise) {
  return UnsafeGetReservedSlot(promise, PROMISE_DEFERREDS_SLOT);
}

function Promise_setValue(promise, value) {
  UnsafeSetReservedSlot(promise, PROMISE_VALUE_SLOT, value);
}

function Promise_getValue(promise) {
  return UnsafeGetReservedSlot(promise, PROMISE_VALUE_SLOT);
}

function Promise_init(promise, fn) {
  Promise_setDeferreds(promise, new List());
  resolveOneStep(fn,
    function(value) { resolveCompletely(promise, value); },
    function(reason) { reject(promise, reason); });
}

function Promise_isThenable(valueOrPromise) {
  if (valueOrPromise && (typeof valueOrPromise === 'object' || IsCallable(valueOrPromise))) {
    var then = valueOrPromise.then;
    if (IsCallable(then))
      return true;
  }
  return false;
}

function Promise_all(promises) {
  var length = promises.length;
  var results = [];
  return NewPromise(function (resolve, reject) {
    if (length === 0)
      return resolve([]);
    var remaining = length;
    function resolveChain(index, valueOrPromise) {
      try {
        if (Promise_isThenable(valueOrPromise)) {
          callFunction(valueOrPromise.then, valueOrPromise,
                       function (valueOrPromise) { resolveChain(index, valueOrPromise); },
                       reject);
        } else {
          _DefineDataProperty(results, index, valueOrPromise);
          if (--remaining === 0)
            resolve(results);
        }
      } catch (ex) {
        reject(ex);
      }
    }
    for (var i = 0; i < length; i++)
      resolveChain(i, promises[i]);
  });
}

function Promise_race(values) {
  ThrowTypeError(JSMSG_NOT_IMPLEMENTED, "Promise.race");
}

function Promise_reject(value) {
  return NewPromise(function (resolve, reject) {
    reject(value);
  });
}

function Promise_resolve(value) {
  if (value && typeof value === 'object' && IsPromise(value))
    return value;

  return NewPromise(function (resolve) {
     resolve(value);
  });
}

function Promise_catch(onRejected) {
  return callFunction(Promise_then, this, undefined, onRejected);
}

function asap(cb) {
  setTimeout(cb, 0);
}

function deferOrExecute(promise, deferred) {
  if (Promise_getState(promise) === PROMISE_STATE_PENDING) {
    Promise_getDeferreds(promise).push(deferred);
    return;
  }

  asap(function() {
    var cb = Promise_getState(promise) === PROMISE_STATE_RESOLVED ?
      deferred.onFulfilled : deferred.onRejected;
    if (cb === null) {
      var value = Promise_getValue(promise);
      (Promise_getState(promise) === PROMISE_STATE_RESOLVED ? deferred.resolve : deferred.reject)(value);
      return;
    }
    var returnValue;
    try {
      returnValue = cb(Promise_getValue(promise));
    } catch (e) {
      deferred.reject(e);
      return;
    }
    deferred.resolve(returnValue);
  });
}

function resolveOneStep(fn, onFulfilled, onRejected) {
  var done = false;
  var callOnce = function(cb) {
    return function(value) {
      if (done) return;
      done = true;
      cb(value);
    };
  };

  try {
    fn(callOnce(onFulfilled), callOnce(onRejected));
  } catch (ex) {
    callOnce(onRejected)(ex);
  }
}

function resolveCompletely(promise, valueOrPromise) {
  try {
    // FIXME this is probably not a type error
    if (valueOrPromise === promise)
      ThrowTypeError(JSMSG_PROMISE_RESOLVED_WITH_ITSELF);

    if (Promise_isThenable(valueOrPromise)) {
      resolveOneStep(function(resolve, reject) { valueOrPromise.then(resolve, reject); },
                     function(value) { resolveCompletely(promise, value); },
                     function(value) { reject(promise, value); });
                   }
    else
      callFunction(resolvingFinished, promise, PROMISE_STATE_RESOLVED, valueOrPromise);
  } catch (ex) {
    callFunction(reject, promise, ex);
  }
}

function reject(promise, reason) {
  callFunction(resolvingFinished, promise, PROMISE_STATE_REJECTED, reason);
}

function resolvingFinished(state, newValue) {
  Promise_setState(this, state);
  Promise_setValue(this, newValue);
  var deferreds = Promise_getDeferreds(this);
  for (var i = 0, len = deferreds.length; i < len; i++)
    deferOrExecute(this, deferreds[i]);
  Promise_setDeferreds(this, null);
}

function Promise_then(onFulfilled, onRejected) {
  var promise = this;
  var newPromise = NewPromise(function(resolve, reject) {
    deferOrExecute(promise,
                   new Handler(onFulfilled, onRejected, resolve, reject));
  });
  runEvents();
  return newPromise;
}
