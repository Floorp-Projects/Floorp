/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that the Clear Recent History menu item and command 
// is disabled inside the private browsing mode.

function test() {
  waitForExplicitFinish();

  function checkDisableOption(aPrivateMode, aWindow, aCallback) {
    executeSoon(function() {
      let crhCommand = aWindow.document.getElementById("Tools:Sanitize");
      ok(crhCommand, "The clear recent history command should exist");

      is(PrivateBrowsingUtils.isWindowPrivate(aWindow), aPrivateMode,
        "PrivateBrowsingUtils should report the correct per-window private browsing status");
      is(crhCommand.hasAttribute("disabled"), aPrivateMode,
        "Clear Recent History command should be disabled according to the private browsing mode");

      executeSoon(aCallback);
    });
  };

  let windowsToClose = [];
  let testURI = "about:blank";

  function testOnWindow(aIsPrivate, aCallback) {
    whenNewWindowLoaded({private: aIsPrivate}, function(aWin) {
      windowsToClose.push(aWin);
      aWin.gBrowser.selectedBrowser.addEventListener("load", function onLoad() {
        if (aWin.content.location.href != testURI) {
          aWin.gBrowser.loadURI(testURI);
          return;
        }
        aWin.gBrowser.selectedBrowser.removeEventListener("load", onLoad, true);
        executeSoon(function() aCallback(aWin));
      }, true);

      aWin.gBrowser.loadURI(testURI);
    });
  };

  registerCleanupFunction(function() {
    windowsToClose.forEach(function(win) {
      win.close();
    });
  });

  testOnWindow(true, function(win) {
    info("Test on private window");
    checkDisableOption(true, win, function() {
      testOnWindow(false, function(win) {
        info("Test on public window");
        checkDisableOption(false, win, finish);
      });
    });
  });
}
