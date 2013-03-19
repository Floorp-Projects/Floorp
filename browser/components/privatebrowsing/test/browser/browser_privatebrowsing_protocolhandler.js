/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that the web pages can't register protocol handlers
// inside the private browsing mode.

function test() {
  // initialization
  waitForExplicitFinish();
  let windowsToClose = [];
  let notificationValue = "Protocol Registration: testprotocol";
  let testURI = "http://example.com/browser/" +
    "browser/components/privatebrowsing/test/browser/browser_privatebrowsing_protocolhandler_page.html";

  function doTest(aIsPrivateMode, aWindow, aCallback) {
    aWindow.gBrowser.selectedBrowser.addEventListener("load", function onLoad() {
      aWindow.gBrowser.selectedBrowser.removeEventListener("load", onLoad, true);

      setTimeout(function() {
        let notificationBox = aWindow.gBrowser.getNotificationBox();
        let notification = notificationBox.getNotificationWithValue(notificationValue);

        if (aIsPrivateMode) {
          // Make sure the notification is correctly displayed without a remember control
          ok(!notification, "Notification box should not be displayed inside of private browsing mode");
        } else {
          // Make sure the notification is correctly displayed with a remember control
          ok(notification, "Notification box should be displaying outside of private browsing mode");
        }

        aCallback();
      }, 100); // remember control is added in a setTimeout(0) call
    }, true);

    aWindow.gBrowser.selectedBrowser.loadURI(testURI);
  }

  function testOnWindow(aOptions, aCallback) {
    whenNewWindowLoaded(aOptions, function(aWin) {
      windowsToClose.push(aWin);
      // execute should only be called when need, like when you are opening
      // web pages on the test. If calling executeSoon() is not necesary, then
      // call whenNewWindowLoaded() instead of testOnWindow() on your test.
      executeSoon(function() aCallback(aWin));
    });
  };

   // this function is called after calling finish() on the test.
  registerCleanupFunction(function() {
    windowsToClose.forEach(function(aWin) {
      aWin.close();
    });
  });

  // test first when not on private mode
  testOnWindow({}, function(aWin) {
    doTest(false, aWin, function() {
      // then test when on private mode
      testOnWindow({private: true}, function(aWin) {
        doTest(true, aWin, finish);
      });
    });
  });
}
