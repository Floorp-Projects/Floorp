/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* Bug 660560 */

"use strict";

function test() {
  waitForExplicitFinish();

  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  gBrowser.selectedBrowser.addEventListener("load", function () {
    Services.prefs.setIntPref("devtools.editor.tabsize", 5);

    openScratchpad(runTests);
  }, {capture: true, once: true});

  content.location = "data:text/html,Scratchpad test for the Tab key, bug 660560";
}

function runTests() {
  let sp = gScratchpadWindow.Scratchpad;
  ok(sp, "Scratchpad object exists in new window");

  ok(sp.editor.hasFocus(), "the editor has focus");

  sp.setText("abcd");
  sp.editor.setCursor({ line: 0, ch: 0 });

  EventUtils.synthesizeKey("VK_TAB", {}, gScratchpadWindow);

  is(sp.getText(), "     " + "abcd", "Tab key added 5 spaces");

  is(sp.editor.getCursor().line, 0, "line is correct");
  is(sp.editor.getCursor().ch, 5, "character is correct");

  // Keep cursor at char 5 and press TAB.
  EventUtils.synthesizeKey("VK_TAB", {}, gScratchpadWindow);

  is(sp.getText(), "     " + "     " + "abcd", "Tab key added 5 spaces");
  is(sp.editor.getCursor().line, 0, "line is correct");
  is(sp.editor.getCursor().ch, 10, "character is correct");

  // Move cursor after "a" and press TAB, only 4 spaces should be added.
  sp.editor.setCursor({ line: 0, ch: 11 });

  EventUtils.synthesizeKey("VK_TAB", {}, gScratchpadWindow);
  is(sp.getText(), "     " + "     " + "a    bcd", "Tab key added 4 spaces");
  is(sp.editor.getCursor().line, 0, "line is correct");
  is(sp.editor.getCursor().ch, 15, "character is correct");

  // Move cursor after "bc" and press TAB, only 3 spaces should be added
  sp.editor.setCursor({ line: 0, ch: 17 });

  EventUtils.synthesizeKey("VK_TAB", {}, gScratchpadWindow);
  is(sp.getText(), "     " + "     " + "a    bc   d", "Tab key added 3 spaces");
  is(sp.editor.getCursor().line, 0, "line is correct");
  is(sp.editor.getCursor().ch, 20, "character is correct");

  gScratchpadWindow.close();

  Services.prefs.setIntPref("devtools.editor.tabsize", 6);
  Services.prefs.setBoolPref("devtools.editor.expandtab", false);

  openScratchpad(runTests2);
}

function runTests2() {
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
