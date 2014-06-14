/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the developer toolbar works properly

const TEST_URI = TEST_URI_ROOT + "browser_toolbar_basic.html";

function test() {
  addTab(TEST_URI, function(browser, tab) {
    info("Starting browser_toolbar_basic.js");
    runTest();
  });
}

function runTest() {
  ok(!DeveloperToolbar.visible, "DeveloperToolbar is not visible in runTest");

  oneTimeObserve(DeveloperToolbar.NOTIFICATIONS.SHOW, catchFail(checkOpen));
  document.getElementById("Tools:DevToolbar").doCommand();
}

function isChecked(b) {
  return b.getAttribute("checked") == "true";
}

function checkOpen() {
  ok(DeveloperToolbar.visible, "DeveloperToolbar is visible in checkOpen");
  let close = document.getElementById("developer-toolbar-closebutton");
  ok(close, "Close button exists");

  let toggleToolbox =
    document.getElementById("devtoolsMenuBroadcaster_DevToolbox");
  ok(!isChecked(toggleToolbox), "toggle toolbox button is not checked");

  let target = TargetFactory.forTab(gBrowser.selectedTab);
  gDevTools.showToolbox(target, "inspector").then(function(toolbox) {
    ok(isChecked(toggleToolbox), "toggle toolbox button is checked");

    addTab("about:blank", function(browser, tab) {
      info("Opened a new tab");

      ok(!isChecked(toggleToolbox), "toggle toolbox button is not checked");

      gBrowser.removeCurrentTab();

      oneTimeObserve(DeveloperToolbar.NOTIFICATIONS.HIDE, catchFail(checkClosed));
      document.getElementById("Tools:DevToolbar").doCommand();
    });
  });
}

function checkClosed() {
  ok(!DeveloperToolbar.visible, "DeveloperToolbar is not visible in checkClosed");

  oneTimeObserve(DeveloperToolbar.NOTIFICATIONS.SHOW, catchFail(checkReOpen));
  document.getElementById("Tools:DevToolbar").doCommand();
}

function checkReOpen() {
  ok(DeveloperToolbar.visible, "DeveloperToolbar is visible in checkReOpen");

  let toggleToolbox =
    document.getElementById("devtoolsMenuBroadcaster_DevToolbox");
  ok(isChecked(toggleToolbox), "toggle toolbox button is checked");

  oneTimeObserve(DeveloperToolbar.NOTIFICATIONS.HIDE, catchFail(checkReClosed));
  document.getElementById("developer-toolbar-closebutton").doCommand();
}

function checkReClosed() {
  ok(!DeveloperToolbar.visible, "DeveloperToolbar is not visible in checkReClosed");

  finish();
}
