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
  Assert.ok(true, "Assert.ok should be available");
  Assert.notEqual("Floorp", "Firefox", "Assert.notEqual should be available");
  Assert.notDeepEqual(
    ["BrowserTestUtils"],
    ["TestUtils"],
    "Assert.notDeepEqual should be available",
  );
  Assert.strictEqual(
    "strict",
    "strict",
    "Assert.strictEqual should be available",
  );
  Assert.notStrictEqual(
    {},
    {},
    "Assert.notStrictEqual should be available",
  );
  ok(TestUtils, "TestUtils should be available");
  ok(BrowserTestUtils, "BrowserTestUtils should be available");
  ok(makeURI("about:blank"), "makeURI should be available");
  todo(false, "todo should be report-only in the compatibility shim");
});

add_task(async function asyncTasksAreAwaited() {
  const value = await Promise.resolve(42);
  is(value, 42, "add_task should await async task functions");
});

add_task(function mozillaTaskProgressIsPublished() {
  const progress =
    /** @type {{ status?: string, index?: number, total?: number, testName?: string } | undefined} */ (
      globalThis.__NORA_TEST_PROGRESS__
    );
  is(progress?.status, "running", "task progress should be running");
  ok((progress?.index ?? 0) > 0, "task progress should include an index");
  ok(
    (progress?.total ?? 0) >= (progress?.index ?? 0),
    "task progress total should cover the current index",
  );
  is(
    progress?.testName,
    "mozillaTaskProgressIsPublished",
    "task progress should include the registered task name",
  );
});

add_task(async function waitForConditionHelpersResolveAndReject() {
  let testUtilsValue = "";
  const testUtilsPromise = TestUtils.waitForCondition(
    () => testUtilsValue,
    "TestUtils.waitForCondition should resolve",
    1,
    10,
  );
  testUtilsValue = "ready";
  is(
    await testUtilsPromise,
    "ready",
    "TestUtils.waitForCondition should resolve with predicate return value",
  );

  let browserTestUtilsValue = 0;
  const browserTestUtilsPromise = BrowserTestUtils.waitForCondition(
    () => browserTestUtilsValue === 2 && "browser-ready",
    "BrowserTestUtils.waitForCondition should resolve",
    1,
    10,
  );
  browserTestUtilsValue = 2;
  is(
    await browserTestUtilsPromise,
    "browser-ready",
    "BrowserTestUtils.waitForCondition should resolve with predicate return value",
  );

  let timedOut = false;
  try {
    await TestUtils.waitForCondition(
      () => false,
      "expected compat timeout",
      1,
      2,
    );
  } catch (error) {
    timedOut = true;
    ok(
      String(error).includes("expected compat timeout"),
      "waitForCondition timeout should include the caller message",
    );
  }
  ok(timedOut, "waitForCondition should reject when the condition stays false");
});

add_task(async function browserTestUtilsWaitForEventResolvesMatchingEvent() {
  const target = new EventTarget();
  const eventPromise = BrowserTestUtils.waitForEvent(
    target,
    "nora-compat-event",
    false,
    (event) => /** @type {CustomEvent} */ (event).detail === "expected-detail",
  );

  target.dispatchEvent(
    new CustomEvent("nora-compat-event", { detail: "ignored-detail" }),
  );
  target.dispatchEvent(
    new CustomEvent("nora-compat-event", { detail: "expected-detail" }),
  );

  const receivedEvent = /** @type {CustomEvent} */ (await eventPromise);
  is(
    receivedEvent.detail,
    "expected-detail",
    "waitForEvent should resolve with the matching event",
  );
});

add_task(async function browserTestUtilsWaitForMutationConditionResolves() {
  const target = document.createElement("div");
  const mutationPromise = BrowserTestUtils.waitForMutationCondition(
    target,
    { attributes: true },
    () => target.getAttribute("data-ready"),
  );

  target.setAttribute("data-ready", "yes");
  is(
    await mutationPromise,
    "yes",
    "waitForMutationCondition should resolve with the condition return value",
  );
});

add_task(async function browserTestUtilsWaitForNotificationBoxResolves() {
  const stack = document.createElement("div");
  const notificationBox = { stack };
  const notificationPromise = BrowserTestUtils
    .waitForNotificationInNotificationBox(notificationBox, "compat-value");

  const unrelated = document.createElement("div");
  unrelated.setAttribute("value", "other-value");
  stack.append(unrelated);

  const notification = document.createElement("div");
  notification.setAttribute("value", "compat-value");
  stack.append(notification);

  is(
    await notificationPromise,
    notification,
    "waitForNotificationInNotificationBox should resolve with the notification",
  );
});

add_task(function cleanupIsDeferredUntilTasksFinish() {
  is(cleanupState, 0, "cleanup should not run before tasks finish");
});
