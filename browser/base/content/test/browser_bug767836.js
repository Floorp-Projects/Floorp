/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// initialization
const pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);
const PREF = "browser.newtab.url";
const NEWTABURL = Services.prefs.getCharPref(PREF) || "about:blank";
const TESTURL = "http://example.com/";
  
function test() {

  waitForExplicitFinish();
  // check whether the mode that we start off with is normal or not
  ok(!pb.privateBrowsingEnabled, "private browsing is disabled");
  // check whether any custom new tab url has been configured
  ok(!Services.prefs.prefHasUserValue(PREF), "No custom newtab url is set");
  
  openNewTab(function () {
    // Check the new tab opened while in normal mode
    is(gBrowser.selectedBrowser.currentURI.spec, NEWTABURL,
       "URL of NewTab should be browser.newtab.url in Normal mode");
    // Set the custom newtab url
    Services.prefs.setCharPref(PREF, TESTURL);
    ok(Services.prefs.prefHasUserValue(PREF), "Custom newtab url is set");
    
    // Open a newtab after setting the custom newtab url
    openNewTab(function () {
      is(gBrowser.selectedBrowser.currentURI.spec, TESTURL,
         "URL of NewTab should be the custom url");
      
      // clear the custom url preference
      Services.prefs.clearUserPref(PREF);
      ok(!Services.prefs.prefHasUserValue(PREF), "No custom newtab url is set");
      
      // enter private browsing mode
      togglePrivateBrowsing(function () {
        ok(pb.privateBrowsingEnabled, "private browsing is enabled");
        
        // Open a new tab page in private browsing mode
        openNewTab(function () {
          // Check the new tab opened while in private browsing mode
          is(gBrowser.selectedBrowser.currentURI.spec, "about:privatebrowsing",
             "URL of NewTab should be about:privatebrowsing in PB mode");
          
          Services.prefs.setCharPref(PREF, TESTURL);
          ok(Services.prefs.prefHasUserValue(PREF), "Custom newtab url is set");
          
          // Open a newtab after setting the custom newtab url
          openNewTab(function () {
            is(gBrowser.selectedBrowser.currentURI.spec, TESTURL,
               "URL of NewTab should be the custom url");

            Services.prefs.clearUserPref(PREF);
            ok(!Services.prefs.prefHasUserValue(PREF), "No custom newtab url is set");
            
            // exit private browsing mode
            togglePrivateBrowsing(function () {
              ok(!pb.privateBrowsingEnabled, "private browsing is disabled");
              
              gBrowser.removeTab(gBrowser.selectedTab);
              gBrowser.removeTab(gBrowser.selectedTab);
              finish();
            });
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
