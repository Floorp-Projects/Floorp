// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assertEquals,
  runTests,
} from "../../../../chrome/test/utils/test_harness.ts";

/** Run with a Firefox language pack active as well as the en-US baseline. */
export async function runAllTests(): Promise<void> {
  await runTests("LegacyBranding.test.mts", [
    {
      name: "legacy product names retain packaged branding in every UI locale",
      fn() {
        const packaged = Services.strings.createBundle(
          "resource:///chrome/en-US/locale/branding/brand.properties",
        );
        const effective = Services.strings.createBundle(
          "chrome://branding/locale/brand.properties",
        );
        for (
          const key of [
            "brandShorterName",
            "brandShortName",
            "brandFullName",
            "vendorShortName",
          ]
        ) {
          assertEquals(
            effective.GetStringFromName(key),
            packaged.GetStringFromName(key),
            `${key} must not be replaced by an active Firefox language pack`,
          );
        }
      },
    },
  ]);
}
