add_task(function*() {
  // This test is only relevant if UnifiedComplete is enabled.
  let ucpref = Services.prefs.getBoolPref("browser.urlbar.unifiedcomplete");
  Services.prefs.setBoolPref("browser.urlbar.unifiedcomplete", true);
  registerCleanupFunction(() => {
    Services.prefs.setBoolPref("browser.urlbar.unifiedcomplete", ucpref);
  });

  let bm = yield PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                url: "http://example.com/?q=%s",
                                                title: "test" });
  yield PlacesUtils.keywords.insert({ keyword: "keyword",
                                      url: "http://example.com/?q=%s" })

  registerCleanupFunction(function* () {
    yield PlacesUtils.bookmarks.remove(bm);
  });

  yield promiseAutocompleteResultPopup("keyword search");
  let result = gURLBar.popup.richlistbox.children[0];

  info("Before override");
  is_element_hidden(result._url, "URL element should be hidden");
  is_element_visible(result._extraBox, "Extra element should be visible");

  info("During override");
  EventUtils.synthesizeKey("VK_SHIFT" , { type: "keydown" });
  is_element_hidden(result._url, "URL element should be hidden");
  is_element_visible(result._extraBox, "Extra element should be visible");

  EventUtils.synthesizeKey("VK_SHIFT" , { type: "keyup" });

  gURLBar.popup.hidePopup();
  yield promisePopupHidden(gURLBar.popup);
});
