/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* Bug 660560 */

function test()
{
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onTabLoad() {
    gBrowser.selectedBrowser.removeEventListener("load", onTabLoad, true);

    ok(window.Scratchpad, "Scratchpad variable exists");

    Services.prefs.setIntPref("devtools.editor.tabsize", 5);

    openScratchpad(runTests);
  }, true);

  content.location = "data:text/html,Scratchpad test for the Tab key, bug 660560";
}

function runTests()
{
  let sp = gScratchpadWindow.Scratchpad;
  ok(sp, "Scratchpad object exists in new window");

  ok(sp.editor.hasFocus(), "the editor has focus");

  sp.setText("window.foo;");
  sp.editor.setCursor({ line: 0, ch: 0 });

  EventUtils.synthesizeKey("VK_TAB", {}, gScratchpadWindow);

  is(sp.getText(), "     window.foo;", "Tab key added 5 spaces");

  is(sp.editor.getCursor().line, 0, "line is correct");
  is(sp.editor.getCursor().ch, 5, "character is correct");

  sp.editor.setCursor({ line: 0, ch: 6 });

  EventUtils.synthesizeKey("VK_TAB", {}, gScratchpadWindow);

  is(sp.getText(), "     w    indow.foo;",
     "Tab key added 4 spaces");

  is(sp.editor.getCursor().line, 0, "line is correct");
  is(sp.editor.getCursor().ch, 10, "character is correct");

  gScratchpadWindow.close();

  Services.prefs.setIntPref("devtools.editor.tabsize", 6);
  Services.prefs.setBoolPref("devtools.editor.expandtab", false);

  openScratchpad(runTests2);
}

function runTests2()
{
  let sp = gScratchpadWindow.Scratchpad;

  sp.setText("window.foo;");
  sp.editor.setCursor({ line: 0, ch: 0 });

  EventUtils.synthesizeKey("VK_TAB", {}, gScratchpadWindow);

  is(sp.getText(), "\twindow.foo;", "Tab key added the tab character");

  is(sp.editor.getCursor().line, 0, "line is correct");
  is(sp.editor.getCursor().ch, 1, "character is correct");

  Services.prefs.clearUserPref("devtools.editor.tabsize");
  Services.prefs.clearUserPref("devtools.editor.expandtab");

  finish();
}
