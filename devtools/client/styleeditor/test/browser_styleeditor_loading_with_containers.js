/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the stylesheets can be loaded correctly with containers
// (bug 1282660).

const TESTCASE_URI = TEST_BASE_HTTP + "simple.html";
const EXPECTED_SHEETS = [
  {
    sheetIndex: 0,
    name: /^simple.css$/,
    rules: 1,
    active: true
  }, {
    sheetIndex: 1,
    name: /^<.*>$/,
    rules: 3,
    active: false
  }
];

add_task(function* () {
  // Using the personal container.
  let userContextId = 1;
  let { tab } = yield* openTabInUserContext(TESTCASE_URI, userContextId);
  let { ui } = yield openStyleEditor(tab);

  is(ui.editors.length, 2, "The UI contains two style sheets.");
  checkSheet(ui.editors[0], EXPECTED_SHEETS[0]);
  checkSheet(ui.editors[1], EXPECTED_SHEETS[1]);
});

function* openTabInUserContext(uri, userContextId) {
  // Open the tab in the correct userContextId.
  let tab = gBrowser.addTab(uri, {userContextId});

  // Select tab and make sure its browser is focused.
  gBrowser.selectedTab = tab;
  tab.ownerDocument.defaultView.focus();

  let browser = gBrowser.getBrowserForTab(tab);
  yield BrowserTestUtils.browserLoaded(browser);
  return {tab, browser};
}

function checkSheet(editor, expected) {
  is(editor.styleSheet.styleSheetIndex, expected.sheetIndex,
    "Style sheet has correct index.");

  let summary = editor.summary;
  let name = summary.querySelector(".stylesheet-name > label")
                    .getAttribute("value");
  ok(expected.name.test(name), "The name '" + name + "' is correct.");

  let ruleCount = summary.querySelector(".stylesheet-rule-count").textContent;
  is(parseInt(ruleCount, 10), expected.rules, "the rule count is correct");

  is(summary.classList.contains("splitview-active"), expected.active,
    "The active status for this sheet is correct.");
}
