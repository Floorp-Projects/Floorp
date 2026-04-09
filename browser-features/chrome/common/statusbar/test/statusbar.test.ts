// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { StatusBarManager } from "../statusbar-manager.tsx";

import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

const PREF_KEY = "noraneko.statusbar.enable";

function withPrefRestore(run: () => void): void {
  const originalValue = Services.prefs.getBoolPref(PREF_KEY, false);
  try {
    run();
  } finally {
    Services.prefs.setBoolPref(PREF_KEY, originalValue);
  }
}

// ---------------------------------------------------------------------------
// Tests – Construction & API shape
// ---------------------------------------------------------------------------

function testStatusBarManagerConstruction(): void {
  withPrefRestore(() => {
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
  });
}

function testInitialSignalMatchesPref(): void {
  withPrefRestore(() => {
    Services.prefs.setBoolPref(PREF_KEY, false);
    const managerOff = new StatusBarManager();
    assertEquals(
      managerOff.showStatusBar(),
      false,
      "signal should be false when pref is false",
    );

    Services.prefs.setBoolPref(PREF_KEY, true);
    const managerOn = new StatusBarManager();
    assertEquals(
      managerOn.showStatusBar(),
      true,
      "signal should be true when pref is true",
    );
  });
}

// ---------------------------------------------------------------------------
// Tests – Pref ↔ Signal synchronization
// ---------------------------------------------------------------------------

function testShowStatusBarReflectsPref(): void {
  withPrefRestore(() => {
    Services.prefs.setBoolPref(PREF_KEY, false);
    const manager = new StatusBarManager();
    assertEquals(
      manager.showStatusBar(),
      false,
      "showStatusBar should reflect pref value (false)",
    );

    Services.prefs.setBoolPref(PREF_KEY, true);
    // The manager observes the pref, so the signal should update
    assertEquals(
      manager.showStatusBar(),
      true,
      "showStatusBar should reflect pref value (true) after change",
    );
  });
}

function testSetShowStatusBarUpdatesPref(): void {
  withPrefRestore(() => {
    Services.prefs.setBoolPref(PREF_KEY, false);
    const manager = new StatusBarManager();
    manager.setShowStatusBar(true);
    // The createEffect in the constructor should sync the signal to the pref
    const prefValue = Services.prefs.getBoolPref(PREF_KEY, false);
    assertEquals(
      prefValue,
      true,
      "Pref should be updated after setShowStatusBar(true)",
    );
  });
}

function testSetShowStatusBarFalseUpdatesPref(): void {
  withPrefRestore(() => {
    Services.prefs.setBoolPref(PREF_KEY, true);
    const manager = new StatusBarManager();
    manager.setShowStatusBar(false);
    assertEquals(
      Services.prefs.getBoolPref(PREF_KEY, true),
      false,
      "Pref should be updated after setShowStatusBar(false)",
    );
  });
}

function testSignalRoundTrip(): void {
  withPrefRestore(() => {
    Services.prefs.setBoolPref(PREF_KEY, false);
    const manager = new StatusBarManager();

    // true → false → true round trip
    manager.setShowStatusBar(true);
    assertEquals(manager.showStatusBar(), true, "signal should be true");
    assertEquals(
      Services.prefs.getBoolPref(PREF_KEY, false),
      true,
      "pref should be true",
    );

    manager.setShowStatusBar(false);
    assertEquals(manager.showStatusBar(), false, "signal should be false");
    assertEquals(
      Services.prefs.getBoolPref(PREF_KEY, true),
      false,
      "pref should be false",
    );

    manager.setShowStatusBar(true);
    assertEquals(manager.showStatusBar(), true, "signal should be true again");
    assertEquals(
      Services.prefs.getBoolPref(PREF_KEY, false),
      true,
      "pref should be true again",
    );
  });
}

// ---------------------------------------------------------------------------
// Tests – global gFloorp binding
// ---------------------------------------------------------------------------

function testGlobalFloorpStatusBarBinding(): void {
  withPrefRestore(() => {
    const _manager = new StatusBarManager();
    assert(
      globalThis.gFloorp?.statusBar !== undefined,
      "gFloorp.statusBar should be defined after construction",
    );
    assert(
      typeof globalThis.gFloorp.statusBar.setShow === "function",
      "gFloorp.statusBar.setShow should be a function",
    );
  });
}

