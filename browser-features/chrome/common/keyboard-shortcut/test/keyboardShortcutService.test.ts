// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";
import { KeyboardShortcutService } from "../service.ts";
import {
  KEYBOARD_SHORTCUT_CONFIG_PREF,
  KEYBOARD_SHORTCUT_ENABLED_PREF,
  setConfig,
  setEnabled,
} from "../config.ts";
import type { KeyboardShortcutConfig } from "../type.ts";

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

function withPrefs(fn: () => void): void {
  const hadEnabled = Services.prefs.prefHasUserValue(
    KEYBOARD_SHORTCUT_ENABLED_PREF,
  );
  const hadConfig = Services.prefs.prefHasUserValue(
    KEYBOARD_SHORTCUT_CONFIG_PREF,
  );
  const savedEnabled = hadEnabled
    ? Services.prefs.getBoolPref(KEYBOARD_SHORTCUT_ENABLED_PREF)
    : null;
  const savedConfig = hadConfig
    ? Services.prefs.getStringPref(KEYBOARD_SHORTCUT_CONFIG_PREF)
    : null;

  try {
    fn();
  } finally {
    if (hadEnabled && savedEnabled !== null) {
      Services.prefs.setBoolPref(KEYBOARD_SHORTCUT_ENABLED_PREF, savedEnabled);
    } else {
      Services.prefs.clearUserPref(KEYBOARD_SHORTCUT_ENABLED_PREF);
    }
    if (hadConfig && savedConfig !== null) {
      Services.prefs.setStringPref(KEYBOARD_SHORTCUT_CONFIG_PREF, savedConfig);
    } else {
      Services.prefs.clearUserPref(KEYBOARD_SHORTCUT_CONFIG_PREF);
    }
  }
}

/** Apply an enabled test config and reset pref state. */
function applyTestConfig(config: KeyboardShortcutConfig): void {
  Services.prefs.setBoolPref(KEYBOARD_SHORTCUT_ENABLED_PREF, true);
  Services.prefs.setStringPref(
    KEYBOARD_SHORTCUT_CONFIG_PREF,
    JSON.stringify(config),
  );
  setEnabled(true);
  setConfig(config);
}

/**
 * Create a fake browser window (EventTarget) that supports addEventListener,
 * dispatchEvent, and the `__keyboardShortcutControllerAttached` marker.
 */
function createFakeWindow(): Window {
  return new EventTarget() as unknown as Window;
}

function isAttached(win: Window): boolean {
  // deno-lint-ignore no-explicit-any
  return (win as any).__keyboardShortcutControllerAttached === true;
}

const TEST_CONFIG: KeyboardShortcutConfig = {
  enabled: true,
  shortcuts: {
    "test-ctrl-t": {
      key: "T",
      modifiers: { alt: false, ctrl: true, meta: false, shift: false },
      action: "test-ctrl-t",
    },
  },
};

// ---------------------------------------------------------------------------
// Tests — attachToWindow
// ---------------------------------------------------------------------------

function testAttachSetsMarker(): void {
  withPrefs(() => {
    applyTestConfig(TEST_CONFIG);
    const win = createFakeWindow();
    const service = new KeyboardShortcutService(null);

    assertEquals(isAttached(win), false, "marker absent before attach");
    service.attachToWindow(win);
    assertEquals(
      isAttached(win),
      true,
      "attachToWindow should set the __keyboardShortcutControllerAttached marker",
    );
  });
}

function testDuplicateAttachIgnored(): void {
  withPrefs(() => {
    applyTestConfig(TEST_CONFIG);
    const win = createFakeWindow();
    const service = new KeyboardShortcutService(null);

    service.attachToWindow(win);
    // Second call should be a no-op because the marker is already set.
    service.attachToWindow(win);
    assertEquals(
      isAttached(win),
      true,
      "duplicate attach should not unattach or corrupt state",
    );
  });
}

function testAttachWhenDisabledIsNoOp(): void {
  withPrefs(() => {
    applyTestConfig(TEST_CONFIG);
    Services.prefs.setBoolPref(KEYBOARD_SHORTCUT_ENABLED_PREF, false);
    setEnabled(false);
    const win = createFakeWindow();
    const service = new KeyboardShortcutService(null);

    service.attachToWindow(win);
    assertEquals(
      isAttached(win),
      false,
      "attachToWindow should not attach when shortcuts are disabled",
    );
  });
}

// ---------------------------------------------------------------------------
// Tests — setEnabled lifecycle
// ---------------------------------------------------------------------------

