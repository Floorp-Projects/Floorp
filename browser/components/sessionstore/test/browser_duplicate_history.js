XPCOMUtils.defineLazyModuleGetter(this, "SessionStore",
  "resource:///modules/sessionstore/SessionStore.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "TabStateCache",
  "resource:///modules/sessionstore/TabStateCache.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "TabStateFlusher",
  "resource:///modules/sessionstore/TabStateFlusher.jsm");

add_task(function* () {
  yield BrowserTestUtils.withNewTab("http://example.com", function* (aBrowser) {
    let tab = gBrowser.getTabForBrowser(aBrowser);
    yield TabStateFlusher.flush(aBrowser);
    let before = TabStateCache.get(aBrowser);

    let newTab = SessionStore.duplicateTab(window, tab);
    yield BrowserTestUtils.browserLoaded(newTab.linkedBrowser);
    let after = TabStateCache.get(newTab.linkedBrowser);

    isnot(before.history.entries, after.history.entries,
          "The entry objects should not be shared");
    yield BrowserTestUtils.removeTab(newTab);
  });
});
