add_task(function*() {
  // This test is only relevant if UnifiedComplete is enabled.
  Services.prefs.setBoolPref("browser.urlbar.unifiedcomplete", true);

  registerCleanupFunction(() => {
    PlacesUtils.bookmarks.removeFolderChildren(PlacesUtils.unfiledBookmarksFolderId);
    Services.prefs.clearUserPref("browser.urlbar.unifiedcomplete");
  });

  let itemId =
    PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                         NetUtil.newURI("http://example.com/?q=%s"),
                                         PlacesUtils.bookmarks.DEFAULT_INDEX,
                                         "test");
  PlacesUtils.bookmarks.setKeywordForBookmark(itemId, "keyword");

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
