/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Ensure that a storage instance used by both private and public sessions at different times does not
// allow any data to leak due to cached values.

// Step 1: Load browser_privatebrowsing_localStorage_before_after_page.html in a private tab, causing a storage
//   item to exist. Close the tab.
// Step 2: Load the same page in a non-private tab, ensuring that the storage instance reports only one item
//   existing.

function test() {
  // initialization
  waitForExplicitFinish();
  let windowsToClose = [];
  let testURI = "about:blank";
  let prefix = 'http://mochi.test:8888/browser/browser/components/privatebrowsing/test/browser/';

  function doTest(aIsPrivateMode, aWindow, aCallback) {
    aWindow.gBrowser.selectedBrowser.addEventListener("load", function onLoad() {
      aWindow.gBrowser.selectedBrowser.removeEventListener("load", onLoad, true);

      if (aIsPrivateMode) {
        // do something when aIsPrivateMode is true
        is(aWindow.gBrowser.contentWindow.document.title, '1', "localStorage should contain 1 item");
      } else {
        // do something when aIsPrivateMode is false
        is(aWindow.gBrowser.contentWindow.document.title, 'null|0', 'localStorage should contain 0 items');
      }

      aCallback();
    }, true);

    aWindow.gBrowser.selectedBrowser.loadURI(testURI);
  }

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

  // test first when on private mode
  testOnWindow({private: true}, function(aWin) {
    testURI = prefix + 'browser_privatebrowsing_localStorage_before_after_page.html';
    doTest(true, aWin, function() {
      // then test when not on private mode
      testOnWindow({}, function(aWin) {
        testURI = prefix + 'browser_privatebrowsing_localStorage_before_after_page2.html';
        doTest(false, aWin, finish);
      });
    });
  });
}
