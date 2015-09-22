/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* Bug 650760 */

function test()
{
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  content.location = "data:text/html,Test keybindings for opening Scratchpad MDN Documentation, bug 650760";
  gBrowser.selectedBrowser.addEventListener("load", function onTabLoad() {
    gBrowser.selectedBrowser.removeEventListener("load", onTabLoad, true);

    ok(window.Scratchpad, "Scratchpad variable exists");

    openScratchpad(runTest);
  }, true);
}

function runTest()
{
  let sp = gScratchpadWindow.Scratchpad;
  ok(sp, "Scratchpad object exists in new window");
  ok(sp.editor.hasFocus(), "the editor has focus");

  let keyid = gScratchpadWindow.document.getElementById("key_openHelp");
  let modifiers = keyid.getAttribute("modifiers");

  let key = null;
  if (keyid.getAttribute("keycode"))
    key = keyid.getAttribute("keycode");

  else if (keyid.getAttribute("key"))
    key = keyid.getAttribute("key");

  isnot(key, null, "Successfully retrieved keycode/key");

  var aEvent = {
    shiftKey: modifiers.match("shift"),
    ctrlKey: modifiers.match("ctrl"),
    altKey: modifiers.match("alt"),
    metaKey: modifiers.match("meta"),
    accelKey: modifiers.match("accel")
  }

  info("check that the MDN page is opened on \"F1\"");
  let linkClicked = false;
  sp.openDocumentationPage = function(event) { linkClicked = true; };

  EventUtils.synthesizeKey(key, aEvent, gScratchpadWindow);

  is(linkClicked, true, "MDN page will open");
  finishTest();
}

function finishTest()
{
  gScratchpadWindow.close();
  finish();
}
