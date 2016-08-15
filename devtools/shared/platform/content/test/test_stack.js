/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// There isn't really very much about the content stack.js that we can
// test, but we'll do what we can.

"use strict";

var Cu = Components.utils;
const {require} = Cu.import("resource://devtools/shared/Loader.jsm", {});

// Make sure to explicitly require the content version of this module.
// We have to use the ".." trick due to the way the loader remaps
// devtools/shared/platform.
const {
  callFunctionWithAsyncStack,
  getStack,
  describeNthCaller
} = require("devtools/shared/platform/../content/stack");

function f3() {
  return describeNthCaller(2);
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

  let stack = getStack();
  equal(JSON.parse(JSON.stringify(stack)), stack, "stack is serializable");

  let desc = f1();
  ok(desc.includes("f1"), "stack description includes f1");
}
