/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

const { on, emit } = require("sdk/event/core");
const { filter, map, merge, expand } = require("sdk/event/utils");
const $ = require("./event/helpers");

function isEven(x) !(x % 2)
function inc(x) x + 1

exports["test filter events"] = function(assert) {
  let input = {};
  let evens = filter(isEven, input);
  let actual = [];
  on(evens, "data", function(e) actual.push(e));

  [1, 2, 3, 4, 5, 6, 7].forEach(function(x) emit(input, "data", x));

  assert.deepEqual(actual, [2, 4, 6], "only even numbers passed through");
};

exports["test filter emits"] = $.emits(function(input, assert) {
  let output = filter(isEven, input);
  assert(output,  [1, 2, 3, 4, 5], [2, 4], "this is `output` & evens passed");
});;

exports["test filter reg once"] = $.registerOnce(function(input, assert) {
  assert(filter(isEven, input),  [1, 2, 3, 4, 5, 6], [2, 4, 6],
         "listener can be registered only once");
});

exports["test filter ignores new"] = $.ignoreNew(function(input, assert) {
  assert(filter(isEven, input), [1, 2, 3], [2],
         "new listener is ignored")
});

exports["test filter is FIFO"] = $.FIFO(function(input, assert) {
  assert(filter(isEven, input), [1, 2, 3, 4], [2, 4],
         "listeners are invoked in fifo order")
});

exports["test map events"] = function(assert) {
  let input = {};
  let incs = map(inc, input);
  let actual = [];
  on(incs, "data", function(e) actual.push(e));

  [1, 2, 3, 4].forEach(function(x) emit(input, "data", x));

  assert.deepEqual(actual, [2, 3, 4, 5], "all numbers were incremented");
};

exports["test map emits"] = $.emits(function(input, assert) {
  let output = map(inc, input);
  assert(output,  [1, 2, 3], [2, 3, 4], "this is `output` & evens passed");
});;

exports["test map reg once"] = $.registerOnce(function(input, assert) {
  assert(map(inc, input),  [1, 2, 3], [2, 3, 4],
         "listener can be registered only once");
});

exports["test map ignores new"] = $.ignoreNew(function(input, assert) {
  assert(map(inc, input), [1], [2],
         "new listener is ignored")
});

exports["test map is FIFO"] = $.FIFO(function(input, assert) {
  assert(map(inc, input), [1, 2, 3, 4], [2, 3, 4, 5],
         "listeners are invoked in fifo order")
});

exports["test merge stream[stream]"] = function(assert) {
  let a = {}, b = {}, c = {};
  let inputs = {};
  let actual = [];

  on(merge(inputs), "data", function($) actual.push($))

  emit(inputs, "data", a);
  emit(a, "data", "a1");
  emit(inputs, "data", b);
  emit(b, "data", "b1");
  emit(a, "data", "a2");
  emit(inputs, "data", c);
  emit(c, "data", "c1");
  emit(c, "data", "c2");
  emit(b, "data", "b2");
  emit(a, "data", "a3");

  assert.deepEqual(actual, ["a1", "b1", "a2", "c1", "c2", "b2", "a3"],
                   "all inputs data merged into one");
};

exports["test merge array[stream]"] = function(assert) {
  let a = {}, b = {}, c = {};
  let inputs = {};
  let actual = [];

  on(merge([a, b, c]), "data", function($) actual.push($))

  emit(a, "data", "a1");
  emit(b, "data", "b1");
  emit(a, "data", "a2");
  emit(c, "data", "c1");
  emit(c, "data", "c2");
  emit(b, "data", "b2");
  emit(a, "data", "a3");

  assert.deepEqual(actual, ["a1", "b1", "a2", "c1", "c2", "b2", "a3"],
                   "all inputs data merged into one");
};

exports["test merge emits"] = $.emits(function(input, assert) {
  let evens = filter(isEven, input)
  let output = merge([evens, input]);
  assert(output, [1, 2, 3], [1, 2, 2, 3], "this is `output` & evens passed");
});


exports["test merge reg once"] = $.registerOnce(function(input, assert) {
  let evens = filter(isEven, input)
  let output = merge([input, evens]);
  assert(output,  [1, 2, 3, 4], [1, 2, 2, 3, 4, 4],
         "listener can be registered only once");
});

exports["test merge ignores new"] = $.ignoreNew(function(input, assert) {
  let evens = filter(isEven, input)
  let output = merge([input, evens])
  assert(output, [1], [1],
         "new listener is ignored")
});

exports["test marge is FIFO"] = $.FIFO(function(input, assert) {
  let evens = filter(isEven, input)
  let output = merge([input, evens])

  assert(output, [1, 2, 3, 4], [1, 2, 2, 3, 4, 4],
         "listeners are invoked in fifo order")
});

exports["test expand"] = function(assert) {
  let a = {}, b = {}, c = {};
  let inputs = {};
  let actual = [];

  on(expand(function($) $(), inputs), "data", function($) actual.push($))

  emit(inputs, "data", function() a);
  emit(a, "data", "a1");
  emit(inputs, "data", function() b);
  emit(b, "data", "b1");
  emit(a, "data", "a2");
  emit(inputs, "data", function() c);
  emit(c, "data", "c1");
  emit(c, "data", "c2");
  emit(b, "data", "b2");
  emit(a, "data", "a3");

  assert.deepEqual(actual, ["a1", "b1", "a2", "c1", "c2", "b2", "a3"],
                   "all inputs data merged into one");
}


require('test').run(exports);
