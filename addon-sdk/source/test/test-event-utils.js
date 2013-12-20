/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

const { on, emit } = require("sdk/event/core");
const { filter, map, merge, expand, pipe } = require("sdk/event/utils");
const $ = require("./event/helpers");

function isEven(x) !(x % 2)
function inc(x) x + 1

exports["test filter events"] = function(assert) {
  let input = {};
  let evens = filter(input, isEven);
  let actual = [];
  on(evens, "data", function(e) actual.push(e));

  [1, 2, 3, 4, 5, 6, 7].forEach(function(x) emit(input, "data", x));

  assert.deepEqual(actual, [2, 4, 6], "only even numbers passed through");
};

exports["test filter emits"] = $.emits(function(input, assert) {
  let output = filter(input, isEven);
  assert(output,  [1, 2, 3, 4, 5], [2, 4], "this is `output` & evens passed");
});;

exports["test filter reg once"] = $.registerOnce(function(input, assert) {
  assert(filter(input, isEven),  [1, 2, 3, 4, 5, 6], [2, 4, 6],
         "listener can be registered only once");
});

exports["test filter ignores new"] = $.ignoreNew(function(input, assert) {
  assert(filter(input, isEven), [1, 2, 3], [2],
         "new listener is ignored")
});

exports["test filter is FIFO"] = $.FIFO(function(input, assert) {
  assert(filter(input, isEven), [1, 2, 3, 4], [2, 4],
         "listeners are invoked in fifo order")
});

exports["test map events"] = function(assert) {
  let input = {};
  let incs = map(input, inc);
  let actual = [];
  on(incs, "data", function(e) actual.push(e));

  [1, 2, 3, 4].forEach(function(x) emit(input, "data", x));

  assert.deepEqual(actual, [2, 3, 4, 5], "all numbers were incremented");
};

exports["test map emits"] = $.emits(function(input, assert) {
  let output = map(input, inc);
  assert(output,  [1, 2, 3], [2, 3, 4], "this is `output` & evens passed");
});;

exports["test map reg once"] = $.registerOnce(function(input, assert) {
  assert(map(input, inc),  [1, 2, 3], [2, 3, 4],
         "listener can be registered only once");
});

exports["test map ignores new"] = $.ignoreNew(function(input, assert) {
  assert(map(input, inc), [1], [2],
         "new listener is ignored")
});

exports["test map is FIFO"] = $.FIFO(function(input, assert) {
  assert(map(input, inc), [1, 2, 3, 4], [2, 3, 4, 5],
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
  let evens = filter(input, isEven)
  let output = merge([evens, input]);
  assert(output, [1, 2, 3], [1, 2, 2, 3], "this is `output` & evens passed");
});


exports["test merge reg once"] = $.registerOnce(function(input, assert) {
  let evens = filter(input, isEven)
  let output = merge([input, evens]);
  assert(output,  [1, 2, 3, 4], [1, 2, 2, 3, 4, 4],
         "listener can be registered only once");
});

exports["test merge ignores new"] = $.ignoreNew(function(input, assert) {
  let evens = filter(input, isEven)
  let output = merge([input, evens])
  assert(output, [1], [1],
         "new listener is ignored")
});

exports["test marge is FIFO"] = $.FIFO(function(input, assert) {
  let evens = filter(input, isEven)
  let output = merge([input, evens])

  assert(output, [1, 2, 3, 4], [1, 2, 2, 3, 4, 4],
         "listeners are invoked in fifo order")
});

exports["test expand"] = function(assert) {
  let a = {}, b = {}, c = {};
  let inputs = {};
  let actual = [];

  on(expand(inputs, function($) $()), "data", function($) actual.push($))

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
};

exports["test pipe"] = function (assert, done) {
  let src = {};
  let dest = {};
  
  let aneventCount = 0, multiargsCount = 0;
  let wildcardCount = {};

  on(dest, 'an-event', arg => {
    assert.equal(arg, 'my-arg', 'piped argument to event');
    ++aneventCount;
    check();
  });
  on(dest, 'multiargs', (...data) => {
    assert.equal(data[0], 'a', 'multiple arguments passed via pipe');
    assert.equal(data[1], 'b', 'multiple arguments passed via pipe');
    assert.equal(data[2], 'c', 'multiple arguments passed via pipe');
    ++multiargsCount;
    check();
  });
  
  on(dest, '*', (name, ...data) => {
    wildcardCount[name] = (wildcardCount[name] || 0) + 1;
    if (name === 'multiargs') {
      assert.equal(data[0], 'a', 'multiple arguments passed via pipe, wildcard');
      assert.equal(data[1], 'b', 'multiple arguments passed via pipe, wildcard');
      assert.equal(data[2], 'c', 'multiple arguments passed via pipe, wildcard');
    }
    if (name === 'an-event')
      assert.equal(data[0], 'my-arg', 'argument passed via pipe, wildcard');
    check();
  });

  pipe(src, dest);

  for (let i = 0; i < 3; i++)
    emit(src, 'an-event', 'my-arg');
  
  emit(src, 'multiargs', 'a', 'b', 'c');

  function check () {
    if (aneventCount === 3 && multiargsCount === 1 &&
        wildcardCount['an-event'] === 3 && 
        wildcardCount['multiargs'] === 1)
      done();
  }
};

exports["test pipe multiple targets"] = function (assert) {
  let src1 = {};
  let src2 = {};
  let middle = {};
  let dest = {};

  pipe(src1, middle);
  pipe(src2, middle);
  pipe(middle, dest);

  let middleFired = 0;
  let destFired = 0;
  let src1Fired = 0;
  let src2Fired = 0;

  on(src1, '*', () => src1Fired++);
  on(src2, '*', () => src2Fired++);
  on(middle, '*', () => middleFired++);
  on(dest, '*', () => destFired++);

  emit(src1, 'ev');
  assert.equal(src1Fired, 1, 'event triggers in source in pipe chain');
  assert.equal(middleFired, 1, 'event passes through the middle of pipe chain');
  assert.equal(destFired, 1, 'event propagates to end of pipe chain');
  assert.equal(src2Fired, 0, 'event does not fire on alternative chain routes');
  
  emit(src2, 'ev');
  assert.equal(src2Fired, 1, 'event triggers in source in pipe chain');
  assert.equal(middleFired, 2,
    'event passes through the middle of pipe chain from different src');
  assert.equal(destFired, 2,
    'event propagates to end of pipe chain from different src');
  assert.equal(src1Fired, 1, 'event does not fire on alternative chain routes');
  
  emit(middle, 'ev');
  assert.equal(middleFired, 3,
    'event triggers in source of pipe chain');
  assert.equal(destFired, 3,
    'event propagates to end of pipe chain from middle src');
  assert.equal(src1Fired, 1, 'event does not fire on alternative chain routes');
  assert.equal(src2Fired, 1, 'event does not fire on alternative chain routes');
};

require('test').run(exports);