function testGlobalSetShowUpdatesSignal(): void {
  withPrefRestore(() => {
    Services.prefs.setBoolPref(PREF_KEY, false);
    const manager = new StatusBarManager();

    globalThis.gFloorp?.statusBar?.setShow(true);
    assertEquals(
      manager.showStatusBar(),
      true,
      "gFloorp.statusBar.setShow(true) should update signal",
    );

    globalThis.gFloorp?.statusBar?.setShow(false);
    assertEquals(
      manager.showStatusBar(),
      false,
      "gFloorp.statusBar.setShow(false) should update signal",
    );
  });
}

function testLatestManagerWinsGlobalBinding(): void {
  withPrefRestore(() => {
    Services.prefs.setBoolPref(PREF_KEY, false);
    const _manager1 = new StatusBarManager();
    const manager2 = new StatusBarManager();

    // The global binding should point to the latest manager
    globalThis.gFloorp?.statusBar?.setShow(true);
    assertEquals(
      manager2.showStatusBar(),
      true,
      "latest manager should respond to global setShow",
    );
  });
}

// ---------------------------------------------------------------------------
// Tests – init() safety
// ---------------------------------------------------------------------------

function testInitDoesNotThrowWhenStatusbarNodeMissing(): void {
  withPrefRestore(() => {
    const manager = new StatusBarManager();
    const original = document?.getElementById(
      "nora-statusbar",
    ) as HTMLElement | null;
    const parent = original?.parentNode ?? null;
    const nextSibling = original?.nextSibling ?? null;

    try {
      original?.remove();

      let threw = false;
      try {
        manager.init();
      } catch {
        threw = true;
      }

      assert(
        !threw,
        "init should not throw even if #nora-statusbar is missing",
      );
    } finally {
      if (original && parent) {
        parent.insertBefore(original, nextSibling);
      }
    }
  });
}

function testInitDoesNotThrowWhenAppContentMissing(): void {
  withPrefRestore(() => {
    const manager = new StatusBarManager();

    let threw = false;
    try {
      manager.init();
    } catch {
      threw = true;
    }
    // Even if #appcontent is missing, init should not throw
    assert(!threw, "init should not throw when DOM elements are missing");
  });
}

function testInitIsIdempotent(): void {
  withPrefRestore(() => {
    const manager = new StatusBarManager();

    let threw = false;
    try {
      manager.init();
      manager.init();
    } catch {
      threw = true;
    }
    assert(!threw, "calling init() multiple times should not throw");
  });
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

export async function runAllTests(): Promise<void> {
  await runTests("statusbar.test.ts", [
    // Construction & API shape
    {
      name: "StatusBarManager construction",
      fn: testStatusBarManagerConstruction,
    },
    {
      name: "initial signal matches pref",
      fn: testInitialSignalMatchesPref,
    },

    // Pref ↔ Signal synchronization
    { name: "showStatusBar reflects pref", fn: testShowStatusBarReflectsPref },
    {
      name: "setShowStatusBar updates pref (true)",
      fn: testSetShowStatusBarUpdatesPref,
    },
    {
      name: "setShowStatusBar updates pref (false)",
      fn: testSetShowStatusBarFalseUpdatesPref,
    },
    { name: "signal round-trip", fn: testSignalRoundTrip },

    // gFloorp global binding
    { name: "gFloorp.statusBar binding", fn: testGlobalFloorpStatusBarBinding },
    {
      name: "gFloorp.statusBar.setShow updates signal",
      fn: testGlobalSetShowUpdatesSignal,
    },
    {
      name: "latest manager wins global binding",
      fn: testLatestManagerWinsGlobalBinding,
    },

    // init() safety
    {
      name: "init is safe when statusbar node is missing",
      fn: testInitDoesNotThrowWhenStatusbarNodeMissing,
    },
    {
      name: "init is safe when #appcontent is missing",
      fn: testInitDoesNotThrowWhenAppContentMissing,
    },
    {
      name: "init is idempotent",
      fn: testInitIsIdempotent,
    },
  ]);
}
