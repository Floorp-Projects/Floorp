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

export async function runAllTests(): Promise<void> {
  await runTests("contextMenuPolicy.test.ts", [
    {
      name: "native menu timing follows actual gesture state",
      fn: testContextMenuTiming,
    },
  ]);
}
