/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-disable mozilla/no-arbitrary-setTimeout */

// This test makes sure that the web pages can't register protocol handlers
// inside the private browsing mode.

add_task(async function test() {
  let notificationValue = "Protocol Registration: web+testprotocol";
  let testURI =
    "https://example.com/browser/" +
    "browser/components/privatebrowsing/test/browser/browser_privatebrowsing_protocolhandler_page.html";

  let doTest = async function (aIsPrivateMode, aWindow) {
    let tab = (aWindow.gBrowser.selectedTab = BrowserTestUtils.addTab(
      aWindow.gBrowser,
      testURI
    ));
    await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

    let promiseFinished = Promise.withResolvers();
    setTimeout(function () {
      let notificationBox = aWindow.gBrowser.getNotificationBox();
      let notification =
        notificationBox.getNotificationWithValue(notificationValue);

      if (aIsPrivateMode) {
        // Make sure the notification is correctly displayed without a remember control
        ok(
          !notification,
          "Notification box should not be displayed inside of private browsing mode"
        );
      } else {
        // Make sure the notification is correctly displayed with a remember control
        ok(
          notification,
          "Notification box should be displaying outside of private browsing mode"
        );
      }

      promiseFinished.resolve();
    }, 100); // remember control is added in a setTimeout(0) call

    await promiseFinished.promise;
  };

  // test first when not on private mode
  let win = await BrowserTestUtils.openNewBrowserWindow();
  await doTest(false, win);

  // then test when on private mode
  let privateWin = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  await doTest(true, privateWin);

  // Cleanup
  await BrowserTestUtils.closeWindow(win);
  await BrowserTestUtils.closeWindow(privateWin);
});
