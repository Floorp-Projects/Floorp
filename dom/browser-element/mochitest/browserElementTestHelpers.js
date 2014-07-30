/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Helpers for managing the browser frame preferences.
"use strict";

function _getPath() {
  return window.location.pathname
               .substring(0, window.location.pathname.lastIndexOf('/'))
               .replace("/priority", "");
}

const browserElementTestHelpers = {
  _getBoolPref: function(pref) {
    try {
      return SpecialPowers.getBoolPref(pref);
    }
    catch (e) {
      return undefined;
    }
  },

  _setPref: function(pref, value) {
    this.lockTestReady();
    if (value !== undefined && value !== null) {
      SpecialPowers.pushPrefEnv({'set': [[pref, value]]}, this.unlockTestReady.bind(this));
    } else {
      SpecialPowers.pushPrefEnv({'clear': [[pref]]}, this.unlockTestReady.bind(this));
    }
  },

  _setPrefs: function() {
    this.lockTestReady();
    SpecialPowers.pushPrefEnv({'set': Array.slice(arguments)}, this.unlockTestReady.bind(this));
  },

  _testReadyLockCount: 0,
  _firedTestReady: false,
  lockTestReady: function() {
    this._testReadyLockCount++;
  },

  unlockTestReady: function() {
    this._testReadyLockCount--;
    if (this._testReadyLockCount == 0 && !this._firedTestReady) {
      this._firedTestReady = true;
      dispatchEvent(new Event("testready"));
    }
  },

  enableProcessPriorityManager: function() {
    this._setPrefs(
      ['dom.ipc.processPriorityManager.testMode', true],
      ['dom.ipc.processPriorityManager.enabled', true],
      ['dom.ipc.processPriorityManager.backgroundLRUPoolLevels', 2]
    );
  },

  setEnabledPref: function(value) {
    this._setPref('dom.mozBrowserFramesEnabled', value);
  },

  setSelectionChangeEnabledPref: function(value) {
    this._setPref('selectioncaret.enabled', value);
  },

  getOOPByDefaultPref: function() {
    return this._getBoolPref("dom.ipc.browser_frames.oop_by_default");
  },

  addPermission: function() {
    SpecialPowers.addPermission("browser", true, document);
    this.tempPermissions.push(location.href)
  },

  'tempPermissions': [],
  addPermissionForUrl: function(url) {
    SpecialPowers.addPermission("browser", true, url);
    this.tempPermissions.push(url);
  },

  _observers: [],

  // This function is a wrapper which lets you register an observer to one of
  // the process priority manager's test-only topics.  observerFn should be a
  // function which takes (subject, topic, data).
  //
  // We'll clean up any observers you add at the end of the test.
  addProcessPriorityObserver: function(processPriorityTopic, observerFn) {
    var topic = "process-priority-manager:TEST-ONLY:" + processPriorityTopic;

    // SpecialPowers appears to require that the observer be an object, not a
    // function.
    var observer = {
      observe: observerFn
    };

    SpecialPowers.addObserver(observer, topic, /* weak = */ false);
    this._observers.push([observer, topic]);
  },

  cleanUp: function() {
    for (var i = 0; i < this.tempPermissions.length; i++) {
      SpecialPowers.removePermission("browser", this.tempPermissions[i]);
    }

    for (var i = 0; i < this._observers.length; i++) {
      SpecialPowers.removeObserver(this._observers[i][0],
                                   this._observers[i][1]);
    }
  },

  // Some basically-empty pages from different domains you can load.
  'emptyPage1': 'http://example.com' + _getPath() + '/file_empty.html',
  'emptyPage2': 'http://example.org' + _getPath() + '/file_empty.html',
  'emptyPage3': 'http://test1.example.org' + _getPath() + '/file_empty.html',
  'focusPage': 'http://example.org' + _getPath() + '/file_focus.html',
};

// Returns a promise which is resolved when a subprocess is created.  The
// argument to resolve() is the childID of the subprocess.
function expectProcessCreated() {
  var deferred = Promise.defer();

  var observed = false;
  browserElementTestHelpers.addProcessPriorityObserver(
    "process-created",
    function(subject, topic, data) {
      // Don't run this observer twice, so we don't ok(true) twice.  (It's fine
      // to resolve a promise twice; the second resolve() call does nothing.)
      if (observed) {
        return;
      }
      observed = true;

      var childID = parseInt(data);
      ok(true, 'Got new process, id=' + childID);
      deferred.resolve(childID);
    }
  );

  return deferred.promise;
}

