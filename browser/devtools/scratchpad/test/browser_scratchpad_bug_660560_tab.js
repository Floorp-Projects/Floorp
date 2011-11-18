/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

var ScratchpadManager = Scratchpad.ScratchpadManager;

function test()
{
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onTabLoad() {
    gBrowser.selectedBrowser.removeEventListener("load", onTabLoad, true);

    ok(window.Scratchpad, "Scratchpad variable exists");

    Services.prefs.setIntPref("devtools.editor.tabsize", 5);

    gScratchpadWindow = Scratchpad.openScratchpad();
    gScratchpadWindow.addEventListener("load", function onScratchpadLoad() {
      gScratchpadWindow.removeEventListener("load", onScratchpadLoad, false);

      gScratchpadWindow.Scratchpad.addObserver({
        onReady: runTests
      });
    }, false);
  }, true);

  content.location = "data:text/html,Scratchpad test for the Tab key, bug 660560";
}

function runTests()
{
  let sp = gScratchpadWindow.Scratchpad;
  ok(sp, "Scratchpad object exists in new window");

  is(this.onReady, runTests, "the handler runs in the context of the observer");
  sp.removeObserver(this);

  ok(sp.editor.hasFocus(), "the editor has focus");

  sp.setText("window.foo;");
  sp.editor.setCaretOffset(0);

  EventUtils.synthesizeKey("VK_TAB", {}, gScratchpadWindow);

  is(sp.getText(), "     window.foo;", "Tab key added 5 spaces");

  is(sp.editor.getCaretOffset(), 5, "caret location is correct");

  sp.editor.setCaretOffset(6);

  EventUtils.synthesizeKey("VK_TAB", {}, gScratchpadWindow);

  is(sp.getText(), "     w    indow.foo;",
     "Tab key added 4 spaces");

  is(sp.editor.getCaretOffset(), 10, "caret location is correct");

  // Test the new insertTextAtCaret() method.

  sp.insertTextAtCaret("omg");

  is(sp.getText(), "     w    omgindow.foo;", "insertTextAtCaret() works");

  is(sp.editor.getCaretOffset(), 13, "caret location is correct after update");

  gScratchpadWindow.close();

  Services.prefs.setIntPref("devtools.editor.tabsize", 6);
  Services.prefs.setBoolPref("devtools.editor.expandtab", false);

  gScratchpadWindow = Scratchpad.openScratchpad();
  gScratchpadWindow.addEventListener("load", function onScratchpadLoad() {
    gScratchpadWindow.removeEventListener("load", onScratchpadLoad, false);
    gScratchpadWindow.Scratchpad.addObserver({
      onReady: runTests2
    });
  }, false);
}

function runTests2()
{
  let sp = gScratchpadWindow.Scratchpad;
  sp.removeObserver(this);

  sp.setText("window.foo;");
  sp.editor.setCaretOffset(0);

  EventUtils.synthesizeKey("VK_TAB", {}, gScratchpadWindow);

  is(sp.getText(), "\twindow.foo;", "Tab key added the tab character");

  is(sp.editor.getCaretOffset(), 1, "caret location is correct");

  Services.prefs.clearUserPref("devtools.editor.tabsize");
  Services.prefs.clearUserPref("devtools.editor.expandtab");

  finish();
}
