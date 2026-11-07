// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { PwaConfig } from "../config.ts";
import type { PwaConfigTestInstance } from "./types.ts";
import { assertEquals, runTests } from "../../../test/utils/test_harness.ts";

const ENABLED_PREF = "floorp.browser.ssb.enabled";
const CONFIG_PREF = "floorp.browser.ssb.config";
// The production singleton keeps its private constructor. Exercise that same
// constructor with a test-owned lifetime, using the real preference service.
const TestPwaConfig = PwaConfig as unknown as {
  new (lifecycleTarget: EventTarget): PwaConfigTestInstance;
};

function testConfigStopsObservingAfterOwnerUnload(): void {
  const hadEnabled = Services.prefs.prefHasUserValue(ENABLED_PREF);
  const previousEnabled = Services.prefs.getBoolPref(ENABLED_PREF, true);
  const hadConfig = Services.prefs.prefHasUserValue(CONFIG_PREF);
  const previousConfig = Services.prefs.getStringPref(CONFIG_PREF, "{}");
  const targets: EventTarget[] = [];
  const close = (target: EventTarget) =>
    target.dispatchEvent(new Event("unload"));
  try {
    for (let cycle = 0; cycle < 3; cycle++) {
      Services.prefs.setBoolPref(ENABLED_PREF, false);
      Services.prefs.setStringPref(CONFIG_PREF, '{"showToolbar":false}');
      const target = new EventTarget();
      targets.push(target);
      const state = new TestPwaConfig(target);
      assertEquals(
        state.enabled[0](),
        false,
        "constructor loads saved disabled",
      );
      assertEquals(
        state.config[0]().showToolbar,
        false,
        "constructor loads saved toolbar choice",
      );
      Services.prefs.setBoolPref(ENABLED_PREF, true);
      Services.prefs.setStringPref(CONFIG_PREF, '{"showToolbar":true}');
      assertEquals(state.enabled[0](), true, "live owner follows enabled pref");
      assertEquals(
        state.config[0]().showToolbar,
        true,
        "live owner follows configuration pref",
      );

      close(target);
      close(target);
      Services.prefs.setBoolPref(ENABLED_PREF, false);
      Services.prefs.setStringPref(CONFIG_PREF, '{"showToolbar":false}');
      assertEquals(
        state.enabled[0](),
        true,
        "closed owner no longer follows enabled changes",
      );
      assertEquals(
        state.config[0]().showToolbar,
        true,
        "closed owner no longer follows configuration changes",
      );
    }
  } finally {
    for (const target of targets) close(target);
    if (hadEnabled) {
      Services.prefs.setBoolPref(ENABLED_PREF, previousEnabled);
    } else {
      Services.prefs.clearUserPref(ENABLED_PREF);
    }
    if (hadConfig) {
      Services.prefs.setStringPref(CONFIG_PREF, previousConfig);
    } else {
      Services.prefs.clearUserPref(CONFIG_PREF);
    }
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("configLifecycle.test.ts", [{
    name:
      "real config constructor releases both pref observers across three owner lifetimes",
    fn: testConfigStopsObservingAfterOwnerUnload,
  }]);
}