// Just like expectProcessCreated(), except we'll call ok(false) if a second
// process is created.
function expectOnlyOneProcessCreated() {
  var p = expectProcessCreated();
  p.then(function() {
    expectProcessCreated().then(function(childID) {
      ok(false, 'Got unexpected process creation, childID=' + childID);
    });
  });
  return p;
}

// Returns a promise which is resolved or rejected the next time the process
// childID changes its priority.  We resolve if the (priority, CPU priority)
// tuple matches (expectedPriority, expectedCPUPriority) and we reject
// otherwise.
//
// expectedCPUPriority is an optional argument; if it's not specified, we
// resolve if priority matches expectedPriority.

function expectPriorityChange(childID, expectedPriority,
                              /* optional */ expectedCPUPriority) {
  var deferred = Promise.defer();

  var observed = false;
  browserElementTestHelpers.addProcessPriorityObserver(
    'process-priority-set',
    function(subject, topic, data) {
      if (observed) {
        return;
      }

      var [id, priority, cpuPriority] = data.split(":");
      if (id != childID) {
        return;
      }

      // Make sure we run the is() calls in this observer only once, otherwise
      // we'll expect /every/ priority change to match expectedPriority.
      observed = true;

      is(priority, expectedPriority,
         'Expected priority of childID ' + childID +
         ' to change to ' + expectedPriority);

      if (expectedCPUPriority) {
        is(cpuPriority, expectedCPUPriority,
           'Expected CPU priority of childID ' + childID +
           ' to change to ' + expectedCPUPriority);
      }

      if (priority == expectedPriority &&
          (!expectedCPUPriority || expectedCPUPriority == cpuPriority)) {
        deferred.resolve();
      } else {
        deferred.reject();
      }
    }
  );

  return deferred.promise;
}

// Returns a promise which is resolved or rejected the next time the background
// process childID changes its priority.  We resolve if the backgroundLRU
// matches expectedBackgroundLRU and we reject otherwise.

function expectPriorityWithBackgroundLRUSet(childID, expectedBackgroundLRU) {
  var deferred = Promise.defer();

  browserElementTestHelpers.addProcessPriorityObserver(
    'process-priority-with-background-LRU-set',
    function(subject, topic, data) {

      var [id, priority, cpuPriority, backgroundLRU] = data.split(":");
      if (id != childID) {
        return;
      }

      is(backgroundLRU, expectedBackgroundLRU,
         'Expected backgroundLRU ' + backgroundLRU + ' of childID ' + childID +
         ' to change to ' + expectedBackgroundLRU);

      if (backgroundLRU == expectedBackgroundLRU) {
        deferred.resolve();
      } else {
        deferred.reject();
      }
    }
  );

  return deferred.promise;
}

// Returns a promise which is resolved the first time the given iframe fires
// the mozbrowser##eventName event.
function expectMozbrowserEvent(iframe, eventName) {
  var deferred = Promise.defer();
  iframe.addEventListener('mozbrowser' + eventName, function handler(e) {
    iframe.removeEventListener('mozbrowser' + eventName, handler);
    deferred.resolve(e);
  });
  return deferred.promise;
}

// Set some prefs:
//
//  * browser.pagethumbnails.capturing_disabled: true
//
//    Disable tab view; it seriously messes us up.
//
//  * dom.ipc.browser_frames.oop_by_default
//
//    Enable or disable OOP-by-default depending on the test's filename.  You
//    can still force OOP on or off with <iframe mozbrowser remote=true/false>,
//    at least until bug 756376 lands.
//
//  * dom.ipc.tabs.disabled: false
//
//    Allow us to create OOP frames.  Even if they're not the default, some
//    "in-process" tests create OOP frames.
//
//  * network.disable.ipc.security: true
//
//    Disable the networking security checks; our test harness just tests
//    browser elements without sticking them in apps, and the security checks
//    dislike that.
//
//    Unfortunately setting network.disable.ipc.security to false before the
//    child process(es) created by this test have shut down can cause us to
//    assert and kill the child process.  That doesn't cause the tests to fail,
//    but it's still scary looking.  So we just set the pref to true and never
//    pop that value.  We'll rely on the tests which test IPC security to set
//    it to false.
//
//  * security.mixed_content.block_active_content: false
//
//    Disable mixed active content blocking, so that tests can confirm that mixed
//    content results in a broken security state.

(function() {
  var oop = location.pathname.indexOf('_inproc_') == -1;

  browserElementTestHelpers.lockTestReady();
  SpecialPowers.setBoolPref("network.disable.ipc.security", true);
  SpecialPowers.pushPrefEnv({set: [["browser.pagethumbnails.capturing_disabled", true],
                                   ["dom.ipc.browser_frames.oop_by_default", oop],
                                   ["dom.ipc.tabs.disabled", false],
                                   ["security.mixed_content.block_active_content", false]]},
                            browserElementTestHelpers.unlockTestReady.bind(browserElementTestHelpers));
})();

