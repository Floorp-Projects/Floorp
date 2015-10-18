/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { on, once, off, emit, count } = require("sdk/event/core");

const { setImmediate, setTimeout } = require("sdk/timers");
const { defer } = require("sdk/core/promise");

/**
 * Utility function that returns a promise once the specified event's `type`
 * is emitted on the given `target`, or the delay specified is passed.
 *
 * @param {Object|Number} [target]
 *    The delay to wait, or the object that receives the event.
 *    If not given, the function returns a promise that will be resolved
 *    as soon as possible.
 * @param {String} [type]
 *    A string representing the event type to waiting for.
 * @param {Boolean} [capture]
 *    If `true`, `capture` indicates that the user wishes to initiate capture.
 *
 * @returns {Promise}
 *    A promise resolved once the delay given is passed, or the object
 *    receives the event specified
 */
const wait = function(target, type, capture) {
  let { promise, resolve, reject } = defer();

  if (!arguments.length) {
    setImmediate(resolve);
  }
  else if (typeof(target) === "number") {
    setTimeout(resolve, target);
  }
  else if (typeof(target.once) === "function") {
    target.once(type, resolve);
  }
  else if (typeof(target.addEventListener) === "function") {
    target.addEventListener(type, function listener(...args) {
      this.removeEventListener(type, listener, capture);
      resolve(...args);
    }, capture);
  }
  else if (typeof(target) === "object" && target !== null) {
    once(target, type, resolve);
  }
  else {
    reject('Invalid target given.');
  }

  return promise;
};
exports.wait = wait;

function scenario(setup) {
  return function(unit) {
    return function(assert) {
      let actual = [];
      let input = {};
      unit(input, function(output, events, expected, message) {
        let result = setup(output, expected, actual);

        events.forEach(event => emit(input, "data", event));

        assert.deepEqual(actual, result, message);
      });
    }
  }
}

exports.emits = scenario(function(output, expected, actual) {
  on(output, "data", function(data) {
    return actual.push(this, data);
  });

  return expected.reduce(($$, $) => $$.concat(output, $), []);
});

exports.registerOnce = scenario(function(output, expected, actual) {
  function listener(data) {
    return actual.push(data);
  }
  on(output, "data", listener);
  on(output, "data", listener);
  on(output, "data", listener);

  return expected;
});

exports.ignoreNew = scenario(function(output, expected, actual) {
  on(output, "data", function(data) {
    actual.push(data + "#1");
    on(output, "data", function(data) {
      actual.push(data + "#2");
    });
  });

  return expected.map($ => $ + "#1");
});

exports.FIFO = scenario(function(target, expected, actual) {
  on(target, "data", $ => actual.push($ + "#1"));
  on(target, "data", $ => actual.push($ + "#2"));
  on(target, "data", $ => actual.push($ + "#3"));

  return expected.reduce(function(result, value) {
    return result.concat(value + "#1", value + "#2", value + "#3");
  }, []);
});
