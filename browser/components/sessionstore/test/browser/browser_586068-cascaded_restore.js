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
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
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

Cu.import("resource://gre/modules/Services.jsm");
let ss = Cc["@mozilla.org/browser/sessionstore;1"].
         getService(Ci.nsISessionStore);

let stateBackup = ss.getBrowserState();

const TAB_STATE_NEEDS_RESTORE = 1;
const TAB_STATE_RESTORING = 2;

function test() {
  /** Test for Bug 586068 - Cascade page loads when restoring **/
  waitForExplicitFinish();
  // This test does a lot of window opening / closing and waiting for loads.
  // In order to prevent timeouts, we'll extend the default that mochitest uses.
  requestLongerTimeout(4);
  runNextTest();
}

// test_reloadCascade, test_reloadReload are generated tests that are run out
// of cycle (since they depend on current state). They're listed in [tests] here
// so that it is obvious when they run in respect to the other tests.
let tests = [test_cascade, test_select, test_multiWindowState,
             test_setWindowStateNoOverwrite, test_setWindowStateOverwrite,
             test_setBrowserStateInterrupted, test_reload,
             /* test_reloadReload, */ test_reloadCascadeSetup,
             /* test_reloadCascade */];
function runNextTest() {
  // Reset the pref
  try {
    Services.prefs.clearUserPref("browser.sessionstore.max_concurrent_tabs");
  } catch (e) {}

  // set an empty state & run the next test, or finish
  if (tests.length) {
    // Enumerate windows and close everything but our primary window. We can't
    // use waitForFocus() because apparently it's buggy. See bug 599253.
    var windowsEnum = Services.wm.getEnumerator("navigator:browser");
    while (windowsEnum.hasMoreElements()) {
      var currentWindow = windowsEnum.getNext();
      if (currentWindow != window) {
        currentWindow.close();
      }
    }

    ss.setBrowserState(JSON.stringify({ windows: [{ tabs: [{ url: 'about:blank' }] }] }));
    let currentTest = tests.shift();
    info("running " + currentTest.name);
    executeSoon(currentTest);
  }
  else {
    ss.setBrowserState(stateBackup);
    executeSoon(finish);
  }
}


function test_cascade() {
  // Set the pref to 1 so we know exactly how many tabs should be restoring at any given time
  Services.prefs.setIntPref("browser.sessionstore.max_concurrent_tabs", 1);

  // We have our own progress listener for this test, which we'll attach before our state is set
  let progressListener = {
    onStateChange: function (aBrowser, aWebProgress, aRequest, aStateFlags, aStatus) {
      dump("\n\nload: " + aBrowser.currentURI.spec + "\n" + JSON.stringify(countTabs()) + "\n\n");
      if (aBrowser.__SS_restoreState == TAB_STATE_RESTORING &&
          aStateFlags & Ci.nsIWebProgressListener.STATE_STOP &&
          aStateFlags & Ci.nsIWebProgressListener.STATE_IS_NETWORK &&
          aStateFlags & Ci.nsIWebProgressListener.STATE_IS_WINDOW)
        test_cascade_progressCallback();
    }
  }

  let state = { windows: [{ tabs: [
    { entries: [{ url: "http://example.com" }], extData: { "uniq": r() } },
    { entries: [{ url: "http://example.com" }], extData: { "uniq": r() } },
    { entries: [{ url: "http://example.com" }], extData: { "uniq": r() } },
    { entries: [{ url: "http://example.com" }], extData: { "uniq": r() } },
    { entries: [{ url: "http://example.com" }], extData: { "uniq": r() } },
    { entries: [{ url: "http://example.com" }], extData: { "uniq": r() } }
  ] }] };

  let loadCount = 0;
  // Since our progress listener is fired before the one in sessionstore, our
  // expected counts look a little weird. This is because we inspect the state
  // before sessionstore has marked the tab as finished restoring and before it
  // starts restoring the next tab
  let expectedCounts = [
    [5, 1, 0],
    [4, 1, 1],
    [3, 1, 2],
    [2, 1, 3],
    [1, 1, 4],
    [0, 1, 5]
  ];

  function test_cascade_progressCallback() {
    loadCount++;
    let counts = countTabs();
    let expected = expectedCounts[loadCount - 1];

    is(counts[0], expected[0], "test_cascade: load " + loadCount + " - # tabs that need to be restored");
    is(counts[1], expected[1], "test_cascade: load " + loadCount + " - # tabs that are restoring");
    is(counts[2], expected[2], "test_cascade: load " + loadCount + " - # tabs that has been restored");

    if (loadCount < state.windows[0].tabs.length)
      return;

    window.gBrowser.removeTabsProgressListener(progressListener);
    runNextTest();
  }

  // This progress listener will get attached before the listener in session store.
  window.gBrowser.addTabsProgressListener(progressListener);
  ss.setBrowserState(JSON.stringify(state));
}


