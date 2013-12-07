/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TESTCASE_URI = TEST_BASE + "four.html";

let gUI;

function test() {
  waitForExplicitFinish();

  let count = 0;
  addTabAndOpenStyleEditor(function(panel) {
    gUI = panel.UI;
    gUI.on("editor-added", function(event, editor) {
      count++;
      if (count == 2) {
        runTests();
      }
    })
  });

  content.location = TESTCASE_URI;
}

let timeoutID;

function runTests() {
  gBrowser.tabContainer.addEventListener("TabOpen", onTabAdded, false);
  gUI.editors[0].getSourceEditor().then(onEditor0Attach);
  gUI.editors[1].getSourceEditor().then(onEditor1Attach);
}

function getStylesheetNameLinkFor(aEditor) {
  return aEditor.summary.querySelector(".stylesheet-name");
}

function onEditor0Attach(aEditor) {
  waitForFocus(function () {
    // left mouse click should focus editor 1
    EventUtils.synthesizeMouseAtCenter(
      getStylesheetNameLinkFor(gUI.editors[1]),
      {button: 0},
      gPanelWindow);
  }, gPanelWindow);
}

function onEditor1Attach(aEditor) {
  ok(aEditor.sourceEditor.hasFocus(),
     "left mouse click has given editor 1 focus");

  // right mouse click should not open a new tab
  EventUtils.synthesizeMouseAtCenter(
    getStylesheetNameLinkFor(gUI.editors[2]),
    {button: 1},
    gPanelWindow);

  setTimeout(finish, 0);
}

function onTabAdded() {
  ok(false, "middle mouse click has opened a new tab");
  finish();
}

registerCleanupFunction(function () {
  gBrowser.tabContainer.removeEventListener("TabOpen", onTabAdded, false);
  gUI = null;
});
