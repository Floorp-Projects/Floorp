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

      aCallback();
    });
  };

  let windowsToClose = [];
  function testOnWindow(options, callback) {
    let win = OpenBrowserWindow(options);
    win.addEventListener("load", function onLoad() {
      win.removeEventListener("load", onLoad, false);
      windowsToClose.push(win);
      callback(win);
    }, false);
  };

  registerCleanupFunction(function() {
    windowsToClose.forEach(function(win) {
      win.close();
    });
  });

  testOnWindow({private: true}, function(win) {
    checkDisableOption(true, win, function() {
      testOnWindow({}, function(win) {
        checkDisableOption(false, win, finish);
      });
    });
  });
}
