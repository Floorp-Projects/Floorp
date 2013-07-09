/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that private browsing mode disables the remember option
// for the popup blocker menu.
function test() {
  // initialization
  waitForExplicitFinish();

  let testURI = "http://mochi.test:8888/browser/browser/components/privatebrowsing/test/browser/popup.html";
  let windowsToClose = [];
  let oldPopupPolicy = gPrefService.getBoolPref("dom.disable_open_during_load");
  gPrefService.setBoolPref("dom.disable_open_during_load", true);

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

  function finishTest() {
    // cleanup
    gPrefService.setBoolPref("dom.disable_open_during_load", oldPopupPolicy);
    finish();
  };

  function testOnWindow(options, callback) {
    let win = whenNewWindowLoaded(options, callback);
    windowsToClose.push(win);
  };

  registerCleanupFunction(function() {
    windowsToClose.forEach(function(win) {
      win.close();
    });
  });

  testOnWindow({}, function(win) {
    testPopupBlockerMenuItem(false, win,
      testOnWindow({private: true}, function(win) {
        testPopupBlockerMenuItem(true, win,
          testOnWindow({}, function(win) {
            testPopupBlockerMenuItem(false, win, finishTest);
          })
        );
      })
    );
  });
}
