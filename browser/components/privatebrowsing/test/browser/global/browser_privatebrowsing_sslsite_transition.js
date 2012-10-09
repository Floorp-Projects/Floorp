/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that SSL sites load correctly after leaving the
// Private Browsing mode (bug 463256 and bug 496335).

function test() {
  // initialization
  gPrefService.setBoolPref("browser.privatebrowsing.keep_current_session", true);
  let pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);

  const TEST_URL = "https://example.com/";

  // load an SSL site in the first tab and wait for it to finish loading
  gBrowser.selectedTab = gBrowser.addTab();
  let browser = gBrowser.selectedBrowser;
  browser.addEventListener("load", function() {
    browser.removeEventListener("load", arguments.callee, true);

    pb.privateBrowsingEnabled = true;
    pb.privateBrowsingEnabled = false;

    // Note: if the page fails to load, the test will time out
    browser.addEventListener("load", function() {
      browser.removeEventListener("load", arguments.callee, true);

      is(content.location, TEST_URL,
        "The original SSL page should be loaded at this stage");

      gBrowser.removeCurrentTab();
      gPrefService.clearUserPref("browser.privatebrowsing.keep_current_session");
      finish();
    }, true);

    executeSoon(function () {
      content.location = TEST_URL;
    });
  }, true);
  content.location = TEST_URL;

  waitForExplicitFinish();
}
