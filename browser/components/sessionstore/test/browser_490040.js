/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  /** Test for Bug 490040 **/

  waitForExplicitFinish();

  function testWithState(aState) {
    // Ensure we can store the window if needed.
    let curClosedWindowCount = ss.getClosedWindowCount();
    gPrefService.setIntPref("browser.sessionstore.max_windows_undo",
                            curClosedWindowCount + 1);

    var origWin;
    function windowObserver(aSubject, aTopic, aData) {
      let theWin = aSubject.QueryInterface(Ci.nsIDOMWindow);
      if (origWin && theWin != origWin)
        return;

      switch (aTopic) {
        case "domwindowopened":
          origWin = theWin;
          theWin.addEventListener("load", function () {
            theWin.removeEventListener("load", arguments.callee, false);
            executeSoon(function () {
              // Close the window as soon as the first tab loads, or
              // immediately if there are no tabs.
              if (aState.windowState.windows[0].tabs[0].entries.length) {
                theWin.gBrowser.addEventListener("load", function() {
                  theWin.gBrowser.removeEventListener("load",
                                                      arguments.callee, true);
                  theWin.close();
                }, true);
              } else {
                executeSoon(function () {
                  theWin.close();
                });
              }
              ss.setWindowState(theWin, JSON.stringify(aState.windowState),
                                true);
            });
          }, false);
          break;

        case "domwindowclosed":
          Services.ww.unregisterNotification(windowObserver);
          // Use executeSoon to ensure this happens after SS observer.
          executeSoon(function () {
            is(ss.getClosedWindowCount(),
               curClosedWindowCount + (aState.shouldBeAdded ? 1 : 0),
               "That window should " + (aState.shouldBeAdded ? "" : "not ") +
               "be restorable");
            executeSoon(runNextTest);
          });
          break;
      }
    }
    Services.ww.registerNotification(windowObserver);
    Services.ww.openWindow(null,
                           location,
                           "_blank",
                           "chrome,all,dialog=no",
                           null);
  }

  // Only windows with open tabs are restorable. Windows where a lone tab is
  // detached may have _closedTabs, but is left with just an empty tab.
  let states = [
    {
      shouldBeAdded: true,
      windowState: {
        windows: [{
          tabs: [{ entries: [{ url: "http://example.com", title: "example.com" }] }],
          selected: 1,
          _closedTabs: []
        }]
      }
    },
    {
      shouldBeAdded: false,
      windowState: {
        windows: [{
          tabs: [{ entries: [] }],
          _closedTabs: []
        }]
      }
    },
    {
      shouldBeAdded: false,
      windowState: {
        windows: [{
          tabs: [{ entries: [] }],
          _closedTabs: [{ state: { entries: [{ url: "http://example.com", index: 1 }] } }]
        }]
      }
    },
    {
      shouldBeAdded: false,
      windowState: {
        windows: [{
          tabs: [{ entries: [] }],
          _closedTabs: [],
          extData: { keyname: "pi != " + Math.random() }
        }]
      }
    }
  ];

  function runNextTest() {
    if (states.length) {
      let state = states.shift();
      testWithState(state);
    }
    else {
      gPrefService.clearUserPref("browser.sessionstore.max_windows_undo");
      finish();
    }
  }
  runNextTest();
}

