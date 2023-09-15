/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that the geolocation prompt does not show a remember
// control inside the private browsing mode.

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.vr.always_support_vr", true]],
  });
});

add_task(async function test() {
  function checkPrompt(aURL, aName, aPrivateMode, aWindow) {
    return (async function () {
      aWindow.gBrowser.selectedTab = BrowserTestUtils.addTab(
        aWindow.gBrowser,
        aURL
      );
      await BrowserTestUtils.browserLoaded(aWindow.gBrowser.selectedBrowser);

      let notification = aWindow.PopupNotifications.getNotification(aName);

      // Wait until the notification is available.
      while (!notification) {
        await new Promise(resolve => {
          executeSoon(resolve);
        });
        notification = aWindow.PopupNotifications.getNotification(aName);
      }

      if (aPrivateMode) {
        // Make sure the notification is correctly displayed without a remember control
        ok(
          !notification.options.checkbox.show,
          "Secondary actions should not exist (always/never remember)"
        );
      } else {
        ok(
          notification.options.checkbox.show,
          "Secondary actions should exist (always/never remember)"
        );
      }
      notification.remove();

      aWindow.gBrowser.removeCurrentTab();
    })();
  }

  function checkPrivateBrowsingRememberPrompt(aURL, aName) {
    return (async function () {
      let win = await BrowserTestUtils.openNewBrowserWindow();
      let browser = win.gBrowser.selectedBrowser;
      BrowserTestUtils.startLoadingURIString(browser, aURL);
      await BrowserTestUtils.browserLoaded(browser);

      await checkPrompt(aURL, aName, false, win);

      let privateWin = await BrowserTestUtils.openNewBrowserWindow({
        private: true,
      });
      let privateBrowser = privateWin.gBrowser.selectedBrowser;
      BrowserTestUtils.startLoadingURIString(privateBrowser, aURL);
      await BrowserTestUtils.browserLoaded(privateBrowser);

      await checkPrompt(aURL, aName, true, privateWin);

      // Cleanup
      await BrowserTestUtils.closeWindow(win);
      await BrowserTestUtils.closeWindow(privateWin);
    })();
  }

  const geoTestPageURL =
    "https://example.com/browser/" +
    "browser/components/privatebrowsing/test/browser/browser_privatebrowsing_geoprompt_page.html";

  await checkPrivateBrowsingRememberPrompt(geoTestPageURL, "geolocation");

  const vrEnabled = Services.prefs.getBoolPref("dom.vr.enabled");

  if (vrEnabled) {
    const xrTestPageURL =
      "https://example.com/browser/" +
      "browser/components/privatebrowsing/test/browser/browser_privatebrowsing_xrprompt_page.html";

    await checkPrivateBrowsingRememberPrompt(xrTestPageURL, "xr");
  }
});
