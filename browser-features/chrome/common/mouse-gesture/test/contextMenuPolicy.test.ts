// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { assertEquals, runTests } from "../../../test/utils/test_harness.ts";
import { handleContextMenuAfterMouseUp } from "../context-menu-policy.ts";

function testContextMenuTiming(): void {
  const pref = "ui.context_menus.after_mouseup";
  const legacyPref = "floorp.mousegesture.last_enabled_state";
  const saved = [pref, legacyPref].map((name) => ({
    name,
    value: Services.prefs.prefHasUserValue(name)
      ? Services.prefs.getBoolPref(name)
      : null,
  }));
  try {
    for (const os of ["Darwin", "Linux"]) {
      Services.prefs.setBoolPref(legacyPref, true);
      Services.prefs.setBoolPref(pref, false);
      handleContextMenuAfterMouseUp(true, os);
      assertEquals(
        Services.prefs.getBoolPref(pref),
        true,
        `${os}: repair stale startup state`,
      );
      Services.prefs.clearUserPref(pref);
      handleContextMenuAfterMouseUp(true, os);
      assertEquals(
        Services.prefs.getBoolPref(pref),
        true,
        `${os}: repair a reset pref`,
      );
      handleContextMenuAfterMouseUp(false, os);
      assertEquals(
        Services.prefs.getBoolPref(pref),
        false,
        `${os}: restore press timing when disabled`,
      );
      handleContextMenuAfterMouseUp(true, os);
      assertEquals(
        Services.prefs.getBoolPref(pref),
        true,
        `${os}: re-enable release timing`,
      );
    }
    for (const enabled of [true, false]) {
      Services.prefs.clearUserPref(pref);
      handleContextMenuAfterMouseUp(enabled, "WINNT");
      assertEquals(
        Services.prefs.prefHasUserValue(pref),
        false,
        "Windows must not write the timing pref",
      );
    }
  } finally {
    for (const { name, value } of saved) {
      if (value === null) Services.prefs.clearUserPref(name);
      else Services.prefs.setBoolPref(name, value);
    }
  }
}

function testLockedContextMenuTiming(): void {
  const pref = "ui.context_menus.after_mouseup";
  const wasLocked = Services.prefs.prefIsLocked(pref);
  const defaults = Services.prefs.getDefaultBranch("");
  const savedDefault = defaults.getBoolPref(pref);
  let savedUser: boolean | null = null;
  const originalError = console.error;
  const errors: unknown[][] = [];
  try {
    Services.prefs.unlockPref(pref);
    // Read the user value after unlocking: a policy lock hides it.
    savedUser = Services.prefs.prefHasUserValue(pref)
      ? Services.prefs.getBoolPref(pref)
      : null;
    defaults.setBoolPref(pref, false);
    Services.prefs.setBoolPref(pref, false);
    Services.prefs.lockPref(pref);
    console.error = (...args: unknown[]) => {
      errors.push(args);
    };
    handleContextMenuAfterMouseUp(true, "Darwin");
    assertEquals(
      Services.prefs.getBoolPref(pref),
      false,
      "policy-controlled timing remains unchanged",
    );
    assertEquals(
      Services.prefs.prefIsLocked(pref),
      true,
      "the helper must not unlock policy",
    );
    Services.prefs.unlockPref(pref);
    assertEquals(
      Services.prefs.getBoolPref(pref),
      false,
      "do not defer a hidden override until policy unlock",
    );
    assertEquals(
      String(errors[0]?.[0]).startsWith("[MouseGestureService]"),
      true,
      "report the failed timing update without aborting initialization",
    );
    handleContextMenuAfterMouseUp(true, "Darwin");
    assertEquals(
      Services.prefs.getBoolPref(pref),
      true,
      "a later unlocked update can succeed",
    );
  } finally {
    console.error = originalError;
    Services.prefs.unlockPref(pref);
    defaults.setBoolPref(pref, savedDefault);
    if (savedUser === null) Services.prefs.clearUserPref(pref);
    else Services.prefs.setBoolPref(pref, savedUser);
    if (wasLocked) Services.prefs.lockPref(pref);
  }
}

function testFailedContextMenuTimingWrite(): void {
  const originalError = console.error;
  const errors: unknown[][] = [];
  const failure = new Error("preference storage unavailable");
  let writes = 0;
  try {
    console.error = (...args: unknown[]) => {
      errors.push(args);
    };
    handleContextMenuAfterMouseUp(true, "Darwin", {
      prefIsLocked: () => false,
      getBoolPref: () => false,
      setBoolPref: () => {
        writes++;
        throw failure;
      },
    });
    assertEquals(writes, 1, "exercise an actual failed write attempt");
    assertEquals(
      errors.length,
      1,
      "report the write failure once without throwing",
    );
    assertEquals(
      String(errors[0][0]).startsWith("[MouseGestureService]"),
      true,
      "use the feature log prefix",
    );
    assertEquals(
      errors[0][1],
      failure,
      "retain the underlying failure for diagnosis",
    );
  } finally {
    console.error = originalError;
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("contextMenuPolicy.test.ts", [
    {
      name: "native menu timing follows actual gesture state",
      fn: testContextMenuTiming,
    },
    {
      name: "locked native menu timing does not abort initialization",
      fn: testLockedContextMenuTiming,
    },
    {
      name: "native menu timing write failure is contained",
      fn: testFailedContextMenuTimingWrite,
    },
  ]);
}
