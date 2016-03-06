"use strict";

/**
 * This test checks that pending tabs are treated like fully loaded tabs when
 * purging session history. Just like for fully loaded tabs we want to remove
 * every but the current shistory entry.
 */

const TAB_STATE = {
  entries: [{url: "about:mozilla"}, {url: "about:robots"}],
  index: 1,
};

function checkTabContents(browser) {
  return ContentTask.spawn(browser, null, function* () {
    let Ci = Components.interfaces;
    let webNavigation = docShell.QueryInterface(Ci.nsIWebNavigation);
    let history = webNavigation.sessionHistory.QueryInterface(Ci.nsISHistoryInternal);
    Assert.ok(history && history.count == 1 && content.document.documentURI == "about:mozilla",
      "expected tab contents found");
  });
}

add_task(function* () {
  // Create a new tab.
  let tab = gBrowser.addTab("about:blank");
  let browser = tab.linkedBrowser;
  yield promiseBrowserLoaded(browser);
  yield promiseTabState(tab, TAB_STATE);

  // Create another new tab.
  let tab2 = gBrowser.addTab("about:blank");
  let browser2 = tab2.linkedBrowser;
  yield promiseBrowserLoaded(browser2);

  // The tab shouldn't be restored right away.
  Services.prefs.setBoolPref("browser.sessionstore.restore_on_demand", true);

  // Prepare the tab state.
  let promise = promiseTabRestoring(tab2);
  ss.setTabState(tab2, JSON.stringify(TAB_STATE));
  ok(tab2.hasAttribute("pending"), "tab is pending");
  yield promise;

  // Purge session history.
  Services.obs.notifyObservers(null, "browser:purge-session-history", "");
  yield checkTabContents(browser);
  ok(tab2.hasAttribute("pending"), "tab is still pending");

  // Kick off tab restoration.
  gBrowser.selectedTab = tab2;
  yield promiseTabRestored(tab2);
  yield checkTabContents(browser2);
  ok(!tab2.hasAttribute("pending"), "tab is not pending anymore");

  // Cleanup.
  gBrowser.removeTab(tab2);
  gBrowser.removeTab(tab);
});
