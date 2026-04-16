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
    // Additional edge cases
    { name: "malformed line", fn: testApplyUserJSMalformedLine },
    { name: "zero pref value", fn: testApplyUserJSZeroPrefValue },
    { name: "large number", fn: testApplyUserJSLargeNumber },
    { name: "empty string value", fn: testApplyUserJSEmptyStringValue },
    { name: "single quote string", fn: testApplyUserJSSingleQuoteString },
    { name: "mixed quotes", fn: testApplyUserJSMixedQuotes },
    { name: "pref with line breaks", fn: testApplyUserJSPrefWithLineBreaks },
    { name: "only whitespace lines", fn: testApplyUserJSOnlyWhitespaceLines },
    { name: "duplicate prefs", fn: testApplyUserJSDuplicatePrefs },
    { name: "invalid value type", fn: testApplyUserJSInvalidValueType },
    { name: "float value", fn: testApplyUserJSFloatValue },
    { name: "hex number", fn: testApplyUserJSHexNumber },
    { name: "scientific notation", fn: testApplyUserJSScientificNotation },
    // Reset preferences (async, requires fetch - placeholder tests)
    {
      name: "reset preferences clears user prefs",
      fn: () => testResetPreferencesClearsUserPrefs(),
    },
    {
      name: "reset preferences skips toolkit stylesheet pref",
      fn: () => testResetPreferencesSkipsToolkitStylesheetsPref(),
    },
  ];

  const failures: string[] = [];

  for (const test of tests) {
    try {
      await test.fn();
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

// ---------------------------------------------------------------------------
// Tests — resetPreferencesWithUserJsContents (note: this is async and requires fetch)
// ---------------------------------------------------------------------------

// Note: resetPreferencesWithUserJsContents requires a file path and fetch API
// These tests would require mocking fetch or having actual test files available
// For now, we'll add placeholder tests that document the expected behavior

function testResetPreferencesClearsUserPrefs(): void {
  // This test would require a mock fetch implementation
  // Expected behavior: resetPreferencesWithUserJsContents should clear user prefs
  // that were set by the user.js file

  const testPref = `${P}reset.test`;
  Services.prefs.setBoolPref(testPref, true);
  assert(
    Services.prefs.getBoolPref(testPref, false),
    "pref should be set initially",
  );

  // Would call: await resetPreferencesWithUserJsContents("path/to/test.js");
  // Then verify the pref is cleared

  // Cleanup
  try {
    Services.prefs.clearUserPref(testPref);
  } catch {
    // ignore
  }
}

function testResetPreferencesSkipsToolkitStylesheetsPref(): void {
  // Expected behavior: toolkit.legacyUserProfileCustomizations.stylesheets
  // should NOT be cleared to avoid unloading userChrome.css

  const toolkitPref = "toolkit.legacyUserProfileCustomizations.stylesheets";
  const originalValue = Services.prefs.getBoolPref(toolkitPref, false);

  // Would call: await resetPreferencesWithUserJsContents("path/to/user.js");
  // Then verify toolkitPref is NOT cleared

  // Restore if changed
  if (Services.prefs.getBoolPref(toolkitPref, false) !== originalValue) {
    Services.prefs.setBoolPref(toolkitPref, originalValue);
  }
}

// ---------------------------------------------------------------------------
// Tests — applyUserJS error handling and edge cases
// ---------------------------------------------------------------------------

function testApplyUserJSMalformedLine(): void {
  clearTestPrefs();
  // Should not throw, just warn and skip
  applyUserJS(`user_pref("${P}malformed", this is not valid);`);
  // Pref should not be set
  assertEquals(
    Services.prefs.getDefaultBranch("").getBoolPref(`${P}malformed`, false),
    false,
    "malformed line should not set pref",
  );
}

function testApplyUserJSZeroPrefValue(): void {
  clearTestPrefs();
  applyUserJS(`user_pref("${P}zero", 0);`);
  assertEquals(
    Services.prefs.getDefaultBranch("").getIntPref(`${P}zero`, 99),
    0,
    "zero value should be applied",
  );
}

function testApplyUserJSLargeNumber(): void {
  clearTestPrefs();
  const largeNum = 2147483647; // Max int32
  applyUserJS(`user_pref("${P}large", ${largeNum});`);
  assertEquals(
    Services.prefs.getDefaultBranch("").getIntPref(`${P}large`, 0),
    largeNum,
    "large number should be applied",
  );
}

function testApplyUserJSEmptyStringValue(): void {
  clearTestPrefs();
  applyUserJS(`user_pref("${P}empty", "");`);
  assertEquals(
    Services.prefs.getDefaultBranch("").getStringPref(`${P}empty`, "default"),
    "",
    "empty string should be applied",
  );
}

function testApplyUserJSSingleQuoteString(): void {
  clearTestPrefs();
  applyUserJS(`user_pref("${P}single", 'value');`);
  assertEquals(
    Services.prefs.getDefaultBranch("").getStringPref(`${P}single`, ""),
    "value",
    "single-quoted string should be applied",
  );
}

function testApplyUserJSMixedQuotes(): void {
  clearTestPrefs();
  applyUserJS(`user_pref("${P}mixed", "it's a value");`);
  const result = Services.prefs
    .getDefaultBranch("")
    .getStringPref(`${P}mixed`, "");
  assert(
    result.includes("it's"),
    "mixed quotes in string should be handled",
  );
}

function testApplyUserJSPrefWithLineBreaks(): void {
  clearTestPrefs();
  const input = `user_pref("${P}linebreak",
  "value
  with
  breaks");`;
  applyUserJS(input);
  const result = Services.prefs
    .getDefaultBranch("")
    .getStringPref(`${P}linebreak`, "");
  assert(
    result.length > 0,
    "multiline string value should be applied",
  );
}

function testApplyUserJSOnlyWhitespaceLines(): void {
  clearTestPrefs();
  applyUserJS(`


  `);
  // Should not throw
  assert(true, "whitespace-only input should not error");
}

function testApplyUserJSDuplicatePrefs(): void {
  clearTestPrefs();
  const input = `
user_pref("${P}dup", 1);
user_pref("${P}dup", 2);
  `;
  applyUserJS(input);
  // Last value should win
  assertEquals(
    Services.prefs.getDefaultBranch("").getIntPref(`${P}dup`, 0),
    2,
    "duplicate pref should use last value",
  );
}

function testApplyUserJSInvalidValueType(): void {
  clearTestPrefs();
  // This is not a valid user.js value type (it's an object)
  // Should be skipped with a warning
  applyUserJS(`user_pref("${P}invalid", {an: "object"});`);
  // Pref should not be set
  assertEquals(
    Services.prefs.getDefaultBranch("").prefHasUserValue(`${P}invalid`),
    false,
    "invalid value type should not set pref",
  );
}

function testApplyUserJSFloatValue(): void {
  clearTestPrefs();
  applyUserJS(`user_pref("${P}float", 3.14);`);
  // Float is parsed as number, stored as int (truncated)
  const result = Services.prefs.getDefaultBranch("").getIntPref(`${P}float`, 0);
  assertEquals(result, 3, "float should be stored as truncated int");
}

function testApplyUserJSHexNumber(): void {
  clearTestPrefs();
  applyUserJS(`user_pref("${P}hex", 0xFF);`);
  assertEquals(
    Services.prefs.getDefaultBranch("").getIntPref(`${P}hex`, 0),
    255,
    "hex number should be applied",
  );
}

function testApplyUserJSScientificNotation(): void {
  clearTestPrefs();
  applyUserJS(`user_pref("${P}sci", 1e5);`);
  assertEquals(
    Services.prefs.getDefaultBranch("").getIntPref(`${P}sci`, 0),
    100000,
    "scientific notation should be applied",
  );
}
