/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that opening a new tab in private browsing mode opens about:privatebrowsing

// initialization
const pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);

const PREF = "browser.newtab.url";

function test() {
  let newTabUrl = Services.prefs.getCharPref(PREF) || "about:blank";

  waitForExplicitFinish();
  // check whether the mode that we start off with is normal or not
  ok(!pb.privateBrowsingEnabled, "private browsing is disabled");

  // Open a new tab page in normal mode
  openNewTab(function () {
    // Check the new tab opened while in normal mode
    is(gBrowser.selectedBrowser.currentURI.spec, newTabUrl,
       "URL of NewTab should be browser.newtab.url in Normal mode");

    // enter private browsing mode
    togglePrivateBrowsing(function () {
      ok(pb.privateBrowsingEnabled, "private browsing is enabled");
    
      // Open a new tab page in private browsing mode
      openNewTab(function () {
        // Check the new tab opened while in private browsing mode
        is(gBrowser.selectedBrowser.currentURI.spec, "about:privatebrowsing",
           "URL of NewTab should be about:privatebrowsing in PB mode");

        // exit private browsing mode
        togglePrivateBrowsing(function () {
          ok(!pb.privateBrowsingEnabled, "private browsing is disabled");
          
          // Open a new tab page in normal mode to check if
          // returning from pb mode restores everything as it should
          openNewTab(function () {
            // Check the new tab opened while in normal mode
            is(gBrowser.selectedBrowser.currentURI.spec, newTabUrl, 
               "URL of NewTab should be browser.newtab.url in Normal mode");
            gBrowser.removeTab(gBrowser.selectedTab);
            gBrowser.removeTab(gBrowser.selectedTab);
            finish();
          });
        });
      });
    });
  });
}

function togglePrivateBrowsing(aCallback) {
  let topic = "private-browsing-transition-complete";

  Services.obs.addObserver(function observe() {
    Services.obs.removeObserver(observe, topic);
    executeSoon(aCallback);
  }, topic, false);

  pb.privateBrowsingEnabled = !pb.privateBrowsingEnabled;
}

function openNewTab(aCallback) {
  // Open a new tab
  BrowserOpenTab();
  
  let browser = gBrowser.selectedBrowser;
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    executeSoon(aCallback);
  }, true);
}
