// The order of these tests matters!

add_task(function* setup () {
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser);
  let bm = yield PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                url: "http://example.com/?q=%s",
                                                title: "test" });
  registerCleanupFunction(function* () {
    yield PlacesUtils.bookmarks.remove(bm);
    yield BrowserTestUtils.removeTab(tab);
  });
  yield PlacesUtils.keywords.insert({ keyword: "keyword",
                                      url: "http://example.com/?q=%s" });
  // Needs at least one success.
  ok(true, "Setup complete");
});

add_task(function* test_keyword() {
  yield promiseAutocompleteResultPopup("keyword bear");
  gURLBar.focus();
  EventUtils.synthesizeKey("d", {});
  EventUtils.synthesizeKey("VK_RETURN", {});
  info("wait for the page to load");
  yield BrowserTestUtils.browserLoaded(gBrowser.selectedTab.linkedBrowser,
                                      false, "http://example.com/?q=beard");
});

add_task(function* test_sametext() {
  yield promiseAutocompleteResultPopup("example.com", window, true);

  // Simulate re-entering the same text searched the last time. This may happen
  // through a copy paste, but clipboard handling is not much reliable, so just
  // fire an input event.
  info("synthesize input event");
  let event = document.createEvent("Events");
  event.initEvent("input", true, true);
  gURLBar.dispatchEvent(event);
  EventUtils.synthesizeKey("VK_RETURN", {});

  info("wait for the page to load");
  yield BrowserTestUtils.browserLoaded(gBrowser.selectedTab.linkedBrowser,
                                       false, "http://example.com/");
});
