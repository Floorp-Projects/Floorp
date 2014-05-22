/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the addon commands works as they should

const csscoverage = require("devtools/server/actors/csscoverage");

const PAGE_1 = TEST_BASE_HTTPS + "browser_cmd_csscoverage_page1.html";
const PAGE_2 = TEST_BASE_HTTPS + "browser_cmd_csscoverage_page2.html";
const PAGE_3 = TEST_BASE_HTTPS + "browser_cmd_csscoverage_page3.html";

const SHEET_A = TEST_BASE_HTTPS + "browser_cmd_csscoverage_sheetA.css";
const SHEET_B = TEST_BASE_HTTPS + "browser_cmd_csscoverage_sheetB.css";
const SHEET_C = TEST_BASE_HTTPS + "browser_cmd_csscoverage_sheetC.css";
const SHEET_D = TEST_BASE_HTTPS + "browser_cmd_csscoverage_sheetD.css";

let test = asyncTest(function*() {
  let options = yield helpers.openTab("about:blank");
  yield helpers.openToolbar(options);

  let usage = yield csscoverage.getUsage(options.target);

  yield usage.start();

  let running = yield usage._testOnly_isRunning();
  ok(running, "csscoverage is running");

  yield helpers.navigate(PAGE_3, options);

  yield usage.stop();

  running = yield usage._testOnly_isRunning();
  ok(!running, "csscoverage not is running");

  // Page1
  let expectedPage1 = { reports: [] };
  let actualPage1 = yield usage.createEditorReport(PAGE_1);
  isEqualJson(actualPage1, expectedPage1, 'Page1');

  // Page2
  let expectedPage2 = { reports: [] };
  let actualPage2 = yield usage.createEditorReport(PAGE_2);
  isEqualJson(actualPage2, expectedPage2, 'Page2');

  // Page3
  let expectedPage3 = {
    reports: [
      {
        selectorText: ".page3-test2",
        start: { line: 9, column: 5 },
      },
      {
        selectorText: ".page3-test3",
        start: { line: 3, column: 5 },
      }
    ]
  };
  let actualPage3 = yield usage.createEditorReport(PAGE_3);
  isEqualJson(actualPage3, expectedPage3, 'Page3');

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
  isEqualJson(actualSheetA, expectedSheetA, 'SheetA');

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
  isEqualJson(actualSheetB, expectedSheetB, 'SheetB');

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
  isEqualJson(actualSheetC, expectedSheetC, 'SheetC');

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
  isEqualJson(actualSheetD, expectedSheetD, 'SheetD');

  yield helpers.closeToolbar(options);
  yield helpers.closeTab(options);
});

function isEqualJson(o1, o2, msg) {
  is(JSON.stringify(o1), JSON.stringify(o2), msg);
}
