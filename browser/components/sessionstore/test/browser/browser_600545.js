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
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Michael Kraft <morac99-firefox2@yahoo.com>
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

let ss = Cc["@mozilla.org/browser/sessionstore;1"].
         getService(Ci.nsISessionStore);

let stateBackup = ss.getBrowserState();

function test() {
  /** Test for Bug 600545 **/
  waitForExplicitFinish();
  testBug600545();
}

function testBug600545() {
  // Set the pref to false to cause non-app tabs to be stripped out on a save
  Services.prefs.setBoolPref("browser.sessionstore.resume_from_crash", false);

  // Need to wait for SessionStore's saveState function to be called
  // so that non-pinned tabs will be stripped from non-active window
  function waitForSaveState(aSaveStateCallback) {
    let topic = "sessionstore-state-write";
    Services.obs.addObserver(function() {
      Services.obs.removeObserver(arguments.callee, topic, false);
      executeSoon(aSaveStateCallback);
    }, topic, false);
  };

  // Need to wait for all tabs to be restored before reading browser state
  function waitForBrowserState(aState, aSetStateCallback) {
    let locationChanges = 0;
    let tabsRestored = getStateTabCount(aState);

    // Used to determine when tabs have been restored
    let progressListener = {
      onLocationChange: function (aBrowser) {
        if (++locationChanges == tabsRestored) {
          // Remove the progress listener from this window, it will be removed from
          // theWin when that window is closed (in setBrowserState).
          window.gBrowser.removeTabsProgressListener(this);
          executeSoon(aSetStateCallback);
        }
      }
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
    ss.setBrowserState(JSON.stringify(aState));
  }

  // This tests the following use case:
  // When multiple windows are open and browser.sessionstore.resume_from_crash
  // preference is false, tab session data for non-active window is stripped for
  // non-pinned tabs.  This occurs after "sessionstore-state-write" fires which
  // will only fire in this case if there is at least one pinned tab.
  let state = { windows: [
    {
      tabs: [
        { entries: [{ url: "http://example.org#0" }], pinned:true },
        { entries: [{ url: "http://example.com#1" }] },
        { entries: [{ url: "http://example.com#2" }] },
      ],
      selected: 2
    },
    {
      tabs: [
        { entries: [{ url: "http://example.com#3" }] },
        { entries: [{ url: "http://example.com#4" }] },
        { entries: [{ url: "http://example.com#5" }] },
        { entries: [{ url: "http://example.com#6" }] }
      ],
      selected: 3
    }
  ] };

  waitForBrowserState(state, function() {
    waitForSaveState(function () {
      let expectedNumberOfTabs = getStateTabCount(state);
      let retrievedState = JSON.parse(ss.getBrowserState());
      let actualNumberOfTabs = getStateTabCount(retrievedState);

      is(actualNumberOfTabs, expectedNumberOfTabs,
        "Number of tabs in retreived session data, matches number of tabs set.");

      done();
    });
  });
}

function done() {
  // Reset the pref
  try {
    Services.prefs.clearUserPref("browser.sessionstore.resume_from_crash");
  } catch (e) {}

  ss.setBrowserState(stateBackup);
  executeSoon(finish);
}

// Count up the number of tabs in the state data
function getStateTabCount(aState) {
  let tabCount = 0;
  for (let i in aState.windows)
    tabCount += aState.windows[i].tabs.length;
  return tabCount;
}