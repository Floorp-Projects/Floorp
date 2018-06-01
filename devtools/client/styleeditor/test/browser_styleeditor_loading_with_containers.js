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

add_task(async function() {
  // Using the personal container.
  const userContextId = 1;
  const { tab } = await openTabInUserContext(TESTCASE_URI, userContextId);
  const { ui } = await openStyleEditor(tab);

  is(ui.editors.length, 2, "The UI contains two style sheets.");
  checkSheet(ui.editors[0], EXPECTED_SHEETS[0]);
  checkSheet(ui.editors[1], EXPECTED_SHEETS[1]);
});

async function openTabInUserContext(uri, userContextId) {
  // Open the tab in the correct userContextId.
  const tab = BrowserTestUtils.addTab(gBrowser, uri, {userContextId});

  // Select tab and make sure its browser is focused.
  gBrowser.selectedTab = tab;
  tab.ownerDocument.defaultView.focus();

  const browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);
  return {tab, browser};
}

function checkSheet(editor, expected) {
  is(editor.styleSheet.styleSheetIndex, expected.sheetIndex,
    "Style sheet has correct index.");

  const summary = editor.summary;
  const name = summary.querySelector(".stylesheet-name > label")
                    .getAttribute("value");
  ok(expected.name.test(name), "The name '" + name + "' is correct.");

  const ruleCount = summary.querySelector(".stylesheet-rule-count").textContent;
  is(parseInt(ruleCount, 10), expected.rules, "the rule count is correct");

  is(summary.classList.contains("splitview-active"), expected.active,
    "The active status for this sheet is correct.");
}
