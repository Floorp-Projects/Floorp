// GENERATED, DO NOT EDIT
// file: assert.js
// Copyright (C) 2017 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: |
    Collection of assertion functions used throughout test262
defines: [assert]
---*/


function assert(mustBeTrue, message) {
  if (mustBeTrue === true) {
    return;
  }

  if (message === undefined) {
    message = 'Expected true but got ' + assert._toString(mustBeTrue);
  }
  throw new Test262Error(message);
}

assert._isSameValue = function (a, b) {
  if (a === b) {
    // Handle +/-0 vs. -/+0
    return a !== 0 || 1 / a === 1 / b;
  }

  // Handle NaN vs. NaN
  return a !== a && b !== b;
};

assert.sameValue = function (actual, expected, message) {
  try {
    if (assert._isSameValue(actual, expected)) {
      return;
    }
  } catch (error) {
    throw new Test262Error(message + ' (_isSameValue operation threw) ' + error);
    return;
  }

  if (message === undefined) {
    message = '';
  } else {
    message += ' ';
  }

  message += 'Expected SameValue(«' + assert._toString(actual) + '», «' + assert._toString(expected) + '») to be true';

  throw new Test262Error(message);
};

assert.notSameValue = function (actual, unexpected, message) {
  if (!assert._isSameValue(actual, unexpected)) {
    return;
  }

  if (message === undefined) {
    message = '';
  } else {
    message += ' ';
  }

  message += 'Expected SameValue(«' + assert._toString(actual) + '», «' + assert._toString(unexpected) + '») to be false';

  throw new Test262Error(message);
};

assert.throws = function (expectedErrorConstructor, func, message) {
  if (typeof func !== "function") {
    throw new Test262Error('assert.throws requires two arguments: the error constructor ' +
      'and a function to run');
    return;
  }
  if (message === undefined) {
    message = '';
  } else {
    message += ' ';
  }

  try {
    func();
  } catch (thrown) {
    if (typeof thrown !== 'object' || thrown === null) {
      message += 'Thrown value was not an object!';
      throw new Test262Error(message);
    } else if (thrown.constructor !== expectedErrorConstructor) {
      message += 'Expected a ' + expectedErrorConstructor.name + ' but got a ' + thrown.constructor.name;
      throw new Test262Error(message);
    }
    return;
  }

  message += 'Expected a ' + expectedErrorConstructor.name + ' to be thrown but no exception was thrown at all';
  throw new Test262Error(message);
};

assert._toString = function (value) {
  try {
    if (value === 0 && 1 / value === -Infinity) {
      return '-0';
    }

    return String(value);
  } catch (err) {
    if (err.name === 'TypeError') {
      return Object.prototype.toString.call(value);
    }

    throw err;
  }
};

// file: compareArray.js
// Copyright (C) 2017 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: |
    Compare the contents of two arrays
defines: [compareArray]
---*/

function compareArray(a, b) {
  if (b.length !== a.length) {
    return false;
  }

  for (var i = 0; i < a.length; i++) {
    if (!compareArray.isSameValue(b[i], a[i])) {
      return false;
    }
  }
  return true;
}

compareArray.isSameValue = function(a, b) {
  if (a === 0 && b === 0) return 1 / a === 1 / b;
  if (a !== a && b !== b) return true;

  return a === b;
};

compareArray.format = function(array) {
  return `[${array.map(String).join(', ')}]`;
};

assert.compareArray = function(actual, expected, message) {
  message  = message === undefined ? '' : message;
  assert(actual != null, `First argument shouldn't be nullish. ${message}`);
  assert(expected != null, `Second argument shouldn't be nullish. ${message}`);
  var format = compareArray.format;
  assert(
    compareArray(actual, expected),
    `Expected ${format(actual)} and ${format(expected)} to have the same contents. ${message}`
  );
};

// file: propertyHelper.js
// Copyright (C) 2017 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: |
    Collection of functions used to safely verify the correctness of
    property descriptors.
