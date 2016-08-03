add_task(function*() {
  yield PlacesTestUtils.clearHistory();

  yield PlacesTestUtils.addVisits([
    { uri: makeURI("http://example.com/foo") },
    { uri: makeURI("http://example.com/foo/bar") },
  ]);

  registerCleanupFunction(function* () {
    yield PlacesTestUtils.clearHistory();
  });

  gBrowser.selectedTab = gBrowser.addTab("about:blank");
  gURLBar.focus();

  yield promiseAutocompleteResultPopup("http://example.com");

  let popup = gURLBar.popup;
  let list = popup.richlistbox;
  let initialIndex = list.selectedIndex;

  info("Key Down to select the next item.");
  EventUtils.synthesizeKey("VK_DOWN", {});

  let nextIndex = initialIndex + 1;
  let nextValue = gURLBar.controller.getFinalCompleteValueAt(nextIndex);
  is(list.selectedIndex, nextIndex, "The next item is selected.");
  is(gURLBar.value, nextValue, "The selected URL is completed.");

  info("Press backspace");
  EventUtils.synthesizeKey("VK_BACK_SPACE", {});
  yield promiseSearchComplete();

  let editedValue = gURLBar.textValue;
  is(list.selectedIndex, initialIndex, "The initial index is selected again.");
  isnot(editedValue, nextValue, "The URL has changed.");

  let docLoad = waitForDocLoadAndStopIt("http://" + editedValue);

  info("Press return to load edited URL.");
  EventUtils.synthesizeKey("VK_RETURN", {});
  yield Promise.all([
    promisePopupHidden(gURLBar.popup),
    docLoad,
  ]);

  gBrowser.removeTab(gBrowser.selectedTab);
});
