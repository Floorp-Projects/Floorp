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

const TAB_STATE_NEEDS_RESTORE = 1;
const TAB_STATE_RESTORING = 2;

let stateBackup = ss.getBrowserState();

function cleanup() {
  // Reset the pref
  try {
    Services.prefs.clearUserPref("browser.sessionstore.restore_on_demand");
  } catch (e) {}
  ss.setBrowserState(stateBackup);
  executeSoon(finish);
}

function test() {
  /** Bug 607016 - If a tab is never restored, attributes (eg. hidden) aren't updated correctly **/
  waitForExplicitFinish();

  // Set the pref to true so we know exactly how many tabs should be restoring at
  // any given time. This guarantees that a finishing load won't start another.
  Services.prefs.setBoolPref("browser.sessionstore.restore_on_demand", true);

  // We have our own progress listener for this test, which we'll attach before our state is set
  let progressListener = {
    onStateChange: function (aBrowser, aWebProgress, aRequest, aStateFlags, aStatus) {
      if (aBrowser.__SS_restoreState == TAB_STATE_RESTORING &&
          aStateFlags & Ci.nsIWebProgressListener.STATE_STOP &&
          aStateFlags & Ci.nsIWebProgressListener.STATE_IS_NETWORK &&
          aStateFlags & Ci.nsIWebProgressListener.STATE_IS_WINDOW)
        progressCallback(aBrowser);
    }
  }

  let state = { windows: [{ tabs: [
    { entries: [{ url: "http://example.org#1" }], extData: { "uniq": r() } },
    { entries: [{ url: "http://example.org#2" }], extData: { "uniq": r() } }, // overwriting
    { entries: [{ url: "http://example.org#3" }], extData: { "uniq": r() } }, // hiding
    { entries: [{ url: "http://example.org#4" }], extData: { "uniq": r() } }, // adding
    { entries: [{ url: "http://example.org#5" }], extData: { "uniq": r() } }, // deleting
    { entries: [{ url: "http://example.org#6" }] } // creating
  ], selected: 1 }] };

  function progressCallback(aBrowser) {
    // We'll remove the progress listener after the first one because we aren't
    // loading any other tabs
    window.gBrowser.removeTabsProgressListener(progressListener);

    let curState = JSON.parse(ss.getBrowserState());
    for (let i = 0; i < curState.windows[0].tabs.length; i++) {
      if (state.windows[0].tabs[i].extData) {
        is(curState.windows[0].tabs[i].extData["uniq"],
           state.windows[0].tabs[i].extData["uniq"],
           "sanity check that tab has correct extData");
      }
      else
        ok(!("extData" in curState.windows[0].tabs[i]),
           "sanity check that tab doesn't have extData");
    }

    // Now we'll set a new unique value on 1 of the tabs
    let newUniq = r();
    ss.setTabValue(gBrowser.tabs[1], "uniq", newUniq);
    gBrowser.removeTab(gBrowser.tabs[1]);
    let closedTabData = (JSON.parse(ss.getClosedTabData(window)))[0];
    is(closedTabData.state.extData.uniq, newUniq,
       "(overwriting) new data is stored in extData");

    // hide the next tab before closing it
    gBrowser.hideTab(gBrowser.tabs[1]);
    gBrowser.removeTab(gBrowser.tabs[1]);
    closedTabData = (JSON.parse(ss.getClosedTabData(window)))[0];
    ok(closedTabData.state.hidden, "(hiding) tab data has hidden == true");

    // set data that's not in a conflicting key
    let stillUniq = r();
    ss.setTabValue(gBrowser.tabs[1], "stillUniq", stillUniq);
    gBrowser.removeTab(gBrowser.tabs[1]);
    closedTabData = (JSON.parse(ss.getClosedTabData(window)))[0];
    is(closedTabData.state.extData.stillUniq, stillUniq,
       "(adding) new data is stored in extData");

    // remove the uniq value and make sure it's not there in the closed data
    ss.deleteTabValue(gBrowser.tabs[1], "uniq");
    gBrowser.removeTab(gBrowser.tabs[1]);
    closedTabData = (JSON.parse(ss.getClosedTabData(window)))[0];
    // Since Panorama might have put data in, first check if there is extData.
    // If there is explicitly check that "uniq" isn't in it. Otherwise, we're ok
    if ("extData" in closedTabData.state) {
      ok(!("uniq" in closedTabData.state.extData),
         "(deleting) uniq not in existing extData");
    }
    else {
      ok(true, "(deleting) no data is stored in extData");
    }

    // set unique data on the tab that never had any set, make sure that's saved
    let newUniq2 = r();
    ss.setTabValue(gBrowser.tabs[1], "uniq", newUniq2);
    gBrowser.removeTab(gBrowser.tabs[1]);
    closedTabData = (JSON.parse(ss.getClosedTabData(window)))[0];
    is(closedTabData.state.extData.uniq, newUniq2,
       "(creating) new data is stored in extData where there was none");

    cleanup();
  }

  window.gBrowser.addTabsProgressListener(progressListener);
  ss.setBrowserState(JSON.stringify(state));
}