function test_select() {
  // Set the pref to 0 so we know exactly how many tabs should be restoring at
  // any given time. This guarantees that a finishing load won't start another.
  Services.prefs.setIntPref("browser.sessionstore.max_concurrent_tabs", 0);

  // We have our own progress listener for this test, which we'll attach before our state is set
  let progressListener = {
    onStateChange: function (aBrowser, aWebProgress, aRequest, aStateFlags, aStatus) {
      if (aBrowser.__SS_restoreState == TAB_STATE_RESTORING &&
          aStateFlags & Ci.nsIWebProgressListener.STATE_STOP &&
          aStateFlags & Ci.nsIWebProgressListener.STATE_IS_NETWORK &&
          aStateFlags & Ci.nsIWebProgressListener.STATE_IS_WINDOW)
        test_select_progressCallback(aBrowser);
    }
  }

  let state = { windows: [{ tabs: [
    { entries: [{ url: "http://example.org" }], extData: { "uniq": r() } },
    { entries: [{ url: "http://example.org" }], extData: { "uniq": r() } },
    { entries: [{ url: "http://example.org" }], extData: { "uniq": r() } },
    { entries: [{ url: "http://example.org" }], extData: { "uniq": r() } },
    { entries: [{ url: "http://example.org" }], extData: { "uniq": r() } },
    { entries: [{ url: "http://example.org" }], extData: { "uniq": r() } }
  ], selected: 1 }] };

  let loadCount = 0;
  // expectedCounts looks a little wierd for the test case, but it works. See
  // comment in test_cascade for an explanation
  let expectedCounts = [
    [5, 1, 0],
    [4, 1, 1],
    [3, 1, 2],
    [2, 1, 3],
    [1, 1, 4],
    [0, 1, 5]
  ];
  let tabOrder = [0, 5, 1, 4, 3, 2];

  function test_select_progressCallback(aBrowser) {
    loadCount++;

    let counts = countTabs();
    let expected = expectedCounts[loadCount - 1];

    is(counts[0], expected[0], "test_select: load " + loadCount + " - # tabs that need to be restored");
    is(counts[1], expected[1], "test_select: load " + loadCount + " - # tabs that are restoring");
    is(counts[2], expected[2], "test_select: load " + loadCount + " - # tabs that has been restored");

    if (loadCount < state.windows[0].tabs.length) {
      // double check that this tab was the right one
      let expectedData = state.windows[0].tabs[tabOrder[loadCount - 1]].extData.uniq;
      let tab;
      for (let i = 0; i < window.gBrowser.tabs.length; i++) {
        if (!tab && window.gBrowser.tabs[i].linkedBrowser == aBrowser)
          tab = window.gBrowser.tabs[i];
      }
      is(ss.getTabValue(tab, "uniq"), expectedData, "test_select: load " + loadCount + " - correct tab was restored");

      // select the next tab
      window.gBrowser.selectTabAtIndex(tabOrder[loadCount]);
      return;
    }

    window.gBrowser.removeTabsProgressListener(progressListener);
    runNextTest();
  }

  window.gBrowser.addTabsProgressListener(progressListener);
  ss.setBrowserState(JSON.stringify(state));
}