addEventListener('unload', function() {
  browserElementTestHelpers.cleanUp();
});

// Wait for the load event before unlocking the test-ready event.
browserElementTestHelpers.lockTestReady();
addEventListener('load', function() {
  SimpleTest.executeSoon(browserElementTestHelpers.unlockTestReady.bind(browserElementTestHelpers));
});

//////////////////////////////////
// promise.js from the addon SDK with some modifications to the module
// boilerplate.
//////////////////////////////////

;(function(id, factory) { // Module boilerplate :(
    var globals = this;
    factory(function require(id) {
      return globals[id];
    }, (globals[id] = {}), { uri: document.location.href + '#' + id, id: id });
}).call(this, 'Promise', function Promise(require, exports, module) {

'use strict';

module.metadata = {
  "stability": "unstable"
};

/**
 * Internal utility: Wraps given `value` into simplified promise, successfully
 * fulfilled to a given `value`. Note the result is not a complete promise
 * implementation, as its method `then` does not returns anything.
 */
function fulfilled(value) {
  return { then: function then(fulfill) { fulfill(value); } };
}

/**
 * Internal utility: Wraps given input into simplified promise, pre-rejected
 * with a given `reason`. Note the result is not a complete promise
 * implementation, as its method `then` does not returns anything.
 */
function rejected(reason) {
  return { then: function then(fulfill, reject) { reject(reason); } };
}

/**
 * Internal utility: Returns `true` if given `value` is a promise. Value is
 * assumed to be a promise if it implements method `then`.
 */
function isPromise(value) {
  return value && typeof(value.then) === 'function';
}

/**
 * Creates deferred object containing fresh promise & methods to either resolve
 * or reject it. The result is an object with the following properties:
 * - `promise` Eventual value representation implementing CommonJS [Promises/A]
 *   (http://wiki.commonjs.org/wiki/Promises/A) API.
 * - `resolve` Single shot function that resolves enclosed `promise` with a
 *   given `value`.
 * - `reject` Single shot function that rejects enclosed `promise` with a given
 *   `reason`.
 *
 * An optional `prototype` argument is used as a prototype of the returned
 * `promise` allowing one to implement additional API. If prototype is not
 * passed then it falls back to `Object.prototype`.
 *
 *  ## Example
 *
 *  function fetchURI(uri, type) {
 *    var deferred = defer();
 *    var request = new XMLHttpRequest();
 *    request.open("GET", uri, true);
 *    request.responseType = type;
 *    request.onload = function onload() {
 *      deferred.resolve(request.response);
 *    }
 *    request.onerror = function(event) {
 *     deferred.reject(event);
 *    }
 *    request.send();
 *
 *    return deferred.promise;
 *  }
 */
function defer(prototype) {
  // Define FIFO queue of observer pairs. Once promise is resolved & all queued
  // observers are forwarded to `result` and variable is set to `null`.
  var observers = [];

  // Promise `result`, which will be assigned a resolution value once promise
  // is resolved. Note that result will always be assigned promise (or alike)
  // object to take care of propagation through promise chains. If result is
  // `null` promise is not resolved yet.
  var result = null;

  prototype = (prototype || prototype === null) ? prototype : Object.prototype;

  // Create an object implementing promise API.
  var promise = Object.create(prototype, {
    then: { value: function then(onFulfill, onError) {
      var deferred = defer(prototype);

      function resolve(value) {
        // If `onFulfill` handler is provided resolve `deferred.promise` with
        // result of invoking it with a resolution value. If handler is not
        // provided propagate value through.
        try {
          deferred.resolve(onFulfill ? onFulfill(value) : value);
        }
        // `onFulfill` may throw exception in which case resulting promise
        // is rejected with thrown exception.
        catch(error) {
          if (exports._reportErrors && typeof(console) === 'object')
            console.error(error);
          // Note: Following is equivalent of `deferred.reject(error)`,
          // we use this shortcut to reduce a stack.
          deferred.resolve(rejected(error));
        }
      }

      function reject(reason) {
        try {
          if (onError) deferred.resolve(onError(reason));
          else deferred.resolve(rejected(reason));
        }
        catch(error) {
          if (exports._reportErrors && typeof(console) === 'object')
            console.error(error)
          deferred.resolve(rejected(error));
        }
      }

      // If enclosed promise (`this.promise`) observers queue is still alive
      // enqueue a new observer pair into it. Note that this does not
      // necessary means that promise is pending, it may already be resolved,
      // but we still have to queue observers to guarantee an order of
      // propagation.
      if (observers) {
        observers.push({ resolve: resolve, reject: reject });
      }
      // Otherwise just forward observer pair right to a `result` promise.
      else {
        result.then(resolve, reject);
      }

      return deferred.promise;
    }}
  })

  var deferred = {
    promise: promise,
    /**
     * Resolves associated `promise` to a given `value`, unless it's already
     * resolved or rejected. Note that resolved promise is not necessary a
     * successfully fulfilled. Promise may be resolved with a promise `value`
     * in which case `value` promise's fulfillment / rejection will propagate
     * up to a promise resolved with `value`.
     */
    resolve: function resolve(value) {
      if (!result) {
        // Store resolution `value` in a `result` as a promise, so that all
        // the subsequent handlers can be simply forwarded to it. Since
        // `result` will be a promise all the value / error propagation will
        // be uniformly taken care of.
        result = isPromise(value) ? value : fulfilled(value);

        // Forward already registered observers to a `result` promise in the
        // order they were registered. Note that we intentionally dequeue
        // observer at a time until queue is exhausted. This makes sure that
        // handlers registered as side effect of observer forwarding are
        // queued instead of being invoked immediately, guaranteeing FIFO
        // order.
        while (observers.length) {
          var observer = observers.shift();
          result.then(observer.resolve, observer.reject);
        }

        // Once `observers` queue is exhausted we `null`-ify it, so that
        // new handlers are forwarded straight to the `result`.
        observers = null;
      }
    },
    /**
     * Rejects associated `promise` with a given `reason`, unless it's already
     * resolved / rejected. This is just a (better performing) convenience
     * shortcut for `deferred.resolve(reject(reason))`.
     */
    reject: function reject(reason) {
      // Note that if promise is resolved that does not necessary means that it
      // is successfully fulfilled. Resolution value may be a promise in which
      // case its result propagates. In other words if promise `a` is resolved
      // with promise `b`, `a` is either fulfilled or rejected depending
      // on weather `b` is fulfilled or rejected. Here `deferred.promise` is
      // resolved with a promise pre-rejected with a given `reason`, there for
      // `deferred.promise` is rejected with a given `reason`. This may feel
      // little awkward first, but doing it this way greatly simplifies
      // propagation through promise chains.
      deferred.resolve(rejected(reason));
    }
  };

  return deferred;
}
exports.defer = defer;

/**
 * Returns a promise resolved to a given `value`. Optionally a second
 * `prototype` argument may be provided to be used as a prototype for the
 * returned promise.
 */
function resolve(value, prototype) {
  var deferred = defer(prototype);
  deferred.resolve(value);
  return deferred.promise;
}
exports.resolve = resolve;

/**
 * Returns a promise rejected with a given `reason`. Optionally a second
 * `prototype` argument may be provided to be used as a prototype for the
 * returned promise.
 */
function reject(reason, prototype) {
  var deferred = defer(prototype);
  deferred.reject(reason);
  return deferred.promise;
}
exports.reject = reject;

var promised = (function() {
  // Note: Define shortcuts and utility functions here in order to avoid
  // slower property accesses and unnecessary closure creations on each
  // call of this popular function.

  var call = Function.call;
  var concat = Array.prototype.concat;

  // Utility function that does following:
  // execute([ f, self, args...]) => f.apply(self, args)
  function execute(args) { return call.apply(call, args) }

  // Utility function that takes promise of `a` array and maybe promise `b`
  // as arguments and returns promise for `a.concat(b)`.
  function promisedConcat(promises, unknown) {
    return promises.then(function(values) {
      return resolve(unknown).then(function(value) {
        return values.concat([ value ])
      });
    });
  }

  return function promised(f, prototype) {
    /**
    Returns a wrapped `f`, which when called returns a promise that resolves to
    `f(...)` passing all the given arguments to it, which by the way may be
    promises. Optionally second `prototype` argument may be provided to be used
    a prototype for a returned promise.

    ## Example

    var promise = promised(Array)(1, promise(2), promise(3))
    promise.then(console.log) // => [ 1, 2, 3 ]
    **/

    return function promised() {
      // create array of [ f, this, args... ]
      return concat.apply([ f, this ], arguments).
        // reduce it via `promisedConcat` to get promised array of fulfillments
        reduce(promisedConcat, resolve([], prototype)).
        // finally map that to promise of `f.apply(this, args...)`
        then(execute);
    }
  }
})();
exports.promised = promised;

var all = promised(Array);
exports.all = all;

});
