/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test WeakMapMap.

"use strict";

const WeakMapMap = require("resource://devtools/client/shared/WeakMapMap.js");

const myWeakMapMap = new WeakMapMap();
const key = { randomObject: true };

// eslint-disable-next-line
function run_test() {
  test_set();
  test_has();
  test_get();
  test_delete();
  test_clear();
}

function test_set() {
  // myWeakMapMap.set
  myWeakMapMap.set(key, "text1", "value1");
  myWeakMapMap.set(key, "text2", "value2");
  myWeakMapMap.set(key, "text3", "value3");
}

function test_has() {
  // myWeakMapMap.has
  ok(myWeakMapMap.has(key, "text1"), "text1 exists");
  ok(myWeakMapMap.has(key, "text2"), "text2 exists");
  ok(myWeakMapMap.has(key, "text3"), "text3 exists");
  ok(!myWeakMapMap.has(key, "notakey"), "notakey does not exist");
}

function test_get() {
  // myWeakMapMap.get
  const value1 = myWeakMapMap.get(key, "text1");
  equal(value1, "value1", "test value1");

  const value2 = myWeakMapMap.get(key, "text2");
  equal(value2, "value2", "test value2");

  const value3 = myWeakMapMap.get(key, "text3");
  equal(value3, "value3", "test value3");

  const value4 = myWeakMapMap.get(key, "notakey");
  equal(value4, undefined, "test value4");
}

function test_delete() {
  // myWeakMapMap.delete
  myWeakMapMap.delete(key, "text2");

  // Check that the correct entry was deleted
  ok(myWeakMapMap.has(key, "text1"), "text1 exists");
  ok(!myWeakMapMap.has(key, "text2"), "text2 no longer exists");
  ok(myWeakMapMap.has(key, "text3"), "text3 exists");
}

function test_clear() {
  // myWeakMapMap.clear
  myWeakMapMap.clear();

  // Ensure myWeakMapMap was properly cleared
  ok(!myWeakMapMap.has(key, "text1"), "text1 no longer exists");
  ok(!myWeakMapMap.has(key, "text3"), "text3 no longer exists");
}
