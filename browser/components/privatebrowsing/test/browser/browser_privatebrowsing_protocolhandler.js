/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that the web pages can't register protocol handlers
// inside the private browsing mode.

function test() {
  // initialization
  let pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);

  const testPageURL = "http://example.com/browser/" +
    "browser/components/privatebrowsing/test/browser/browser_privatebrowsing_protocolhandler_page.html";
  waitForExplicitFinish();

  const notificationValue = "Protocol Registration: testprotocol";

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function () {
    gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);

    setTimeout(function() {
      // Make sure the notification is correctly displayed with a remember control
      let notificationBox = gBrowser.getNotificationBox();
      let notification = notificationBox.getNotificationWithValue(notificationValue);
      ok(notification, "Notification box should be displaying outside of private browsing mode");
      gBrowser.removeCurrentTab();

      // enter the private browsing mode
      pb.privateBrowsingEnabled = true;

      gBrowser.selectedTab = gBrowser.addTab();
      gBrowser.selectedBrowser.addEventListener("load", function () {
        gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);

        setTimeout(function () {
          // Make sure the notification is correctly displayed without a remember control
          let notificationBox = gBrowser.getNotificationBox();
          let notification = notificationBox.getNotificationWithValue(notificationValue);
          ok(!notification, "Notification box should not be displayed inside of private browsing mode");

          gBrowser.removeCurrentTab();

          // cleanup
          pb.privateBrowsingEnabled = false;
          finish();
        }, 100); // remember control is added in a setTimeout(0) call
      }, true);
      content.location = testPageURL;
    }, 100); // remember control is added in a setTimeout(0) call
  }, true);
  content.location = testPageURL;
}
