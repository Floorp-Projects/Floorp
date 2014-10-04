/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

///////////////////
//
// Whitelisting this test.
// As part of bug 1077403, the leaking uncaught rejection should be fixed. 
//
thisTestLeaksUncaughtRejectionsAndShouldBeFixed("Error: Unknown sheet source");

const TESTCASE_URI = TEST_BASE + "four.html";

let gUI;

function test()
{
  waitForExplicitFinish();

  addTabAndOpenStyleEditors(4, runTests);

  content.location = TESTCASE_URI;
}

function runTests(panel)
{
  gUI = panel.UI;
  gUI.editors[0].getSourceEditor().then(onEditor0Attach);
  gUI.editors[2].getSourceEditor().then(onEditor2Attach);
}

function getStylesheetNameLinkFor(aEditor)
{
  return aEditor.summary.querySelector(".stylesheet-name");
}

function onEditor0Attach(aEditor)
{
  waitForFocus(function () {
    let summary = aEditor.summary;
    EventUtils.synthesizeMouseAtCenter(summary, {}, gPanelWindow);

    let item = getStylesheetNameLinkFor(gUI.editors[0]);
    is(gPanelWindow.document.activeElement, item,
       "editor 0 item is the active element");

    EventUtils.synthesizeKey("VK_DOWN", {}, gPanelWindow);
    item = getStylesheetNameLinkFor(gUI.editors[1]);
    is(gPanelWindow.document.activeElement, item,
       "editor 1 item is the active element");

    EventUtils.synthesizeKey("VK_HOME", {}, gPanelWindow);
    item = getStylesheetNameLinkFor(gUI.editors[0]);
    is(gPanelWindow.document.activeElement, item,
       "fist editor item is the active element");

    EventUtils.synthesizeKey("VK_END", {}, gPanelWindow);
    item = getStylesheetNameLinkFor(gUI.editors[3]);
    is(gPanelWindow.document.activeElement, item,
       "last editor item is the active element");

    EventUtils.synthesizeKey("VK_UP", {}, gPanelWindow);
    item = getStylesheetNameLinkFor(gUI.editors[2]);
    is(gPanelWindow.document.activeElement, item,
       "editor 2 item is the active element");

    EventUtils.synthesizeKey("VK_RETURN", {}, gPanelWindow);
    // this will attach and give focus editor 2
  }, gPanelWindow);
}

function onEditor2Attach(aEditor)
{
  // Wait for the focus to be set.
  executeSoon(function () {
    ok(aEditor.sourceEditor.hasFocus(),
       "editor 2 has focus");

    gUI = null;
    finish();
  });
}