function test_multiWindowState() {
  // We have our own progress listener for this test, which we'll attach before our state is set
  let progressListener = {
    onStateChange: function (aBrowser, aWebProgress, aRequest, aStateFlags, aStatus) {
      // We only care about load events when the tab still has
      // __SS_restoreState == TAB_STATE_RESTORING on it.
      // Since our listener is attached before the sessionstore one, this works out.
      if (aBrowser.__SS_restoreState == TAB_STATE_RESTORING &&
          aStateFlags & Ci.nsIWebProgressListener.STATE_STOP &&
          aStateFlags & Ci.nsIWebProgressListener.STATE_IS_NETWORK &&
          aStateFlags & Ci.nsIWebProgressListener.STATE_IS_WINDOW)
        test_multiWindowState_progressCallback(aBrowser);
    }
  }

  // The first window will be put into the already open window and the second
  // window will be opened with _openWindowWithState, which is the source of the problem.
  let state = { windows: [
    {
      tabs: [
        { entries: [{ url: "http://example.org#0" }], extData: { "uniq": r() } }
      ],
      selected: 1
    },
    {
      tabs: [
        { entries: [{ url: "http://example.com#1" }], extData: { "uniq": r() } },
        { entries: [{ url: "http://example.com#2" }], extData: { "uniq": r() } },
        { entries: [{ url: "http://example.com#3" }], extData: { "uniq": r() } },
        { entries: [{ url: "http://example.com#4" }], extData: { "uniq": r() } },
        { entries: [{ url: "http://example.com#5" }], extData: { "uniq": r() } },
        { entries: [{ url: "http://example.com#6" }], extData: { "uniq": r() } }
      ],
      selected: 4
    }
  ] };
  let numTabs = state.windows[0].tabs.length + state.windows[1].tabs.length;

  let loadCount = 0;
  function test_multiWindowState_progressCallback(aBrowser) {
    loadCount++;

    if (loadCount < numTabs)
      return;

    // We don't actually care about load order in this test, just that they all
    // do load.
    is(loadCount, numTabs, "test_multiWindowState: all tabs were restored");
    let count = countTabs();
    is(count[0], 0,
       "test_multiWindowState: there are no tabs left needing restore");

    // Remove the progress listener from this window, it will be removed from
    // theWin when that window is closed (in setBrowserState).
    window.gBrowser.removeTabsProgressListener(progressListener);
    runNextTest();
  }

  // We also want to catch the 2nd window, so we need to observe domwindowopened
  function windowObserver(aSubject, aTopic, aData) {
    let theWin = aSubject.QueryInterface(Ci.nsIDOMWindow);
    if (aTopic == "domwindowopened") {
      theWin.addEventListener("load", function() {
        theWin.removeEventListener("load", arguments.callee, false);

        Services.ww.unregisterNotification(windowObserver);
        theWin.gBrowser.addTabsProgressListener(progressListener);
      }, false);
    }
  }
  Services.ww.registerNotification(windowObserver);

  window.gBrowser.addTabsProgressListener(progressListener);
  ss.setBrowserState(JSON.stringify(state));
}


