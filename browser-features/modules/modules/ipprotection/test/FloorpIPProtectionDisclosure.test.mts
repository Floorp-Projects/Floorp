// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  createFloorpIPProtectionDisclosureStringsOrFallback,
  resolveFloorpIPProtectionDisclosureStrings,
} from "../FloorpIPProtectionDisclosure.sys.mts";

import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../../chrome/test/utils/test_harness.ts";

function testJapaneseLocaleVariantsResolveConsistently(): void {
  const japanese = resolveFloorpIPProtectionDisclosureStrings("ja-JP");
  for (const locale of ["ja", "ja-JP"]) {
    assertEquals(
      resolveFloorpIPProtectionDisclosureStrings(locale),
      japanese,
      `${locale} should resolve to the same disclosure`,
    );
  }
}

function testEnglishLocalesUseEnglish(): void {
  const english = resolveFloorpIPProtectionDisclosureStrings("en-US");
  for (const locale of ["en", "en-US", "en-GB"]) {
    assertEquals(
      resolveFloorpIPProtectionDisclosureStrings(locale),
      english,
      `${locale} should use the English disclosure`,
    );
  }
}

function testUnsupportedLocalesFallBackToEnglish(): void {
  const english = resolveFloorpIPProtectionDisclosureStrings("en-US");
  for (const locale of ["de", "fr-FR", "zh-CN"]) {
    assertEquals(
      resolveFloorpIPProtectionDisclosureStrings(locale),
      english,
      `${locale} should fall back to English`,
    );
  }
}

function testInvalidJapaneseResourceFallsBackToEnglish(): void {
  const english = resolveFloorpIPProtectionDisclosureStrings("en-US");
  for (const invalidResource of [undefined, "", { title: "Browser VPN" }]) {
    assertEquals(
      createFloorpIPProtectionDisclosureStringsOrFallback(
        invalidResource,
        english,
      ),
      english,
      "missing, blank, and partial Japanese resources should use English",
    );
  }
}

function testResolvedLocaleDisclosuresAreCompleteAndNonEmpty(): void {
  const english = resolveFloorpIPProtectionDisclosureStrings("en-US");
  const japanese = resolveFloorpIPProtectionDisclosureStrings("ja-JP");
  const englishKeys = Object.keys(english);
  const japaneseKeys = Object.keys(japanese);

  assertEquals(
    JSON.stringify(japaneseKeys),
    JSON.stringify(englishKeys),
    "Resolved English and Japanese disclosures should contain the same keys",
  );

  for (
    const [locale, strings] of [
      ["en-US", english],
      ["ja-JP", japanese],
    ] as const
  ) {
    for (const [key, value] of Object.entries(strings)) {
      assert(
        typeof value === "string" && value.length > 0,
        `${locale} disclosure ${key} should be a non-empty string`,
      );
    }
  }
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "Japanese locales resolve disclosures consistently",
      fn: testJapaneseLocaleVariantsResolveConsistently,
    },
    {
      name: "English locales use English disclosures",
      fn: testEnglishLocalesUseEnglish,
    },
    {
      name: "unsupported locales fall back to English disclosures",
      fn: testUnsupportedLocalesFallBackToEnglish,
    },
    {
      name: "invalid Japanese resources fall back to English disclosures",
      fn: testInvalidJapaneseResourceFallsBackToEnglish,
    },
    {
      name: "resolved locale disclosures are complete and non-empty",
      fn: testResolvedLocaleDisclosuresAreCompleteAndNonEmpty,
    },
  ];

  await runTests("FloorpIPProtectionDisclosure.test.mts", tests);
}
