/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

var tab1;
var tab2;
var sp;

function test()
{
  waitForExplicitFinish();

  tab1 = BrowserTestUtils.addTab(gBrowser);
  gBrowser.selectedTab = tab1;
  gBrowser.selectedBrowser.addEventListener("load", function () {
    tab2 = BrowserTestUtils.addTab(gBrowser);
    gBrowser.selectedTab = tab2;
    gBrowser.selectedBrowser.addEventListener("load", function () {
      openScratchpad(runTests);
    }, {capture: true, once: true});
    content.location = "data:text/html,test context switch in Scratchpad tab 2";
  }, {capture: true, once: true});

  content.location = "data:text/html,test context switch in Scratchpad tab 1";
}

function runTests()
{
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

  isnot(browserMenu.getAttribute("checked"), "true",
     "chrome menuitem is not checked");

  is(notificationBox.currentNotification, null,
     "there is no notification currently shown for content context");

  sp.setText("window.foosbug653108 = 'aloha';");

  ok(!content.wrappedJSObject.foosbug653108,
     "no content.foosbug653108");

  sp.run().then(function () {
    is(content.wrappedJSObject.foosbug653108, "aloha",
       "content.foosbug653108 has been set");

    gBrowser.tabContainer.addEventListener("TabSelect", runTests2, true);
    gBrowser.selectedTab = tab1;
  });
}

function runTests2() {
  gBrowser.tabContainer.removeEventListener("TabSelect", runTests2, true);

  ok(!window.foosbug653108, "no window.foosbug653108");

  sp.setText("window.foosbug653108");
  sp.run().then(function ([, , result]) {
    isnot(result, "aloha", "window.foosbug653108 is not aloha");

    sp.setText("window.foosbug653108 = 'ahoyhoy';");
    sp.run().then(function () {
      is(content.wrappedJSObject.foosbug653108, "ahoyhoy",
         "content.foosbug653108 has been set 2");

      gBrowser.selectedBrowser.addEventListener("load", runTests3, true);
      content.location = "data:text/html,test context switch in Scratchpad location 2";
    });
  });
}

function runTests3() {
  gBrowser.selectedBrowser.removeEventListener("load", runTests3, true);
  // Check that the sandbox is not cached.

  sp.setText("typeof foosbug653108;");
  sp.run().then(function ([, , result]) {
    is(result, "undefined", "global variable does not exist");

    tab1 = null;
    tab2 = null;
    sp = null;
    finish();
  });
}
