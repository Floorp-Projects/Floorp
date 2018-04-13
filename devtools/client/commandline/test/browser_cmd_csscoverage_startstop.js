/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the addon commands works as they should

const csscoverage = require("devtools/shared/fronts/csscoverage");
const {gDevTools} = require("devtools/client/framework/devtools");

const PAGE_1 = TEST_BASE_HTTPS + "browser_cmd_csscoverage_page1.html";
const PAGE_2 = TEST_BASE_HTTPS + "browser_cmd_csscoverage_page2.html";
const PAGE_3 = TEST_BASE_HTTPS + "browser_cmd_csscoverage_page3.html";

const SHEET_A = TEST_BASE_HTTPS + "browser_cmd_csscoverage_sheetA.css";
const SHEET_B = TEST_BASE_HTTPS + "browser_cmd_csscoverage_sheetB.css";
const SHEET_C = TEST_BASE_HTTPS + "browser_cmd_csscoverage_sheetC.css";
const SHEET_D = TEST_BASE_HTTPS + "browser_cmd_csscoverage_sheetD.css";

add_task(async function() {
  let options = await helpers.openTab("about:blank");
  await helpers.openToolbar(options);

  let usage = await csscoverage.getUsage(options.target);

  await navigate(usage, options);
  await checkPages(usage);
  await checkEditorReport(usage);
  await checkPageReport(usage);

  await helpers.closeToolbar(options);
  await helpers.closeTab(options);
});

/**
 * Visit all the pages in the test
 */
function* navigate(usage, options) {
  yield usage.start(options.chromeWindow, options.target);

  ok(usage.isRunning(), "csscoverage is running");

  // Load page 1.
  options.browser.loadURI(PAGE_1);
  // And wait until page 1 and page 2 (an iframe inside page 1) are both loaded.
  yield Promise.all([
    BrowserTestUtils.browserLoaded(options.browser, false, PAGE_1),
    BrowserTestUtils.browserLoaded(options.browser, true, PAGE_2)
  ]);
  is(options.browser.currentURI.spec, PAGE_1, "page 1 loaded");

  // page 2 has JS that navigates to page 3 after a timeout.
  yield BrowserTestUtils.browserLoaded(options.browser, false, PAGE_3);
  is(options.browser.currentURI.spec, PAGE_3, "page 3 loaded");

  let toolboxReady = gDevTools.once("toolbox-ready");

  yield usage.stop();

  ok(!usage.isRunning(), "csscoverage not is running");

  yield toolboxReady;
}

/**
 * Check the expected pages have been visited
 */
function* checkPages(usage) {
  // 'load' event order. '' is for the initial location
  let expectedVisited = [ "", PAGE_2, PAGE_1, PAGE_3 ];
  let actualVisited = yield usage._testOnlyVisitedPages();
  isEqualJson(actualVisited, expectedVisited, "Visited");
}

/**
 * Check that createEditorReport returns the expected JSON
 */
function* checkEditorReport(usage) {
  // Page1
  let expectedPage1 = {
    reports: [
      {
        selectorText: ".page1-test2",
        start: { line: 8, column: 5 },
      }
    ]
  };
  let actualPage1 = yield usage.createEditorReport(PAGE_1 + " \u2192 <style> index 0");
  isEqualJson(actualPage1, expectedPage1, "Page1");

  // Page2
  let expectedPage2 = {
    reports: [
      {
        selectorText: ".page2-test2",
        start: { line: 9, column: 5 },
      },
    ]
  };
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
  let expectedSummary = { "used": 92, "unused": 22, "preload": 28 };
  isEqualJson(actualReport.summary, expectedSummary, "summary");

  checkPageReportPreload(actualReport);
  checkPageReportUnused(actualReport);
}

/**
 * Check that checkPageReport returns the expected preload JSON
 */
