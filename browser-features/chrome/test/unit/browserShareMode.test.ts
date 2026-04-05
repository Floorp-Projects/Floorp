// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  shareModeEnabled,
  setShareModeEnabled,
} from "../../common/browser-share-mode/browser-share-mode.tsx";
import BrowserShareMode from "../../common/browser-share-mode/index.ts";

import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../utils/test_harness.ts";

function testShareModeSignalReadable(): void {
  const value = shareModeEnabled();
  assert(
    typeof value === "boolean",
    "shareModeEnabled should return a boolean",
  );
}

function testShareModeDefaultIsFalse(): void {
  // Reset to known state
  setShareModeEnabled(false);
  assertEquals(
    shareModeEnabled(),
    false,
    "shareModeEnabled default should be false",
  );
}

function testSetShareModeEnabledToggles(): void {
  const original = shareModeEnabled();
  try {
    setShareModeEnabled(true);
    assertEquals(
      shareModeEnabled(),
      true,
      "shareModeEnabled should be true after setting true",
    );

    setShareModeEnabled(false);
    assertEquals(
      shareModeEnabled(),
      false,
      "shareModeEnabled should be false after setting false",
    );
  } finally {
    setShareModeEnabled(original);
  }
}

function testSetShareModeEnabledWithCallback(): void {
  const original = shareModeEnabled();
  try {
    setShareModeEnabled(false);
    setShareModeEnabled((prev) => !prev);
    assertEquals(
      shareModeEnabled(),
      true,
      "shareModeEnabled should toggle from false to true via callback",
    );

    setShareModeEnabled((prev) => !prev);
    assertEquals(
      shareModeEnabled(),
      false,
      "shareModeEnabled should toggle from true to false via callback",
    );
  } finally {
    setShareModeEnabled(original);
  }
}

function testBrowserShareModeClassExists(): void {
  assert(
    typeof BrowserShareMode === "function",
    "BrowserShareMode should be a constructor function",
  );
}

function testBrowserShareModeHasInitMethod(): void {
  assert(
    typeof BrowserShareMode.prototype.init === "function",
    "BrowserShareMode.prototype.init should be a function",
  );
}

export async function runAllTests(): Promise<void> {
  await runTests("browserShareMode.test.ts", [
    {
      name: "shareModeEnabled signal is readable",
      fn: testShareModeSignalReadable,
    },
    { name: "share mode default is false", fn: testShareModeDefaultIsFalse },
    {
      name: "setShareModeEnabled toggles value",
      fn: testSetShareModeEnabledToggles,
    },
    {
      name: "setShareModeEnabled works with callback",
      fn: testSetShareModeEnabledWithCallback,
    },
    {
      name: "BrowserShareMode class exists",
      fn: testBrowserShareModeClassExists,
    },
    {
      name: "BrowserShareMode has init method",
      fn: testBrowserShareModeHasInitMethod,
    },
  ]);
}
