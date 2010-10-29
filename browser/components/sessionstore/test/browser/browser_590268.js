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

const NUM_TABS = 12;

Cu.import("resource://gre/modules/Services.jsm");
let ss = Cc["@mozilla.org/browser/sessionstore;1"].
         getService(Ci.nsISessionStore);

let stateBackup = ss.getBrowserState();


function test() {
  /** Test for Bug 590268 - Provide access to sessionstore tab data sooner **/
  waitForExplicitFinish();

  let startedTest = false;

  // wasLoaded will be used to keep track of tabs that have already had SSTabRestoring
  // fired for them.
  let wasLoaded = { };
  let restoringTabsCount = 0;
  let uniq2 = { };
  let uniq2Count = 0;
  let state = { windows: [{ tabs: [] }] };
  // We're going to put a bunch of tabs into this state
  for (let i = 0; i < NUM_TABS; i++) {
    let uniq = r();
    let tabData = {
      entries: [{ url: "http://example.com/#" + i }],
      extData: { "uniq": uniq }
    };
    state.windows[0].tabs.push(tabData);
    wasLoaded[uniq] = false;
  }


  function onSSTabRestoring(aEvent) {
    restoringTabsCount++;
    let uniq = ss.getTabValue(aEvent.originalTarget, "uniq");
    wasLoaded[uniq] = true;

    // On the first SSTabRestoring we're going to run the the real test.
    // We'll keep this listener around so we can keep marking tabs as restored.
    if (restoringTabsCount == 1)
      onFirstSSTabRestoring();
    else if (restoringTabsCount == NUM_TABS)
      onLastSSTabRestoring();
  }

  // This does the actual testing. SSTabRestoring should be firing on tabs from
  // left to right, so we're going to start with the rightmost tab.
  function onFirstSSTabRestoring() {
    info("onFirstSSTabRestoring...");
    for (let i = gBrowser.tabs.length - 1; i >= 0; i--) {
      let tab = gBrowser.tabs[i];
      let actualUniq = ss.getTabValue(tab, "uniq");
      let expectedUniq = state.windows[0].tabs[i].extData["uniq"];

      if (wasLoaded[actualUniq]) {
        info("tab " + i + ": already restored");
        continue;
      }
      is(actualUniq, expectedUniq, "tab " + i + ": extData was correct");

      // Now we're going to set a piece of data back on the tab so it can be read
      // to test setting a value "early".
      uniq2[actualUniq] = r();
      ss.setTabValue(tab, "uniq2", uniq2[actualUniq]);
      // This will be used in the final comparison to make sure we checked the
      // same number as we set.
      uniq2Count++;
    }
  }

  function onLastSSTabRestoring() {
    let checked = 0;
    for (let i = 0; i < gBrowser.tabs.length; i++) {
      let tab = gBrowser.tabs[i];
      let uniq = ss.getTabValue(tab, "uniq");

      // Look to see if we set a uniq2 value for this uniq value
      if (uniq in uniq2) {
        is(ss.getTabValue(tab, "uniq2"), uniq2[uniq], "tab " + i + " has correct uniq2 value");
        checked++;
      }
    }
    is(checked, uniq2Count, "checked the same number of uniq2 as we set");
    cleanup();
  }

  function cleanup() {
    // remove the event listener and clean up before finishing
    gBrowser.tabContainer.removeEventListener("SSTabRestoring", onSSTabRestoring, false);
    // Put this in an executeSoon because we still haven't called restoreNextTab
    // in sessionstore for the last tab (we'll call it after this). We end up
    // trying to restore the tab (since we then add a closed tab to the array).
    executeSoon(function() {
      ss.setBrowserState(stateBackup);
      executeSoon(finish);
    });
  }

  // Add the event listener
  gBrowser.tabContainer.addEventListener("SSTabRestoring", onSSTabRestoring, false);
  // Restore state
  ss.setBrowserState(JSON.stringify(state));
}

// Helper function to create a random value
function r() {
  return "" + Date.now() + Math.random();
}

