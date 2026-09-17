// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import { config, deepMerge, migrateDesignConfig } from "../configs.ts";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";

export async function runAllTests() {
  await runTests("design config migration", [
    {
      name: "Preserves old true and false values before applying defaults",
      fn: () => {
        for (const enabled of [true, false]) {
          const saved = Object.freeze({
            tab: Object.freeze({
              tabDubleClickToClose: enabled,
              tabMinWidth: 180,
            }),
            other: "keep",
          });
          const migrated = deepMerge({
            tab: { tabDoubleClickToClose: !enabled, tabMinWidth: 76 },
          }, migrateDesignConfig(saved));
          assertEquals(
            migrated.tab.tabDoubleClickToClose,
            enabled,
            "Saved value wins over defaults",
          );
          assertEquals(migrated.tab.tabMinWidth, 180, "Other settings survive");
          assert(
            !Object.hasOwn(migrated.tab, "tabDubleClickToClose"),
            "Old key is removed",
          );
          assertEquals(
            JSON.stringify(migrateDesignConfig(migrated)),
            JSON.stringify(migrated),
            "Migration is idempotent",
          );
        }
      },
    },
    {
      name: "Prefers a valid new value when both keys exist",
      fn: () => {
        for (const enabled of [true, false]) {
          const migrated = deepMerge(
            { tab: { tabDoubleClickToClose: !enabled } },
            migrateDesignConfig({
              tab: {
                tabDoubleClickToClose: enabled,
                tabDubleClickToClose: !enabled,
              },
            }),
          );
          assertEquals(
            migrated.tab.tabDoubleClickToClose,
            enabled,
            "New value wins",
          );
        }
      },
    },
    {
      name: "Does not coerce invalid legacy values",
      fn: () => {
        for (const value of ["true", "false", 0, 1, null, undefined]) {
          const migrated = deepMerge(
            { tab: { tabDoubleClickToClose: false } },
            migrateDesignConfig({ tab: { tabDubleClickToClose: value } }),
          );
          assertEquals(
            migrated.tab.tabDoubleClickToClose,
            false,
            "Invalid legacy value should use default",
          );
        }
        for (const value of [null, [], "broken", { tab: null }]) {
          assertEquals(
            migrateDesignConfig(value),
            value,
            "Malformed input remains available for normal validation",
          );
        }
      },
    },
    {
      name: "Preference changes migrate and persist the corrected key",
      fn: async () => {
        const name = "floorp.design.configs";
        const original = Services.prefs.getStringPref(name);
        try {
          for (const enabled of [true, false]) {
            const saved = JSON.parse(original);
            delete saved.tab.tabDoubleClickToClose;
            saved.tab.tabDubleClickToClose = enabled;
            Services.prefs.setStringPref(name, JSON.stringify(saved));
            await new Promise((resolve) => setTimeout(resolve, 30));
            assertEquals(
              config().tab.tabDoubleClickToClose,
              enabled,
              "Observer loaded legacy value",
            );
            const persisted = JSON.parse(Services.prefs.getStringPref(name));
            assertEquals(
              persisted.tab.tabDoubleClickToClose,
              enabled,
              "Corrected value persisted",
            );
            assert(
              !Object.hasOwn(persisted.tab, "tabDubleClickToClose"),
              "Old key is removed from storage",
            );
          }
        } finally {
          Services.prefs.setStringPref(name, original);
        }
      },
    },
  ]);
}