function test_setWindowStateNoOverwrite() {
  // Set the pref to 1 so we know exactly how many tabs should be restoring at any given time
  Services.prefs.setIntPref("browser.sessionstore.max_concurrent_tabs", 1);

  // We have our own progress listener for this test, which we'll attach before our state is set
  let progressListener = {
    onStateChange: function (aBrowser, aWebProgress, aRequest, aStateFlags, aStatus) {
      // We only care about load events when the tab still has
      // __SS_restoreState == TAB_STATE_RESTORING on it.
      // Since our listener is attached before the sessionstore one, this works out.
      if (aBrowser.__SS_restoreState == TAB_STATE_RESTORING &&
          aStateFlags & Ci.nsIWebProgressListener.STATE_STOP &&
          aStateFlags & Ci.nsIWebProgressListener.STATE_IS_NETWORK &&
          aStateFlags & Ci.nsIWebProgressListener.STATE_IS_WINDOW)
        test_setWindowStateNoOverwrite_progressCallback(aBrowser);
    }
  }

  // We'll use 2 states so that we can make sure calling setWindowState doesn't
  // wipe out currently restoring data.
  let state1 = { windows: [{ tabs: [
    { entries: [{ url: "http://example.com#1" }] },
    { entries: [{ url: "http://example.com#2" }] },
    { entries: [{ url: "http://example.com#3" }] },
    { entries: [{ url: "http://example.com#4" }] },
    { entries: [{ url: "http://example.com#5" }] },
  ] }] };
  let state2 = { windows: [{ tabs: [
    { entries: [{ url: "http://example.org#1" }] },
    { entries: [{ url: "http://example.org#2" }] },
    { entries: [{ url: "http://example.org#3" }] },
    { entries: [{ url: "http://example.org#4" }] },
    { entries: [{ url: "http://example.org#5" }] }
  ] }] };

  let numTabs = state1.windows[0].tabs.length + state2.windows[0].tabs.length;

  let loadCount = 0;
  function test_setWindowStateNoOverwrite_progressCallback(aBrowser) {
    loadCount++;

    // When loadCount == 2, we'll also restore state2 into the window
    if (loadCount == 2)
      ss.setWindowState(window, JSON.stringify(state2), false);

    if (loadCount < numTabs)
      return;

    // We don't actually care about load order in this test, just that they all
    // do load.
    is(loadCount, numTabs, "test_setWindowStateNoOverwrite: all tabs were restored");
    // window.__SS_tabsToRestore isn't decremented until after the progress
    // listener is called. Since we get in here before that, we still expect
    // the count to be 1.
    is(window.__SS_tabsToRestore, 1,
       "test_setWindowStateNoOverwrite: window doesn't think there are more tabs to restore");
    let count = countTabs();
    is(count[0], 0,
       "test_setWindowStateNoOverwrite: there are no tabs left needing restore");

    // Remove the progress listener from this window, it will be removed from
    // theWin when that window is closed (in setBrowserState).
    window.gBrowser.removeTabsProgressListener(progressListener);

    runNextTest();
  }

  window.gBrowser.addTabsProgressListener(progressListener);
  ss.setWindowState(window, JSON.stringify(state1), true);
}


function test_setWindowStateOverwrite() {
  // Set the pref to 1 so we know exactly how many tabs should be restoring at any given time
  Services.prefs.setIntPref("browser.sessionstore.max_concurrent_tabs", 1);

  // We have our own progress listener for this test, which we'll attach before our state is set
  let progressListener = {
    onStateChange: function (aBrowser, aWebProgress, aRequest, aStateFlags, aStatus) {
      // We only care about load events when the tab still has
      // __SS_restoreState == TAB_STATE_RESTORING on it.
      // Since our listener is attached before the sessionstore one, this works out.
      if (aBrowser.__SS_restoreState == TAB_STATE_RESTORING &&
          aStateFlags & Ci.nsIWebProgressListener.STATE_STOP &&
          aStateFlags & Ci.nsIWebProgressListener.STATE_IS_NETWORK &&
          aStateFlags & Ci.nsIWebProgressListener.STATE_IS_WINDOW)
        test_setWindowStateOverwrite_progressCallback(aBrowser);
    }
  }

  // We'll use 2 states so that we can make sure calling setWindowState doesn't
  // wipe out currently restoring data.
  let state1 = { windows: [{ tabs: [
    { entries: [{ url: "http://example.com#1" }] },
    { entries: [{ url: "http://example.com#2" }] },
    { entries: [{ url: "http://example.com#3" }] },
    { entries: [{ url: "http://example.com#4" }] },
    { entries: [{ url: "http://example.com#5" }] },
  ] }] };
  let state2 = { windows: [{ tabs: [
    { entries: [{ url: "http://example.org#1" }] },
    { entries: [{ url: "http://example.org#2" }] },
    { entries: [{ url: "http://example.org#3" }] },
    { entries: [{ url: "http://example.org#4" }] },
    { entries: [{ url: "http://example.org#5" }] }
  ] }] };

  let numTabs = 2 + state2.windows[0].tabs.length;

  let loadCount = 0;
  function test_setWindowStateOverwrite_progressCallback(aBrowser) {
    loadCount++;

    // When loadCount == 2, we'll also restore state2 into the window
    if (loadCount == 2)
      ss.setWindowState(window, JSON.stringify(state2), true);

    if (loadCount < numTabs)
      return;

    // We don't actually care about load order in this test, just that they all
    // do load.
    is(loadCount, numTabs, "test_setWindowStateOverwrite: all tabs were restored");
    // window.__SS_tabsToRestore isn't decremented until after the progress
    // listener is called. Since we get in here before that, we still expect
    // the count to be 1.
    is(window.__SS_tabsToRestore, 1,
       "test_setWindowStateOverwrite: window doesn't think there are more tabs to restore");
    let count = countTabs();
    is(count[0], 0,
       "test_setWindowStateOverwrite: there are no tabs left needing restore");

    // Remove the progress listener from this window, it will be removed from
    // theWin when that window is closed (in setBrowserState).
    window.gBrowser.removeTabsProgressListener(progressListener);

    runNextTest();
  }

  window.gBrowser.addTabsProgressListener(progressListener);
  ss.setWindowState(window, JSON.stringify(state1), true);
}


