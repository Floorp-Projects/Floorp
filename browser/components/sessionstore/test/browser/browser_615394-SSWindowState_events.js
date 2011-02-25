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
 *   Paul O’Shannessy <paul@oshannessy.com>
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
const ss = Cc["@mozilla.org/browser/sessionstore;1"].
           getService(Ci.nsISessionStore);

const stateBackup = ss.getBrowserState();
const testState = {
  windows: [{
    tabs: [
      { entries: [{ url: "about:blank" }] },
      { entries: [{ url: "about:robots" }] }
    ]
  }]
};
const lameMultiWindowState = { windows: [
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


function getOuterWindowID(aWindow) {
  return aWindow.QueryInterface(Ci.nsIInterfaceRequestor).
         getInterface(Ci.nsIDOMWindowUtils).outerWindowID;
}

function test() {
  /** Test for Bug 615394 - Session Restore should notify when it is beginning and ending a restore **/
  waitForExplicitFinish();
  // Preemptively extend the timeout to prevent [orange]
  requestLongerTimeout(2);
  runNextTest();
}


let tests = [
  test_setTabState,
  test_duplicateTab,
  test_undoCloseTab,
  test_setWindowState,
  test_setBrowserState,
  test_undoCloseWindow
];
function runNextTest() {
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

    let currentTest = tests.shift();
    info("prepping for " + currentTest.name);
    waitForBrowserState(testState, currentTest);
  }
  else {
    ss.setBrowserState(stateBackup);
    finish();
  }
}

/**  ACTUAL TESTS  **/

function test_setTabState() {
  let tab = gBrowser.tabs[1];
  let newTabState = JSON.stringify({ entries: [{ url: "http://example.org" }], extData: { foo: "bar" } });
  let busyEventCount = 0;
  let readyEventCount = 0;

  function onSSWindowStateBusy(aEvent) {
    busyEventCount++;
  }

  function onSSWindowStateReady(aEvent) {
    readyEventCount++;
    is(ss.getTabValue(tab, "foo"), "bar");
    ss.setTabValue(tab, "baz", "qux");
  }

  function onSSTabRestored(aEvent) {
    is(busyEventCount, 1);
    is(readyEventCount, 1);
    is(ss.getTabValue(tab, "baz"), "qux");
    is(tab.linkedBrowser.currentURI.spec, "http://example.org/");

    window.removeEventListener("SSWindowStateBusy", onSSWindowStateBusy, false);
    window.removeEventListener("SSWindowStateReady", onSSWindowStateReady, false);
    gBrowser.tabContainer.removeEventListener("SSTabRestored", onSSTabRestored, false);

    runNextTest();
  }

  window.addEventListener("SSWindowStateBusy", onSSWindowStateBusy, false);
  window.addEventListener("SSWindowStateReady", onSSWindowStateReady, false);
  gBrowser.tabContainer.addEventListener("SSTabRestored", onSSTabRestored, false);
  ss.setTabState(tab, newTabState);
}


function test_duplicateTab() {
  let tab = gBrowser.tabs[1];
  let busyEventCount = 0;
  let readyEventCount = 0;
  let newTab;

  // We'll look to make sure this value is on the duplicated tab
  ss.setTabValue(tab, "foo", "bar");

  function onSSWindowStateBusy(aEvent) {
    busyEventCount++;
  }

  // duplicateTab is "synchronous" in tab creation. Since restoreHistory is called
  // via setTimeout, newTab will be assigned before the SSWindowStateReady event
  function onSSWindowStateReady(aEvent) {
    readyEventCount++;
    is(ss.getTabValue(newTab, "foo"), "bar");
    ss.setTabValue(newTab, "baz", "qux");
  }

  function onSSTabRestored(aEvent) {
    is(busyEventCount, 1);
    is(readyEventCount, 1);
    is(ss.getTabValue(newTab, "baz"), "qux");
    is(newTab.linkedBrowser.currentURI.spec, "about:robots");

    window.removeEventListener("SSWindowStateBusy", onSSWindowStateBusy, false);
    window.removeEventListener("SSWindowStateReady", onSSWindowStateReady, false);
    gBrowser.tabContainer.removeEventListener("SSTabRestored", onSSTabRestored, false);

    runNextTest();
  }

  window.addEventListener("SSWindowStateBusy", onSSWindowStateBusy, false);
  window.addEventListener("SSWindowStateReady", onSSWindowStateReady, false);
  gBrowser.tabContainer.addEventListener("SSTabRestored", onSSTabRestored, false);

  newTab = ss.duplicateTab(window, tab);
}


function test_undoCloseTab() {
  let tab = gBrowser.tabs[1],
      busyEventCount = 0,
      readyEventCount = 0,
      reopenedTab;

  ss.setTabValue(tab, "foo", "bar");

  function onSSWindowStateBusy(aEvent) {
    busyEventCount++;
  }

  // undoCloseTab is "synchronous" in tab creation. Since restoreHistory is called
  // via setTimeout, reopenedTab will be assigned before the SSWindowStateReady event
  function onSSWindowStateReady(aEvent) {
    readyEventCount++;
    is(ss.getTabValue(reopenedTab, "foo"), "bar");
    ss.setTabValue(reopenedTab, "baz", "qux");
  }

  function onSSTabRestored(aEvent) {
    is(busyEventCount, 1);
    is(readyEventCount, 1);
    is(ss.getTabValue(reopenedTab, "baz"), "qux");
    is(reopenedTab.linkedBrowser.currentURI.spec, "about:robots");

    window.removeEventListener("SSWindowStateBusy", onSSWindowStateBusy, false);
    window.removeEventListener("SSWindowStateReady", onSSWindowStateReady, false);
    gBrowser.tabContainer.removeEventListener("SSTabRestored", onSSTabRestored, false);

    runNextTest();
  }

  window.addEventListener("SSWindowStateBusy", onSSWindowStateBusy, false);
  window.addEventListener("SSWindowStateReady", onSSWindowStateReady, false);
  gBrowser.tabContainer.addEventListener("SSTabRestored", onSSTabRestored, false);

  gBrowser.removeTab(tab);
  reopenedTab = ss.undoCloseTab(window, 0);
}


function test_setWindowState() {
  let testState = {
    windows: [{
      tabs: [
        { entries: [{ url: "about:mozilla" }], extData: { "foo": "bar" } },
        { entries: [{ url: "http://example.org" }], extData: { "baz": "qux" } }
      ]
    }]
  };

  let busyEventCount = 0,
      readyEventCount = 0,
      tabRestoredCount = 0;

  function onSSWindowStateBusy(aEvent) {
    busyEventCount++;
  }

  function onSSWindowStateReady(aEvent) {
    readyEventCount++;
    is(ss.getTabValue(gBrowser.tabs[0], "foo"), "bar");
    is(ss.getTabValue(gBrowser.tabs[1], "baz"), "qux");
  }

  function onSSTabRestored(aEvent) {
    if (++tabRestoredCount < 2)
      return;

    is(busyEventCount, 1);
    is(readyEventCount, 1);
    is(gBrowser.tabs[0].linkedBrowser.currentURI.spec, "about:mozilla");
    is(gBrowser.tabs[1].linkedBrowser.currentURI.spec, "http://example.org/");

    window.removeEventListener("SSWindowStateBusy", onSSWindowStateBusy, false);
    window.removeEventListener("SSWindowStateReady", onSSWindowStateReady, false);
    gBrowser.tabContainer.removeEventListener("SSTabRestored", onSSTabRestored, false);

    runNextTest();
  }

  window.addEventListener("SSWindowStateBusy", onSSWindowStateBusy, false);
  window.addEventListener("SSWindowStateReady", onSSWindowStateReady, false);
  gBrowser.tabContainer.addEventListener("SSTabRestored", onSSTabRestored, false);

  ss.setWindowState(window, JSON.stringify(testState), true);
}


function test_setBrowserState() {
  // We'll track events per window so we are sure that they are each happening once
  // pre window.
  let windowEvents = {};
  windowEvents[getOuterWindowID(window)] = { busyEventCount: 0, readyEventCount: 0 };

  // waitForBrowserState does it's own observing for windows, but doesn't attach
  // the listeners we want here, so do it ourselves.
  function windowObserver(aSubject, aTopic, aData) {
    if (aTopic == "domwindowopened") {
      let newWindow = aSubject.QueryInterface(Ci.nsIDOMWindow);
      newWindow.addEventListener("load", function() {
        newWindow.removeEventListener("load", arguments.callee, false);
        Services.ww.unregisterNotification(windowObserver);

        windowEvents[getOuterWindowID(newWindow)] = { busyEventCount: 0, readyEventCount: 0 };

        newWindow.addEventListener("SSWindowStateBusy", onSSWindowStateBusy, false);
        newWindow.addEventListener("SSWindowStateReady", onSSWindowStateReady, false);
      }, false);
    }
  }

  function onSSWindowStateBusy(aEvent) {
    windowEvents[getOuterWindowID(aEvent.originalTarget)].busyEventCount++;
  }

  function onSSWindowStateReady(aEvent) {
    windowEvents[getOuterWindowID(aEvent.originalTarget)].readyEventCount++;
  }

  window.addEventListener("SSWindowStateBusy", onSSWindowStateBusy, false);
  window.addEventListener("SSWindowStateReady", onSSWindowStateReady, false);
  Services.ww.registerNotification(windowObserver);

  waitForBrowserState(lameMultiWindowState, function() {
    let checkedWindows = 0;
    for each (let [id, winEvents] in Iterator(windowEvents)) {
      is(winEvents.busyEventCount, 1,
         "[test_setBrowserState] window" + id + " busy event count correct");
      is(winEvents.readyEventCount, 1,
         "[test_setBrowserState] window" + id + " ready event count correct");
      checkedWindows++;
    }
    is(checkedWindows, 2,
       "[test_setBrowserState] checked 2 windows");
    window.removeEventListener("SSWindowStateBusy", onSSWindowStateBusy, false);
    window.removeEventListener("SSWindowStateReady", onSSWindowStateReady, false);
    runNextTest();
  });
}


function test_undoCloseWindow() {
  let newWindow, reopenedWindow;

  function firstWindowObserver(aSubject, aTopic, aData) {
    if (aTopic == "domwindowopened") {
      newWindow = aSubject.QueryInterface(Ci.nsIDOMWindow);
      Services.ww.unregisterNotification(firstWindowObserver);
    }
  }
  Services.ww.registerNotification(firstWindowObserver);

  waitForBrowserState(lameMultiWindowState, function() {
    // Close the window which isn't window
    newWindow.close();
    reopenedWindow = ss.undoCloseWindow(0);
    reopenedWindow.addEventListener("SSWindowStateBusy", onSSWindowStateBusy, false);
    reopenedWindow.addEventListener("SSWindowStateReady", onSSWindowStateReady, false);

    reopenedWindow.addEventListener("load", function() {
      reopenedWindow.removeEventListener("load", arguments.callee, false);
      reopenedWindow.gBrowser.tabContainer.addEventListener("SSTabRestored", onSSTabRestored, false);
    }, false);
  });

  let busyEventCount = 0,
      readyEventCount = 0,
      tabRestoredCount = 0;
  // These will listen to the reopened closed window...
  function onSSWindowStateBusy(aEvent) {
    busyEventCount++;
  }

  function onSSWindowStateReady(aEvent) {
    readyEventCount++;
  }

  function onSSTabRestored(aEvent) {
    if (++tabRestoredCount < 4)
      return;

    is(busyEventCount, 1);
    is(readyEventCount, 1);

    reopenedWindow.removeEventListener("SSWindowStateBusy", onSSWindowStateBusy, false);
    reopenedWindow.removeEventListener("SSWindowStateReady", onSSWindowStateReady, false);
    reopenedWindow.gBrowser.tabContainer.removeEventListener("SSTabRestored", onSSTabRestored, false);

    reopenedWindow.close();

    runNextTest();
  }
}

