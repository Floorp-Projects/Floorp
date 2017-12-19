/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that private browsing mode disables the remember option
// for the popup blocker menu.
add_task(async function test() {
  let testURI = "http://mochi.test:8888/browser/browser/components/privatebrowsing/test/browser/popup.html";
  let oldPopupPolicy = Services.prefs.getBoolPref("dom.disable_open_during_load");
  Services.prefs.setBoolPref("dom.disable_open_during_load", true);

  registerCleanupFunction(() => {
    Services.prefs.setBoolPref("dom.disable_open_during_load", oldPopupPolicy);
  });

  function testPopupBlockerMenuItem(aExpectedDisabled, aWindow, aCallback) {

    aWindow.gBrowser.addEventListener("DOMUpdatePageReport", function() {
      executeSoon(function() {
        let notification = aWindow.gBrowser.getNotificationBox().getNotificationWithValue("popup-blocked");
        ok(notification, "The notification box should be displayed");

        function checkMenuItem(callback) {
          dump("CMI: in\n");
          aWindow.document.addEventListener("popupshown", function listener(event) {
            dump("CMI: popupshown\n");
            aWindow.document.removeEventListener("popupshown", listener);

            if (aExpectedDisabled)
              is(aWindow.document.getElementById("blockedPopupAllowSite").getAttribute("disabled"), "true",
                 "The allow popups menu item should be disabled");

            event.originalTarget.hidePopup();
            dump("CMI: calling back\n");
            callback();
            dump("CMI: called back\n");
          });
          dump("CMI: out\n");
        }

        checkMenuItem(function() {
          aCallback();
        });
        notification.querySelector("button").doCommand();
      });

    }, {once: true});

    aWindow.gBrowser.selectedBrowser.loadURI(testURI);
  }

  let win1 = await BrowserTestUtils.openNewBrowserWindow();
  await new Promise(resolve => waitForFocus(resolve, win1));
  await new Promise(resolve => testPopupBlockerMenuItem(false, win1, resolve));

  let win2 = await BrowserTestUtils.openNewBrowserWindow({private: true});
  await new Promise(resolve => waitForFocus(resolve, win2));
  await new Promise(resolve => testPopupBlockerMenuItem(true, win2, resolve));

  let win3 = await BrowserTestUtils.openNewBrowserWindow();
  await new Promise(resolve => waitForFocus(resolve, win3));
  await new Promise(resolve => testPopupBlockerMenuItem(false, win3, resolve));

  // Cleanup
  await BrowserTestUtils.closeWindow(win1);
  await BrowserTestUtils.closeWindow(win2);
  await BrowserTestUtils.closeWindow(win3);
});
