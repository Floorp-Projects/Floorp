/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Reference to the Scratchpad chrome window object.
let gScratchpadWindow;
let tab1;
let tab2;
let sp;

function test()
{
  waitForExplicitFinish();

  tab1 = gBrowser.addTab();
  gBrowser.selectedTab = tab1;
  gBrowser.selectedBrowser.addEventListener("load", function() {
    gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);

    tab2 = gBrowser.addTab();
    gBrowser.selectedTab = tab2;
    gBrowser.selectedBrowser.addEventListener("load", function() {
      gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);
      gScratchpadWindow = Scratchpad.openScratchpad();
      gScratchpadWindow.addEventListener("load", runTests, false);
    }, true);
    content.location = "data:text/html,test context switch in Scratchpad tab 2";
  }, true);

  content.location = "data:text/html,test context switch in Scratchpad tab 1";
}

function runTests()
{
  gScratchpadWindow.removeEventListener("load", runTests, true);

  sp = gScratchpadWindow.Scratchpad;

  let contentMenu = gScratchpadWindow.document.getElementById("sp-menu-content");
  let browserMenu = gScratchpadWindow.document.getElementById("sp-menu-browser");
  let notificationBox = sp.notificationBox;

  ok(contentMenu, "found #sp-menu-content");
  ok(browserMenu, "found #sp-menu-browser");
  ok(notificationBox, "found Scratchpad.notificationBox");

  sp.setContentContext();

  is(sp.executionContext, gScratchpadWindow.SCRATCHPAD_CONTEXT_CONTENT,
     "executionContext is content");

  is(contentMenu.getAttribute("checked"), "true",
     "content menuitem is checked");

  ok(!browserMenu.hasAttribute("checked"),
     "chrome menuitem is not checked");

  is(notificationBox.currentNotification, null,
     "there is no notification currently shown for content context");

  sp.setText("window.foosbug653108 = 'aloha';");

  ok(!content.wrappedJSObject.foosbug653108,
     "no content.foosbug653108");

  sp.run();

  is(content.wrappedJSObject.foosbug653108, "aloha",
     "content.foosbug653108 has been set");

  gBrowser.tabContainer.addEventListener("TabSelect", runTests2, true);
  gBrowser.selectedTab = tab1;
}

function runTests2() {
  gBrowser.tabContainer.removeEventListener("TabSelect", runTests2, true);

  ok(!window.foosbug653108, "no window.foosbug653108");

  sp.setText("window.foosbug653108");
  let result = sp.run();

  isnot(result, "aloha", "window.foosbug653108 is not aloha");

  sp.setText("window.foosbug653108 = 'ahoyhoy';");
  sp.run();

  is(content.wrappedJSObject.foosbug653108, "ahoyhoy",
     "content.foosbug653108 has been set 2");

  gBrowser.selectedBrowser.addEventListener("load", runTests3, true);
  content.location = "data:text/html,test context switch in Scratchpad location 2";
}

function runTests3() {
  gBrowser.selectedBrowser.removeEventListener("load", runTests3, true);
  // Check that the sandbox is not cached.

  sp.setText("typeof foosbug653108;");
  is(sp.run()[2], "undefined", "global variable does not exist");

  tab1 = null;
  tab2 = null;
  sp = null;
  finish();
}
