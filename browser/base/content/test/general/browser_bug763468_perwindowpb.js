/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that opening a new tab in private browsing mode opens about:privatebrowsing
function test() {
  // initialization
  waitForExplicitFinish();
  let windowsToClose = [];
  let newTab;
  let newTabPrefName = "browser.newtab.url";
  let newTabURL;
  let mode;

  function doTest(aIsPrivateMode, aWindow, aCallback) {
    whenNewTabLoaded(aWindow, function () {
      if (aIsPrivateMode) {
        mode = "per window private browsing";
        newTabURL = "about:privatebrowsing";
      } else {
        mode = "normal";
        newTabURL = Services.prefs.getCharPref(newTabPrefName) || "about:blank";
      }

      is(aWindow.gBrowser.currentURI.spec, newTabURL,
        "URL of NewTab should be " + newTabURL + " in " + mode +  " mode");

      aWindow.gBrowser.removeTab(aWindow.gBrowser.selectedTab);
      aCallback()
    });
  };

  function testOnWindow(aOptions, aCallback) {
    whenNewWindowLoaded(aOptions, function(aWin) {
      windowsToClose.push(aWin);
      // execute should only be called when need, like when you are opening
      // web pages on the test. If calling executeSoon() is not necesary, then
      // call whenNewWindowLoaded() instead of testOnWindow() on your test.
      executeSoon(function() aCallback(aWin));
    });
  };

   // this function is called after calling finish() on the test.
  registerCleanupFunction(function() {
    windowsToClose.forEach(function(aWin) {
      aWin.close();
    });
  });

  // test first when not on private mode
  testOnWindow({}, function(aWin) {
    doTest(false, aWin, function() {
      // then test when on private mode
      testOnWindow({private: true}, function(aWin) {
        doTest(true, aWin, function() {
          // then test again when not on private mode
          testOnWindow({}, function(aWin) {
            doTest(false, aWin, finish);
          });
        });
      });
    });
  });
}
