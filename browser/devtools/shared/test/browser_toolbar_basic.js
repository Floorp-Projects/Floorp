/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the developer toolbar works properly

let imported = {};
Components.utils.import("resource:///modules/HUDService.jsm", imported);
registerCleanupFunction(function() {
  imported = {};
});

const TEST_URI = "http://example.com/browser/browser/devtools/shared/test/browser_toolbar_basic.html";

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

function checkOpen() {
  ok(DeveloperToolbar.visible, "DeveloperToolbar is visible in checkOpen");

  let close = document.getElementById("developer-toolbar-closebutton");
  let webconsole = document.getElementById("developer-toolbar-webconsole");
  let inspector = document.getElementById("developer-toolbar-inspector");
  let debuggr = document.getElementById("developer-toolbar-debugger");

  ok(close, "Close button exists");

  ok(!webconsole.checked, "web console button state 1");
  ok(!inspector.checked, "inspector button state 1");
  ok(!debuggr.checked, "debugger button state 1");

  document.getElementById("Tools:WebConsole").doCommand();

  ok(webconsole.checked, "web console button state 2");
  ok(!inspector.checked, "inspector button state 2");
  ok(!debuggr.checked, "debugger button state 2");

  document.getElementById("Tools:Inspect").doCommand();

  ok(webconsole.checked, "web console button state 3");
  ok(inspector.checked, "inspector button state 3");
  ok(!debuggr.checked, "debugger button state 3");

  // Christmas tree!

  // The web console opens synchronously, but closes asynchronously.
  let hud = imported.HUDService.getHudByWindow(content);
  imported.HUDService.disableAnimation(hud.hudId);

  document.getElementById("Tools:WebConsole").doCommand();

  ok(!webconsole.checked, "web console button state 6");
  ok(inspector.checked, "inspector button state 6");
  ok(!debuggr.checked, "debugger button state 6");

  document.getElementById("Tools:Inspect").doCommand();

  ok(!webconsole.checked, "web console button state 7");
  ok(!inspector.checked, "inspector button state 7");
  ok(!debuggr.checked, "debugger button state 7");

  // All closed

  // Check we can open and close and retain button state
  document.getElementById("Tools:Inspect").doCommand();

  ok(!webconsole.checked, "web console button state 8");
  ok(inspector.checked, "inspector button state 8");
  ok(!debuggr.checked, "debugger button state 8");

  oneTimeObserve(DeveloperToolbar.NOTIFICATIONS.HIDE, catchFail(checkClosed));
  document.getElementById("Tools:DevToolbar").doCommand();
}

function checkClosed() {
  ok(!DeveloperToolbar.visible, "DeveloperToolbar is not visible in checkClosed");

  // Check we grok state even when closed
  document.getElementById("Tools:WebConsole").doCommand();

  oneTimeObserve(DeveloperToolbar.NOTIFICATIONS.SHOW, catchFail(checkReOpen));
  document.getElementById("Tools:DevToolbar").doCommand();
}

function checkReOpen() {
  ok(DeveloperToolbar.visible, "DeveloperToolbar is visible in checkReOpen");

  let webconsole = document.getElementById("developer-toolbar-webconsole");
  let inspector = document.getElementById("developer-toolbar-inspector");
  let debuggr = document.getElementById("developer-toolbar-debugger");

  ok(webconsole.checked, "web console button state 9");
  ok(inspector.checked, "inspector button state 9");
  ok(!debuggr.checked, "debugger button state 9");

  oneTimeObserve(DeveloperToolbar.NOTIFICATIONS.HIDE, catchFail(checkReClosed));
  document.getElementById("developer-toolbar-closebutton").doCommand();
}

function checkReClosed() {
  ok(!DeveloperToolbar.visible, "DeveloperToolbar is not visible in checkReClosed");

  finish();
}

//------------------------------------------------------------------------------

function oneTimeObserve(name, callback) {
  var func = function() {
    Services.obs.removeObserver(func, name);
    callback();
  };
  Services.obs.addObserver(func, name, false);
}
