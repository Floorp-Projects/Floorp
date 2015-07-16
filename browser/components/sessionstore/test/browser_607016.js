"use strict";

let stateBackup = ss.getBrowserState();

add_task(function* () {
  /** Bug 607016 - If a tab is never restored, attributes (eg. hidden) aren't updated correctly **/
  ignoreAllUncaughtExceptions();

  // Set the pref to true so we know exactly how many tabs should be restoring at
  // any given time. This guarantees that a finishing load won't start another.
  Services.prefs.setBoolPref("browser.sessionstore.restore_on_demand", true);

  let state = { windows: [{ tabs: [
    { entries: [{ url: "http://example.org#1" }], extData: { "uniq": r() } },
    { entries: [{ url: "http://example.org#2" }], extData: { "uniq": r() } }, // overwriting
    { entries: [{ url: "http://example.org#3" }], extData: { "uniq": r() } }, // hiding
    { entries: [{ url: "http://example.org#4" }], extData: { "uniq": r() } }, // adding
    { entries: [{ url: "http://example.org#5" }], extData: { "uniq": r() } }, // deleting
    { entries: [{ url: "http://example.org#6" }] } // creating
  ], selected: 1 }] };

  function* progressCallback() {
    let curState = JSON.parse(ss.getBrowserState());
    for (let i = 0; i < curState.windows[0].tabs.length; i++) {
      let tabState = state.windows[0].tabs[i];
      let tabCurState = curState.windows[0].tabs[i];
      if (tabState.extData) {
        is(tabCurState.extData["uniq"], tabState.extData["uniq"],
           "sanity check that tab has correct extData");
      }
      else {
        // We aren't expecting there to be any data on extData, but panorama
        // may be setting something, so we need to make sure that if we do have
        // data, we just don't have anything for "uniq".
        ok(!("extData" in tabCurState) || !("uniq" in tabCurState.extData),
           "sanity check that tab doesn't have extData or extData doesn't have 'uniq'");
      }
    }

    // Now we'll set a new unique value on 1 of the tabs
    let newUniq = r();
    ss.setTabValue(gBrowser.tabs[1], "uniq", newUniq);
    yield promiseRemoveTab(gBrowser.tabs[1]);
    let closedTabData = (JSON.parse(ss.getClosedTabData(window)))[0];
    is(closedTabData.state.extData.uniq, newUniq,
       "(overwriting) new data is stored in extData");

    // hide the next tab before closing it
    gBrowser.hideTab(gBrowser.tabs[1]);
    yield promiseRemoveTab(gBrowser.tabs[1]);
    closedTabData = (JSON.parse(ss.getClosedTabData(window)))[0];
    ok(closedTabData.state.hidden, "(hiding) tab data has hidden == true");

    // set data that's not in a conflicting key
    let stillUniq = r();
    ss.setTabValue(gBrowser.tabs[1], "stillUniq", stillUniq);
    yield promiseRemoveTab(gBrowser.tabs[1]);
    closedTabData = (JSON.parse(ss.getClosedTabData(window)))[0];
    is(closedTabData.state.extData.stillUniq, stillUniq,
       "(adding) new data is stored in extData");

    // remove the uniq value and make sure it's not there in the closed data
    ss.deleteTabValue(gBrowser.tabs[1], "uniq");
    yield promiseRemoveTab(gBrowser.tabs[1]);
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
    yield promiseRemoveTab(gBrowser.tabs[1]);
    closedTabData = (JSON.parse(ss.getClosedTabData(window)))[0];
    is(closedTabData.state.extData.uniq, newUniq2,
       "(creating) new data is stored in extData where there was none");
  }

  // Set the test state.
  ss.setBrowserState(JSON.stringify(state));

  // Wait until the selected tab is restored and all others are pending.
  yield Promise.all(Array.map(gBrowser.tabs, tab => {
    return (tab == gBrowser.selectedTab) ?
      promiseTabRestored(tab) : promiseTabRestoring(tab)
  }));

  // Kick off the actual tests.
  yield progressCallback();

  // Cleanup.
  yield promiseBrowserState(stateBackup);
});
