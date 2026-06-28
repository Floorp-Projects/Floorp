// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
// @ts-check
/// <reference path="../../@types/mochitest-compat.d.ts" />

let cleanupState = 0;

registerCleanupFunction(function firstCleanup() {
  is(
    cleanupState,
    1,
    "first registered cleanup should run after the second cleanup",
  );
  cleanupState = 2;
});

registerCleanupFunction(function secondCleanup() {
  is(cleanupState, 0, "second registered cleanup should run first");
  cleanupState = 1;
});

add_task(function assertionsAreAvailable() {
  info("running Mozilla-style compatibility assertions");
  ok(true, "ok should accept truthy values");
  is("Floorp", "Floorp", "is should compare expected values");
  isnot("Floorp", "Firefox", "isnot should reject unexpected values");
  Assert.deepEqual(
    ["BrowserTestUtils", "makeURI"],
    ["BrowserTestUtils", "makeURI"],
    "Assert.deepEqual should be available",
  );
  ok(BrowserTestUtils, "BrowserTestUtils should be available");
  ok(makeURI("about:blank"), "makeURI should be available");
  todo(false, "todo should be report-only in the compatibility shim");
});

add_task(async function asyncTasksAreAwaited() {
  const value = await Promise.resolve(42);
  is(value, 42, "add_task should await async task functions");
});

add_task(function cleanupIsDeferredUntilTasksFinish() {
  is(cleanupState, 0, "cleanup should not run before tasks finish");
});
