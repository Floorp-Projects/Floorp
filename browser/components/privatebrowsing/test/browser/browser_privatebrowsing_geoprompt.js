/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that the geolocation prompt does not show a remember
// control inside the private browsing mode.

function test() {
  // initialization
  let pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);

  const testPageURL = "http://mochi.test:8888/browser/" +
    "browser/components/privatebrowsing/test/browser/browser_privatebrowsing_geoprompt_page.html";
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function () {
    gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);

    let notification = PopupNotifications.getNotification("geolocation");
    ok(notification, "Notification should exist");
    ok(notification.secondaryActions.length > 1, "Secondary actions should exist (always/never remember)");
    notification.remove();

    gBrowser.removeCurrentTab();

    // enter the private browsing mode
    pb.privateBrowsingEnabled = true;

    gBrowser.selectedTab = gBrowser.addTab();
    gBrowser.selectedBrowser.addEventListener("load", function () {
      gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);

      // Make sure the notification is correctly displayed without a remember control
      let notification = PopupNotifications.getNotification("geolocation");
      ok(notification, "Notification should exist");
      is(notification.secondaryActions.length, 0, "Secondary actions shouldn't exist (always/never remember)");
      notification.remove();

      gBrowser.removeCurrentTab();

      // cleanup
      pb.privateBrowsingEnabled = false;
      finish();
    }, true);
    content.location = testPageURL;
  }, true);
  content.location = testPageURL;
}