function test_setBrowserStateInterrupted() {
  // Set the pref to 1 so we know exactly how many tabs should be restoring at any given time
  Services.prefs.setIntPref("browser.sessionstore.max_concurrent_tabs", 1);

  // We have our own progress listener for this test, which we'll attach before our state is set
  let progressListener = {
    onStateChange: function (aBrowser, aWebProgress, aRequest, aStateFlags, aStatus) {
      // We only care about load events when the tab still has
      // __SS_restoreState == TAB_STATE_RESTORING on it.
      // Since our listener is attached before the sessionstore one, this works out.
      if (aBrowser.__SS_restoreState == TAB_STATE_RESTORING &&
          aStateFlags & Ci.nsIWebProgressListener.STATE_STOP &&
          aStateFlags & Ci.nsIWebProgressListener.STATE_IS_NETWORK &&
          aStateFlags & Ci.nsIWebProgressListener.STATE_IS_WINDOW)
        test_setBrowserStateInterrupted_progressCallback(aBrowser);
    }
  }

  // The first state will be loaded using setBrowserState, followed by the 2nd
  // state also being loaded using setBrowserState, interrupting the first restore.
  let state1 = { windows: [
    {
      tabs: [
        { entries: [{ url: "http://example.org#1" }], extData: { "uniq": r() } },
        { entries: [{ url: "http://example.org#2" }], extData: { "uniq": r() } },
        { entries: [{ url: "http://example.org#3" }], extData: { "uniq": r() } },
        { entries: [{ url: "http://example.org#4" }], extData: { "uniq": r() } }
      ],
      selected: 1
    },
    {
      tabs: [
        { entries: [{ url: "http://example.com#1" }], extData: { "uniq": r() } },
        { entries: [{ url: "http://example.com#2" }], extData: { "uniq": r() } },
        { entries: [{ url: "http://example.com#3" }], extData: { "uniq": r() } },
        { entries: [{ url: "http://example.com#4" }], extData: { "uniq": r() } },
      ],
      selected: 3
    }
  ] };
  let state2 = { windows: [
    {
      tabs: [
        { entries: [{ url: "http://example.org#5" }], extData: { "uniq": r() } },
        { entries: [{ url: "http://example.org#6" }], extData: { "uniq": r() } },
        { entries: [{ url: "http://example.org#7" }], extData: { "uniq": r() } },
        { entries: [{ url: "http://example.org#8" }], extData: { "uniq": r() } }
      ],
      selected: 3
    },
    {
      tabs: [
        { entries: [{ url: "http://example.com#5" }], extData: { "uniq": r() } },
        { entries: [{ url: "http://example.com#6" }], extData: { "uniq": r() } },
        { entries: [{ url: "http://example.com#7" }], extData: { "uniq": r() } },
        { entries: [{ url: "http://example.com#8" }], extData: { "uniq": r() } },
      ],
      selected: 1
    }
  ] };

  // interruptedAfter will be set after the selected tab from each window have loaded.
  let interruptedAfter = 0;
  let loadedWindow1 = false;
  let loadedWindow2 = false;
  let numTabs = state2.windows[0].tabs.length + state2.windows[1].tabs.length;

  let loadCount = 0;
  function test_setBrowserStateInterrupted_progressCallback(aBrowser) {
    loadCount++;

    if (aBrowser.currentURI.spec == state1.windows[0].tabs[2].entries[0].url)
      loadedWindow1 = true;
    if (aBrowser.currentURI.spec == state1.windows[1].tabs[0].entries[0].url)
      loadedWindow2 = true;

    if (!interruptedAfter && loadedWindow1 && loadedWindow2) {
      interruptedAfter = loadCount;
      ss.setBrowserState(JSON.stringify(state2));
      return;
    }

    if (loadCount < numTabs + interruptedAfter)
      return;

    // We don't actually care about load order in this test, just that they all
    // do load.
    is(loadCount, numTabs + interruptedAfter,
       "test_setBrowserStateInterrupted: all tabs were restored");
    let count = countTabs();
    is(count[0], 0,
       "test_setBrowserStateInterrupted: there are no tabs left needing restore");

    // Remove the progress listener from this window, it will be removed from
    // theWin when that window is closed (in setBrowserState).
    window.gBrowser.removeTabsProgressListener(progressListener);
    Services.ww.unregisterNotification(windowObserver);
    runNextTest();
  }

  // We also want to catch the extra windows (there should be 2), so we need to observe domwindowopened
  function windowObserver(aSubject, aTopic, aData) {
    let theWin = aSubject.QueryInterface(Ci.nsIDOMWindow);
    if (aTopic == "domwindowopened") {
      theWin.addEventListener("load", function() {
        theWin.removeEventListener("load", arguments.callee, false);

        Services.ww.unregisterNotification(windowObserver);
        theWin.gBrowser.addTabsProgressListener(progressListener);
      }, false);
    }
  }
  Services.ww.registerNotification(windowObserver);

  window.gBrowser.addTabsProgressListener(progressListener);
  ss.setBrowserState(JSON.stringify(state1));
}


