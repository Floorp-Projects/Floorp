/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that the title of existing history entries does not
// change inside the private browsing mode.

function test() {
  // initialization
  let pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);
  let cm = Cc["@mozilla.org/cookiemanager;1"].
           getService(Ci.nsICookieManager);
  waitForExplicitFinish();
  ignoreAllUncaughtExceptions();

  const TEST_URL = "http://mochi.test:8888/browser/browser/components/privatebrowsing/test/browser/title.sjs";

  function waitForCleanup(aCallback) {
    // delete all cookies
    cm.removeAll();
    // delete all history items
    waitForClearHistory(aCallback);
  }

  let observer = {
    pass: 1,
    onTitleChanged: function(aURI, aPageTitle) {
      if (aURI.spec != TEST_URL)
        return;
      switch (this.pass++) {
      case 1: // the first time that the page is loaded
        is(aPageTitle, "No Cookie", "The page should be loaded without any cookie for the first time");
        gBrowser.selectedTab = gBrowser.addTab(TEST_URL);
        break;
      case 2: // the second time that the page is loaded
        is(aPageTitle, "Cookie", "The page should be loaded with a cookie for the second time");
        waitForCleanup(function () {
          gBrowser.selectedTab = gBrowser.addTab(TEST_URL);
        });
        break;
      case 3: // before entering the private browsing mode
        is(aPageTitle, "No Cookie", "The page should be loaded without any cookie again");
        // enter private browsing mode
        pb.privateBrowsingEnabled = true;
        gBrowser.selectedTab = gBrowser.addTab(TEST_URL);
        executeSoon(function() {
          PlacesUtils.history.removeObserver(observer);
          pb.privateBrowsingEnabled = false;
          while (gBrowser.browsers.length > 1) {
            gBrowser.removeCurrentTab();
          }
          waitForCleanup(finish);
        });
        break;
      default:
        ok(false, "Unexpected pass: " + (this.pass - 1));
      }
    },

    onBeginUpdateBatch: function () {},
    onEndUpdateBatch: function () {},
    onVisit: function () {},
    onBeforeDeleteURI: function () {},
    onDeleteURI: function () {},
    onClearHistory: function () {},
    onPageChanged: function () {},
    onDeleteVisits: function() {},

    QueryInterface: XPCOMUtils.generateQI([Ci.nsINavHistoryObserver])
  };
  PlacesUtils.history.addObserver(observer, false);

  waitForCleanup(function () {
    gBrowser.selectedTab = gBrowser.addTab(TEST_URL);
  });
}
