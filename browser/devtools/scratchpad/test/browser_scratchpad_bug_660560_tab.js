/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Reference to the Scratchpad chrome window object.
let gScratchpadWindow;

function test()
{
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function() {
    gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);

    ok(window.Scratchpad, "Scratchpad variable exists");

    Services.prefs.setIntPref("devtools.editor.tabsize", 5);

    gScratchpadWindow = Scratchpad.openScratchpad();
    gScratchpadWindow.addEventListener("load", runTests, false);
  }, true);

  content.location = "data:text/html,Scratchpad test for the Tab key, bug 660560";
}

function runTests()
{
  gScratchpadWindow.removeEventListener("load", arguments.callee, false);

  let sp = gScratchpadWindow.Scratchpad;
  ok(sp, "Scratchpad object exists in new window");

  is(gScratchpadWindow.document.activeElement, sp.textbox.inputField,
     "The textbox has focus");

  is(sp.textbox.style.MozTabSize, 5, "-moz-tab-size is correct");

  sp.textbox.value = "window.foo;";
  sp.selectRange(1, 3);

  EventUtils.synthesizeKey("VK_TAB", {}, gScratchpadWindow);

  is(sp.textbox.value, "w     dow.foo;",
     "Tab key added 5 spaces");

  is(sp.textbox.selectionStart, 6, "caret location is correct");

  is(sp.textbox.selectionStart, sp.textbox.selectionEnd,
     "caret location is correct, confirmed");

  // Test the new insertTextAtCaret() method.

  sp.insertTextAtCaret("omg");

  is(sp.textbox.value, "w     omgdow.foo;", "insertTextAtCaret() works");

  is(sp.textbox.selectionStart, 9, "caret location is correct after update");

  is(sp.textbox.selectionStart, sp.textbox.selectionEnd,
     "caret location is correct, confirmed");

  gScratchpadWindow.close();

  Services.prefs.setIntPref("devtools.editor.tabsize", 6);
  Services.prefs.setBoolPref("devtools.editor.expandtab", false);
  gScratchpadWindow = Scratchpad.openScratchpad();
  gScratchpadWindow.addEventListener("load", runTests2, false);
}

function runTests2()
{
  gScratchpadWindow.removeEventListener("load", arguments.callee, false);

  let sp = gScratchpadWindow.Scratchpad;

  is(sp.textbox.style.MozTabSize, 6, "-moz-tab-size is correct");

  sp.textbox.value = "window.foo;";
  sp.selectRange(1, 3);

  EventUtils.synthesizeKey("VK_TAB", {}, gScratchpadWindow);

  is(sp.textbox.value, "w\tdow.foo;", "Tab key added the tab character");

  is(sp.textbox.selectionStart, 2, "caret location is correct");

  is(sp.textbox.selectionStart, sp.textbox.selectionEnd,
     "caret location is correct, confirmed");

  gScratchpadWindow.close();

  // check with an invalid tabsize value.
  Services.prefs.setIntPref("devtools.editor.tabsize", 0);
  Services.prefs.setBoolPref("devtools.editor.expandtab", true);
  gScratchpadWindow = Scratchpad.openScratchpad();
  gScratchpadWindow.addEventListener("load", runTests3, false);
}

function runTests3()
{
  gScratchpadWindow.removeEventListener("load", arguments.callee, false);

  let sp = gScratchpadWindow.Scratchpad;

  is(sp.textbox.style.MozTabSize, 4, "-moz-tab-size is correct");

  Services.prefs.clearUserPref("devtools.editor.tabsize");
  Services.prefs.clearUserPref("devtools.editor.expandtab");

  gScratchpadWindow.close();
  gScratchpadWindow = null;

  gBrowser.removeCurrentTab();
  finish();
}
