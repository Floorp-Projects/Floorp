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

// Mirrors WINDOW_ATTRIBUTES IN nsSessionStore.js
const WINDOW_ATTRIBUTES = ["width", "height", "screenX", "screenY", "sizemode"];

Cu.import("resource://gre/modules/Services.jsm");
let ss = Cc["@mozilla.org/browser/sessionstore;1"].
         getService(Ci.nsISessionStore);

let stateBackup = ss.getBrowserState();

let originalWarnOnClose = gPrefService.getBoolPref("browser.tabs.warnOnClose");
let originalStartupPage = gPrefService.getIntPref("browser.startup.page");
let originalWindowType = document.documentElement.getAttribute("windowtype");

let gotLastWindowClosedTopic = false;
let shouldPinTab = false;
let shouldOpenTabs = false;
let shouldCloseTab = false;
let testNum = 0;
let afterTestCallback;

// Set state so we know the closed windows content
let testState = {
  windows: [
    { tabs: [{ entries: [{ url: "http://example.org" }] }] }
  ],
  _closedWindows: []
};

// We'll push a set of conditions and callbacks into this array
// Ideally we would also test win/linux under a complete set of conditions, but
// the tests for osx mirror the other set of conditions possible on win/linux.
let tests = [];

// the third & fourth test share a condition check, keep it DRY
function checkOSX34Generator(num) {
  return function(aPreviousState, aCurState) {
    // In here, we should have restored the pinned tab, so only the unpinned tab
    // should be in aCurState. So let's shape our expectations.
    let expectedState = JSON.parse(aPreviousState);
    expectedState[0].tabs.shift();
    // size attributes are stripped out in _prepDataForDeferredRestore in nsSessionStore.
    // This isn't the best approach, but neither is comparing JSON strings
    WINDOW_ATTRIBUTES.forEach(function (attr) delete expectedState[0][attr]);

    is(aCurState, JSON.stringify(expectedState),
       "test #" + num + ": closedWindowState is as expected");
  };
}
function checkNoWindowsGenerator(num) {
  return function(aPreviousState, aCurState) {
    is(aCurState, "[]", "test #" + num + ": there should be no closedWindowsLeft");
  };
}

// The first test has 0 pinned tabs and 1 unpinned tab
tests.push({
  pinned: false,
  extra: false,
  close: false,
  checkWinLin: checkNoWindowsGenerator(1),
  checkOSX: function(aPreviousState, aCurState) {
    is(aCurState, aPreviousState, "test #1: closed window state is unchanged");
  }
});

// The second test has 1 pinned tab and 0 unpinned tabs.
tests.push({
  pinned: true,
  extra: false,
  close: false,
  checkWinLin: checkNoWindowsGenerator(2),
  checkOSX: checkNoWindowsGenerator(2)
});

// The third test has 1 pinned tab and 2 unpinned tabs.
tests.push({
  pinned: true,
  extra: true,
  close: false,
  checkWinLin: checkNoWindowsGenerator(3),
  checkOSX: checkOSX34Generator(3)
});

// The fourth test has 1 pinned tab, 2 unpinned tabs, and closes one unpinned tab.
tests.push({
  pinned: true,
  extra: true,
  close: "one",
  checkWinLin: checkNoWindowsGenerator(4),
  checkOSX: checkOSX34Generator(4)
});

// The fifth test has 1 pinned tab, 2 unpinned tabs, and closes both unpinned tabs.
tests.push({
  pinned: true,
  extra: true,
  close: "both",
  checkWinLin: checkNoWindowsGenerator(5),
  checkOSX: checkNoWindowsGenerator(5)
});


function test() {
  /** Test for Bug 589246 - Closed window state getting corrupted when closing
      and reopening last browser window without exiting browser **/
  waitForExplicitFinish();
  // windows opening & closing, so extending the timeout
  requestLongerTimeout(2);

  // We don't want the quit dialog pref
  gPrefService.setBoolPref("browser.tabs.warnOnClose", false);
  // Ensure that we would restore the session (important for Windows)
  gPrefService.setIntPref("browser.startup.page", 3);

  runNextTestOrFinish();
}

