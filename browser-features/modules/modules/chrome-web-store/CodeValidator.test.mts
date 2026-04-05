// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  validateSourceCode,
  validateSourceCodeFull,
  validateMultipleFiles,
  isJavaScriptFile,
  mightContainUnsupportedPatterns,
  getValidationSummary,
} from "./CodeValidator.sys.mts";

import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../chrome/test/utils/test_harness.ts";

// ---------------------------------------------------------------------------
// Tests — isJavaScriptFile
// ---------------------------------------------------------------------------

function testIsJSFileJs(): void {
  assert(isJavaScriptFile("app.js"), ".js should be JS");
}

function testIsJSFileMjs(): void {
  assert(isJavaScriptFile("module.mjs"), ".mjs should be JS");
}

function testIsJSFileTs(): void {
  assert(isJavaScriptFile("types.ts"), ".ts should be JS");
}

function testIsJSFileTsx(): void {
  assert(isJavaScriptFile("component.tsx"), ".tsx should be JS");
}

function testIsJSFileJson(): void {
  assertEquals(isJavaScriptFile("data.json"), false, ".json should not be JS");
}

function testIsJSFileCss(): void {
  assertEquals(isJavaScriptFile("styles.css"), false, ".css should not be JS");
}

// ---------------------------------------------------------------------------
// Tests — mightContainUnsupportedPatterns
// ---------------------------------------------------------------------------

function testQuickCheckTabCapture(): void {
  assert(
    mightContainUnsupportedPatterns("chrome.tabCapture.getMediaStreamId()"),
    "chrome.tabCapture should be detected",
  );
}

function testQuickCheckOffscreen(): void {
  assert(
    mightContainUnsupportedPatterns("chrome.offscreen.createDocument()"),
    "chrome.offscreen should be detected",
  );
}

function testQuickCheckClean(): void {
  assertEquals(
    mightContainUnsupportedPatterns("chrome.tabs.query({})"),
    false,
    "supported API should not be flagged",
  );
}

// ---------------------------------------------------------------------------
// Tests — validateSourceCode
// ---------------------------------------------------------------------------

function testValidateCleanCode(): void {
  assertEquals(
    validateSourceCode("bg.js", "chrome.tabs.query({});"),
    null,
    "clean code should return null",
  );
}

function testValidateUnsupportedTabCapture(): void {
  const result = validateSourceCode(
    "bg.js",
    'chrome.tabCapture.getMediaStreamId("test");',
  );
  assert(result !== null, "tabCapture should be detected");
  assert(result.includes("tabCapture"), "error should mention tabCapture");
}

// ---------------------------------------------------------------------------
// Tests — validateSourceCodeFull
// ---------------------------------------------------------------------------

function testValidateFullClean(): void {
  const result = validateSourceCodeFull("bg.js", "console.log('hello');");
  assert(result.valid, "clean code should be valid");
  assertEquals(result.errors.length, 0, "should have no errors");
  assertEquals(result.warnings.length, 0, "should have no warnings");
}

function testValidateFullWithWarning(): void {
  const result = validateSourceCodeFull(
    "bg.js",
    "chrome.sidePanel.setOptions({});",
  );
  assert(result.warnings.length > 0, "sidePanel should produce warning");
}

function testValidateFullErrorHasLineInfo(): void {
  const code = "const x = 1;\nconst y = 2;\nchrome.tabCapture.start();";
  const result = validateSourceCodeFull("bg.js", code);
  assert(result.errors.length > 0, "should have error");
  assertEquals(result.errors[0].line, 3, "error should be on line 3");
}

// ---------------------------------------------------------------------------
// Tests — validateMultipleFiles
// ---------------------------------------------------------------------------

function testValidateMultipleFilesSkipsNonJS(): void {
  const files = new Map([
    ["manifest.json", "chrome.tabCapture.start();"],
    ["bg.js", "console.log('ok');"],
  ]);
  const result = validateMultipleFiles(files);
  assert(result.valid, "JSON files should be skipped");
}

function testValidateMultipleFilesAggregatesErrors(): void {
  const files = new Map([
    ["a.js", "chrome.tabCapture.start();"],
    ["b.js", "console.log('ok');"],
  ]);
  const result = validateMultipleFiles(files);
  assertEquals(result.errors.length, 1, "should have 1 error from a.js");
  assertEquals(result.errors[0].file, "a.js", "error should be from a.js");
}

// ---------------------------------------------------------------------------
// Tests — getValidationSummary
// ---------------------------------------------------------------------------

function testSummaryNoIssues(): void {
  assertEquals(
    getValidationSummary({ valid: true, errors: [], warnings: [] }),
    "No issues found",
    "clean result should say no issues",
  );
}

function testSummaryWithErrors(): void {
  const summary = getValidationSummary({
    valid: false,
    errors: [{ file: "a.js", line: 1, column: 1, message: "err", code: "X" }],
    warnings: [],
  });
  assert(summary.includes("1 error"), "should mention 1 error");
}

function testSummaryWithBoth(): void {
  const summary = getValidationSummary({
    valid: false,
    errors: [{ file: "a.js", line: 1, column: 1, message: "err", code: "X" }],
    warnings: [{ file: "b.js", message: "warn" }],
  });
  assert(summary.includes("1 error"), "should mention errors");
  assert(summary.includes("1 warning"), "should mention warnings");
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "isJSFile .js", fn: testIsJSFileJs },
    { name: "isJSFile .mjs", fn: testIsJSFileMjs },
    { name: "isJSFile .ts", fn: testIsJSFileTs },
    { name: "isJSFile .tsx", fn: testIsJSFileTsx },
    { name: "isJSFile .json", fn: testIsJSFileJson },
    { name: "isJSFile .css", fn: testIsJSFileCss },
    { name: "quickCheck tabCapture", fn: testQuickCheckTabCapture },
    { name: "quickCheck offscreen", fn: testQuickCheckOffscreen },
    { name: "quickCheck clean", fn: testQuickCheckClean },
    { name: "validate clean code", fn: testValidateCleanCode },
    { name: "validate tabCapture", fn: testValidateUnsupportedTabCapture },
    { name: "validateFull clean", fn: testValidateFullClean },
    { name: "validateFull warning", fn: testValidateFullWithWarning },
    { name: "validateFull line info", fn: testValidateFullErrorHasLineInfo },
    {
      name: "validateMultiple skips non-JS",
      fn: testValidateMultipleFilesSkipsNonJS,
    },
    {
      name: "validateMultiple aggregates",
      fn: testValidateMultipleFilesAggregatesErrors,
    },
    { name: "summary no issues", fn: testSummaryNoIssues },
    { name: "summary with errors", fn: testSummaryWithErrors },
    { name: "summary with both", fn: testSummaryWithBoth },
  ];

  await runTests("CodeValidator.test.mts", tests);
}
