// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { DataManager } from "../dataStore.ts";
import type { LegacyPWAEntry, Manifest } from "../type.ts";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

type TestableDataManager = {
  migrateFromLegacyFormat(
    legacyData: Record<string, LegacyPWAEntry>,
  ): Record<string, Manifest>;
};

function migrateFromLegacyFormat(
  legacyData: Record<string, LegacyPWAEntry>,
): Record<string, Manifest> {
  const dataManager = new DataManager() as unknown as TestableDataManager;
  return dataManager.migrateFromLegacyFormat(legacyData);
}

const tests: TestCase[] = [
  {
    name:
      "legacy migration stores entries under composite default-container key",
    fn() {
      const startUrl = "https://example.com/app/";
      const migrated = migrateFromLegacyFormat({
        legacy: {
          id: "legacy-id",
          name: "Legacy App",
          startURI: startUrl,
          manifest: {
            dir: "ltr",
            start_url: startUrl,
            display: "standalone",
            name: "Legacy App",
            short_name: "Legacy",
            theme_color: "#fff",
            background_color: "#000",
            scope: "https://example.com/",
            id: "manifest-id",
            icons: {
              src: [{ src: "data:image/png;base64,AA==", sizes: ["128x128"] }],
            },
          },
          scope: "https://example.com/",
          config: {
            needsUpdate: false,
            persisted: true,
          },
        },
      });

      assert(
        migrated[`${startUrl}:0`] !== undefined,
        "legacy entry must be reachable by default-container composite key",
      );
      assertEquals(
        migrated[startUrl],
        undefined,
        "legacy migration must not leave start_url-only keys",
      );
      assertEquals(migrated[`${startUrl}:0`].start_url, startUrl);
    },
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("dataStore.test.ts", tests);
}
