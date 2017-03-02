/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that the title of existing history entries does not
// change inside a private window.

add_task(function* test() {
  const TEST_URL = "http://mochi.test:8888/browser/browser/components/" +
                   "privatebrowsing/test/browser/title.sjs";
  let cm = Services.cookies;

  function cleanup() {
    // delete all cookies
    cm.removeAll();
    // delete all history items
    return PlacesTestUtils.clearHistory();
  }

  yield cleanup();

  let deferredFirst = PromiseUtils.defer();
  let deferredSecond = PromiseUtils.defer();
  let deferredThird = PromiseUtils.defer();

  let testNumber = 0;
  let historyObserver = {
    onTitleChanged(aURI, aPageTitle) {
      if (aURI.spec != TEST_URL)
        return;
      switch (++testNumber) {
        case 1:
          // The first time that the page is loaded
          deferredFirst.resolve(aPageTitle);
          break;
        case 2:
          // The second time that the page is loaded
          deferredSecond.resolve(aPageTitle);
          break;
        case 3:
          // After clean up
          deferredThird.resolve(aPageTitle);
          break;
        default:
          // Checks that opening the page in a private window should not fire a
          // title change.
          ok(false, "Title changed. Unexpected pass: " + testNumber);
      }
    },

    onBeginUpdateBatch() {},
    onEndUpdateBatch() {},
    onVisit() {},
    onDeleteURI() {},
    onClearHistory() {},
    onPageChanged() {},
    onDeleteVisits() {},
    QueryInterface: XPCOMUtils.generateQI([Ci.nsINavHistoryObserver])
  };
  PlacesUtils.history.addObserver(historyObserver, false);


  let win = yield BrowserTestUtils.openNewBrowserWindow();
  win.gBrowser.selectedTab = win.gBrowser.addTab(TEST_URL);
  let aPageTitle = yield deferredFirst.promise;
  // The first time that the page is loaded
  is(aPageTitle, "No Cookie",
     "The page should be loaded without any cookie for the first time");

  win.gBrowser.selectedTab = win.gBrowser.addTab(TEST_URL);
  aPageTitle = yield deferredSecond.promise;
  // The second time that the page is loaded
  is(aPageTitle, "Cookie",
     "The page should be loaded with a cookie for the second time");

  yield cleanup();

  win.gBrowser.selectedTab = win.gBrowser.addTab(TEST_URL);
  aPageTitle = yield deferredThird.promise;
  // After clean up
  is(aPageTitle, "No Cookie",
     "The page should be loaded without any cookie again");

  let win2 = yield BrowserTestUtils.openNewBrowserWindow({private: true});

  let private_tab = win2.gBrowser.addTab(TEST_URL);
  win2.gBrowser.selectedTab = private_tab;
  yield BrowserTestUtils.browserLoaded(private_tab.linkedBrowser);

  // Cleanup
  yield cleanup();
  PlacesUtils.history.removeObserver(historyObserver);
  yield BrowserTestUtils.closeWindow(win);
  yield BrowserTestUtils.closeWindow(win2);
});
