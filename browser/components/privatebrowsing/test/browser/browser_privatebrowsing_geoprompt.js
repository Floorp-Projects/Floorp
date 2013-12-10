/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that the geolocation prompt does not show a remember
// control inside the private browsing mode.

function test() {
  const testPageURL = "http://mochi.test:8888/browser/" +
    "browser/components/privatebrowsing/test/browser/browser_privatebrowsing_geoprompt_page.html";
  waitForExplicitFinish();

    function checkGeolocation(aPrivateMode, aWindow, aCallback) {
    executeSoon(function() {
      aWindow.gBrowser.selectedTab = aWindow.gBrowser.addTab();
      aWindow.gBrowser.selectedBrowser.addEventListener("load", function () {
        if (aWindow.content.location != testPageURL) {
          aWindow.content.location = testPageURL;
          return;
        }
        aWindow.gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);

        function runTest() {
          let notification = aWindow.PopupNotifications.getNotification("geolocation");
          if (!notification) {
            // Wait until the notification is available
            executeSoon(runTest);
            return;
          }
          if (aPrivateMode) {
            // Make sure the notification is correctly displayed without a remember control
            is(notification.secondaryActions.length, 0, "Secondary actions shouldn't exist (always/never remember)");
          } else {
            ok(notification.secondaryActions.length > 1, "Secondary actions should exist (always/never remember)");
          }
          notification.remove();

          aWindow.gBrowser.removeCurrentTab();
          aCallback();
        }
        runTest();
      }, true);
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

  testOnWindow({private: false}, function(win) {
    checkGeolocation(false, win, function() {
      testOnWindow({private: true}, function(win) {
        checkGeolocation(true, win, finish);
      });
    });
  });
}
