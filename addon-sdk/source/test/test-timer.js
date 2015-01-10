/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var timer = require("sdk/timers");
const { Loader } = require("sdk/test/loader");
const { gc } = require("sdk/test/memory");
const { all, defer } = require("sdk/core/promise");

exports.testSetTimeout = function(assert, end) {
  timer.setTimeout(function() {
    assert.pass("testSetTimeout passed");
    end();
  }, 1);
};

exports.testSetTimeoutGC = function*(assert) {
  let called = defer();
  let gcDone = defer();

  timer.setTimeout(called.resolve, 300);
  gc().then(gcDone.resolve);

  yield called.promise;
  yield gcDone.promise;

  assert.pass("setTimeout passed!");
};

exports.testParamedSetTimeout = function(assert, end) {
  let params = [1, 'foo', { bar: 'test' }, null, undefined];
  timer.setTimeout.apply(null, [function() {
    assert.equal(arguments.length, params.length);
    for (let i = 0, ii = params.length; i < ii; i++)
      assert.equal(params[i], arguments[i]);
    end();
  }, 1].concat(params));
};

exports.testClearTimeout = function(assert, end) {
  var myFunc = function myFunc() {
    assert.fail("myFunc() should not be called in testClearTimeout");
  };
  var id = timer.setTimeout(myFunc, 1);
  timer.setTimeout(function() {
    assert.pass("testClearTimeout passed");
    end();
  }, 2);
  timer.clearTimeout(id);
};

exports.testParamedClearTimeout = function(assert, end) {
  let params = [1, 'foo', { bar: 'test' }, null, undefined];
  var myFunc = function myFunc() {
    assert.fail("myFunc() should not be called in testClearTimeout");
  };
  var id = timer.setTimeout(myFunc, 1);
  timer.setTimeout.apply(null, [function() {
    assert.equal(arguments.length, params.length);
    for (let i = 0, ii = params.length; i < ii; i++)
      assert.equal(params[i], arguments[i]);
    end();
  }, 1].concat(params));
  timer.clearTimeout(id);
};

exports.testSetInterval = function (assert, end) {
  var count = 0;
  var id = timer.setInterval(function () {
    count++;
    if (count >= 5) {
      timer.clearInterval(id);
      assert.pass("testSetInterval passed");
      end();
    }
  }, 1);
};

exports.testSetIntervalGC = function*(assert) {
  let called = defer();
  let gcDone = defer();
  let done = false;
  let count = 0;

  let id = timer.setInterval(() => {
    assert.pass("call count is " + count);

    if (done) {
      timer.clearInterval(id);
      called.resolve();
      return null;
    }

    if (count++ == 0) {
      assert.pass("first call to setInterval worked!");

      gc().then(() => {
        assert.pass("gc is complete!");
        done = true;
        gcDone.resolve();
      });

      assert.pass("called gc()!");
    }

    return null;
  }, 1);

  yield gcDone.promise;
  yield called.promise;

  assert.pass("setInterval was called after the gc!");
};

exports.testParamedSetInerval = function(assert, end) {
  let params = [1, 'foo', { bar: 'test' }, null, undefined];
  let count = 0;
  let id = timer.setInterval.apply(null, [function() {
    count ++;
    if (count < 5) {
      assert.equal(arguments.length, params.length);
      for (let i = 0, ii = params.length; i < ii; i++)
        assert.equal(params[i], arguments[i]);
    } else {
      timer.clearInterval(id);
      end();
    }
  }, 1].concat(params));
};

exports.testClearInterval = function (assert, end) {
  timer.clearInterval(timer.setInterval(function () {
    assert.fail("setInterval callback should not be called");
  }, 1));
  var id = timer.setInterval(function () {
    timer.clearInterval(id);
    assert.pass("testClearInterval passed");
    end();
  }, 2);
};

exports.testParamedClearInterval = function(assert, end) {
  timer.clearInterval(timer.setInterval(function () {
    assert.fail("setInterval callback should not be called");
  }, 1, timer, {}, null));

  let id = timer.setInterval(function() {
    timer.clearInterval(id);
    assert.equal(3, arguments.length);
    end();
  }, 2, undefined, 'test', {});
};


exports.testImmediate = function(assert, end) {
  let actual = [];
  let ticks = 0;
  timer.setImmediate(function(...params) {
    actual.push(params);
    assert.equal(ticks, 1, "is a next tick");
    assert.deepEqual(actual, [["start", "immediates"]]);
  }, "start", "immediates");

  timer.setImmediate(function(...params) {
    actual.push(params);
    assert.deepEqual(actual, [["start", "immediates"],
                                  ["added"]]);
    assert.equal(ticks, 1, "is a next tick");
    timer.setImmediate(function(...params) {
      actual.push(params);
      assert.equal(ticks, 2, "is second tick");
      assert.deepEqual(actual, [["start", "immediates"],
                                    ["added"],
                                    [],
                                    ["last", "immediate", "handler"],
                                    ["side-effect"]]);
      end();
    }, "side-effect");
  }, "added");

  timer.setImmediate(function(...params) {
    actual.push(params);
    assert.equal(ticks, 1, "is a next tick");
    assert.deepEqual(actual, [["start", "immediates"],
                              ["added"],
                              []]);
    timer.clearImmediate(removeID);
  });

  function removed() {
    assert.fail("should be removed");
  }
  let removeID = timer.setImmediate(removed);

  timer.setImmediate(function(...params) {
    actual.push(params);
    assert.equal(ticks, 1, "is a next tick");
    assert.deepEqual(actual, [["start", "immediates"],
                              ["added"],
                              [],
                              ["last", "immediate", "handler"]]);
    ticks = ticks + 1;
  }, "last", "immediate", "handler");


  ticks = ticks + 1;
};

exports.testUnload = function(assert, end) {
  var loader = Loader(module);
  var sbtimer = loader.require("sdk/timers");

  var myFunc = function myFunc() {
    assert.fail("myFunc() should not be called in testUnload");
  };

  sbtimer.setTimeout(myFunc, 1);
  sbtimer.setTimeout(myFunc, 1, 'foo', 4, {}, undefined);
  sbtimer.setInterval(myFunc, 1);
  sbtimer.setInterval(myFunc, 1, {}, null, 'bar', undefined, 87);
  loader.unload();
  timer.setTimeout(function() {
    assert.pass("timer testUnload passed");
    end();
  }, 2);
};

require("sdk/test").run(exports);