defines:
  - verifyProperty
  - verifyEqualTo
  - verifyWritable
  - verifyNotWritable
  - verifyEnumerable
  - verifyNotEnumerable
  - verifyConfigurable
  - verifyNotConfigurable
---*/

// @ts-check

/**
 * @param {object} obj
 * @param {string|symbol} name
 * @param {PropertyDescriptor|undefined} desc
 * @param {object} [options]
 * @param {boolean} [options.restore]
 */
function verifyProperty(obj, name, desc, options) {
  assert(
    arguments.length > 2,
    'verifyProperty should receive at least 3 arguments: obj, name, and descriptor'
  );

  var originalDesc = Object.getOwnPropertyDescriptor(obj, name);
  var nameStr = String(name);

  // Allows checking for undefined descriptor if it's explicitly given.
  if (desc === undefined) {
    assert.sameValue(
      originalDesc,
      undefined,
      "obj['" + nameStr + "'] descriptor should be undefined"
    );

    // desc and originalDesc are both undefined, problem solved;
    return true;
  }

  assert(
    Object.prototype.hasOwnProperty.call(obj, name),
    "obj should have an own property " + nameStr
  );

  assert.notSameValue(
    desc,
    null,
    "The desc argument should be an object or undefined, null"
  );

  assert.sameValue(
    typeof desc,
    "object",
    "The desc argument should be an object or undefined, " + String(desc)
  );

  var failures = [];

  if (Object.prototype.hasOwnProperty.call(desc, 'value')) {
    if (!isSameValue(desc.value, originalDesc.value)) {
      failures.push("descriptor value should be " + desc.value);
    }
  }

  if (Object.prototype.hasOwnProperty.call(desc, 'enumerable')) {
    if (desc.enumerable !== originalDesc.enumerable ||
        desc.enumerable !== isEnumerable(obj, name)) {
      failures.push('descriptor should ' + (desc.enumerable ? '' : 'not ') + 'be enumerable');
    }
  }

  if (Object.prototype.hasOwnProperty.call(desc, 'writable')) {
    if (desc.writable !== originalDesc.writable ||
        desc.writable !== isWritable(obj, name)) {
      failures.push('descriptor should ' + (desc.writable ? '' : 'not ') + 'be writable');
    }
  }

  if (Object.prototype.hasOwnProperty.call(desc, 'configurable')) {
    if (desc.configurable !== originalDesc.configurable ||
        desc.configurable !== isConfigurable(obj, name)) {
      failures.push('descriptor should ' + (desc.configurable ? '' : 'not ') + 'be configurable');
    }
  }

  assert(!failures.length, failures.join('; '));

  if (options && options.restore) {
    Object.defineProperty(obj, name, originalDesc);
  }

  return true;
}

function isConfigurable(obj, name) {
  var hasOwnProperty = Object.prototype.hasOwnProperty;
  try {
    delete obj[name];
  } catch (e) {
    if (!(e instanceof TypeError)) {
      throw new Test262Error("Expected TypeError, got " + e);
    }
  }
  return !hasOwnProperty.call(obj, name);
}

function isEnumerable(obj, name) {
  var stringCheck = false;

  if (typeof name === "string") {
    for (var x in obj) {
      if (x === name) {
        stringCheck = true;
        break;
      }
    }
  } else {
    // skip it if name is not string, works for Symbol names.
    stringCheck = true;
  }

  return stringCheck &&
    Object.prototype.hasOwnProperty.call(obj, name) &&
    Object.prototype.propertyIsEnumerable.call(obj, name);
}

function isSameValue(a, b) {
  if (a === 0 && b === 0) return 1 / a === 1 / b;
  if (a !== a && b !== b) return true;

  return a === b;
}

