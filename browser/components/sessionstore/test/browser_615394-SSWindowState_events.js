/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const stateBackup = JSON.parse(ss.getBrowserState());
const testState = {
  windows: [{
    tabs: [
      { entries: [{ url: "about:blank", triggeringPrincipal_base64 }] },
      { entries: [{ url: "about:rights", triggeringPrincipal_base64 }] }
    ]
  }]
};
const lameMultiWindowState = { windows: [
    {
      tabs: [
        { entries: [{ url: "http://example.org#1", triggeringPrincipal_base64 }], extData: { "uniq": r() } },
        { entries: [{ url: "http://example.org#2", triggeringPrincipal_base64 }], extData: { "uniq": r() } },
        { entries: [{ url: "http://example.org#3", triggeringPrincipal_base64 }], extData: { "uniq": r() } },
        { entries: [{ url: "http://example.org#4", triggeringPrincipal_base64 }], extData: { "uniq": r() } }
      ],
      selected: 1
    },
    {
      tabs: [
        { entries: [{ url: "http://example.com#1", triggeringPrincipal_base64 }], extData: { "uniq": r() } },
        { entries: [{ url: "http://example.com#2", triggeringPrincipal_base64 }], extData: { "uniq": r() } },
        { entries: [{ url: "http://example.com#3", triggeringPrincipal_base64 }], extData: { "uniq": r() } },
        { entries: [{ url: "http://example.com#4", triggeringPrincipal_base64 }], extData: { "uniq": r() } },
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
  requestLongerTimeout(4);
  runNextTest();
}


var tests = [
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
    let closeWinPromises = [];
    while (windowsEnum.hasMoreElements()) {
      var currentWindow = windowsEnum.getNext();
      if (currentWindow != window) {
        closeWinPromises.push(BrowserTestUtils.closeWindow(currentWindow));
      }
    }

    Promise.all(closeWinPromises).then(() => {
      let currentTest = tests.shift();
      info("prepping for " + currentTest.name);
      waitForBrowserState(testState, currentTest);
    });
  } else {
    waitForBrowserState(stateBackup, finish);
  }
}

/**  ACTUAL TESTS  **/

function test_setTabState() {
  let tab = gBrowser.tabs[1];
  let newTabState = JSON.stringify({ entries: [{ url: "http://example.org", triggeringPrincipal_base64 }], extData: { foo: "bar" } });
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

  function onSSTabRestoring(aEvent) {
    if (aEvent.target == tab) {
      is(busyEventCount, 1);
      is(readyEventCount, 1);
      is(ss.getTabValue(tab, "baz"), "qux");
      is(tab.linkedBrowser.currentURI.spec, "http://example.org/");

      window.removeEventListener("SSWindowStateBusy", onSSWindowStateBusy);
      window.removeEventListener("SSWindowStateReady", onSSWindowStateReady);
      gBrowser.tabContainer.removeEventListener("SSTabRestoring", onSSTabRestoring);

      runNextTest();
    }
  }

  window.addEventListener("SSWindowStateBusy", onSSWindowStateBusy);
  window.addEventListener("SSWindowStateReady", onSSWindowStateReady);
  gBrowser.tabContainer.addEventListener("SSTabRestoring", onSSTabRestoring);
  // Browser must be inserted in order to restore.
  gBrowser._insertBrowser(tab);
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

  function onSSWindowStateReady(aEvent) {
    newTab = gBrowser.tabs[2];
    readyEventCount++;
    is(ss.getTabValue(newTab, "foo"), "bar");
    ss.setTabValue(newTab, "baz", "qux");
  }

  function onSSTabRestoring(aEvent) {
    if (aEvent.target == newTab) {
      is(busyEventCount, 1);
      is(readyEventCount, 1);
      is(ss.getTabValue(newTab, "baz"), "qux");
      is(newTab.linkedBrowser.currentURI.spec, "about:rights");

      window.removeEventListener("SSWindowStateBusy", onSSWindowStateBusy);
      window.removeEventListener("SSWindowStateReady", onSSWindowStateReady);
      gBrowser.tabContainer.removeEventListener("SSTabRestoring", onSSTabRestoring);

      runNextTest();
    }
  }

  window.addEventListener("SSWindowStateBusy", onSSWindowStateBusy);
  window.addEventListener("SSWindowStateReady", onSSWindowStateReady);
  gBrowser.tabContainer.addEventListener("SSTabRestoring", onSSTabRestoring);

  gBrowser._insertBrowser(tab);
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

  function onSSWindowStateReady(aEvent) {
    reopenedTab = gBrowser.tabs[1];
    readyEventCount++;
    is(ss.getTabValue(reopenedTab, "foo"), "bar");
    ss.setTabValue(reopenedTab, "baz", "qux");
  }

  function onSSTabRestored(aEvent) {
    is(busyEventCount, 1);
    is(readyEventCount, 1);
    is(ss.getTabValue(reopenedTab, "baz"), "qux");
    is(reopenedTab.linkedBrowser.currentURI.spec, "about:rights");

    window.removeEventListener("SSWindowStateBusy", onSSWindowStateBusy);
    window.removeEventListener("SSWindowStateReady", onSSWindowStateReady);
    gBrowser.tabContainer.removeEventListener("SSTabRestored", onSSTabRestored);

    runNextTest();
  }

  window.addEventListener("SSWindowStateBusy", onSSWindowStateBusy);
  window.addEventListener("SSWindowStateReady", onSSWindowStateReady);
  gBrowser.tabContainer.addEventListener("SSTabRestored", onSSTabRestored);

  gBrowser.removeTab(tab);
  reopenedTab = ss.undoCloseTab(window, 0);
}


function test_setWindowState() {
  let newState = {
    windows: [{
      tabs: [
        { entries: [{ url: "about:mozilla", triggeringPrincipal_base64 }], extData: { "foo": "bar" } },
        { entries: [{ url: "http://example.org", triggeringPrincipal_base64 }], extData: { "baz": "qux" } }
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

    window.removeEventListener("SSWindowStateBusy", onSSWindowStateBusy);
    window.removeEventListener("SSWindowStateReady", onSSWindowStateReady);
    gBrowser.tabContainer.removeEventListener("SSTabRestored", onSSTabRestored);

    runNextTest();
  }

  window.addEventListener("SSWindowStateBusy", onSSWindowStateBusy);
  window.addEventListener("SSWindowStateReady", onSSWindowStateReady);
  gBrowser.tabContainer.addEventListener("SSTabRestored", onSSTabRestored);

  ss.setWindowState(window, JSON.stringify(newState), true);
}


function test_setBrowserState() {
  // We'll track events per window so we are sure that they are each happening once
  // pre window.
  let windowEvents = {};
  windowEvents[getOuterWindowID(window)] = { busyEventCount: 0, readyEventCount: 0 };

  // waitForBrowserState does it's own observing for windows, but doesn't attach
  // the listeners we want here, so do it ourselves.
  let newWindow;
  function windowObserver(aSubject, aTopic, aData) {
    if (aTopic == "domwindowopened") {
      newWindow = aSubject.QueryInterface(Ci.nsIDOMWindow);
      newWindow.addEventListener("load", function() {
        Services.ww.unregisterNotification(windowObserver);

        windowEvents[getOuterWindowID(newWindow)] = { busyEventCount: 0, readyEventCount: 0 };

        newWindow.addEventListener("SSWindowStateBusy", onSSWindowStateBusy);
        newWindow.addEventListener("SSWindowStateReady", onSSWindowStateReady);
      }, {once: true});
    }
  }

  function onSSWindowStateBusy(aEvent) {
    windowEvents[getOuterWindowID(aEvent.originalTarget)].busyEventCount++;
  }

  function onSSWindowStateReady(aEvent) {
    windowEvents[getOuterWindowID(aEvent.originalTarget)].readyEventCount++;
  }

  window.addEventListener("SSWindowStateBusy", onSSWindowStateBusy);
  window.addEventListener("SSWindowStateReady", onSSWindowStateReady);
  Services.ww.registerNotification(windowObserver);

  waitForBrowserState(lameMultiWindowState, function() {
    let checkedWindows = 0;
    for (let id of Object.keys(windowEvents)) {
      let winEvents = windowEvents[id];
      is(winEvents.busyEventCount, 1,
         "[test_setBrowserState] window" + id + " busy event count correct");
      is(winEvents.readyEventCount, 1,
         "[test_setBrowserState] window" + id + " ready event count correct");
      checkedWindows++;
    }
    is(checkedWindows, 2,
       "[test_setBrowserState] checked 2 windows");
    window.removeEventListener("SSWindowStateBusy", onSSWindowStateBusy);
    window.removeEventListener("SSWindowStateReady", onSSWindowStateReady);
    newWindow.removeEventListener("SSWindowStateBusy", onSSWindowStateBusy);
    newWindow.removeEventListener("SSWindowStateReady", onSSWindowStateReady);
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
    BrowserTestUtils.closeWindow(newWindow).then(() => {
      // Now give it time to close
      reopenedWindow = ss.undoCloseWindow(0);
      reopenedWindow.addEventListener("SSWindowStateBusy", onSSWindowStateBusy);
      reopenedWindow.addEventListener("SSWindowStateReady", onSSWindowStateReady);

      reopenedWindow.addEventListener("load", function() {
        reopenedWindow.gBrowser.tabContainer.addEventListener("SSTabRestored", onSSTabRestored);
      }, {once: true});
    });
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

    reopenedWindow.removeEventListener("SSWindowStateBusy", onSSWindowStateBusy);
    reopenedWindow.removeEventListener("SSWindowStateReady", onSSWindowStateReady);
    reopenedWindow.gBrowser.tabContainer.removeEventListener("SSTabRestored", onSSTabRestored);

    BrowserTestUtils.closeWindow(reopenedWindow).then(runNextTest);
  }
}
