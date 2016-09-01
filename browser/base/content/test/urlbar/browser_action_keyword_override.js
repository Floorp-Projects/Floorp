add_task(function*() {
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
  let titleHbox = result._titleText.parentNode.parentNode;
  ok(titleHbox.classList.contains("ac-title"), "Title hbox element sanity check");
  is_element_visible(titleHbox, "Title element should be visible");

  let urlHbox = result._urlText.parentNode.parentNode;
  ok(urlHbox.classList.contains("ac-url"), "URL hbox element sanity check");
  is_element_hidden(urlHbox, "URL element should be hidden");

  let actionHbox = result._actionText.parentNode.parentNode;
  ok(actionHbox.classList.contains("ac-action"), "Action hbox element sanity check");
  is_element_visible(actionHbox, "Action element should be visible");
  is(result._actionText.textContent, "", "Action text should be empty");

  info("During override");
  EventUtils.synthesizeKey("VK_SHIFT", { type: "keydown" });
  is_element_visible(titleHbox, "Title element should be visible");
  is_element_hidden(urlHbox, "URL element should be hidden");
  is_element_visible(actionHbox, "Action element should be visible");
  is(result._actionText.textContent, "", "Action text should be empty");

  EventUtils.synthesizeKey("VK_SHIFT", { type: "keyup" });

  gURLBar.popup.hidePopup();
  yield promisePopupHidden(gURLBar.popup);
});
