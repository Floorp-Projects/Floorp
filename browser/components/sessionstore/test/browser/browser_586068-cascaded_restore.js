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


function test() {
  /** Test for Bug 586068 - Cascade page loads when restoring **/
  waitForExplicitFinish();
  runNextTest();
}

let tests = [test_cascade, test_select];
function runNextTest() {
  if (tests.length) {
    ss.setWindowState(window,
                      JSON.stringify({ windows: [{ tabs: [{ url: 'about:blank' }], }] }),
                      true);
    executeSoon(tests.shift());
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
      if (aStateFlags & Ci.nsIWebProgressListener.STATE_STOP &&
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
    // We'll get the first <length of windows[0].tabs> load events here, even
    // though they are ignored by sessionstore. Those are due to explicit
    // "stop" events before any restoring action takes place. We can safely
    // ignore these events.
    if (loadCount <= state.windows[0].tabs.length)
      return;

    let counts = countTabs();
    let expected = expectedCounts[loadCount - state.windows[0].tabs.length - 1];

    is(counts[0], expected[0], "test_cascade: load " + loadCount + " - # tabs that need to be restored");
    is(counts[1], expected[1], "test_cascade: load " + loadCount + " - # tabs that are restoring");
    is(counts[2], expected[2], "test_cascade: load " + loadCount + " - # tabs that has been restored");

    if (loadCount == state.windows[0].tabs.length * 2) {
      window.gBrowser.removeTabsProgressListener(progressListener);
      // Reset the pref
      try {
        Services.prefs.clearUserPref("browser.sessionstore.max_concurrent_tabs");
      } catch (e) {}
      runNextTest();
    }
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
      if (aStateFlags & Ci.nsIWebProgressListener.STATE_STOP &&
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
  ], selectedIndex: 1 }] };

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
    // We'll get the first <length of windows[0].tabs> load events here, even
    // though they are ignored by sessionstore. Those are due to explicit
    // "stop" events before any restoring action takes place. We can safely
    // ignore these events.
    if (loadCount <= state.windows[0].tabs.length)
      return;

    let loadIndex = loadCount - state.windows[0].tabs.length - 1;
    let counts = countTabs();
    let expected = expectedCounts[loadIndex];

    is(counts[0], expected[0], "test_select: load " + loadCount + " - # tabs that need to be restored");
    is(counts[1], expected[1], "test_select: load " + loadCount + " - # tabs that are restoring");
    is(counts[2], expected[2], "test_select: load " + loadCount + " - # tabs that has been restored");

    if (loadCount == state.windows[0].tabs.length * 2) {
      window.gBrowser.removeTabsProgressListener(progressListener);
      // Reset the pref
      try {
        Services.prefs.clearUserPref("browser.sessionstore.max_concurrent_tabs");
      } catch (e) {}
      runNextTest();
    }
    else {
      // double check that this tab was the right one
      let expectedData = state.windows[0].tabs[tabOrder[loadIndex]].extData.uniq;
      let tab;
      for (let i = 0; i < window.gBrowser.tabs.length; i++) {
        if (!tab && window.gBrowser.tabs[i].linkedBrowser == aBrowser)
          tab = window.gBrowser.tabs[i];
      }
      is(ss.getTabValue(tab, "uniq"), expectedData, "test_select: load " + loadCount + " - correct tab was restored");

      // select the next tab
      window.gBrowser.selectTabAtIndex(tabOrder[loadIndex + 1]);
    }
  }

  window.gBrowser.addTabsProgressListener(progressListener);
  ss.setBrowserState(JSON.stringify(state));
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
      if (browser.__SS_restoring)
        isRestoring++;
      else if (browser.__SS_needsRestore)
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

