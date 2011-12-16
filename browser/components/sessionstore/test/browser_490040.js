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

