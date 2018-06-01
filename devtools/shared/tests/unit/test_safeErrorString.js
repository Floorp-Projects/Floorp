/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test DevToolsUtils.safeErrorString

function run_test() {
  test_with_error();
  test_with_tricky_error();
  test_with_string();
  test_with_thrower();
  test_with_psychotic();
}

function test_with_error() {
  const s = DevToolsUtils.safeErrorString(new Error("foo bar"));
  // Got the message.
  Assert.ok(s.includes("foo bar"));
  // Got the stack.
  Assert.ok(s.includes("test_with_error"));
  Assert.ok(s.includes("test_safeErrorString.js"));
  // Got the lineNumber and columnNumber.
  Assert.ok(s.includes("Line"));
  Assert.ok(s.includes("column"));
}

function test_with_tricky_error() {
  const e = new Error("batman");
  e.stack = { toString: Object.create(null) };
  const s = DevToolsUtils.safeErrorString(e);
  // Still got the message, despite a bad stack property.
  Assert.ok(s.includes("batman"));
}

function test_with_string() {
  const s = DevToolsUtils.safeErrorString("not really an error");
  // Still get the message.
  Assert.ok(s.includes("not really an error"));
}

function test_with_thrower() {
  const s = DevToolsUtils.safeErrorString({
    toString: () => {
      throw new Error("Muahahaha");
    }
  });
  // Still don't fail, get string back.
  Assert.equal(typeof s, "string");
}

function test_with_psychotic() {
  const s = DevToolsUtils.safeErrorString({
    toString: () => Object.create(null)
  });
  // Still get a string out, and no exceptions thrown
  Assert.equal(typeof s, "string");
  Assert.equal(s, "[object Object]");
}
