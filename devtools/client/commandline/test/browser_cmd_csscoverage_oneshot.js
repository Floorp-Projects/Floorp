/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the addon commands works as they should

const csscoverage = require("devtools/shared/fronts/csscoverage");

const PAGE_1 = TEST_BASE_HTTPS + "browser_cmd_csscoverage_page1.html";
const PAGE_2 = TEST_BASE_HTTPS + "browser_cmd_csscoverage_page2.html";
const PAGE_3 = TEST_BASE_HTTPS + "browser_cmd_csscoverage_page3.html";

const SHEET_A = TEST_BASE_HTTPS + "browser_cmd_csscoverage_sheetA.css";
const SHEET_B = TEST_BASE_HTTPS + "browser_cmd_csscoverage_sheetB.css";
const SHEET_C = TEST_BASE_HTTPS + "browser_cmd_csscoverage_sheetC.css";
const SHEET_D = TEST_BASE_HTTPS + "browser_cmd_csscoverage_sheetD.css";

add_task(function* () {
  let options = yield helpers.openTab(PAGE_3);
  yield helpers.openToolbar(options);

  let usage = yield csscoverage.getUsage(options.target);

  yield navigate(usage, options);
  yield checkPages(usage);
  yield checkEditorReport(usage);
  // usage.createPageReport is not supported for usage.oneshot data as of
  // bug 1035300 because the page report assumed we have preload data which
  // oneshot can't gather. The ideal solution is to have a special no-preload
  // mode for the page report, but since oneshot isn't needed for the UI to
  // function, we're currently not supporting page report for oneshot data
  // yield checkPageReport(usage);

  yield helpers.closeToolbar(options);
  yield helpers.closeTab(options);
});

/**
 * Just check current page
 */
function* navigate(usage, options) {
  ok(!usage.isRunning(), "csscoverage is not running");

  yield usage.oneshot();

  ok(!usage.isRunning(), "csscoverage is still not running");
}

/**
 * Check the expected pages have been visited
 */
function* checkPages(usage) {
  let expectedVisited = [ PAGE_3 ];
  let actualVisited = yield usage._testOnlyVisitedPages();
  isEqualJson(actualVisited, expectedVisited, "Visited");
}

/**
 * Check that createEditorReport returns the expected JSON
 */
function* checkEditorReport(usage) {
  // Page1
  let expectedPage1 = { reports: [] };
  let actualPage1 = yield usage.createEditorReport(PAGE_1 + " \u2192 <style> index 0");
  isEqualJson(actualPage1, expectedPage1, "Page1");

  // Page2
  let expectedPage2 = { reports: [] };
  let actualPage2 = yield usage.createEditorReport(PAGE_2 + " \u2192 <style> index 0");
  isEqualJson(actualPage2, expectedPage2, "Page2");

  // Page3a
  let expectedPage3a = {
    reports: [
      {
        selectorText: ".page3-test2",
        start: { line: 9, column: 5 },
      }
    ]
  };
  let actualPage3a = yield usage.createEditorReport(PAGE_3 + " \u2192 <style> index 0");
  isEqualJson(actualPage3a, expectedPage3a, "Page3a");

  // Page3b
  let expectedPage3b = {
    reports: [
      {
        selectorText: ".page3-test3",
        start: { line: 3, column: 5 },
      }
    ]
  };
  let actualPage3b = yield usage.createEditorReport(PAGE_3 + " \u2192 <style> index 1");
  isEqualJson(actualPage3b, expectedPage3b, "Page3b");

  // SheetA
  let expectedSheetA = {
    reports: [
      {
        selectorText: ".sheetA-test2",
        start: { line: 8, column: 1 },
      },
      {
        selectorText: ".sheetA-test3",
        start: { line: 12, column: 1 },
      },
      {
        selectorText: ".sheetA-test4",
        start: { line: 16, column: 1 },
      }
    ]
  };
  let actualSheetA = yield usage.createEditorReport(SHEET_A);
  isEqualJson(actualSheetA, expectedSheetA, "SheetA");

  // SheetB
  let expectedSheetB = {
    reports: [
      {
        selectorText: ".sheetB-test2",
        start: { line: 6, column: 1 },
      },
      {
        selectorText: ".sheetB-test3",
        start: { line: 10, column: 1 },
      },
      {
        selectorText: ".sheetB-test4",
        start: { line: 14, column: 1 },
      }
    ]
  };
  let actualSheetB = yield usage.createEditorReport(SHEET_B);
  isEqualJson(actualSheetB, expectedSheetB, "SheetB");

  // SheetC
  let expectedSheetC = {
    reports: [
      {
        selectorText: ".sheetC-test2",
        start: { line: 6, column: 1 },
      },
      {
        selectorText: ".sheetC-test3",
        start: { line: 10, column: 1 },
      },
      {
        selectorText: ".sheetC-test4",
        start: { line: 14, column: 1 },
      }
    ]
  };
  let actualSheetC = yield usage.createEditorReport(SHEET_C);
  isEqualJson(actualSheetC, expectedSheetC, "SheetC");

  // SheetD
  let expectedSheetD = {
    reports: [
      {
        selectorText: ".sheetD-test2",
        start: { line: 6, column: 1 },
      },
      {
        selectorText: ".sheetD-test3",
        start: { line: 10, column: 1 },
      },
      {
        selectorText: ".sheetD-test4",
        start: { line: 14, column: 1 },
      }
    ]
  };
  let actualSheetD = yield usage.createEditorReport(SHEET_D);
  isEqualJson(actualSheetD, expectedSheetD, "SheetD");
}

