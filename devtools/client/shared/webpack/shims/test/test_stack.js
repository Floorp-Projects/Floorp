/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// There isn't really very much about the content stack.js that we can
// test, but we'll do what we can.

"use strict";

const {require} = ChromeUtils.import("resource://devtools/shared/Loader.jsm", {});

const {
  callFunctionWithAsyncStack,
  getStack,
  getNthPathExcluding
} = require("devtools/client/shared/webpack/shims/platform-stack-stub");

function f3() {
  return getNthPathExcluding(2);
}

function f2() {
  return f3();
}

function f1() {
  return f2();
}

function run_test() {
  let value = 7;

  const changeValue = () => {
    value = 9;
  };

  callFunctionWithAsyncStack(changeValue, getStack(), "test_stack");
  equal(value, 9, "callFunctionWithAsyncStack worked");

  const stack = getStack();
  equal(JSON.parse(JSON.stringify(stack)), stack, "stack is serializable");

  const desc = f1();
  ok(desc.includes("f1"), "stack description includes f1");
}
