/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that private browsing mode disables the remember option
// for the popup blocker menu.
add_task(function* test() {
  let testURI = "http://mochi.test:8888/browser/browser/components/privatebrowsing/test/browser/popup.html";
  let oldPopupPolicy = gPrefService.getBoolPref("dom.disable_open_during_load");
  gPrefService.setBoolPref("dom.disable_open_during_load", true);

  registerCleanupFunction(() => {
    gPrefService.setBoolPref("dom.disable_open_during_load", oldPopupPolicy);
  });

  function testPopupBlockerMenuItem(aExpectedDisabled, aWindow, aCallback) {

    aWindow.gBrowser.addEventListener("DOMUpdatePageReport", function() {
      aWindow.gBrowser.removeEventListener("DOMUpdatePageReport", arguments.callee, false);

      executeSoon(function() {
        let notification = aWindow.gBrowser.getNotificationBox().getNotificationWithValue("popup-blocked");
        ok(notification, "The notification box should be displayed");

        function checkMenuItem(callback) {
          dump("CMI: in\n");
          aWindow.document.addEventListener("popupshown", function(event) {
            dump("CMI: popupshown\n");
            aWindow.document.removeEventListener("popupshown", arguments.callee, false);

            if (aExpectedDisabled)
              is(aWindow.document.getElementById("blockedPopupAllowSite").getAttribute("disabled"), "true",
                 "The allow popups menu item should be disabled");

            event.originalTarget.hidePopup();
            dump("CMI: calling back\n");
            callback();
            dump("CMI: called back\n");
          }, false);
          dump("CMI: out\n");
        }

        checkMenuItem(function() {
          aCallback();
        });
        notification.querySelector("button").doCommand();
      });

    }, false);

    aWindow.gBrowser.selectedBrowser.loadURI(testURI);
  }

  let win1 = yield BrowserTestUtils.openNewBrowserWindow();
  yield new Promise(resolve => waitForFocus(resolve, win1));
  yield new Promise(resolve => testPopupBlockerMenuItem(false, win1, resolve));

  let win2 = yield BrowserTestUtils.openNewBrowserWindow({private: true});
  yield new Promise(resolve => waitForFocus(resolve, win2));
  yield new Promise(resolve => testPopupBlockerMenuItem(true, win2, resolve));

  let win3 = yield BrowserTestUtils.openNewBrowserWindow();
  yield new Promise(resolve => waitForFocus(resolve, win3));
  yield new Promise(resolve => testPopupBlockerMenuItem(false, win3, resolve));

  // Cleanup
  yield BrowserTestUtils.closeWindow(win1);
  yield BrowserTestUtils.closeWindow(win2);
  yield BrowserTestUtils.closeWindow(win3);
});
