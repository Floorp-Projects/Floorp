/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the developer toolbar works properly

const URL = "http://example.com/browser/browser/devtools/shared/test/browser_toolbar_basic.html";

function test() {
  addTab(URL, function(browser, tab) {
    info("Starting browser_toolbar_basic.js");
    runTest();
  });
}

function runTest() {
  Services.obs.addObserver(checkOpen, DeveloperToolbar.NOTIFICATIONS.SHOW, false);
  // TODO: reopen the window so the pref has a chance to take effect
  // EventUtils.synthesizeKey("v", { ctrlKey: true, shiftKey: true });
  DeveloperToolbarTest.show();
}

function checkOpen() {
  Services.obs.removeObserver(checkOpen, DeveloperToolbar.NOTIFICATIONS.SHOW, false);
  ok(DeveloperToolbar.visible, "DeveloperToolbar is visible");

  Services.obs.addObserver(checkClosed, DeveloperToolbar.NOTIFICATIONS.HIDE, false);
  // EventUtils.synthesizeKey("v", { ctrlKey: true, shiftKey: true });
  DeveloperToolbarTest.hide();
}

function checkClosed() {
  Services.obs.removeObserver(checkClosed, DeveloperToolbar.NOTIFICATIONS.HIDE, false);
  ok(!DeveloperToolbar.visible, "DeveloperToolbar is not visible");

  finish();
}
