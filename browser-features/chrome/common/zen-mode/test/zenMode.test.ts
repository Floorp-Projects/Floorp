// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  zenModeEnabled,
  setZenModeEnabled,
  initZenModeState,
} from "../zen-mode.tsx";

import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

const ZENMODE_PREF = "floorp.zenmode.enabled";

function testZenModeSignalReadable(): void {
  const value = zenModeEnabled();
  assert(typeof value === "boolean", "zenModeEnabled should return a boolean");
}

function testSetZenModeEnabledToggles(): void {
  const original = zenModeEnabled();
  try {
    setZenModeEnabled(true);
    assertEquals(
      zenModeEnabled(),
      true,
      "zenModeEnabled should be true after setting true",
    );

    setZenModeEnabled(false);
    assertEquals(
      zenModeEnabled(),
      false,
      "zenModeEnabled should be false after setting false",
    );
  } finally {
    setZenModeEnabled(original);
  }
}

function testZenModePrefSync(): void {
  const originalPref = Services.prefs.getBoolPref(ZENMODE_PREF, false);
  const originalSignal = zenModeEnabled();

  try {
    initZenModeState();

    setZenModeEnabled(true);
    assertEquals(
      Services.prefs.getBoolPref(ZENMODE_PREF, false),
      true,
      "Pref should be true after setZenModeEnabled(true)",
    );

    setZenModeEnabled(false);
    assertEquals(
      Services.prefs.getBoolPref(ZENMODE_PREF, false),
      false,
      "Pref should be false after setZenModeEnabled(false)",
    );
  } finally {
    setZenModeEnabled(originalSignal);
    Services.prefs.setBoolPref(ZENMODE_PREF, originalPref);
  }
}

function testZenModeAttributeOnDocumentElement(): void {
  const originalPref = Services.prefs.getBoolPref(ZENMODE_PREF, false);
  const originalSignal = zenModeEnabled();

  try {
    initZenModeState();

    setZenModeEnabled(true);
    assert(
      document?.documentElement?.hasAttribute("zenmode"),
      "documentElement should have 'zenmode' attribute when zen mode is enabled",
    );

    setZenModeEnabled(false);
    assert(
      !document?.documentElement?.hasAttribute("zenmode"),
      "documentElement should not have 'zenmode' attribute when zen mode is disabled",
    );
  } finally {
    setZenModeEnabled(originalSignal);
    Services.prefs.setBoolPref(ZENMODE_PREF, originalPref);
    document?.documentElement?.removeAttribute("zenmode");
    document?.documentElement?.removeAttribute("zenmode-reveal-top");
    document?.documentElement?.removeAttribute("zenmode-reveal-bottom");
    document?.documentElement?.removeAttribute("zenmode-reveal-side");
  }
}

function testZenModePrefObserverUpdatesSignal(): void {
  const originalPref = Services.prefs.getBoolPref(ZENMODE_PREF, false);
  const originalSignal = zenModeEnabled();

  try {
    initZenModeState();

    Services.prefs.setBoolPref(ZENMODE_PREF, true);
    assertEquals(
      zenModeEnabled(),
      true,
      "Signal should be true after pref set to true externally",
    );

    Services.prefs.setBoolPref(ZENMODE_PREF, false);
    assertEquals(
      zenModeEnabled(),
      false,
      "Signal should be false after pref set to false externally",
    );
  } finally {
    setZenModeEnabled(originalSignal);
    Services.prefs.setBoolPref(ZENMODE_PREF, originalPref);
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("zenMode.test.ts", [
    {
      name: "zenModeEnabled signal is readable",
      fn: testZenModeSignalReadable,
    },
    {
      name: "setZenModeEnabled toggles value",
      fn: testSetZenModeEnabledToggles,
    },
    { name: "zen mode pref sync", fn: testZenModePrefSync },
    {
      name: "zen mode attribute on documentElement",
      fn: testZenModeAttributeOnDocumentElement,
    },
    {
      name: "pref observer updates signal",
      fn: testZenModePrefObserverUpdatesSignal,
    },
  ]);
}
