"use strict";

const URL = "http://example.com/browser_switch_remoteness_";

function countHistoryEntries(browser, expected) {
  return ContentTask.spawn(browser, { expected }, function* (args) {
    let Ci = Components.interfaces;
    let webNavigation = docShell.QueryInterface(Ci.nsIWebNavigation);
    let history = webNavigation.sessionHistory.QueryInterface(Ci.nsISHistoryInternal);
    Assert.equal(history && history.count, args.expected,
      "correct number of shistory entries");
  });
}

add_task(function* () {
  // Add a new tab.
  let tab = gBrowser.addTab("about:blank");
  let browser = tab.linkedBrowser;
  yield promiseBrowserLoaded(browser);
  ok(browser.isRemoteBrowser, "browser is remote");

  // Get the maximum number of preceding entries to save.
  const MAX_BACK = Services.prefs.getIntPref("browser.sessionstore.max_serialize_back");
  ok(MAX_BACK > -1, "check that the default has a value that caps data");

  // Load more pages than we would save to disk on a clean shutdown.
  for (let i = 0; i < MAX_BACK + 2; i++) {
    browser.loadURI(URL + i);
    yield promiseBrowserLoaded(browser);
    ok(browser.isRemoteBrowser, "browser is still remote");
  }

  // Check we have the right number of shistory entries.
  yield countHistoryEntries(browser, MAX_BACK + 2);

  // Load a non-remote page.
  browser.loadURI("about:robots");
  yield promiseTabRestored(tab);
  ok(!browser.isRemoteBrowser, "browser is not remote anymore");

  // Check that we didn't lose any shistory entries.
  yield countHistoryEntries(browser, MAX_BACK + 3);

  // Cleanup.
  gBrowser.removeTab(tab);
});
