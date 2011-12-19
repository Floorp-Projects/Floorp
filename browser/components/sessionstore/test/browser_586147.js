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
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Edward Lee <edilee@mozilla.com>
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

function observeOneRestore(callback) {
  let topic = "sessionstore-browser-state-restored";
  Services.obs.addObserver(function() {
    Services.obs.removeObserver(arguments.callee, topic, false);
    callback();
  }, topic, false);
};

function test() {
  waitForExplicitFinish();
  ignoreAllUncaughtExceptions();

  // There should be one tab when we start the test
  let [origTab] = gBrowser.visibleTabs;
  let hiddenTab = gBrowser.addTab();

  is(gBrowser.visibleTabs.length, 2, "should have 2 tabs before hiding");
  gBrowser.showOnlyTheseTabs([origTab]);
  is(gBrowser.visibleTabs.length, 1, "only 1 after hiding");
  ok(hiddenTab.hidden, "sanity check that it's hidden");

  let extraTab = gBrowser.addTab();
  let state = ss.getBrowserState();
  let stateObj = JSON.parse(state);
  let tabs = stateObj.windows[0].tabs;
  is(tabs.length, 3, "just checking that browser state is correct");
  ok(!tabs[0].hidden, "first tab is visible");
  ok(tabs[1].hidden, "second is hidden");
  ok(!tabs[2].hidden, "third is visible");

  // Make the third tab hidden and then restore the modified state object
  tabs[2].hidden = true;

  observeOneRestore(function() {
    let testWindow = Services.wm.getEnumerator("navigator:browser").getNext();
    is(testWindow.gBrowser.visibleTabs.length, 1, "only restored 1 visible tab");
    let tabs = testWindow.gBrowser.tabs;
    ok(!tabs[0].hidden, "first is still visible");
    ok(tabs[1].hidden, "second tab is still hidden");
    ok(tabs[2].hidden, "third tab is now hidden");

    // Restore the original state and clean up now that we're done
    gBrowser.removeTab(hiddenTab);
    gBrowser.removeTab(extraTab);
    finish();
  });
  ss.setBrowserState(JSON.stringify(stateObj));
}
