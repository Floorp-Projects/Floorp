/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that the page info dialogs will close when entering or
// exiting the private browsing mode.

function test() {
  // initialization
  let pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);

  function runTest(aPBMode, aCallBack) {
    let tab1 = gBrowser.addTab();
    gBrowser.selectedTab = tab1;
    let browser1 = gBrowser.getBrowserForTab(tab1);
    browser1.addEventListener("load", function () {
      browser1.removeEventListener("load", arguments.callee, true);

      let pageInfo1 = BrowserPageInfo();
      pageInfo1.addEventListener("load", function () {
        pageInfo1.removeEventListener("load", arguments.callee, false);

        let tab2 = gBrowser.addTab();
        gBrowser.selectedTab = tab2;
        let browser2 = gBrowser.getBrowserForTab(tab2);
        browser2.addEventListener("load", function () {
          browser2.removeEventListener("load", arguments.callee, true);

          let pageInfo2 = BrowserPageInfo();
          pageInfo2.addEventListener("load", function () {
            pageInfo2.removeEventListener("load", arguments.callee, false);

            pageInfo1.addEventListener("unload", function () {
              pageInfo1.removeEventListener("unload", arguments.callee, false);
              pageInfo1 = null;
              ok(true, "Page info 1 being closed as expected");
              if (!pageInfo2)
                aCallBack();
            }, false);

            pageInfo2.addEventListener("unload", function () {
              pageInfo2.removeEventListener("unload", arguments.callee, false);
              pageInfo2 = null;
              ok(true, "Page info 2 being closed as expected");
              if (!pageInfo1)
                aCallBack();
            }, false);

            pb.privateBrowsingEnabled = aPBMode;
          }, false);
        }, true);
        browser2.loadURI("data:text/plain,Test Page 2");
      }, false);
    }, true);
    browser1.loadURI("data:text/html,Test Page 1");
  }

  runTest(true, function() {
    runTest(false, function() {
      gBrowser.removeCurrentTab();
      gBrowser.removeCurrentTab();

      finish();
    });
  });

  waitForExplicitFinish();
  ignoreAllUncaughtExceptions();
}
