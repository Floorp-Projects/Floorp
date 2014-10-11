/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

///////////////////
//
// Whitelisting this test.
// As part of bug 1077403, the leaking uncaught rejection should be fixed. 
//
thisTestLeaksUncaughtRejectionsAndShouldBeFixed("Error: Unknown sheet source");

const TESTCASE_URI = TEST_BASE_HTTPS + "simple.html";
const NEW_URI = TEST_BASE_HTTPS + "media.html";

const LINE_NO = 5;
const COL_NO  = 0;

let gContentWin;
let gUI;

function test()
{
  waitForExplicitFinish();

  addTabAndOpenStyleEditors(2, function(panel) {
    gContentWin = gBrowser.selectedTab.linkedBrowser.contentWindow.wrappedJSObject;
    gUI = panel.UI;
    gUI.editors[0].getSourceEditor().then(runTests);
  });

  content.location = TESTCASE_URI;
}

function runTests()
{
  let count = 0;

  // Make sure Editor doesn't go into an infinite loop when
  // column isn't passed. See bug 941018.
  gUI.on("editor-selected", function editorSelected(event, editor) {
    if (editor.styleSheet != gUI.editors[1].styleSheet) {
      return;
    }
    gUI.off("editor-selected", editorSelected);

    editor.getSourceEditor().then(() => {
      is(gUI.selectedEditor, gUI.editors[1], "second editor is selected");
      let {line, ch} = gUI.selectedEditor.sourceEditor.getCursor();

      is(line, LINE_NO, "correct line selected");
      is(ch, COL_NO, "correct column selected");

      gUI = null;
      finish();
    });
  });
  gUI.selectStyleSheet(gUI.editors[1].styleSheet.href, LINE_NO);
}
