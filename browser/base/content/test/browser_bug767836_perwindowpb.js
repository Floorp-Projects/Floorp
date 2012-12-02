/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  //initialization
  waitForExplicitFinish();
  let newTabPrefName = "browser.newtab.url";
  let newTabURL;
  let testURL = "http://example.com/";
  let mode;

  function doTest(aIsPrivateMode, aWindow, aCallback) {
    openNewTab(aWindow, function () {
      if (aIsPrivateMode) {
        mode = "per window private browsing";
        newTabURL = "about:privatebrowsing";
      } else {
        mode = "normal";
        newTabURL = Services.prefs.getCharPref(newTabPrefName) || "about:blank";
      }

      // Check the new tab opened while in normal/private mode
      is(aWindow.gBrowser.selectedBrowser.currentURI.spec, newTabURL,
        "URL of NewTab should be " + newTabURL + " in " + mode +  " mode");
      // Set the custom newtab url
      Services.prefs.setCharPref(newTabPrefName, testURL);
      ok(Services.prefs.prefHasUserValue(newTabPrefName), "Custom newtab url is set");

      // Open a newtab after setting the custom newtab url
      openNewTab(aWindow, function () {
        is(aWindow.gBrowser.selectedBrowser.currentURI.spec, testURL,
           "URL of NewTab should be the custom url");

        // clear the custom url preference
        Services.prefs.clearUserPref(newTabPrefName);
        ok(!Services.prefs.prefHasUserValue(newTabPrefName), "No custom newtab url is set");

        aWindow.gBrowser.removeTab(aWindow.gBrowser.selectedTab);
        aWindow.gBrowser.removeTab(aWindow.gBrowser.selectedTab);
        aWindow.close();
        aCallback()
      });
    });
  }

  function testOnWindow(aIsPrivate, aCallback) {
    whenNewWindowLoaded({private: aIsPrivate}, function(win) {
      executeSoon(function() aCallback(win));
    });
  }

  // check whether any custom new tab url has been configured
  ok(!Services.prefs.prefHasUserValue(newTabPrefName), "No custom newtab url is set");

  // test normal mode
  testOnWindow(false, function(aWindow) {
    doTest(false, aWindow, function() {
      // test private mode
      testOnWindow(true, function(aWindow) {
        doTest(true, aWindow, function() {
          finish();
        });
      });
    });
  });
}

function openNewTab(aWindow, aCallback) {
  // Open a new tab
  aWindow.BrowserOpenTab();

  let browser = aWindow.gBrowser.selectedBrowser;
  if (browser.contentDocument.readyState == "complete") {
    executeSoon(aCallback);
    return;
  }

  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    executeSoon(aCallback);
  }, true);
}