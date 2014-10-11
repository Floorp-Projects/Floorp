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
const COL_NO = 3;

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
  gUI.on("editor-selected", function editorSelected(event, editor) {
    if (editor.styleSheet != gUI.editors[1].styleSheet) {
      return;
    }
    gUI.off("editor-selected", editorSelected);
    editor.getSourceEditor().then(() => {
      info("selected second editor, about to reload page");
      reloadPage();

      gUI.on("editor-added", function editorAdded(event, editor) {
        if (++count == 2) {
          info("all editors added after reload");
          gUI.off("editor-added", editorAdded);
          gUI.editors[1].getSourceEditor().then(testRemembered);
        }
      })
    });
  });
  gUI.selectStyleSheet(gUI.editors[1].styleSheet, LINE_NO, COL_NO);
}

function testRemembered()
{
  is(gUI.selectedEditor, gUI.editors[1], "second editor is selected");

  let {line, ch} = gUI.selectedEditor.sourceEditor.getCursor();
  is(line, LINE_NO, "correct line selected");
  is(ch, COL_NO, "correct column selected");

  testNewPage();
}

function testNewPage()
{
  let count = 0;
  gUI.on("editor-added", function editorAdded(event, editor) {
    info("editor added here")
    if (++count == 2) {
      info("all editors added after navigating page");
      gUI.off("editor-added", editorAdded);
      gUI.editors[0].getSourceEditor().then(testNotRemembered);
    }
  })

  info("navigating to a different page");
  navigatePage();
}

function testNotRemembered()
{
  is(gUI.selectedEditor, gUI.editors[0], "first editor is selected");

  let {line, ch} = gUI.selectedEditor.sourceEditor.getCursor();
  is(line, 0, "first line is selected");
  is(ch, 0, "first column is selected");

  gUI = null;
  finish();
}

function reloadPage()
{
  gContentWin.location.reload();
}

function navigatePage()
{
  gContentWin.location = NEW_URI;
}
