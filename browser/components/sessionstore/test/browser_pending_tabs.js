"use strict";

const TAB_STATE = {
  entries: [{ url: "about:mozilla" }, { url: "about:robots" }],
  index: 1,
};

add_task(function* () {
  // Create a background tab.
  let tab = gBrowser.addTab("about:blank");
  let browser = tab.linkedBrowser;
  yield promiseBrowserLoaded(browser);

  // The tab shouldn't be restored right away.
  Services.prefs.setBoolPref("browser.sessionstore.restore_on_demand", true);

  // Prepare the tab state.
  let promise = promiseTabRestoring(tab);
  ss.setTabState(tab, JSON.stringify(TAB_STATE));
  ok(tab.hasAttribute("pending"), "tab is pending");
  yield promise;

  // Flush to ensure the parent has all data.
  yield TabStateFlusher.flush(browser);

  // Check that the shistory index is the one we restored.
  let tabState = TabState.collect(tab);
  is(tabState.index, TAB_STATE.index, "correct shistory index");

  // Check we don't collect userTypedValue when we shouldn't.
  ok(!tabState.userTypedValue, "tab didn't have a userTypedValue");

  // Cleanup.
  gBrowser.removeTab(tab);
});
