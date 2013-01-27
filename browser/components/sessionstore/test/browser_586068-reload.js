/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const PREF_RESTORE_ON_DEMAND = "browser.sessionstore.restore_on_demand";

function test() {
  TestRunner.run();
}

function runTests() {
  // Request a longer timeout because the test takes quite a while
  // to complete on slow Windows debug machines and we would otherwise
  // see a lot of (not so) intermittent test failures.
  requestLongerTimeout(2);

  Services.prefs.setBoolPref(PREF_RESTORE_ON_DEMAND, true);
  registerCleanupFunction(function () {
    Services.prefs.clearUserPref(PREF_RESTORE_ON_DEMAND);
  });

  let state = { windows: [{ tabs: [
    { entries: [{ url: "http://example.org/#1" }], extData: { "uniq": r() } },
    { entries: [{ url: "http://example.org/#2" }], extData: { "uniq": r() } },
    { entries: [{ url: "http://example.org/#3" }], extData: { "uniq": r() } },
    { entries: [{ url: "http://example.org/#4" }], extData: { "uniq": r() } },
    { entries: [{ url: "http://example.org/#5" }], extData: { "uniq": r() } },
    { entries: [{ url: "http://example.org/#6" }], extData: { "uniq": r() } },
    { entries: [{ url: "http://example.org/#7" }], extData: { "uniq": r() } },
    { entries: [{ url: "http://example.org/#8" }], extData: { "uniq": r() } },
    { entries: [{ url: "http://example.org/#9" }], extData: { "uniq": r() } },
    { entries: [{ url: "http://example.org/#10" }], extData: { "uniq": r() } },
    { entries: [{ url: "http://example.org/#11" }], extData: { "uniq": r() } },
    { entries: [{ url: "http://example.org/#12" }], extData: { "uniq": r() } },
    { entries: [{ url: "http://example.org/#13" }], extData: { "uniq": r() } },
    { entries: [{ url: "http://example.org/#14" }], extData: { "uniq": r() } },
    { entries: [{ url: "http://example.org/#15" }], extData: { "uniq": r() } },
    { entries: [{ url: "http://example.org/#16" }], extData: { "uniq": r() } },
    { entries: [{ url: "http://example.org/#17" }], extData: { "uniq": r() } },
    { entries: [{ url: "http://example.org/#18" }], extData: { "uniq": r() } }
  ], selected: 1 }] };

  let loadCount = 0;
  gProgressListener.setCallback(function (aBrowser, aNeedRestore, aRestoring, aRestored) {
    loadCount++;
    is(aBrowser.currentURI.spec, state.windows[0].tabs[loadCount - 1].entries[0].url,
       "load " + loadCount + " - browser loaded correct url");

    if (loadCount <= state.windows[0].tabs.length) {
      // double check that this tab was the right one
      let expectedData = state.windows[0].tabs[loadCount - 1].extData.uniq;
      let tab;
      for (let i = 0; i < window.gBrowser.tabs.length; i++) {
        if (!tab && window.gBrowser.tabs[i].linkedBrowser == aBrowser)
          tab = window.gBrowser.tabs[i];
      }
      is(ss.getTabValue(tab, "uniq"), expectedData,
         "load " + loadCount + " - correct tab was restored");

      if (loadCount == state.windows[0].tabs.length) {
        gProgressListener.unsetCallback();
        executeSoon(function () {
          reloadAllTabs(state, function () {
            waitForBrowserState(TestRunner.backupState, testCascade);
          });
        });
      } else {
        // reload the next tab
        window.gBrowser.reloadTab(window.gBrowser.tabs[loadCount]);
      }
    }
  });

  yield ss.setBrowserState(JSON.stringify(state));
}

function testCascade() {
  Services.prefs.setBoolPref(PREF_RESTORE_ON_DEMAND, false);

  let state = { windows: [{ tabs: [
    { entries: [{ url: "http://example.com/#1" }], extData: { "uniq": r() } },
    { entries: [{ url: "http://example.com/#2" }], extData: { "uniq": r() } },
    { entries: [{ url: "http://example.com/#3" }], extData: { "uniq": r() } },
    { entries: [{ url: "http://example.com/#4" }], extData: { "uniq": r() } },
    { entries: [{ url: "http://example.com/#5" }], extData: { "uniq": r() } },
    { entries: [{ url: "http://example.com/#6" }], extData: { "uniq": r() } }
  ] }] };

  let loadCount = 0;
  gProgressListener.setCallback(function (aBrowser, aNeedRestore, aRestoring, aRestored) {
    if (++loadCount < state.windows[0].tabs.length) {
      return;
    }

    gProgressListener.unsetCallback();
    executeSoon(function () {
      reloadAllTabs(state, next);
    });
  });

  ss.setBrowserState(JSON.stringify(state));
}

function reloadAllTabs(aState, aCallback) {
  // Simulate a left mouse button click with no modifiers, which is what
  // Command-R, or clicking reload does.
  let fakeEvent = {
    button: 0,
    metaKey: false,
    altKey: false,
    ctrlKey: false,
    shiftKey: false
  };

  let loadCount = 0;
  gProgressListener.setCallback(function (aBrowser, aNeedRestore, aRestoring, aRestored) {
    if (++loadCount <= aState.windows[0].tabs.length) {
      // double check that this tab was the right one
      let expectedData = aState.windows[0].tabs[loadCount - 1].extData.uniq;
      let tab;
      for (let i = 0; i < window.gBrowser.tabs.length; i++) {
        if (!tab && window.gBrowser.tabs[i].linkedBrowser == aBrowser)
          tab = window.gBrowser.tabs[i];
      }
      is(ss.getTabValue(tab, "uniq"), expectedData,
         "load " + loadCount + " - correct tab was reloaded");

      if (loadCount == aState.windows[0].tabs.length) {
        gProgressListener.unsetCallback();
        executeSoon(aCallback);
      } else {
        // reload the next tab
        window.gBrowser.selectTabAtIndex(loadCount);
        BrowserReloadOrDuplicate(fakeEvent);
      }
    }
  }, false);

  BrowserReloadOrDuplicate(fakeEvent);
}
