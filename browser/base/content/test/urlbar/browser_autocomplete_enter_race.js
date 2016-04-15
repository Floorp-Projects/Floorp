add_task(function*() {
  // This test is only relevant if UnifiedComplete is enabled.
  Services.prefs.setBoolPref("browser.urlbar.unifiedcomplete", true);

  registerCleanupFunction(function* () {
    Services.prefs.clearUserPref("browser.urlbar.unifiedcomplete");
    yield PlacesUtils.bookmarks.remove(bm);
  });

  let bm = yield PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                url: "http://example.com/?q=%s",
                                                title: "test" });
  yield PlacesUtils.keywords.insert({ keyword: "keyword",
                                      url: "http://example.com/?q=%s" });

  yield new Promise(resolve => waitForFocus(resolve, window));

  yield promiseAutocompleteResultPopup("keyword bear");
  gURLBar.focus();
  EventUtils.synthesizeKey("d", {});
  EventUtils.synthesizeKey("VK_RETURN", {});

  yield promiseTabLoadEvent(gBrowser.selectedTab);
  is(gBrowser.selectedBrowser.currentURI.spec,
     "http://example.com/?q=beard",
     "Latest typed characters should have been used");
});
