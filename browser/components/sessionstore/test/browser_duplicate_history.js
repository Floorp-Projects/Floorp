XPCOMUtils.defineLazyModuleGetter(this, "SessionStore",
  "resource:///modules/sessionstore/SessionStore.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "TabStateCache",
  "resource:///modules/sessionstore/TabStateCache.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "TabStateFlusher",
  "resource:///modules/sessionstore/TabStateFlusher.jsm");

add_task(async function() {
  await BrowserTestUtils.withNewTab("http://example.com", async function(aBrowser) {
    let tab = gBrowser.getTabForBrowser(aBrowser);
    await TabStateFlusher.flush(aBrowser);
    let before = TabStateCache.get(aBrowser);

    let newTab = SessionStore.duplicateTab(window, tab);
    await BrowserTestUtils.browserLoaded(newTab.linkedBrowser);
    let after = TabStateCache.get(newTab.linkedBrowser);

    isnot(before.history.entries, after.history.entries,
          "The entry objects should not be shared");
    await BrowserTestUtils.removeTab(newTab);
  });
});