function checkPageReportPreload(actualReport) {
  // Check the preload header
  isEqualJson(actualReport.preload.length, 3, "preload length");

  // Check the preload rules
  isEqualJson(actualReport.preload[0].url, PAGE_2, "preload url 0");
  let expectedPreloadRules0 = [
    // TODO: This is already pre-loaded, we should note this
    {
      url: PAGE_2 + " \u2192 <style> index 0",
      start: { line: 5, column: 5 },
      selectorText: ".page2-test1"
    },
    {
      url: SHEET_A,
      start: { line: 4, column: 1 },
      selectorText: ".sheetA-test1"
    },
    {
      url: SHEET_A,
      start: { line: 16, column: 1 },
      selectorText: ".sheetA-test4"
    },
    {
      url: SHEET_B,
      start: { line: 2, column: 1 },
      selectorText: ".sheetB-test1"
    },
    {
      url: SHEET_B,
      start: { line: 14, column: 1 },
      selectorText: ".sheetB-test4"
    },
    {
      url: SHEET_D,
      start: { line: 2, column: 1 },
      selectorText: ".sheetD-test1"
    },
    {
      url: SHEET_D,
      start: { line: 14, column: 1 },
      selectorText: ".sheetD-test4"
    },
    {
      url: SHEET_C,
      start: { line: 2, column: 1 },
      selectorText: ".sheetC-test1"
    },
    {
      url: SHEET_C,
      start: { line: 14, column: 1 },
      selectorText: ".sheetC-test4"
    }
  ];
  isEqualJson(actualReport.preload[0].rules, expectedPreloadRules0, "preload rules 0");

  isEqualJson(actualReport.preload[1].url, PAGE_1, "preload url 1");
  let expectedPreloadRules1 = [
    {
      url:  SHEET_A,
      start: { line: 4, column: 1 },
      selectorText: ".sheetA-test1"
    },
    {
      url: SHEET_A,
      start: { line: 12, column: 1 },
      selectorText: ".sheetA-test3"
    },
    {
      url: SHEET_B,
      start: { line: 2, column: 1 },
      selectorText: ".sheetB-test1"
    },
    {
      url: SHEET_B,
      start: { line: 10, column: 1 },
      selectorText: ".sheetB-test3"
    },
    {
      url: SHEET_D,
      start: { line: 2, column: 1 },
      selectorText: ".sheetD-test1"
    },
    {
      url: SHEET_D,
      start: { line: 10, column: 1 },
      selectorText: ".sheetD-test3"
    },
    {
      url: SHEET_C,
      start: { line: 2, column: 1 },
      selectorText: ".sheetC-test1"
    },
    {
      url: SHEET_C,
      start: { line: 10, column: 1 },
      selectorText: ".sheetC-test3"
    },
    {
      url: PAGE_1 + " \u2192 <style> index 0",
      start: { line: 4, column: 5 },
      selectorText: ".page1-test1"
    },
    {
      url: PAGE_1 + " \u2192 <style> index 0",
      start: { line: 12, column: 5 },
      selectorText: ".page1-test3:hover"
    }
  ];
  isEqualJson(actualReport.preload[1].rules, expectedPreloadRules1, "preload rules 1");

  isEqualJson(actualReport.preload[2].url, PAGE_3, "preload url 2");
  let expectedPreloadRules2 = [
    {
      url: SHEET_A,
      start: { line: 4, column: 1 },
      selectorText: ".sheetA-test1"
    },
    {
      url: SHEET_A,
      start: { line: 20, column: 1 },
      selectorText: ".sheetA-test5"
    },
    {
      url: SHEET_B,
      start: { line: 2, column: 1 },
      selectorText: ".sheetB-test1"
    },
    {
      url: SHEET_B,
      start: { line: 18, column: 1 },
      selectorText: ".sheetB-test5"
    },
    {
      url: SHEET_D,
      start: { line: 2, column: 1 },
      selectorText: ".sheetD-test1"
    },
    {
      url: SHEET_D,
      start: { line: 18, column: 1 },
      selectorText: ".sheetD-test5"
    },
    {
      url: SHEET_C,
      start: { line: 2, column: 1 },
      selectorText: ".sheetC-test1"
    },
    {
      url: SHEET_C,
      start: { line: 18, column: 1 },
      selectorText: ".sheetC-test5"
    },
    {
      url: PAGE_3 + " \u2192 <style> index 0",
      start: { line: 5, column: 5 },
      selectorText: ".page3-test1"
    },
  ];
  isEqualJson(actualReport.preload[2].rules, expectedPreloadRules2, "preload rules 2");
}

