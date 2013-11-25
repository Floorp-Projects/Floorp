/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  /** Test for Bug 522545 **/

  waitForExplicitFinish();
  requestLongerTimeout(2);

  // This tests the following use case:
  // User opens a new tab which gets focus. The user types something into the
  // address bar, then crashes or quits.
  function test_newTabFocused() {
    let state = {
      windows: [{
        tabs: [
          { entries: [{ url: "about:mozilla" }] },
          { entries: [], userTypedValue: "example.com", userTypedClear: 0 }
        ],
        selected: 2
      }]
    };

    waitForBrowserState(state, function() {
      let browser = gBrowser.selectedBrowser;
      is(browser.currentURI.spec, "about:blank",
         "No history entries still sets currentURI to about:blank");
      is(browser.userTypedValue, "example.com",
         "userTypedValue was correctly restored");
      is(browser.userTypedClear, 0,
         "userTypeClear restored as expected");
      is(gURLBar.value, "example.com",
         "Address bar's value correctly restored");
      // Change tabs to make sure address bar value gets updated
      gBrowser.selectedTab = gBrowser.tabContainer.getItemAtIndex(0);
      is(gURLBar.value, "about:mozilla",
         "Address bar's value correctly updated");
      runNextTest();
    });
  }

  // This tests the following use case:
  // User opens a new tab which gets focus. The user types something into the
  // address bar, switches back to the first tab, then crashes or quits.
  function test_newTabNotFocused() {
    let state = {
      windows: [{
        tabs: [
          { entries: [{ url: "about:mozilla" }] },
          { entries: [], userTypedValue: "example.org", userTypedClear: 0 }
        ],
        selected: 1
      }]
    };

    waitForBrowserState(state, function() {
      let browser = gBrowser.getBrowserAtIndex(1);
      is(browser.currentURI.spec, "about:blank",
         "No history entries still sets currentURI to about:blank");
      is(browser.userTypedValue, "example.org",
         "userTypedValue was correctly restored");
      is(browser.userTypedClear, 0,
         "userTypeClear restored as expected");
      is(gURLBar.value, "about:mozilla",
         "Address bar's value correctly restored");
      // Change tabs to make sure address bar value gets updated
      gBrowser.selectedTab = gBrowser.tabContainer.getItemAtIndex(1);
      is(gURLBar.value, "example.org",
         "Address bar's value correctly updated");
      runNextTest();
    });
  }

  // This tests the following use case:
  // User is in a tab with session history, then types something in the
  // address bar, then crashes or quits.
  function test_existingSHEnd_noClear() {
    let state = {
      windows: [{
        tabs: [{
          entries: [{ url: "about:mozilla" }, { url: "about:config" }],
          index: 2,
          userTypedValue: "example.com",
          userTypedClear: 0
        }]
      }]
    };

    waitForBrowserState(state, function() {
      let browser = gBrowser.selectedBrowser;
      is(browser.currentURI.spec, "about:config",
         "browser.currentURI set to current entry in SH");
      is(browser.userTypedValue, "example.com",
         "userTypedValue was correctly restored");
      is(browser.userTypedClear, 0,
         "userTypeClear restored as expected");
      is(gURLBar.value, "example.com",
         "Address bar's value correctly restored to userTypedValue");
      runNextTest();
    });
  }

  // This tests the following use case:
  // User is in a tab with session history, presses back at some point, then
  // types something in the address bar, then crashes or quits.
  function test_existingSHMiddle_noClear() {
    let state = {
      windows: [{
        tabs: [{
          entries: [{ url: "about:mozilla" }, { url: "about:config" }],
          index: 1,
          userTypedValue: "example.org",
          userTypedClear: 0
        }]
      }]
    };

    waitForBrowserState(state, function() {
      let browser = gBrowser.selectedBrowser;
      is(browser.currentURI.spec, "about:mozilla",
         "browser.currentURI set to current entry in SH");
      is(browser.userTypedValue, "example.org",
         "userTypedValue was correctly restored");
      is(browser.userTypedClear, 0,
         "userTypeClear restored as expected");
      is(gURLBar.value, "example.org",
         "Address bar's value correctly restored to userTypedValue");
      runNextTest();
    });
  }

  // This test simulates lots of tabs opening at once and then quitting/crashing.
  function test_getBrowserState_lotsOfTabsOpening() {
    gBrowser.stop();

    let uris = [];
    for (let i = 0; i < 25; i++)
      uris.push("http://example.com/" + i);

    // We're waiting for the first location change, which should indicate
    // one of the tabs has loaded and the others haven't. So one should
    // be in a non-userTypedValue case, while others should still have
    // userTypedValue and userTypedClear set.
    gBrowser.addTabsProgressListener({
      onLocationChange: function (aBrowser) {
        if (uris.indexOf(aBrowser.currentURI.spec) > -1) {
          gBrowser.removeTabsProgressListener(this);
          firstLocationChange();
        }
      }
    });

    function firstLocationChange() {
      let state = JSON.parse(ss.getBrowserState());
      let hasUTV = state.windows[0].tabs.some(function(aTab) {
        return aTab.userTypedValue && aTab.userTypedClear && !aTab.entries.length;
      });

      ok(hasUTV, "At least one tab has a userTypedValue with userTypedClear with no loaded URL");

      gBrowser.addEventListener("load", firstLoad, true);
    }

    function firstLoad() {
      gBrowser.removeEventListener("load", firstLoad, true);

      let state = JSON.parse(ss.getBrowserState());
      let hasSH = state.windows[0].tabs.some(function(aTab) {
        return !("userTypedValue" in aTab) && aTab.entries[0].url;
      });

      ok(hasSH, "At least one tab has its entry in SH");

      runNextTest();
    }

    gBrowser.loadTabs(uris);
  }

  // This simulates setting a userTypedValue and ensures that just typing in the
  // URL bar doesn't set userTypedClear as well.
  function test_getBrowserState_userTypedValue() {
    let state = {
      windows: [{
        tabs: [{ entries: [] }]
      }]
    };

    waitForBrowserState(state, function() {
      let browser = gBrowser.selectedBrowser;
      // Make sure this tab isn't loading and state is clear before we test.
      is(browser.userTypedValue, null, "userTypedValue is empty to start");
      is(browser.userTypedClear, 0, "userTypedClear is 0 to start");

      gURLBar.value = "example.org";
      let event = document.createEvent("Events");
      event.initEvent("input", true, false);
      gURLBar.dispatchEvent(event);

      executeSoon(function () {
        is(browser.userTypedValue, "example.org",
           "userTypedValue was set when changing gURLBar.value");
        is(browser.userTypedClear, 0,
           "userTypedClear was not changed when changing gURLBar.value");

        // Now make sure ss gets these values too
        let newState = JSON.parse(ss.getBrowserState());
        is(newState.windows[0].tabs[0].userTypedValue, "example.org",
           "sessionstore got correct userTypedValue");
        is(newState.windows[0].tabs[0].userTypedClear, 0,
           "sessionstore got correct userTypedClear");
        runNextTest();
      });
    });
  }

  // test_getBrowserState_lotsOfTabsOpening tested userTypedClear in a few cases,
  // but not necessarily any that had legitimate URIs in the state of loading
  // (eg, "http://example.com"), so this test will cover that case.
  function test_userTypedClearLoadURI() {
    let state = {
      windows: [{
        tabs: [
          { entries: [], userTypedValue: "http://example.com", userTypedClear: 2 }
        ]
      }]
    };

    waitForBrowserState(state, function() {
      let browser = gBrowser.selectedBrowser;
      is(browser.currentURI.spec, "http://example.com/",
         "userTypedClear=2 caused userTypedValue to be loaded");
      is(browser.userTypedValue, null,
         "userTypedValue was null after loading a URI");
      is(browser.userTypedClear, 0,
         "userTypeClear reset to 0");
      is(gURLBar.value, gURLBar.trimValue("http://example.com/"),
         "Address bar's value set after loading URI");
      runNextTest();
    });
  }


  let tests = [test_newTabFocused, test_newTabNotFocused,
               test_existingSHEnd_noClear, test_existingSHMiddle_noClear,
               test_getBrowserState_lotsOfTabsOpening,
               test_getBrowserState_userTypedValue, test_userTypedClearLoadURI];
  let originalState = ss.getBrowserState();
  let state = {
    windows: [{
      tabs: [{ entries: [{ url: "about:blank" }] }]
    }]
  };
  function runNextTest() {
    if (tests.length) {
      waitForBrowserState(state, tests.shift());
    } else {
      ss.setBrowserState(originalState);
      executeSoon(finish);
    }
  }

  // Run the tests!
  runNextTest();
}
