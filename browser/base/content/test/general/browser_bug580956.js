function numClosedTabs()
  SessionStore.getClosedTabCount(window);

function isUndoCloseEnabled() {
  updateTabContextMenu();
  return !document.getElementById("context_undoCloseTab").disabled;
}

function test() {
  waitForExplicitFinish();

  gPrefService.setIntPref("browser.sessionstore.max_tabs_undo", 0);
  gPrefService.clearUserPref("browser.sessionstore.max_tabs_undo");
  is(numClosedTabs(), 0, "There should be 0 closed tabs.");
  ok(!isUndoCloseEnabled(), "Undo Close Tab should be disabled.");

  var tab = gBrowser.addTab("http://mochi.test:8888/");
  var browser = gBrowser.getBrowserForTab(tab);
  browser.addEventListener("load", function() {
    browser.removeEventListener("load", arguments.callee, true);

    BrowserTestUtils.removeTab(tab).then(() => {
      ok(isUndoCloseEnabled(), "Undo Close Tab should be enabled.");
      finish();
    });
  }, true);
}
