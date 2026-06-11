// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { gestureActions } from "../utils/gestures.ts";
import {
  assert,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

// ---------------------------------------------------------------------------
// Test: link hints open current tab action is registered
// ---------------------------------------------------------------------------
function testOpenCurrentTabRegistered(): void {
  const fn = gestureActions.getAction("floorp-link-hints-open-current-tab");
  assert(
    fn !== undefined,
    "floorp-link-hints-open-current-tab should be registered in gestureActions",
  );
}

// ---------------------------------------------------------------------------
// Test: link hints open new tab action is registered
// ---------------------------------------------------------------------------
function testOpenNewTabRegistered(): void {
  const fn = gestureActions.getAction("floorp-link-hints-open-new-tab");
  assert(
    fn !== undefined,
    "floorp-link-hints-open-new-tab should be registered in gestureActions",
  );
}

// ---------------------------------------------------------------------------
// Test: link hints open new background tab action is registered
// ---------------------------------------------------------------------------
function testOpenNewBackgroundTabRegistered(): void {
  const fn = gestureActions.getAction("floorp-link-hints-open-new-background-tab");
  assert(
    fn !== undefined,
    "floorp-link-hints-open-new-background-tab should be registered in gestureActions",
  );
}

// ---------------------------------------------------------------------------
// Test: link hints copy url action is registered
// ---------------------------------------------------------------------------
function testCopyUrlRegistered(): void {
  const fn = gestureActions.getAction("floorp-link-hints-copy-url");
  assert(
    fn !== undefined,
    "floorp-link-hints-copy-url should be registered in gestureActions",
  );
}

// ---------------------------------------------------------------------------
// Test: link hints hover action is registered
// ---------------------------------------------------------------------------
function testHoverRegistered(): void {
  const fn = gestureActions.getAction("floorp-link-hints-hover");
  assert(
    fn !== undefined,
    "floorp-link-hints-hover should be registered in gestureActions",
  );
}

// ---------------------------------------------------------------------------
// Test: all link hints actions are functions
// ---------------------------------------------------------------------------
function testAllLinkHintsActionsAreFunctions(): void {
  const actionNames = [
    "floorp-link-hints-open-current-tab",
    "floorp-link-hints-open-new-tab",
    "floorp-link-hints-open-new-background-tab",
    "floorp-link-hints-copy-url",
    "floorp-link-hints-hover",
  ];
  for (const name of actionNames) {
    const fn = gestureActions.getAction(name);
    assert(
      typeof fn === "function",
      `link hints action '${name}' should be a function`,
    );
  }
}

// ---------------------------------------------------------------------------
// Test: link hints actions are distinct (not the same function reference)
// ---------------------------------------------------------------------------
function testLinkHintsActionsAreDistinct(): void {
  const actionNames = [
    "floorp-link-hints-open-current-tab",
    "floorp-link-hints-open-new-tab",
    "floorp-link-hints-open-new-background-tab",
    "floorp-link-hints-copy-url",
    "floorp-link-hints-hover",
  ];
  const fns = actionNames.map((name) => gestureActions.getAction(name));
  for (let i = 0; i < fns.length; i++) {
    for (let j = i + 1; j < fns.length; j++) {
      assert(
        fns[i] !== fns[j],
        `actions '${actionNames[i]}' and '${actionNames[j]}' should be distinct functions`,
      );
    }
  }
}

// ---------------------------------------------------------------------------
// Test: link hints actions don't throw when called with null gBrowser
// ---------------------------------------------------------------------------
function testActionsHandleNullGBrowser(): void {
  const actionNames = [
    "floorp-link-hints-open-current-tab",
    "floorp-link-hints-open-new-tab",
    "floorp-link-hints-open-new-background-tab",
    "floorp-link-hints-copy-url",
    "floorp-link-hints-hover",
  ];
  const mockWin = {} as Window;
  for (const name of actionNames) {
    const fn = gestureActions.getAction(name);
    assert(typeof fn === "function", `action '${name}' should be a function`);
    // Should not throw even with minimal window (no gBrowser)
    try {
      fn(mockWin);
    } catch {
      assert(false, `action '${name}' should not throw when window has no gBrowser`);
    }
  }
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "link hints open current tab action is registered", fn: testOpenCurrentTabRegistered },
    { name: "link hints open new tab action is registered", fn: testOpenNewTabRegistered },
    { name: "link hints open new background tab action is registered", fn: testOpenNewBackgroundTabRegistered },
    { name: "link hints copy url action is registered", fn: testCopyUrlRegistered },
    { name: "link hints hover action is registered", fn: testHoverRegistered },
    { name: "all link hints actions are functions", fn: testAllLinkHintsActionsAreFunctions },
    { name: "link hints actions are distinct functions", fn: testLinkHintsActionsAreDistinct },
    { name: "link hints actions handle null gBrowser gracefully", fn: testActionsHandleNullGBrowser },
  ];
  await runTests("linkHintsActions.test.ts", tests);
}
