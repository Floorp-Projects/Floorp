/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

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
  }).then(null, error => {
    do_throw(error);
  });
}

// Test that arguments are correctly passed through to the async function.
function test_async_args(async) {
  let obj = {
    method: async(function* (a, b) {
      do_check_eq(this, obj);
      do_check_eq(a, "foo");
      do_check_eq(b, "bar");
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
    do_check_eq(ret, "foobar");
  });
}

// Test that the throwing from an async function rejects the promise.
function test_async_throw(async) {
  let obj = {
    method: async(function* () {
      throw "boom";
    })
  };

  return obj.method().then(null, error => {
    do_check_eq(error, "boom");
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

  do_check_neq(p1, p2);

  let p3 = foo1.method();
  do_check_eq(p1, p3);
  do_check_false(foo1.ran);

  let p4 = foo2.method();
  do_check_eq(p2, p4);
  do_check_false(foo2.ran);

  return p1.then(ret => {
    do_check_true(foo1.ran);
    do_check_eq(ret, 0);
    return p2;
  }).then(ret => {
    do_check_true(foo2.ran);
    do_check_eq(ret, 1);
  });
}

// Test invoke and call.
function test_async_invoke() {
  return Task.spawn(function* () {
    function func(a, b, expectedThis, callback) {
      "use strict";
      do_check_eq(a, "foo");
      do_check_eq(b, "bar");
      do_check_eq(this, expectedThis);
      callback(a + b);
    }

    // Test call.
    let callResult = yield promiseCall(func, "foo", "bar", undefined);
    do_check_eq(callResult, "foobar");


    // Test invoke.
    let obj = { method: func };
    let invokeResult = yield promiseInvoke(obj, obj.method, "foo", "bar", obj);
    do_check_eq(invokeResult, "foobar");


    // Test passing multiple values to the callback.
    function multipleResults(callback) {
      callback("foo", "bar");
    }

    let results = yield promiseCall(multipleResults);
    do_check_eq(results.length, 2);
    do_check_eq(results[0], "foo");
    do_check_eq(results[1], "bar");


    // Test throwing from the function.
    function thrower() {
      throw "boom";
    }

    yield promiseCall(thrower).then(null, error => {
      do_check_eq(error, "boom");
    });
  });
}