function runNextTestOrFinish() {
  if (tests.length) {
    setupForTest(tests.shift())
  }
  else {
    // some state is cleaned up at the end of each test, but not all
    ["browser.tabs.warnOnClose", "browser.startup.page"].forEach(function(p) {
      if (gPrefService.prefHasUserValue(p))
        gPrefService.clearUserPref(p);
    });

    ss.setBrowserState(stateBackup);
    executeSoon(finish);
  }
}

function setupForTest(aConditions) {
  // reset some checks
  gotLastWindowClosedTopic = false;
  shouldPinTab = aConditions.pinned;
  shouldOpenTabs = aConditions.extra;
  shouldCloseTab = aConditions.close;
  testNum++;

  // set our test callback
  afterTestCallback = /Mac/.test(navigator.platform) ? aConditions.checkOSX
                                                     : aConditions.checkWinLin;

  // Add observers
  Services.obs.addObserver(onLastWindowClosed, "browser-lastwindow-close-granted", false);

  // Set the state
  Services.obs.addObserver(onStateRestored, "sessionstore-browser-state-restored", false);
  ss.setBrowserState(JSON.stringify(testState));
}

function onStateRestored(aSubject, aTopic, aData) {
  info("test #" + testNum + ": onStateRestored");
  Services.obs.removeObserver(onStateRestored, "sessionstore-browser-state-restored", false);

  // change this window's windowtype so that closing a new window will trigger
  // browser-lastwindow-close-granted.
  document.documentElement.setAttribute("windowtype", "navigator:testrunner");

  let newWin = openDialog(location, "_blank", "chrome,all,dialog=no", "http://example.com");
  newWin.addEventListener("load", function(aEvent) {
    newWin.removeEventListener("load", arguments.callee, false);

    newWin.gBrowser.selectedBrowser.addEventListener("load", function() {
      newWin.gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);

      // pin this tab
      if (shouldPinTab)
        newWin.gBrowser.pinTab(newWin.gBrowser.selectedTab);

      newWin.addEventListener("unload", onWindowUnloaded, false);
      // Open a new tab as well. On Windows/Linux this will be restored when the
      // new window is opened below (in onWindowUnloaded). On OS X we'll just
      // restore the pinned tabs, leaving the unpinned tab in the closedWindowsData.
      if (shouldOpenTabs) {
        let newTab = newWin.gBrowser.addTab("about:config");
        let newTab2 = newWin.gBrowser.addTab("about:buildconfig");

        newTab.linkedBrowser.addEventListener("load", function() {
          newTab.linkedBrowser.removeEventListener("load", arguments.callee, true);

          if (shouldCloseTab == "one") {
            newWin.gBrowser.removeTab(newTab2);
          }
          else if (shouldCloseTab == "both") {
            newWin.gBrowser.removeTab(newTab);
            newWin.gBrowser.removeTab(newTab2);
          }
          newWin.BrowserTryToCloseWindow();
        }, true);
      }
      else {
        newWin.BrowserTryToCloseWindow();
      }
    }, true);
  }, false);
}

// This will be called before the window is actually closed
function onLastWindowClosed(aSubject, aTopic, aData) {
  info("test #" + testNum + ": onLastWindowClosed");
  Services.obs.removeObserver(onLastWindowClosed, "browser-lastwindow-close-granted", false);
  gotLastWindowClosedTopic = true;
}

// This is the unload event listener on the new window (from onStateRestored).
// Unload is fired after the window is closed, so sessionstore has already
// updated _closedWindows (which is important). We'll open a new window here
// which should actually trigger the bug.
function onWindowUnloaded() {
  info("test #" + testNum + ": onWindowClosed");
  ok(gotLastWindowClosedTopic, "test #" + testNum + ": browser-lastwindow-close-granted was notified prior");

  let previousClosedWindowData = ss.getClosedWindowData();

  // Now we want to open a new window
  let newWin = openDialog(location, "_blank", "chrome,all,dialog=no", "about:robots");
  newWin.addEventListener("load", function(aEvent) {
    newWin.removeEventListener("load", arguments.callee, false);

    newWin.gBrowser.selectedBrowser.addEventListener("load", function () {
      // Good enough for checking the state
      afterTestCallback(previousClosedWindowData, ss.getClosedWindowData());
      afterTestCleanup(newWin);
    }, true);

  }, false);
}

function afterTestCleanup(aNewWin) {
  executeSoon(function() {
    aNewWin.close();
    document.documentElement.setAttribute("windowtype", originalWindowType);
    runNextTestOrFinish();
  });
}

