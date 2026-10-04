// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { effect } from "@preact/signals";
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

async function testConfigStorePartialUpdatesAndPreferenceSync(): Promise<void> {
  const original = { ...configStore };
  const originalPref = Services.prefs.getStringPref(
    WORKSPACED_CONFIG_PREF_NAME,
  );
  let observed = configStore.closePopupAfterClick;
  const dispose = effect(() => {
    observed = configStore.closePopupAfterClick;
  });
  try {
    assertEquals(
      Object.keys(original).length,
      4,
      "all config properties remain enumerable",
    );
    setConfigStore({ closePopupAfterClick: !original.closePopupAfterClick });
    assertEquals(
      observed,
      !original.closePopupAfterClick,
      "object updates notify signal consumers",
    );
    assertEquals(
      configStore.manageOnBms,
      original.manageOnBms,
      "partial updates preserve unrelated fields",
    );
    setConfigStore("closePopupAfterClick", (previous) => !previous);
    assertEquals(
      observed,
      original.closePopupAfterClick,
      "field updater receives current value",
    );
    setConfigStore((previous) => ({
      closePopupAfterClick: !previous.closePopupAfterClick,
    }));
    assertEquals(
      observed,
      !original.closePopupAfterClick,
      "object updater remains supported",
    );
    Services.prefs.setStringPref(
      WORKSPACED_CONFIG_PREF_NAME,
      JSON.stringify(original),
    );
    await Promise.resolve();
    assertEquals(
      observed,
      original.closePopupAfterClick,
      "external preference updates notify consumers",
    );
    assertEquals(
      JSON.stringify(
        JSON.parse(Services.prefs.getStringPref(WORKSPACED_CONFIG_PREF_NAME)),
      ),
      JSON.stringify(original),
      "preference synchronization preserves every field",
    );
  } finally {
    dispose();
    setConfigStore(original);
    Services.prefs.setStringPref(WORKSPACED_CONFIG_PREF_NAME, originalPref);
  }
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name:
        "config API preserves partial updates and live preference synchronization",
      fn: testConfigStorePartialUpdatesAndPreferenceSync,
    },
    {
      name: "config-store update persists to workspace config pref",
      fn: testConfigStoreUpdatePersistsToPref,
    },
  ];

  await runTests("configPersistence.test.ts", tests);
}
