// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import {
  assertEquals,
  runTests,
} from "../../../../chrome/test/utils/test_harness.ts";
import { resolveSystemLocale } from "../../../src/app/localization/system-locale.ts";

export async function runAllTests() {
  await runTests("system-locale", [
    {
      name: "English uses an available regional locale",
      fn: () =>
        assertEquals(
          resolveSystemLocale({
            baseName: "en-US",
            language: "en",
            region: "US",
          }, ["en-GB", "en-US"]),
          "en-US",
          "US region retained",
        ),
    },
    {
      name: "Japanese falls back to its language-only pack",
      fn: () =>
        assertEquals(
          resolveSystemLocale({
            baseName: "ja-JP",
            language: "ja",
            region: "JP",
          }, ["en-US", "ja"]),
          "ja",
          "Japanese pack selected",
        ),
    },
    {
      name: "Negotiated region is used when the OS region has no pack",
      fn: () =>
        assertEquals(
          resolveSystemLocale(
            { baseName: "pt-AO", language: "pt", region: "AO" },
            ["pt-BR", "pt-PT"],
            "pt-PT",
          ),
          "pt-PT",
          "Negotiated pack selected",
        ),
    },
    {
      name: "Matching preserves installed locale spelling",
      fn: () =>
        assertEquals(
          resolveSystemLocale({
            baseName: "en-us",
            language: "en",
            region: "us",
          }, ["en-US"]),
          "en-US",
          "Canonical spelling",
        ),
    },
    {
      name: "Unavailable language is not sent to installation",
      fn: () =>
        assertEquals(
          resolveSystemLocale({
            baseName: "xx-XX",
            language: "xx",
            region: "XX",
          }, ["en-US"]),
          undefined,
          "Unsupported locale",
        ),
    },
  ]);
}