/**
 * Check that checkPageReport returns the expected JSON
 */
function* checkPageReport(usage) {
  let actualReport = yield usage.createPageReport();

  // Quick check on trivial things. See doc comment for checkRuleProperties
  actualReport.preload.forEach(page => page.rules.forEach(checkRuleProperties));
  actualReport.unused.forEach(page => page.rules.forEach(checkRuleProperties));

  // Check the summary
  let expectedSummary = { "used": 23, "unused": 9, "preload": 0 };
  isEqualJson(actualReport.summary, expectedSummary, "summary");

  // Check the preload header
  isEqualJson(actualReport.preload.length, 0, "preload length");

  // Check the unused header
  isEqualJson(actualReport.unused.length, 6, "unused length");

  // Check the unused rules
  isEqualJson(actualReport.unused[0].url, PAGE_3 + " \u2192 <style> index 0", "unused url 0");
  let expectedUnusedRules0 = [
    {
      "url": PAGE_3 + " \u2192 <style> index 0",
      "start": { "line": 9, "column": 5 },
      "selectorText": ".page3-test2"
    }
  ];
  isEqualJson(actualReport.unused[0].rules, expectedUnusedRules0, "unused rules 0");

  isEqualJson(actualReport.unused[1].url, PAGE_3 + " \u2192 <style> index 1", "unused url 1");
  let expectedUnusedRules1 = [
    {
      "url": PAGE_3 + " \u2192 <style> index 1",
      "start": { "line": 3, "column": 5 },
      "selectorText": ".page3-test3"
    }
  ];
  isEqualJson(actualReport.unused[1].rules, expectedUnusedRules1, "unused rules 1");

  isEqualJson(actualReport.unused[2].url, SHEET_A, "unused url 2");
  let expectedUnusedRules2 = [
    {
      "url": SHEET_A,
      "start": { "line": 8, "column": 1 },
      "selectorText": ".sheetA-test2"
    },
    {
      "url": SHEET_A,
      "start": { "line": 12, "column": 1 },
      "selectorText": ".sheetA-test3"
    },
    {
      "url": SHEET_A,
      "start": { "line": 16, "column": 1 },
      "selectorText": ".sheetA-test4"
    }
  ];
  isEqualJson(actualReport.unused[2].rules, expectedUnusedRules2, "unused rules 2");

  isEqualJson(actualReport.unused[3].url, SHEET_B, "unused url 3");
  let expectedUnusedRules3 = [
    {
      "url": SHEET_B,
      "start": { "line": 6, "column": 1 },
      "selectorText": ".sheetB-test2"
    },
    {
      "url": SHEET_B,
      "start": { "line": 10, "column": 1 },
      "selectorText": ".sheetB-test3"
    },
    {
      "url": SHEET_B,
      "start": { "line": 14, "column": 1 },
      "selectorText": ".sheetB-test4"
    }
  ];
  isEqualJson(actualReport.unused[3].rules, expectedUnusedRules3, "unused rules 3");

  isEqualJson(actualReport.unused[4].url, SHEET_D, "unused url 4");
  let expectedUnusedRules4 = [
    {
      "url": SHEET_D,
      "start": { "line": 6, "column": 1 },
      "selectorText": ".sheetD-test2"
    },
    {
      "url": SHEET_D,
      "start": { "line": 10, "column": 1 },
      "selectorText": ".sheetD-test3"
    },
    {
      "url": SHEET_D,
      "start": { "line": 14, "column": 1 },
      "selectorText": ".sheetD-test4"
    }
  ];
  isEqualJson(actualReport.unused[4].rules, expectedUnusedRules4, "unused rules 4");

  isEqualJson(actualReport.unused[5].url, SHEET_C, "unused url 5");
  let expectedUnusedRules5 = [
    {
      "url": SHEET_C,
      "start": { "line": 6, "column": 1 },
      "selectorText": ".sheetC-test2"
    },
    {
      "url": SHEET_C,
      "start": { "line": 10, "column": 1 },
      "selectorText": ".sheetC-test3"
    },
    {
      "url": SHEET_C,
      "start": { "line": 14, "column": 1 },
      "selectorText": ".sheetC-test4"
    }
  ];
  isEqualJson(actualReport.unused[5].rules, expectedUnusedRules5, "unused rules 5");
}

/**
 * We do basic tests on the shortUrl and formattedCssText because they are
 * very derivative, and so make for fragile tests, and having done those quick
 * existence checks we remove them so the JSON check later can ignore them
 */
function checkRuleProperties(rule, index) {
  is(typeof rule.shortUrl, "string", "typeof rule.shortUrl for " + index);
  is(rule.shortUrl.indexOf("http://"), -1, "http not in rule.shortUrl for" + index);
  delete rule.shortUrl;

  is(typeof rule.formattedCssText, "string", "typeof rule.formattedCssText for " + index);
  ok(rule.formattedCssText.indexOf("{") > 0, "{ in rule.formattedCssText for " + index);
  delete rule.formattedCssText;
}

/**
 * Utility to compare JSON structures
 */
function isEqualJson(o1, o2, msg) {
  is(JSON.stringify(o1), JSON.stringify(o2), msg);
}
