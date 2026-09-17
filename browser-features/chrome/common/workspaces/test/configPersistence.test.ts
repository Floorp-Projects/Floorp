// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { configStore, setConfigStore } from "../data/config.ts";
import { WORKSPACED_CONFIG_PREF_NAME } from "../utils/workspaces-static-names.ts";
import {
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

async function testConfigStoreUpdatePersistsToPref(): Promise<void> {
  const originalValue = configStore.closePopupAfterClick;
  const originalPref = Services.prefs.getStringPref(
    WORKSPACED_CONFIG_PREF_NAME,
  );
  const updatedValue = !originalValue;

  try {
    setConfigStore("closePopupAfterClick", updatedValue);
    await Promise.resolve();

    const persisted = JSON.parse(
      Services.prefs.getStringPref(WORKSPACED_CONFIG_PREF_NAME),
    ) as Record<string, unknown>;

    assertEquals(
      persisted.closePopupAfterClick,
      updatedValue,
      "setConfigStore update should persist to floorp.workspaces.v4.config",
    );
  } finally {
    setConfigStore("closePopupAfterClick", originalValue);
    await Promise.resolve();
    Services.prefs.setStringPref(WORKSPACED_CONFIG_PREF_NAME, originalPref);
  }
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "config-store update persists to workspace config pref",
      fn: testConfigStoreUpdatePersistsToPref,
    },
  ];

  await runTests("configPersistence.test.ts", tests);
}
