function numClosedTabs() {
  return SessionStore.getClosedTabCount();
}

function isUndoCloseEnabled() {
  updateTabContextMenu();
  return !document.getElementById("context_undoCloseTab").disabled;
}

add_task(async function test() {
  Services.prefs.setIntPref("browser.sessionstore.max_tabs_undo", 0);
  Services.prefs.clearUserPref("browser.sessionstore.max_tabs_undo");
  is(numClosedTabs(), 0, "There should be 0 closed tabs.");
  ok(!isUndoCloseEnabled(), "Undo Close Tab should be disabled.");

  var tab = BrowserTestUtils.addTab(gBrowser, "http://mochi.test:8888/");
  var browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  let sessionUpdatePromise = BrowserTestUtils.waitForSessionStoreUpdate(tab);
  BrowserTestUtils.removeTab(tab);
  await sessionUpdatePromise;

  ok(isUndoCloseEnabled(), "Undo Close Tab should be enabled.");
});
