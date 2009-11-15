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
 * Simon Bünzli <zeniko@gmail.com>.
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

function waitForBrowserState(aState, aSetStateCallback) {
  let os = Cc["@mozilla.org/observer-service;1"].
           getService(Ci.nsIObserverService);
  let observer = {
    observe: function(aSubject, aTopic, aData) {
      os.removeObserver(this, "sessionstore-browser-state-restored");
      executeSoon(aSetStateCallback);
    }
  };
  os.addObserver(observer, "sessionstore-browser-state-restored", false);
  let ss = Cc["@mozilla.org/browser/sessionstore;1"].
           getService(Ci.nsISessionStore);
  ss.setBrowserState(JSON.stringify(aState));
}

function test() {
  /** Test for Bug 394759 **/
  
  // test setup
  let ss = Cc["@mozilla.org/browser/sessionstore;1"].getService(Ci.nsISessionStore);
  let pb = Cc["@mozilla.org/privatebrowsing;1"].getService(Ci.nsIPrivateBrowsingService);
  waitForExplicitFinish();
  
  function test_basic(callback) {
  
    let testURL = "about:config";
    let uniqueKey = "bug 394759";
    let uniqueValue = "unik" + Date.now();
    let uniqueText = "pi != " + Math.random();
  
  
    // make sure that the next closed window will increase getClosedWindowCount
    let max_windows_undo = gPrefService.getIntPref("browser.sessionstore.max_windows_undo");
    gPrefService.setIntPref("browser.sessionstore.max_windows_undo", max_windows_undo + 1);
    let closedWindowCount = ss.getClosedWindowCount();
  
    let newWin = openDialog(location, "_blank", "chrome,all,dialog=no", testURL);
    newWin.addEventListener("load", function(aEvent) {
      newWin.removeEventListener("load", arguments.callee, false);
      newWin.gBrowser.addEventListener("load", function(aEvent) {
        newWin.gBrowser.removeEventListener("load", arguments.callee, true);

        executeSoon(function() {
          newWin.gBrowser.addTab().linkedBrowser.stop();
          executeSoon(function() {
            // mark the window with some unique data to be restored later on
            ss.setWindowValue(newWin, uniqueKey, uniqueValue);
            let textbox = newWin.content.document.getElementById("textbox");
            textbox.wrappedJSObject.value = uniqueText;

            newWin.close();

            is(ss.getClosedWindowCount(), closedWindowCount + 1,
               "The closed window was added to Recently Closed Windows");
            let data = JSON.parse(ss.getClosedWindowData())[0];
            ok(data.title == testURL && data.toSource().indexOf(uniqueText) > -1,
               "The closed window data was stored correctly");

            // reopen the closed window and ensure its integrity
            let newWin2 = ss.undoCloseWindow(0);

            ok(newWin2 instanceof ChromeWindow,
               "undoCloseWindow actually returned a window");
            is(ss.getClosedWindowCount(), closedWindowCount,
               "The reopened window was removed from Recently Closed Windows");

            newWin2.addEventListener("load", function(aEvent) {
              newWin2.removeEventListener("load", arguments.callee, false);
              newWin2.gBrowser.addEventListener("SSTabRestored", function(aEvent) {
                newWin2.gBrowser.removeEventListener("SSTabRestored", arguments.callee, true);

                executeSoon(function() {
                  is(newWin2.gBrowser.tabContainer.childNodes.length, 2,
                     "The window correctly restored 2 tabs");
                  is(newWin2.gBrowser.currentURI.spec, testURL,
                     "The window correctly restored the URL");

                  let textbox = newWin2.content.document.getElementById("textbox");
                  is(textbox.wrappedJSObject.value, uniqueText,
                     "The window correctly restored the form");
                  is(ss.getWindowValue(newWin2, uniqueKey), uniqueValue,
                     "The window correctly restored the data associated with it");

                  // clean up
                  newWin2.close();
                  if (gPrefService.prefHasUserValue("browser.sessionstore.max_windows_undo"))
                    gPrefService.clearUserPref("browser.sessionstore.max_windows_undo");
                  executeSoon(callback);
                });
              }, true);
            }, false);
          });
        });
      }, true);
    }, false);
  }
  
  function test_behavior (callback) {
    // helper function that does the actual testing
    function openWindowRec(windowsToOpen, expectedResults, recCallback) {
      // do actual checking
      if (!windowsToOpen.length) {
        let closedWindowData = JSON.parse(ss.getClosedWindowData());
        let numPopups = closedWindowData.filter(function(el, i, arr) {
          return el.isPopup;
        }).length;
        let numNormal = ss.getClosedWindowCount() - numPopups;
        // #ifdef doesn't work in browser-chrome tests, so do a simple regex on platform
        let oResults = navigator.platform.match(/Mac/) ? expectedResults.mac
                                                       : expectedResults.other;
        is(numPopups, oResults.popup,
           "There were " + oResults.popup + " popup windows to repoen");
        is(numNormal, oResults.normal,
           "There were " + oResults.normal + " normal windows to repoen");

        // cleanup & return
        executeSoon(recCallback);
        return;
      }
      // hack to force window to be considered a popup (toolbar=no didn't work)
      let winData = windowsToOpen.shift();
      let settings = "chrome,dialog=no," +
                     (winData.isPopup ? "all=no" : "all");
      let url = "http://window" + windowsToOpen.length + ".example.com";
      let window = openDialog(location, "_blank", settings, url);
      window.addEventListener("load", function(aEvent) {
        this.removeEventListener("load", arguments.callee, false);
        window.gBrowser.addEventListener("load", function(aEvent) {
          this.removeEventListener("load", arguments.callee, true);
          // the window _should_ have state with a tab of url, but it doesn't
          // always happend before window.close(). addTab ensure we don't treat
          // this window as a stateless window
          window.gBrowser.addTab();

          executeSoon(function() {
            window.close();
            executeSoon(function() {
              openWindowRec(windowsToOpen, expectedResults, recCallback);
            });
          });
        }, true);
      }, false);
    }

    let windowsToOpen = [{isPopup: false},
                         {isPopup: false},
                         {isPopup: true},
                         {isPopup: true},
                         {isPopup: true}];
    let expectedResults = {mac: {popup: 3, normal: 0},
                           other: {popup: 3, normal: 1}};
    let windowsToOpen2 = [{isPopup: false},
                          {isPopup: false},
                          {isPopup: false},
                          {isPopup: false},
                          {isPopup: false}];
    let expectedResults2 = {mac: {popup: 0, normal: 3},
                            other: {popup: 0, normal: 3}};
    openWindowRec(windowsToOpen, expectedResults, function() {
      openWindowRec(windowsToOpen2, expectedResults2, callback);
    });
  }
  
  function test_purge(callback) {
    // utility functions
    function countClosedTabsByTitle(aClosedTabList, aTitle)
      aClosedTabList.filter(function(aData) aData.title == aTitle).length;

    function countOpenTabsByTitle(aOpenTabList, aTitle)
      aOpenTabList.filter(function(aData) aData.entries.some(function(aEntry) aEntry.title == aTitle) ).length

    // create a new state for testing
    const REMEMBER = Date.now(), FORGET = Math.random();
    let testState = {
      windows: [ { tabs: [{ entries: [{ url: "http://example.com/" }] }], selected: 1 } ],
      _closedWindows : [
        // _closedWindows[0]
        {
          tabs: [
            { entries: [{ url: "http://example.com/", title: REMEMBER }] },
            { entries: [{ url: "http://mozilla.org/", title: FORGET }] }
          ],
          selected: 2,
          title: "mozilla.org",
          _closedTabs: []
        },
        // _closedWindows[1]
        {
          tabs: [
           { entries: [{ url: "http://mozilla.org/", title: FORGET }] },
           { entries: [{ url: "http://example.com/", title: REMEMBER }] },
           { entries: [{ url: "http://example.com/", title: REMEMBER }] },
           { entries: [{ url: "http://mozilla.org/", title: FORGET }] },
           { entries: [{ url: "http://example.com/", title: REMEMBER }] }
          ],
          selected: 5,
          _closedTabs: []
        },
        // _closedWindows[2]
        {
          tabs: [
            { entries: [{ url: "http://example.com/", title: REMEMBER }] }
          ],
          selected: 1,
          _closedTabs: [
            {
              state: {
                entries: [
                  { url: "http://mozilla.org/", title: FORGET },
                  { url: "http://mozilla.org/again", title: "doesn't matter" }
                ]
              },
              pos: 1,
              title: FORGET
            },
            {
              state: {
                entries: [
                  { url: "http://example.com", title: REMEMBER }
                ]
              },
              title: REMEMBER
            }
          ]
        }
      ]
    };
    
    // set browser to test state
    waitForBrowserState(testState, function() {
      // purge domain & check that we purged correctly for closed windows
      pb.removeDataFromDomain("mozilla.org");

      let closedWindowData = JSON.parse(ss.getClosedWindowData());

      // First set of tests for _closedWindows[0] - tests basics
      let window = closedWindowData[0];
      is(window.tabs.length, 1, "1 tab was removed");
      is(countOpenTabsByTitle(window.tabs, FORGET), 0,
         "The correct tab was removed");
      is(countOpenTabsByTitle(window.tabs, REMEMBER), 1,
         "The correct tab was remembered");
      is(window.selected, 1, "Selected tab has changed");
      is(window.title, REMEMBER, "The window title was correctly updated");

      // Test more complicated case 
      window = closedWindowData[1];
      is(window.tabs.length, 3, "2 tabs were removed");
      is(countOpenTabsByTitle(window.tabs, FORGET), 0,
         "The correct tabs were removed");
      is(countOpenTabsByTitle(window.tabs, REMEMBER), 3,
         "The correct tabs were remembered");
      is(window.selected, 3, "Selected tab has changed");
      is(window.title, REMEMBER, "The window title was correctly updated");

      // Tests handling of _closedTabs
      window = closedWindowData[2];
      is(countClosedTabsByTitle(window._closedTabs, REMEMBER), 1,
         "The correct number of tabs were removed, and the correct ones");
      is(countClosedTabsByTitle(window._closedTabs, FORGET), 0,
         "All tabs to be forgotten were indeed removed");

      // Restore blank state.
      let blankState = {
        windows: [{
          tabs: [{ entries: [{ url: "about:blank" }] }],
          _closedTabs: []
        }],
        _closedWindows: []
      };
      waitForBrowserState(blankState, callback);
    });
  }
  
  test_basic(function() {
    test_behavior(function() {
      test_purge(function() {
        finish();
      });
    });
  });
}
