add_task(async function() {
  await PlacesUtils.history.clear();

  await PlacesTestUtils.addVisits([
    { uri: makeURI("http://example.com/foo") },
    { uri: makeURI("http://example.com/foo/bar") },
  ]);

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank");
  registerCleanupFunction(async function() {
    BrowserTestUtils.removeTab(tab);
    await PlacesUtils.history.clear();
  });

  await promiseAutocompleteResultPopup("http://example.com");
  await waitForAutocompleteResultAt(1);

  let popup = gURLBar.popup;
  let list = popup.richlistbox;
  let initialIndex = list.selectedIndex;

  info("Key Down to select the next item.");
  EventUtils.synthesizeKey("KEY_ArrowDown");

  let nextIndex = initialIndex + 1;
  let nextValue = gURLBar.controller.getFinalCompleteValueAt(nextIndex);
  is(list.selectedIndex, nextIndex, "The next item is selected.");
  is(gURLBar.value, nextValue, "The selected URL is completed.");

  info("Press backspace");
  EventUtils.synthesizeKey("KEY_Backspace");
  await promiseSearchComplete();

  let editedValue = gURLBar.textValue;
  is(list.selectedIndex, initialIndex, "The initial index is selected again.");
  isnot(editedValue, nextValue, "The URL has changed.");

  let docLoad = waitForDocLoadAndStopIt("http://" + editedValue);

  info("Press return to load edited URL.");
  EventUtils.synthesizeKey("KEY_Enter");
  await Promise.all([
    promisePopupHidden(gURLBar.popup),
    docLoad,
  ]);

});
