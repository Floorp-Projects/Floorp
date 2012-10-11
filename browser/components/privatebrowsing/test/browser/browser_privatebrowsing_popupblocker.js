/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that private browsing mode disables the remember option
// for the popup blocker menu.

function test() {
  // initialization
  let pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);

  let oldPopupPolicy = gPrefService.getBoolPref("dom.disable_open_during_load");
  gPrefService.setBoolPref("dom.disable_open_during_load", true);

  const TEST_URI = "http://mochi.test:8888/browser/browser/components/privatebrowsing/test/browser/popup.html";

  waitForExplicitFinish();

  function testPopupBlockerMenuItem(expectedDisabled, callback) {
    gBrowser.addEventListener("DOMUpdatePageReport", function() {
      gBrowser.removeEventListener("DOMUpdatePageReport", arguments.callee, false);
      executeSoon(function() {
        let notification = gBrowser.getNotificationBox().getNotificationWithValue("popup-blocked");

        ok(notification, "The notification box should be displayed");

        function checkMenuItem(callback) {
          dump("CMI: in\n");
          document.addEventListener("popupshown", function(event) {
            dump("CMI: popupshown\n");
            document.removeEventListener("popupshown", arguments.callee, false);

            if (expectedDisabled)
              is(document.getElementById("blockedPopupAllowSite").getAttribute("disabled"), "true",
                 "The allow popups menu item should be disabled");

            event.originalTarget.hidePopup();
            dump("CMI: calling back\n");
            callback();
            dump("CMI: called back\n");
          }, false);
          dump("CMI: out\n");
        }

        checkMenuItem(function() {
          gBrowser.removeTab(tab);
          callback();
        });
        notification.querySelector("button").doCommand();
      });
    }, false);

    let tab = gBrowser.addTab(TEST_URI);
    gBrowser.selectedTab = tab;
  }

  testPopupBlockerMenuItem(false, function() {
    pb.privateBrowsingEnabled = true;
    testPopupBlockerMenuItem(true, function() {
      pb.privateBrowsingEnabled = false;
      testPopupBlockerMenuItem(false, function() {
        gPrefService.setBoolPref("dom.disable_open_during_load", oldPopupPolicy);
        finish();
      });
    });
  });
}
