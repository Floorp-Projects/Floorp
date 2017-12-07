/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

function test() {
  // initialization
  waitForExplicitFinish();

  let aboutNewTabService = Components.classes["@mozilla.org/browser/aboutnewtab-service;1"]
                                     .getService(Components.interfaces.nsIAboutNewTabService);
  let newTabURL;
  let testURL = "http://example.com/";
  let defaultURL = aboutNewTabService.newTabURL;
  let mode;

  function doTest(aIsPrivateMode, aWindow, aCallback) {
    openNewTab(aWindow, function() {
      if (aIsPrivateMode) {
        mode = "per window private browsing";
        newTabURL = "about:privatebrowsing";
      } else {
        mode = "normal";
        newTabURL = "about:newtab";
      }

      // Check the new tab opened while in normal/private mode
      is(aWindow.gBrowser.selectedBrowser.currentURI.spec, newTabURL,
        "URL of NewTab should be " + newTabURL + " in " + mode + " mode");
      // Set the custom newtab url
      aboutNewTabService.newTabURL = testURL;
      is(aboutNewTabService.newTabURL, testURL, "Custom newtab url is set");

      // Open a newtab after setting the custom newtab url
      openNewTab(aWindow, function() {
        is(aWindow.gBrowser.selectedBrowser.currentURI.spec, testURL,
           "URL of NewTab should be the custom url");

        // Clear the custom url.
        aboutNewTabService.resetNewTabURL();
        is(aboutNewTabService.newTabURL, defaultURL, "No custom newtab url is set");

        aWindow.gBrowser.removeTab(aWindow.gBrowser.selectedTab);
        aWindow.gBrowser.removeTab(aWindow.gBrowser.selectedTab);
        aWindow.close();
        aCallback();
      });
    });
  }

  function testOnWindow(aIsPrivate, aCallback) {
    whenNewWindowLoaded({private: aIsPrivate}, function(win) {
      executeSoon(() => aCallback(win));
    });
  }

  // check whether any custom new tab url has been configured
  ok(!aboutNewTabService.overridden, "No custom newtab url is set");

  // test normal mode
  testOnWindow(false, function(aWindow) {
    doTest(false, aWindow, function() {
      // test private mode
      testOnWindow(true, function(aWindow2) {
        doTest(true, aWindow2, function() {
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
  // eslint-disable-next-line mozilla/no-cpows-in-tests
  if (browser.contentDocument.readyState === "complete") {
    executeSoon(aCallback);
    return;
  }

  browser.addEventListener("load", function() {
    executeSoon(aCallback);
  }, {capture: true, once: true});
}
