// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { defaultEnabled, strDefaultConfig } from "../default-pref.ts";
import { zPwaConfig } from "../type.ts";
import { isRight } from "fp-ts/Either";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

function testDefaultEnabledIsTrue(): void {
  assertEquals(defaultEnabled, true, "default enabled flag should be true");
}

function testDefaultConfigJsonIsParseable(): void {
  const parsed = JSON.parse(strDefaultConfig) as unknown;
  assert(typeof parsed === "object" && parsed !== null, "config should parse");
}

function testDefaultConfigMatchesSchema(): void {
  const parsed = JSON.parse(strDefaultConfig) as unknown;
  const decoded = zPwaConfig.decode(parsed);
  assert(isRight(decoded), "default config should satisfy zPwaConfig schema");
  if (isRight(decoded)) {
    assertEquals(
      decoded.right.showToolbar,
      true,
      "default config should enable toolbar",
    );
  }
}

function testInvalidConfigFailsSchema(): void {
  const decoded = zPwaConfig.decode({ showToolbar: "yes" });
  assert(!isRight(decoded), "invalid config should fail schema decode");
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "defaultEnabled is true", fn: testDefaultEnabledIsTrue },
    {
      name: "default config json is parseable",
      fn: testDefaultConfigJsonIsParseable,
    },
    {
      name: "default config matches schema",
      fn: testDefaultConfigMatchesSchema,
    },
    { name: "invalid config fails schema", fn: testInvalidConfigFailsSchema },
  ];

  await runTests("pwaDefaultPref.test.ts", tests);
}