var __isArray = Array.isArray;
function isWritable(obj, name, verifyProp, value) {
  var unlikelyValue = __isArray(obj) && name === "length" ?
    Math.pow(2, 32) - 1 :
    "unlikelyValue";
  var newValue = value || unlikelyValue;
  var hadValue = Object.prototype.hasOwnProperty.call(obj, name);
  var oldValue = obj[name];
  var writeSucceeded;

  try {
    obj[name] = newValue;
  } catch (e) {
    if (!(e instanceof TypeError)) {
      throw new Test262Error("Expected TypeError, got " + e);
    }
  }

  writeSucceeded = isSameValue(obj[verifyProp || name], newValue);

  // Revert the change only if it was successful (in other cases, reverting
  // is unnecessary and may trigger exceptions for certain property
  // configurations)
  if (writeSucceeded) {
    if (hadValue) {
      obj[name] = oldValue;
    } else {
      delete obj[name];
    }
  }

  return writeSucceeded;
}

function verifyEqualTo(obj, name, value) {
  if (!isSameValue(obj[name], value)) {
    throw new Test262Error("Expected obj[" + String(name) + "] to equal " + value +
           ", actually " + obj[name]);
  }
}

function verifyWritable(obj, name, verifyProp, value) {
  if (!verifyProp) {
    assert(Object.getOwnPropertyDescriptor(obj, name).writable,
         "Expected obj[" + String(name) + "] to have writable:true.");
  }
  if (!isWritable(obj, name, verifyProp, value)) {
    throw new Test262Error("Expected obj[" + String(name) + "] to be writable, but was not.");
  }
}

function verifyNotWritable(obj, name, verifyProp, value) {
  if (!verifyProp) {
    assert(!Object.getOwnPropertyDescriptor(obj, name).writable,
         "Expected obj[" + String(name) + "] to have writable:false.");
  }
  if (isWritable(obj, name, verifyProp)) {
    throw new Test262Error("Expected obj[" + String(name) + "] NOT to be writable, but was.");
  }
}

function verifyEnumerable(obj, name) {
  assert(Object.getOwnPropertyDescriptor(obj, name).enumerable,
       "Expected obj[" + String(name) + "] to have enumerable:true.");
  if (!isEnumerable(obj, name)) {
    throw new Test262Error("Expected obj[" + String(name) + "] to be enumerable, but was not.");
  }
}

function verifyNotEnumerable(obj, name) {
  assert(!Object.getOwnPropertyDescriptor(obj, name).enumerable,
       "Expected obj[" + String(name) + "] to have enumerable:false.");
  if (isEnumerable(obj, name)) {
    throw new Test262Error("Expected obj[" + String(name) + "] NOT to be enumerable, but was.");
  }
}

function verifyConfigurable(obj, name) {
  assert(Object.getOwnPropertyDescriptor(obj, name).configurable,
       "Expected obj[" + String(name) + "] to have configurable:true.");
  if (!isConfigurable(obj, name)) {
    throw new Test262Error("Expected obj[" + String(name) + "] to be configurable, but was not.");
  }
}

function verifyNotConfigurable(obj, name) {
  assert(!Object.getOwnPropertyDescriptor(obj, name).configurable,
       "Expected obj[" + String(name) + "] to have configurable:false.");
  if (isConfigurable(obj, name)) {
    throw new Test262Error("Expected obj[" + String(name) + "] NOT to be configurable, but was.");
  }
}

// file: sta.js
// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: |
    Provides both:

    - An error class to avoid false positives when testing for thrown exceptions
    - A function to explicitly throw an exception using the Test262Error class
defines: [Test262Error, $DONOTEVALUATE]
---*/


function Test262Error(message) {
  this.message = message || "";
}

Test262Error.prototype.toString = function () {
  return "Test262Error: " + this.message;
};

Test262Error.thrower = (message) => {
  throw new Test262Error(message);
};

function $DONOTEVALUATE() {
  throw "Test262: This statement should not be evaluated.";
}

