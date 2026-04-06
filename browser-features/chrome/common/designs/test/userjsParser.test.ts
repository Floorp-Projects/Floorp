// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { applyUserJS } from "../utils/userjs-parser.ts";
import {
  type TestCase,
  type assert,
  assertEquals,
} from "../../../test/utils/test_harness.ts";

// Prefix all test prefs to avoid collisions
const P = "test.userjsparser.";

function clearTestPrefs(): void {
  const branch = Services.prefs.getDefaultBranch("");
  for (const suffix of [
    "bool",
    "string",
    "int",
    "multiline",
    "a",
    "b",
    "c",
    "comma",
  ]) {
    try {
      branch.clearUserPref(`${P}${suffix}`);
    } catch {
      // pref may not exist — ignore
    }
    try {
      Services.prefs.clearUserPref(`${P}${suffix}`);
    } catch {
      // ignore
    }
  }
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

function testApplyBooleanPrefTrue(): void {
  clearTestPrefs();
  applyUserJS(`user_pref("${P}bool", true);`);
  assertEquals(
    Services.prefs.getDefaultBranch("").getBoolPref(`${P}bool`, false),
    true,
    "boolean true should be applied to default branch",
  );
}

function testApplyBooleanPrefFalse(): void {
  clearTestPrefs();
  applyUserJS(`user_pref("${P}bool", false);`);
  assertEquals(
    Services.prefs.getDefaultBranch("").getBoolPref(`${P}bool`, true),
    false,
    "boolean false should be applied to default branch",
  );
}

function testApplyStringPref(): void {
  clearTestPrefs();
  applyUserJS(`user_pref("${P}string", "hello world");`);
  assertEquals(
    Services.prefs.getDefaultBranch("").getStringPref(`${P}string`, ""),
    "hello world",
    "string pref should be applied",
  );
}

function testApplyIntPref(): void {
  clearTestPrefs();
  applyUserJS(`user_pref("${P}int", 42);`);
  assertEquals(
    Services.prefs.getDefaultBranch("").getIntPref(`${P}int`, 0),
    42,
    "int pref should be applied",
  );
}

function testSkipBlockComments(): void {
  clearTestPrefs();
  applyUserJS(`/* user_pref("${P}bool", true); */`);
  // The pref should NOT be set (default branch would return the fallback)
  assertEquals(
    Services.prefs.getDefaultBranch("").getBoolPref(`${P}bool`, false),
    false,
    "block-commented pref should not be applied",
  );
}

function testSkipLineComments(): void {
  clearTestPrefs();
  applyUserJS(`// user_pref("${P}bool", true);`);
  assertEquals(
    Services.prefs.getDefaultBranch("").getBoolPref(`${P}bool`, false),
    false,
    "line-commented pref should not be applied",
  );
}

function testMultiLinePref(): void {
  clearTestPrefs();
  const input = `user_pref("${P}multiline",
    true);`;
  applyUserJS(input);
  assertEquals(
    Services.prefs.getDefaultBranch("").getBoolPref(`${P}multiline`, false),
    true,
    "multi-line pref should be applied",
  );
}

function testMultiplePrefs(): void {
  clearTestPrefs();
  const input = `
user_pref("${P}a", true);
user_pref("${P}b", "value");
user_pref("${P}c", 99);
  `;
  applyUserJS(input);
  const branch = Services.prefs.getDefaultBranch("");
  assertEquals(branch.getBoolPref(`${P}a`, false), true, "a should be true");
  assertEquals(
    branch.getStringPref(`${P}b`, ""),
    "value",
    'b should be "value"',
  );
  assertEquals(branch.getIntPref(`${P}c`, 0), 99, "c should be 99");
}

function testCommaInString(): void {
  clearTestPrefs();
  applyUserJS(`user_pref("${P}comma", "a,b,c");`);
  assertEquals(
    Services.prefs.getDefaultBranch("").getStringPref(`${P}comma`, ""),
    "a,b,c",
    "comma within string value should be preserved",
  );
}

function testEmptyInput(): void {
  clearTestPrefs();
  // Should not throw
  applyUserJS("");
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "apply boolean true", fn: testApplyBooleanPrefTrue },
    { name: "apply boolean false", fn: testApplyBooleanPrefFalse },
    { name: "apply string pref", fn: testApplyStringPref },
    { name: "apply int pref", fn: testApplyIntPref },
    { name: "skip block comments", fn: testSkipBlockComments },
    { name: "skip line comments", fn: testSkipLineComments },
    { name: "multi-line pref", fn: testMultiLinePref },
    { name: "multiple prefs", fn: testMultiplePrefs },
    { name: "comma in string value", fn: testCommaInString },
    { name: "empty input", fn: testEmptyInput },
  ];

  const failures: string[] = [];

  for (const test of tests) {
    try {
      test.fn();
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      failures.push(`${test.name}: ${message}`);
    }
  }

  clearTestPrefs();

  if (failures.length > 0) {
    throw new Error(`userjsParser.test.ts failures: ${failures.join(" | ")}`);
  }
}