function test_reload() {
  // Set the pref to 0 so we know exactly how many tabs should be restoring at
  // any given time. This guarantees that a finishing load won't start another.
  Services.prefs.setIntPref("browser.sessionstore.max_concurrent_tabs", 0);

  // We have our own progress listener for this test, which we'll attach before our state is set
  let progressListener = {
    onStateChange: function (aBrowser, aWebProgress, aRequest, aStateFlags, aStatus) {
      if (aBrowser.__SS_restoreState == TAB_STATE_RESTORING &&
          aStateFlags & Ci.nsIWebProgressListener.STATE_STOP &&
          aStateFlags & Ci.nsIWebProgressListener.STATE_IS_NETWORK &&
          aStateFlags & Ci.nsIWebProgressListener.STATE_IS_WINDOW)
        test_reload_progressCallback(aBrowser);
    }
  }

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
  function test_reload_progressCallback(aBrowser) {
    loadCount++;

    is(aBrowser.currentURI.spec, state.windows[0].tabs[loadCount - 1].entries[0].url,
       "test_reload: load " + loadCount + " - browser loaded correct url");

    if (loadCount <= state.windows[0].tabs.length) {
      // double check that this tab was the right one
      let expectedData = state.windows[0].tabs[loadCount - 1].extData.uniq;
      let tab;
      for (let i = 0; i < window.gBrowser.tabs.length; i++) {
        if (!tab && window.gBrowser.tabs[i].linkedBrowser == aBrowser)
          tab = window.gBrowser.tabs[i];
      }
      is(ss.getTabValue(tab, "uniq"), expectedData,
         "test_reload: load " + loadCount + " - correct tab was restored");

      if (loadCount == state.windows[0].tabs.length) {
        window.gBrowser.removeTabsProgressListener(progressListener);
        executeSoon(function() {
          _test_reloadAfter("test_reloadReload", state, runNextTest);
        });
      }
      else {
        // reload the next tab
        window.gBrowser.reloadTab(window.gBrowser.tabs[loadCount]);
      }
    }

  }

  window.gBrowser.addTabsProgressListener(progressListener);
  ss.setBrowserState(JSON.stringify(state));
}


