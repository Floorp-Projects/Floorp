/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test()
{
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function browserLoad() {
    gBrowser.selectedBrowser.removeEventListener("load", browserLoad, true);
    openScratchpad(runTests);
  }, true);

  content.location = "data:text/html,<p>test the 'Jump to line' feature in Scratchpad";
}

function runTests(aWindow, aScratchpad)
{
  let editor = aScratchpad.editor;
  let text = "foobar bug650345\nBug650345 bazbaz\nfoobar omg\ntest";
  editor.setText(text);
  editor.setCaretOffset(0);

  let oldPrompt = Services.prompt;
  let desiredValue = null;
  Services.prompt = {
    prompt: function(aWindow, aTitle, aMessage, aValue) {
      aValue.value = desiredValue;
      return true;
    },
  };

  desiredValue = 3;
  EventUtils.synthesizeKey("J", {accelKey: true}, aWindow);
  is(editor.getCaretOffset(), 34, "caret offset is correct");

  desiredValue = 2;
  aWindow.goDoCommand("cmd_gotoLine")
  is(editor.getCaretOffset(), 17, "caret offset is correct (again)");

  Services.prompt = oldPrompt;

  finish();
}
