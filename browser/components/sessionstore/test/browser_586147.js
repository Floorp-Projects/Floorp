/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
