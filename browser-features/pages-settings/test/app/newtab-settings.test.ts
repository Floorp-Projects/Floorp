// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import { createInstance } from "i18next";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../chrome/test/utils/test_harness.ts";
import {
  parseStartDisabled,
  patchStartDisabled,
  setStartDisabled,
} from "../../src/app/newtab/design-setting.ts";
import {
  NEWTAB_FIELDS,
  resolveNewTabTab,
} from "../../src/app/newtab/fields.ts";
import { buildSearchDocuments } from "../../src/lib/search/index.tsx";
import type { DesignPreferenceTransport } from "../../src/app/newtab/types.ts";

const tests: TestCase[] = [
  {
    name: "New Tab search destinations reveal the matching category",
    fn: () => {
      for (const field of NEWTAB_FIELDS) {
        assertEquals(
          resolveNewTabTab(`?tab=general&setting=${field.id}`),
          field.tab,
          `Search reveals ${field.id} in its category`,
        );
      }
      assertEquals(
        resolveNewTabTab("?tab=feed"),
        "feed",
        "Explicit feed category",
      );
      assertEquals(
        resolveNewTabTab("?tab=unknown"),
        "general",
        "Unknown category falls back",
      );
      assertEquals(
        new Set(NEWTAB_FIELDS.map((field) => field.id)).size,
        NEWTAB_FIELDS.length,
        "Search destination IDs are unique",
      );
    },
  },
  {
    name: "Floorp Start patch changes only its existing design preference",
    fn: () => {
      const original = {
        future: { keep: true },
        uiCustomization: {
          disableFloorpStart: false,
          chromeExtras: { futureToggle: true },
        },
      };
      const updated = JSON.parse(
        patchStartDisabled(JSON.stringify(original), true),
      );
      assertEquals(
        parseStartDisabled(JSON.stringify(updated)),
        true,
        "Floorp Start changes",
      );
      assertEquals(updated.future.keep, true, "Unknown root fields survive");
      assertEquals(
        updated.uiCustomization.chromeExtras.futureToggle,
        true,
        "Neighbor settings survive",
      );
      let rejected = false;
      try {
        patchStartDisabled("[]", true);
      } catch {
        rejected = true;
      }
      assert(rejected, "Invalid settings are not replaced with defaults");
    },
  },
  {
    name:
      "Floorp Start compares and retries without losing another design edit",
    fn: async () => {
      let raw = JSON.stringify({
        uiCustomization: { disableFloorpStart: false, future: "before" },
      });
      let writes = 0;
      const transport: DesignPreferenceTransport = {
        getStringPrefState: () =>
          Promise.resolve({ value: raw, typeMismatch: false }),
        compareAndSetStringPref: (_name, expected, updated) => {
          writes++;
          if (writes === 1) {
            raw = JSON.stringify({
              uiCustomization: {
                disableFloorpStart: false,
                future: "concurrent",
              },
            });
            return Promise.resolve({ updated: false, currentValue: raw });
          }
          assertEquals(expected, raw, "Retry uses the fresh raw value");
          raw = updated;
          return Promise.resolve({ updated: true, currentValue: raw });
        },
      };
      await setStartDisabled(true, transport);
      assertEquals(writes, 2, "Conflicted write retries once");
      assertEquals(
        JSON.parse(raw).uiCustomization.future,
        "concurrent",
        "Concurrent edit is preserved",
      );
      assertEquals(
        parseStartDisabled(raw),
        true,
        "Requested disabled value is stored",
      );
    },
  },
  {
    name:
      "New Tab setting fields are indexed at the new route and old control is not duplicated",
    fn: async () => {
      const i18n = createInstance();
      await i18n.init({
        lng: "en-US",
        initImmediate: false,
        resources: {
          "en-US": {
            translation: {
              pages: { newtab: "New Tab" },
              newtab: {
                description: "Background and feeds",
                refresh: "Automatic feed updates",
                columns: "Columns",
              },
              design: {
                uiCustomization: {
                  newtab: { disableFloorpStart: "Disable Floorp Start" },
                },
              },
            },
          },
        },
      });
      const documents = buildSearchDocuments(i18n);
      assert(
        documents.some(
          (item) => item.route === "/features/newtab?setting=newtab-refresh",
        ),
        "Feed setting has a direct search link",
      );
      assert(
        documents.some(
          (item) =>
            item.route === "/features/newtab?setting=disable-floorp-start",
        ),
        "Floorp Start lives in New Tab settings",
      );
      assert(
        !documents.some(
          (item) =>
            item.route === "/features/design?setting=disable-floorp-start",
        ),
        "The moved control is not indexed twice",
      );
    },
  },
];
export async function runAllTests(): Promise<void> {
  await runTests("newtab-settings.test.ts", tests);
}