// file: test262-host.js
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// https://github.com/tc39/test262/blob/main/INTERPRETING.md#host-defined-functions
;(function createHostObject(global) {
    "use strict";

    // Save built-in functions and constructors.
    var FunctionToString = global.Function.prototype.toString;
    var ReflectApply = global.Reflect.apply;
    var Atomics = global.Atomics;
    var Error = global.Error;
    var SharedArrayBuffer = global.SharedArrayBuffer;
    var Int32Array = global.Int32Array;

    // Save built-in shell functions.
    var NewGlobal = global.newGlobal;
    var setSharedArrayBuffer = global.setSharedArrayBuffer;
    var getSharedArrayBuffer = global.getSharedArrayBuffer;
    var evalInWorker = global.evalInWorker;
    var monotonicNow = global.monotonicNow;
    var gc = global.gc;
    var clearKeptObjects = global.clearKeptObjects;

    var hasCreateIsHTMLDDA = "createIsHTMLDDA" in global;
    var hasThreads = ("helperThreadCount" in global ? global.helperThreadCount() > 0 : true);
    var hasMailbox = typeof setSharedArrayBuffer === "function" && typeof getSharedArrayBuffer === "function";
    var hasEvalInWorker = typeof evalInWorker === "function";

    if (!hasCreateIsHTMLDDA && !("document" in global && "all" in global.document))
        throw new Error("no [[IsHTMLDDA]] object available for testing");

    var IsHTMLDDA = hasCreateIsHTMLDDA
                    ? global.createIsHTMLDDA()
                    : global.document.all;

    // The $262.agent framework is not appropriate for browsers yet, and some
    // test cases can't work in browsers (they block the main thread).

    var shellCode = hasMailbox && hasEvalInWorker;
    var sabTestable = Atomics && SharedArrayBuffer && hasThreads && shellCode;

    global.$262 = {
        __proto__: null,
        createRealm() {
            var newGlobalObject = NewGlobal();
            var createHostObjectFn = ReflectApply(FunctionToString, createHostObject, []);
            newGlobalObject.Function(`${createHostObjectFn} createHostObject(this);`)();
            return newGlobalObject.$262;
        },
        detachArrayBuffer: global.detachArrayBuffer,
        evalScript: global.evaluateScript || global.evaluate,
        global,
        IsHTMLDDA,
        gc() {
            gc();
        },
        clearKeptObjects() {
            clearKeptObjects();
        },
        agent: (function () {

            // SpiderMonkey complication: With run-time argument --no-threads
            // our test runner will not properly filter test cases that can't be
            // run because agents can't be started, and so we do a little
            // filtering here: We will quietly succeed and exit if an agent test
            // should not have been run because threads cannot be started.
            //
            // Firefox complication: The test cases that use $262.agent can't
            // currently work in the browser, so for now we rely on them not
            // being run at all.

            if (!sabTestable) {
                let {reportCompare, quit} = global;

                function notAvailable() {
                    // See comment above.
                    if (!hasThreads && shellCode) {
                        reportCompare(0, 0);
                        quit(0);
                    }
                    throw new Error("Agents not available");
                }

                return {
                    start(script) { notAvailable() },
                    broadcast(sab, id) { notAvailable() },
                    getReport() { notAvailable() },
                    sleep(s) { notAvailable() },
                    monotonicNow,
                }
            }

            // The SpiderMonkey implementation uses a designated shared buffer _ia
            // for coordination, and spinlocks for everything except sleeping.

            var _MSG_LOC = 0;           // Low bit set: broadcast available; High bits: seq #
            var _ID_LOC = 1;            // ID sent with broadcast
            var _ACK_LOC = 2;           // Worker increments this to ack that broadcast was received
            var _RDY_LOC = 3;           // Worker increments this to ack that worker is up and running
            var _LOCKTXT_LOC = 4;       // Writer lock for the text buffer: 0=open, 1=closed
            var _NUMTXT_LOC = 5;        // Count of messages in text buffer
            var _NEXT_LOC = 6;          // First free location in the buffer
            var _SLEEP_LOC = 7;         // Used for sleeping

            var _FIRST = 10;            // First location of first message

            var _ia = new Int32Array(new SharedArrayBuffer(65536));
            _ia[_NEXT_LOC] = _FIRST;

            var _worker_prefix =
// BEGIN WORKER PREFIX
`if (typeof $262 === 'undefined')
    $262 = {};
$262.agent = (function (global) {
    var ReflectApply = global.Reflect.apply;
    var StringCharCodeAt = global.String.prototype.charCodeAt;
    var {
        add: Atomics_add,
        compareExchange: Atomics_compareExchange,
        load: Atomics_load,
        store: Atomics_store,
        wait: Atomics_wait,
    } = global.Atomics;

    var {getSharedArrayBuffer} = global;

    var _ia = new Int32Array(getSharedArrayBuffer());
    var agent = {
        receiveBroadcast(receiver) {
            var k;
            while (((k = Atomics_load(_ia, ${_MSG_LOC})) & 1) === 0)
                ;
            var received_sab = getSharedArrayBuffer();
            var received_id = Atomics_load(_ia, ${_ID_LOC});
            Atomics_add(_ia, ${_ACK_LOC}, 1);
            while (Atomics_load(_ia, ${_MSG_LOC}) === k)
                ;
            receiver(received_sab, received_id);
        },

        report(msg) {
            while (Atomics_compareExchange(_ia, ${_LOCKTXT_LOC}, 0, 1) === 1)
                ;
            msg = "" + msg;
            var i = _ia[${_NEXT_LOC}];
            _ia[i++] = msg.length;
            for ( let j=0 ; j < msg.length ; j++ )
                _ia[i++] = ReflectApply(StringCharCodeAt, msg, [j]);
            _ia[${_NEXT_LOC}] = i;
            Atomics_add(_ia, ${_NUMTXT_LOC}, 1);
            Atomics_store(_ia, ${_LOCKTXT_LOC}, 0);
        },

        sleep(s) {
            Atomics_wait(_ia, ${_SLEEP_LOC}, 0, s);
        },

        leaving() {},

        monotonicNow: global.monotonicNow,
    };
    Atomics_add(_ia, ${_RDY_LOC}, 1);
    return agent;
})(this);`;
// END WORKER PREFIX

            var _numWorkers = 0;
            var _numReports = 0;
            var _reportPtr = _FIRST;
            var {
                add: Atomics_add,
                load: Atomics_load,
                store: Atomics_store,
                wait: Atomics_wait,
            } = Atomics;
            var StringFromCharCode = global.String.fromCharCode;

            return {
                start(script) {
                    setSharedArrayBuffer(_ia.buffer);
                    var oldrdy = Atomics_load(_ia, _RDY_LOC);
                    evalInWorker(_worker_prefix + script);
                    while (Atomics_load(_ia, _RDY_LOC) === oldrdy)
                        ;
                    _numWorkers++;
                },

                broadcast(sab, id) {
                    setSharedArrayBuffer(sab);
                    Atomics_store(_ia, _ID_LOC, id);
                    Atomics_store(_ia, _ACK_LOC, 0);
                    Atomics_add(_ia, _MSG_LOC, 1);
                    while (Atomics_load(_ia, _ACK_LOC) < _numWorkers)
                        ;
                    Atomics_add(_ia, _MSG_LOC, 1);
                },

                getReport() {
                    if (_numReports === Atomics_load(_ia, _NUMTXT_LOC))
                        return null;
                    var s = "";
                    var i = _reportPtr;
                    var len = _ia[i++];
                    for ( let j=0 ; j < len ; j++ )
                        s += StringFromCharCode(_ia[i++]);
                    _reportPtr = i;
                    _numReports++;
                    return s;
                },

                sleep(s) {
                    Atomics_wait(_ia, _SLEEP_LOC, 0, s);
                },

                monotonicNow,
            };
        })()
    };
})(this);

var $mozAsyncTestDone = false;
function $DONE(failure) {
    // This function is generally called from within a Promise handler, so any
    // exception thrown by this method will be swallowed and most likely
    // ignored by the Promise machinery.
    if ($mozAsyncTestDone) {
        reportFailure("$DONE() already called");
        return;
    }
    $mozAsyncTestDone = true;

    if (failure)
        reportFailure(failure);
    else
        reportCompare(0, 0);

    if (typeof jsTestDriverEnd === "function") {
        gDelayTestDriverEnd = false;
        jsTestDriverEnd();
    }
}

// Some tests in test262 leave promise rejections unhandled.
if ("ignoreUnhandledRejections" in this) {
  ignoreUnhandledRejections();
}