/**
 * Check that checkPageReport returns the expected unused JSON
 */
function checkPageReportUnused(actualReport) {
  // Check the unused header
  isEqualJson(actualReport.unused.length, 8, "unused length");

  // Check the unused rules
  isEqualJson(actualReport.unused[0].url, PAGE_2 + " \u2192 <style> index 0", "unused url 0");
  let expectedUnusedRules0 = [
    {
      url: PAGE_2 + " \u2192 <style> index 0",
      start: { line: 9, column: 5 },
      selectorText: ".page2-test2"
    }
  ];
  isEqualJson(actualReport.unused[0].rules, expectedUnusedRules0, "unused rules 0");

  isEqualJson(actualReport.unused[1].url, SHEET_A, "unused url 1");
  let expectedUnusedRules1 = [
    {
      url: SHEET_A,
      start: { line: 8, column: 1 },
      selectorText: ".sheetA-test2"
    }
  ];
  isEqualJson(actualReport.unused[1].rules, expectedUnusedRules1, "unused rules 1");

  isEqualJson(actualReport.unused[2].url, SHEET_B, "unused url 2");
  let expectedUnusedRules2 = [
    {
      url: SHEET_B,
      start: { line: 6, column: 1 },
      selectorText: ".sheetB-test2"
    }
  ];
  isEqualJson(actualReport.unused[2].rules, expectedUnusedRules2, "unused rules 2");

  isEqualJson(actualReport.unused[3].url, SHEET_D, "unused url 3");
  let expectedUnusedRules3 = [
    {
      url: SHEET_D,
      start: { line: 6, column: 1 },
      selectorText: ".sheetD-test2"
    }
  ];
  isEqualJson(actualReport.unused[3].rules, expectedUnusedRules3, "unused rules 3");

  isEqualJson(actualReport.unused[4].url, SHEET_C, "unused url 4");
  let expectedUnusedRules4 = [
    {
      url: SHEET_C,
      start: { line: 6, column: 1 },
      selectorText: ".sheetC-test2"
    }
  ];
  isEqualJson(actualReport.unused[4].rules, expectedUnusedRules4, "unused rules 4");

  isEqualJson(actualReport.unused[5].url, PAGE_1 + " \u2192 <style> index 0", "unused url 5");
  let expectedUnusedRules5 = [
    {
      url: PAGE_1 + " \u2192 <style> index 0",
      start: { line: 8, column: 5 },
      selectorText: ".page1-test2"
    }
  ];
  isEqualJson(actualReport.unused[5].rules, expectedUnusedRules5, "unused rules 5");

  isEqualJson(actualReport.unused[6].url, PAGE_3 + " \u2192 <style> index 0", "unused url 6");
  let expectedUnusedRules6 = [
    {
      url: PAGE_3 + " \u2192 <style> index 0",
      start: { line: 9, column: 5 },
      selectorText: ".page3-test2"
    }
  ];
  isEqualJson(actualReport.unused[6].rules, expectedUnusedRules6, "unused rules 6");

  isEqualJson(actualReport.unused[7].url, PAGE_3 + " \u2192 <style> index 1", "unused url 7");
  let expectedUnusedRules7 = [
    {
      url: PAGE_3 + " \u2192 <style> index 1",
      start: { line: 3, column: 5 },
      selectorText: ".page3-test3"
    }
  ];
  isEqualJson(actualReport.unused[7].rules, expectedUnusedRules7, "unused rules 7");
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
