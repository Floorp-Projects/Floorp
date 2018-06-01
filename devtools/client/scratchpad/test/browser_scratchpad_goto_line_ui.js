/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* Bug 714942 */

function test() {
  waitForExplicitFinish();

  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser).then(function() {
    openScratchpad(runTests);
  });

  gBrowser.loadURI("data:text/html,<p>test the 'Jump to line' feature in Scratchpad");
}

function runTests(aWindow, aScratchpad) {
  const editor = aScratchpad.editor;
  const text = "foobar bug650345\nBug650345 bazbaz\nfoobar omg\ntest";
  editor.setText(text);
  editor.setCursor({ line: 0, ch: 0 });

  const oldPrompt = editor.openDialog;
  let desiredValue;

  editor.openDialog = function(text, cb) {
    cb(desiredValue);
  };

  desiredValue = 3;
  EventUtils.synthesizeKey("J", {accelKey: true}, aWindow);
  is(editor.getCursor().line, 2, "line is correct");

  desiredValue = 2;
  EventUtils.synthesizeKey("J", {accelKey: true}, aWindow);
  is(editor.getCursor().line, 1, "line is correct (again)");

  editor.openDialog = oldPrompt;
  finish();
}
