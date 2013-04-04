/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { on, once, off, emit, count } = require("sdk/event/core");

function scenario(setup) {
  return function(unit) {
    return function(assert) {
      let actual = [];
      let input = {};
      unit(input, function(output, events, expected, message) {
        let result = setup(output, expected, actual);

        events.forEach(function(event) emit(input, "data", event));

        assert.deepEqual(actual, result, message);
      });
    }
  }
}

exports.emits = scenario(function(output, expected, actual) {
  on(output, "data", function(data) actual.push(this, data));

  return expected.reduce(function($$, $) $$.concat(output, $), []);
});

exports.registerOnce = scenario(function(output, expected, actual) {
  function listener(data) actual.push(data);
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

  return expected.map(function($) $ + "#1");
});

exports.FIFO = scenario(function(target, expected, actual) {
  on(target, "data", function($) actual.push($ + "#1"));
  on(target, "data", function($) actual.push($ + "#2"));
  on(target, "data", function($) actual.push($ + "#3"));

  return expected.reduce(function(result, value) {
    return result.concat(value + "#1", value + "#2", value + "#3");
  }, []);
});