function testDisableClearsMarkers(): void {
  withPrefs(() => {
    applyTestConfig(TEST_CONFIG);
    const winA = createFakeWindow();
    const winB = createFakeWindow();
    const service = new KeyboardShortcutService(null);
    service.attachToWindow(winA);
    service.attachToWindow(winB);

    assertEquals(isAttached(winA), true, "winA should be attached");
    assertEquals(isAttached(winB), true, "winB should be attached");

    service.setEnabled(false);

    assertEquals(
      isAttached(winA),
      false,
      "setEnabled(false) should clear the marker on window A",
    );
    assertEquals(
      isAttached(winB),
      false,
      "setEnabled(false) should clear the marker on window B",
    );
  });
}

function testReEnableReattaches(): void {
  withPrefs(() => {
    applyTestConfig(TEST_CONFIG);
    // attachToAllWindows() enumerates real navigator:browser windows via
    // Services.wm, so the reattach path requires a real window.
    const win = Services.wm.getMostRecentWindow("navigator:browser") as Window;
    assert(
      win !== null && win !== undefined,
      "test browser must have at least one navigator:browser window",
    );
    const service = new KeyboardShortcutService(null);

    try {
      service.attachToWindow(win);

      service.setEnabled(false);
      assertEquals(
        isAttached(win),
        false,
        "win should be detached after disable",
      );

      service.setEnabled(true);
      assertEquals(
        isAttached(win),
        true,
        "setEnabled(true) should re-attach controllers to real browser windows",
      );
    } finally {
      service.setEnabled(false);
    }
  });
}


// ---------------------------------------------------------------------------
// Tests — updateConfig lifecycle
// ---------------------------------------------------------------------------

function testUpdateConfigDestroysAndReattaches(): void {
  withPrefs(() => {
    applyTestConfig(TEST_CONFIG);
    const win = Services.wm.getMostRecentWindow("navigator:browser") as Window;
    assert(
      win !== null && win !== undefined,
      "test browser must have at least one navigator:browser window",
    );
    const service = new KeyboardShortcutService(null);

    try {
      service.attachToWindow(win);

      const newConfig: KeyboardShortcutConfig = {
        enabled: true,
        shortcuts: {
          "test-alt-shift-f": {
            key: "F",
            modifiers: { alt: true, ctrl: false, meta: false, shift: true },
            action: "test-alt-shift-f",
          },
        },
      };
      service.updateConfig(newConfig);

      assertEquals(
        isAttached(win),
        true,
        "updateConfig should re-attach controllers when still enabled",
      );
    } finally {
      service.setEnabled(false);
    }
  });
}

function testUpdateConfigWhenDisabledDoesNotReattach(): void {
  withPrefs(() => {
    applyTestConfig(TEST_CONFIG);
    const win = createFakeWindow();
    const service = new KeyboardShortcutService(null);
    service.attachToWindow(win);

    service.setEnabled(false);
    assertEquals(
      isAttached(win),
      false,
      "win should remain detached after updateConfig",
    );

    const newConfig: KeyboardShortcutConfig = {
      enabled: true,
      shortcuts: {},
    };
    service.updateConfig(newConfig);

    assertEquals(
      isAttached(win),
      false,
      "updateConfig should not re-attach when disabled",
    );
  });
}

// ---------------------------------------------------------------------------
// Tests — window unload cleanup
// ---------------------------------------------------------------------------

function testWindowUnloadCleansUp(): void {
  withPrefs(() => {
    applyTestConfig(TEST_CONFIG);
    const win = createFakeWindow();
    const service = new KeyboardShortcutService(null);
    service.attachToWindow(win);
    assertEquals(isAttached(win), true, "win should be attached before unload");

    // Dispatch the unload event; the { once: true } listener should clean up.
    win.dispatchEvent(new Event("unload"));

    assertEquals(
      isAttached(win),
      false,
      "unload event should remove the controller and clean up the marker",
    );
  });
}

// ---------------------------------------------------------------------------
// Test runner
// ---------------------------------------------------------------------------

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    // attachToWindow
    { name: "attach sets marker", fn: testAttachSetsMarker },
    { name: "duplicate attach ignored", fn: testDuplicateAttachIgnored },
    { name: "attach when disabled is no-op", fn: testAttachWhenDisabledIsNoOp },
    // setEnabled lifecycle
    { name: "disable clears markers", fn: testDisableClearsMarkers },
    { name: "re-enable reattaches", fn: testReEnableReattaches },
    // updateConfig lifecycle
    {
      name: "updateConfig destroys and reattaches",
      fn: testUpdateConfigDestroysAndReattaches,
    },
    {
      name: "updateConfig when disabled does not reattach",
      fn: testUpdateConfigWhenDisabledDoesNotReattach,
    },
    // window unload cleanup
    { name: "window unload cleans up", fn: testWindowUnloadCleansUp },
  ];

  await runTests("keyboardShortcutService.test.ts", tests);
}
