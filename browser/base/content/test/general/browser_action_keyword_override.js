add_task(function*() {
  // This test is only relevant if UnifiedComplete is enabled.
  if (!Services.prefs.getBoolPref("browser.urlbar.unifiedcomplete")) {
    todo(false, "Stop supporting old autocomplete components.");
    return;
  }

  let itemId =
    PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                         NetUtil.newURI("http://example.com/?q=%s"),
                                         PlacesUtils.bookmarks.DEFAULT_INDEX,
                                         "test");
  PlacesUtils.bookmarks.setKeywordForBookmark(itemId, "keyword");

  registerCleanupFunction(() => {
    PlacesUtils.bookmarks.removeItem(itemId);
  });

  yield promiseAutocompleteResultPopup("keyword search");
  let result = gURLBar.popup.richlistbox.children[0];

  info("Before override");
  is_element_hidden(result._url, "URL element should be hidden");
  is_element_visible(result._extra, "Extra element should be visible");

  info("During override");
  EventUtils.synthesizeKey("VK_SHIFT" , { type: "keydown" });
  is_element_hidden(result._url, "URL element should be hidden");
  is_element_visible(result._extra, "Extra element should be visible");

  EventUtils.synthesizeKey("VK_SHIFT" , { type: "keyup" });

  gURLBar.popup.hidePopup();
  yield promisePopupHidden(gURLBar.popup);
});
