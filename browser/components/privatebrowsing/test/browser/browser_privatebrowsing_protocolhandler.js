/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that the web pages can't register protocol handlers
// inside the private browsing mode.

add_task(function* test() {
  let notificationValue = "Protocol Registration: testprotocol";
  let testURI = "http://example.com/browser/" +
    "browser/components/privatebrowsing/test/browser/browser_privatebrowsing_protocolhandler_page.html";

  let doTest = Task.async(function* (aIsPrivateMode, aWindow) {
    let tab = aWindow.gBrowser.selectedTab = aWindow.gBrowser.addTab(testURI);
    yield BrowserTestUtils.browserLoaded(tab.linkedBrowser);

    let promiseFinished = PromiseUtils.defer();
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

      promiseFinished.resolve();
    }, 100); // remember control is added in a setTimeout(0) call

    yield promiseFinished.promise;
  });

  // test first when not on private mode
  let win = yield BrowserTestUtils.openNewBrowserWindow();
  yield doTest(false, win);

  // then test when on private mode
  let privateWin = yield BrowserTestUtils.openNewBrowserWindow({private: true});
  yield doTest(true, privateWin);

  // Cleanup
  yield BrowserTestUtils.closeWindow(win);
  yield BrowserTestUtils.closeWindow(privateWin);
});
