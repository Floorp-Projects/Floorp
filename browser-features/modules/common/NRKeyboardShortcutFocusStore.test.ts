// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assertEquals,
  runTests,
  type TestCase,
} from "../../chrome/test/utils/test_harness.ts";
import { NRKeyboardShortcutFocusStore } from "./NRKeyboardShortcutFocusStore.ts";

function testAggregatesEditableFramesPerBrowser(): void {
  const store = new NRKeyboardShortcutFocusStore();
  const browser = {};
  const firstFrame = {};
  const secondFrame = {};

  store.setFrameEditable(browser, firstFrame, true);
  store.setFrameEditable(browser, secondFrame, true);
  store.removeFrame(firstFrame);
  assertEquals(
    store.isEditableFocused(browser),
    true,
    "one remaining editable frame should keep the browser focused",
  );
  store.removeFrame(secondFrame);
  assertEquals(
    store.isEditableFocused(browser),
    false,
    "the browser should clear after its last editable frame",
  );
}

function testFramesMoveBetweenTopBrowsers(): void {
  const store = new NRKeyboardShortcutFocusStore();
  const firstBrowser = {};
  const secondBrowser = {};
  const frame = {};

  store.setFrameEditable(firstBrowser, frame, true);
  store.setFrameEditable(secondBrowser, frame, true);
  assertEquals(
    store.isEditableFocused(firstBrowser),
    false,
    "moving a frame should remove its old browser entry",
  );
  assertEquals(
    store.isEditableFocused(secondBrowser),
    true,
    "moving a frame should add its new browser entry",
  );
}

function testFalseUpdateAndFrameRemovalAreIdempotent(): void {
  const store = new NRKeyboardShortcutFocusStore();
  const browser = {};
  const frame = {};

  store.setFrameEditable(browser, frame, true);
  store.setFrameEditable(browser, frame, false);
  store.removeFrame(frame);
  assertEquals(
    store.isEditableFocused(browser),
    false,
    "false updates and repeated teardown should leave no stale state",
  );
}

function testClearBrowserRemovesEveryFrame(): void {
  const store = new NRKeyboardShortcutFocusStore();
  const browser = {};
  const firstFrame = {};
  const secondFrame = {};

  store.setFrameEditable(browser, firstFrame, true);
  store.setFrameEditable(browser, secondFrame, true);
  store.clearBrowser(browser);
  assertEquals(
    store.isEditableFocused(browser),
    false,
    "tab/browser teardown should clear all aggregated frames",
  );

  store.removeFrame(firstFrame);
  store.removeFrame(secondFrame);
  assertEquals(
    store.isEditableFocused(browser),
    false,
    "later actor teardown should stay idempotent",
  );
}

function testTwoBrowsersStayIsolated(): void {
  const store = new NRKeyboardShortcutFocusStore();
  const firstBrowser = {};
  const secondBrowser = {};

  store.setFrameEditable(firstBrowser, {}, true);
  assertEquals(
    store.isEditableFocused(firstBrowser),
    true,
    "the focused browser should report editable state",
  );
  assertEquals(
    store.isEditableFocused(secondBrowser),
    false,
    "another window's browser should remain isolated",
  );
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "aggregates editable frames per browser",
      fn: testAggregatesEditableFramesPerBrowser,
    },
    {
      name: "moves frames between browsers",
      fn: testFramesMoveBetweenTopBrowsers,
    },
    {
      name: "false updates and removal are idempotent",
      fn: testFalseUpdateAndFrameRemovalAreIdempotent,
    },
    {
      name: "clearBrowser removes every frame",
      fn: testClearBrowserRemovesEveryFrame,
    },
    { name: "two browsers stay isolated", fn: testTwoBrowsersStayIsolated },
  ];
  await runTests("NRKeyboardShortcutFocusStore.test.ts", tests);
}
