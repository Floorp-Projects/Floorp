add_task(async function() {
  await PlacesTestUtils.clearHistory();

  await PlacesTestUtils.addVisits([
    { uri: makeURI("http://example.com/foo") },
    { uri: makeURI("http://example.com/foo/bar") },
  ]);

  registerCleanupFunction(async function() {
    await PlacesTestUtils.clearHistory();
  });

  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, "about:blank");
  gURLBar.focus();

  await promiseAutocompleteResultPopup("http://example.com");

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
  await promiseSearchComplete();

  let editedValue = gURLBar.textValue;
  is(list.selectedIndex, initialIndex, "The initial index is selected again.");
  isnot(editedValue, nextValue, "The URL has changed.");

  let docLoad = waitForDocLoadAndStopIt("http://" + editedValue);

  info("Press return to load edited URL.");
  EventUtils.synthesizeKey("VK_RETURN", {});
  await Promise.all([
    promisePopupHidden(gURLBar.popup),
    docLoad,
  ]);

  gBrowser.removeTab(gBrowser.selectedTab);
});
