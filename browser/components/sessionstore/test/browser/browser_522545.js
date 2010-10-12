/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is sessionstore test code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Paul O’Shannessy <paul@oshannessy.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

function browserWindowsCount() {
  let count = 0;
  let e = Services.wm.getEnumerator("navigator:browser");
  while (e.hasMoreElements()) {
    if (!e.getNext().closed)
      ++count;
  }
  return count;
}

function test() {
  /** Test for Bug 522545 **/
  is(browserWindowsCount(), 1, "Only one browser window should be open initially");

  waitForExplicitFinish();
  requestLongerTimeout(2);

  let ss = Cc["@mozilla.org/browser/sessionstore;1"].
           getService(Ci.nsISessionStore);

  function waitForBrowserState(aState, aSetStateCallback) {
    var locationChanges = 0;
    gBrowser.addTabsProgressListener({
      onLocationChange: function (aBrowser) {
        if (++locationChanges == aState.windows[0].tabs.length) {
          gBrowser.removeTabsProgressListener(this);
          executeSoon(aSetStateCallback);
        }
      }
    });
    ss.setBrowserState(JSON.stringify(aState));
  }

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

      gURLBar.value = "mozilla.org";
      let event = document.createEvent("Events");
      event.initEvent("input", true, false);
      gURLBar.dispatchEvent(event);

      is(browser.userTypedValue, "mozilla.org",
         "userTypedValue was set when changing gURLBar.value");
      is(browser.userTypedClear, 0,
         "userTypedClear was not changed when changing gURLBar.value");

      // Now make sure ss gets these values too
      let newState = JSON.parse(ss.getBrowserState());
      is(newState.windows[0].tabs[0].userTypedValue, "mozilla.org",
         "sessionstore got correct userTypedValue");
      is(newState.windows[0].tabs[0].userTypedClear, 0,
         "sessionstore got correct userTypedClear");
      runNextTest();
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

    // Set state here and listen for load event because waitForBrowserState
    // doesn't guarantee all the tabs have loaded, so the test could continue
    // before we're in a testable state. This is important here because of the
    // distinction between "http://example.com" and "http://example.com/".
    ss.setBrowserState(JSON.stringify(state));
    gBrowser.addEventListener("load", function(aEvent) {
      if (gBrowser.currentURI.spec == "about:blank")
        return;
      gBrowser.removeEventListener("load", arguments.callee, true);

      let browser = gBrowser.selectedBrowser;
      is(browser.currentURI.spec, "http://example.com/",
         "userTypedClear=2 caused userTypedValue to be loaded");
      is(browser.userTypedValue, null,
         "userTypedValue was null after loading a URI");
      is(browser.userTypedClear, 0,
         "userTypeClear reset to 0");
      is(gURLBar.value, "http://example.com/",
         "Address bar's value set after loading URI");
      runNextTest();
    }, true);
  }


  let tests = [test_newTabFocused, test_newTabNotFocused,
               test_existingSHEnd_noClear, test_existingSHMiddle_noClear,
               test_getBrowserState_lotsOfTabsOpening,
               test_getBrowserState_userTypedValue, test_userTypedClearLoadURI];
  let originalState = ss.getBrowserState();
  function runNextTest() {
    if (tests.length) {
      tests.shift()();
    } else {
      ss.setBrowserState(originalState);
      executeSoon(function () {
        is(browserWindowsCount(), 1, "Only one browser window should be open eventually");
        finish();
      });
    }
  }

  // Run the tests!
  runNextTest();
}