// This doesn't actually test anything, just does a cascaded restore with default
// settings. This really just sets up to test that reloads work.
function test_reloadCascadeSetup() {
  // We have our own progress listener for this test, which we'll attach before our state is set
  let progressListener = {
    onStateChange: function (aBrowser, aWebProgress, aRequest, aStateFlags, aStatus) {
      if (aBrowser.__SS_restoreState == TAB_STATE_RESTORING &&
          aStateFlags & Ci.nsIWebProgressListener.STATE_STOP &&
          aStateFlags & Ci.nsIWebProgressListener.STATE_IS_NETWORK &&
          aStateFlags & Ci.nsIWebProgressListener.STATE_IS_WINDOW)
        test_cascadeReloadSetup_progressCallback();
    }
  }

  let state = { windows: [{ tabs: [
    { entries: [{ url: "http://example.com/#1" }], extData: { "uniq": r() } },
    { entries: [{ url: "http://example.com/#2" }], extData: { "uniq": r() } },
    { entries: [{ url: "http://example.com/#3" }], extData: { "uniq": r() } },
    { entries: [{ url: "http://example.com/#4" }], extData: { "uniq": r() } },
    { entries: [{ url: "http://example.com/#5" }], extData: { "uniq": r() } },
    { entries: [{ url: "http://example.com/#6" }], extData: { "uniq": r() } }
  ] }] };

  let loadCount = 0;
  function test_cascadeReloadSetup_progressCallback() {
    loadCount++;
    if (loadCount < state.windows[0].tabs.length)
      return;

    window.gBrowser.removeTabsProgressListener(progressListener);
    executeSoon(function() {
      _test_reloadAfter("test_reloadCascade", state, runNextTest);
    });
  }

  // This progress listener will get attached before the listener in session store.
  window.gBrowser.addTabsProgressListener(progressListener);
  ss.setBrowserState(JSON.stringify(state));
}


// This is a generic function that will attempt to reload each test. We do this
// a couple times, so make it utilitarian.
// This test expects that aState contains a single window and that each tab has
// a unique extData value eg. { "uniq": value }.
function _test_reloadAfter(aTestName, aState, aCallback) {
  info("starting " + aTestName);
  let progressListener = {
    onStateChange: function (aBrowser, aWebProgress, aRequest, aStateFlags, aStatus) {
      if (aStateFlags & Ci.nsIWebProgressListener.STATE_STOP &&
          aStateFlags & Ci.nsIWebProgressListener.STATE_IS_NETWORK &&
          aStateFlags & Ci.nsIWebProgressListener.STATE_IS_WINDOW)
        test_reloadAfter_progressCallback(aBrowser);
    }
  }

  // Simulate a left mouse button click with no modifiers, which is what
  // Command-R, or clicking reload does.
  let fakeEvent = {
    button: 0,
    metaKey: false,
    altKey: false,
    ctrlKey: false,
    shiftKey: false,
  }

  let loadCount = 0;
  function test_reloadAfter_progressCallback(aBrowser) {
    loadCount++;

    if (loadCount <= aState.windows[0].tabs.length) {
      // double check that this tab was the right one
      let expectedData = aState.windows[0].tabs[loadCount - 1].extData.uniq;
      let tab;
      for (let i = 0; i < window.gBrowser.tabs.length; i++) {
        if (!tab && window.gBrowser.tabs[i].linkedBrowser == aBrowser)
          tab = window.gBrowser.tabs[i];
      }
      is(ss.getTabValue(tab, "uniq"), expectedData,
         aTestName + ": load " + loadCount + " - correct tab was reloaded");

      if (loadCount == aState.windows[0].tabs.length) {
        window.gBrowser.removeTabsProgressListener(progressListener);
        aCallback();
      }
      else {
        // reload the next tab
        window.gBrowser.selectTabAtIndex(loadCount);
        BrowserReloadOrDuplicate(fakeEvent);
      }
    }
  }

  window.gBrowser.addTabsProgressListener(progressListener);
  BrowserReloadOrDuplicate(fakeEvent);
}


function countTabs() {
  let needsRestore = 0,
      isRestoring = 0,
      wasRestored = 0;

  let windowsEnum = Services.wm.getEnumerator("navigator:browser");

  while (windowsEnum.hasMoreElements()) {
    let window = windowsEnum.getNext();
    if (window.closed)
      continue;

    for (let i = 0; i < window.gBrowser.tabs.length; i++) {
      let browser = window.gBrowser.tabs[i].linkedBrowser;
      if (browser.__SS_restoreState == TAB_STATE_RESTORING)
        isRestoring++;
      else if (browser.__SS_restoreState == TAB_STATE_NEEDS_RESTORE)
        needsRestore++;
      else
        wasRestored++;
    }
  }
  return [needsRestore, isRestoring, wasRestored];
}

function r() {
  return "" + Date.now() + Math.random();
}

