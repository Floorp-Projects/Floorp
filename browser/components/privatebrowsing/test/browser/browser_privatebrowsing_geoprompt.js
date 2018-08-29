/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that the geolocation prompt does not show a remember
// control inside the private browsing mode.

add_task(async function test() {
  const testPageURL = "https://example.com/browser/" +
    "browser/components/privatebrowsing/test/browser/browser_privatebrowsing_geoprompt_page.html";

  function checkGeolocation(aPrivateMode, aWindow) {
    return (async function() {
      aWindow.gBrowser.selectedTab = BrowserTestUtils.addTab(aWindow.gBrowser, testPageURL);
      await BrowserTestUtils.browserLoaded(aWindow.gBrowser.selectedBrowser);

      let notification = aWindow.PopupNotifications.getNotification("geolocation");

      // Wait until the notification is available.
      while (!notification) {
        await new Promise(resolve => { executeSoon(resolve); });
        notification = aWindow.PopupNotifications.getNotification("geolocation");
      }

      if (aPrivateMode) {
        // Make sure the notification is correctly displayed without a remember control
        ok(!notification.options.checkbox.show, "Secondary actions should exist (always/never remember)");
      } else {
        ok(notification.options.checkbox.show, "Secondary actions should exist (always/never remember)");
      }
      notification.remove();

      aWindow.gBrowser.removeCurrentTab();
    })();
  }

  let win = await BrowserTestUtils.openNewBrowserWindow();
  let browser = win.gBrowser.selectedBrowser;
  BrowserTestUtils.loadURI(browser, testPageURL);
  await BrowserTestUtils.browserLoaded(browser);

  await checkGeolocation(false, win);

  let privateWin = await BrowserTestUtils.openNewBrowserWindow({private: true});
  let privateBrowser = privateWin.gBrowser.selectedBrowser;
  BrowserTestUtils.loadURI(privateBrowser, testPageURL);
  await BrowserTestUtils.browserLoaded(privateBrowser);

  await checkGeolocation(true, privateWin);

  // Cleanup
  await BrowserTestUtils.closeWindow(win);
  await BrowserTestUtils.closeWindow(privateWin);
});
