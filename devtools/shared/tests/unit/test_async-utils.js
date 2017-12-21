/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test async-utils.js

const {Task} = require("devtools/shared/task");
// |const| will not work because
// it will make the Promise object immutable before assigning.
// Using Object.defineProperty() instead.
Object.defineProperty(this, "Promise", {
  value: require("promise"),
  writable: false, configurable: false
});
const {asyncOnce, promiseInvoke, promiseCall} = require("devtools/shared/async-utils");

function run_test() {
  do_test_pending();
  Task.spawn(function* () {
    yield test_async_args(asyncOnce);
    yield test_async_return(asyncOnce);
    yield test_async_throw(asyncOnce);

    yield test_async_once();
    yield test_async_invoke();
    do_test_finished();
  }).catch(error => {
    do_throw(error);
  });
}

// Test that arguments are correctly passed through to the async function.
function test_async_args(async) {
  let obj = {
    method: async(function* (a, b) {
      Assert.equal(this, obj);
      Assert.equal(a, "foo");
      Assert.equal(b, "bar");
    })
  };

  return obj.method("foo", "bar");
}

// Test that the return value from the async function is resolution value of
// the promise.
function test_async_return(async) {
  let obj = {
    method: async(function* (a, b) {
      return a + b;
    })
  };

  return obj.method("foo", "bar").then(ret => {
    Assert.equal(ret, "foobar");
  });
}

// Test that the throwing from an async function rejects the promise.
function test_async_throw(async) {
  let obj = {
    method: async(function* () {
      throw new Error("boom");
    })
  };

  return obj.method().catch(error => {
    Assert.ok(error instanceof Error);
    Assert.equal(error.message, "boom");
  });
}

// Test that asyncOnce only runs the async function once per instance and
// returns the same promise for that instance.
function test_async_once() {
  let counter = 0;

  function Foo() {}
  Foo.prototype = {
    ran: false,
    method: asyncOnce(function* () {
      yield Promise.resolve();
      if (this.ran) {
        do_throw("asyncOnce function unexpectedly ran twice on the same object");
      }
      this.ran = true;
      return counter++;
    })
  };

  let foo1 = new Foo();
  let foo2 = new Foo();
  let p1 = foo1.method();
  let p2 = foo2.method();

  Assert.notEqual(p1, p2);

  let p3 = foo1.method();
  Assert.equal(p1, p3);
  Assert.ok(!foo1.ran);

  let p4 = foo2.method();
  Assert.equal(p2, p4);
  Assert.ok(!foo2.ran);

  return p1.then(ret => {
    Assert.ok(foo1.ran);
    Assert.equal(ret, 0);
    return p2;
  }).then(ret => {
    Assert.ok(foo2.ran);
    Assert.equal(ret, 1);
  });
}

// Test invoke and call.
function test_async_invoke() {
  return Task.spawn(function* () {
    function func(a, b, expectedThis, callback) {
      Assert.equal(a, "foo");
      Assert.equal(b, "bar");
      Assert.equal(this, expectedThis);
      callback(a + b);
    }

    // Test call.
    let callResult = yield promiseCall(func, "foo", "bar", undefined);
    Assert.equal(callResult, "foobar");

    // Test invoke.
    let obj = { method: func };
    let invokeResult = yield promiseInvoke(obj, obj.method, "foo", "bar", obj);
    Assert.equal(invokeResult, "foobar");

    // Test passing multiple values to the callback.
    function multipleResults(callback) {
      callback("foo", "bar");
    }

    let results = yield promiseCall(multipleResults);
    Assert.equal(results.length, 2);
    Assert.equal(results[0], "foo");
    Assert.equal(results[1], "bar");

    // Test throwing from the function.
    function thrower() {
      throw new Error("boom");
    }

    yield promiseCall(thrower).catch(error => {
      Assert.ok(error instanceof Error);
      Assert.equal(error.message, "boom");
    });
  });
}
