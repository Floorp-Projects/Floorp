// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { StatusBarManager } from "../statusbar-manager.tsx";

import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

function testStatusBarManagerConstruction(): void {
  const manager = new StatusBarManager();
  assert(manager !== null, "StatusBarManager should be constructable");
  assert(
    typeof manager.showStatusBar === "function",
    "showStatusBar should be a signal accessor function",
  );
  assert(
    typeof manager.setShowStatusBar === "function",
    "setShowStatusBar should be a signal setter function",
  );
}

function testShowStatusBarReflectsPref(): void {
  const prefKey = "noraneko.statusbar.enable";
  const originalValue = Services.prefs.getBoolPref(prefKey, false);

  try {
    Services.prefs.setBoolPref(prefKey, false);
    const manager = new StatusBarManager();
    assertEquals(
      manager.showStatusBar(),
      false,
      "showStatusBar should reflect pref value (false)",
    );

    Services.prefs.setBoolPref(prefKey, true);
    // The manager observes the pref, so the signal should update
    assertEquals(
      manager.showStatusBar(),
      true,
      "showStatusBar should reflect pref value (true) after change",
    );
  } finally {
    Services.prefs.setBoolPref(prefKey, originalValue);
  }
}

function testSetShowStatusBarUpdatesPref(): void {
  const prefKey = "noraneko.statusbar.enable";
  const originalValue = Services.prefs.getBoolPref(prefKey, false);

  try {
    Services.prefs.setBoolPref(prefKey, false);
    const manager = new StatusBarManager();
    manager.setShowStatusBar(true);
    // The createEffect in the constructor should sync the signal to the pref
    const prefValue = Services.prefs.getBoolPref(prefKey, false);
    assertEquals(
      prefValue,
      true,
      "Pref should be updated after setShowStatusBar(true)",
    );
  } finally {
    Services.prefs.setBoolPref(prefKey, originalValue);
  }
}

function testGlobalFloorpStatusBarBinding(): void {
  const _manager = new StatusBarManager();
  assert(
    globalThis.gFloorp?.statusBar !== undefined,
    "gFloorp.statusBar should be defined after construction",
  );
  assert(
    typeof globalThis.gFloorp.statusBar.setShow === "function",
    "gFloorp.statusBar.setShow should be a function",
  );
}

export async function runAllTests(): Promise<void> {
  await runTests("statusbar.test.ts", [
    {
      name: "StatusBarManager construction",
      fn: testStatusBarManagerConstruction,
    },
    { name: "showStatusBar reflects pref", fn: testShowStatusBarReflectsPref },
    {
      name: "setShowStatusBar updates pref",
      fn: testSetShowStatusBarUpdatesPref,
    },
    { name: "gFloorp.statusBar binding", fn: testGlobalFloorpStatusBarBinding },
  ]);
}
