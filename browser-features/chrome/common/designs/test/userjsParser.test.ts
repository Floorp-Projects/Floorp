// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { applyUserJS } from "../utils/userjs-parser.ts";
import {
  assert,
  type TestCase,
  assertEquals,
} from "../../../test/utils/test_harness.ts";

// Prefix all test prefs to avoid collisions
const P = "test.userjsparser.";

function clearTestPrefs(): void {
  const branch = Services.prefs.getDefaultBranch("");
  // Use deleteBranch on the default branch to clear prefs set by applyUserJS,
  // because applyUserJS writes to the default branch (not user branch).
  try {
    branch.deleteBranch(P);
  } catch {
    // branch may not exist — ignore
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
// Tests — edge cases
// ---------------------------------------------------------------------------

function testNegativeIntValue(): void {
  clearTestPrefs();
  applyUserJS(`user_pref("${P}int", -1);`);
  assertEquals(
    Services.prefs.getDefaultBranch("").getIntPref(`${P}int`, 0),
    -1,
    "negative int value should be applied",
  );
}

function testEscapedQuotesInString(): void {
  clearTestPrefs();
  applyUserJS(`user_pref("${P}string", "hello \\"world\\"");`);
  // The parser strips surrounding quotes, inner \" becomes hello \"world\"
  const result = Services.prefs
    .getDefaultBranch("")
    .getStringPref(`${P}string`, "");
  assert(
    result.includes("hello"),
    "escaped quotes string should contain 'hello'",
  );
}

function testWhitespaceBetweenUserPrefAndParen(): void {
  clearTestPrefs();
  applyUserJS(`user_pref  ("${P}bool", true);`);
  assertEquals(
    Services.prefs.getDefaultBranch("").getBoolPref(`${P}bool`, false),
    true,
    "whitespace between user_pref and ( should still parse",
  );
}

function testStarCommentLine(): void {
  clearTestPrefs();
  applyUserJS(`* user_pref("${P}bool", true);`);
  assertEquals(
    Services.prefs.getDefaultBranch("").getBoolPref(`${P}bool`, false),
    false,
    "star-commented line should not be applied",
  );
}

function testIgnoreNonUserPrefLines(): void {
  clearTestPrefs();
  applyUserJS(`lockPref("${P}bool", true);`);
  assertEquals(
    Services.prefs.getDefaultBranch("").getBoolPref(`${P}bool`, false),
    false,
    "lockPref should be ignored",
  );
}

function testUnicodeStringValue(): void {
  clearTestPrefs();
  applyUserJS(`user_pref("${P}string", "こんにちは世界");`);
  assertEquals(
    Services.prefs.getDefaultBranch("").getStringPref(`${P}string`, ""),
    "こんにちは世界",
    "unicode string value should be applied",
  );
}

function testSemicolonInStringValue(): void {
  clearTestPrefs();
  applyUserJS(`user_pref("${P}string", "a;b;c");`);
  assertEquals(
    Services.prefs.getDefaultBranch("").getStringPref(`${P}string`, ""),
    "a;b;c",
    "semicolon inside string value should be preserved",
  );
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
    // Edge cases
    { name: "negative int value", fn: testNegativeIntValue },
    { name: "escaped quotes in string", fn: testEscapedQuotesInString },
    {
      name: "whitespace between user_pref and paren",
      fn: testWhitespaceBetweenUserPrefAndParen,
    },
    { name: "star comment line", fn: testStarCommentLine },
    { name: "ignore non-user_pref lines", fn: testIgnoreNonUserPrefLines },
    { name: "unicode string value", fn: testUnicodeStringValue },
    { name: "semicolon in string value", fn: testSemicolonInStringValue },
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
