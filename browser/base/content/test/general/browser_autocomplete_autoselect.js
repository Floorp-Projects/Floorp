function repeat(limit, func) {
  for (let i = 0; i < limit; i++) {
    func(i);
  }
}

function* promiseAutoComplete(inputText) {
  gURLBar.focus();
  gURLBar.value = inputText.slice(0, -1);
  EventUtils.synthesizeKey(inputText.slice(-1), {});
  yield promiseSearchComplete();
}

function is_selected(index) {
  is(gURLBar.popup.richlistbox.selectedIndex, index, `Item ${index + 1} should be selected`);
}

add_task(function*() {
  registerCleanupFunction(promiseClearHistory);

  let visits = [];
  repeat(10, i => {
    visits.push({
      uri: makeURI("http://example.com/autocomplete/?" + i),
    });
  });
  yield PlacesTestUtils.addVisits(visits);

  yield promiseAutoComplete("example.com/autocomplete");

  let popup = gURLBar.popup;
  let results = popup.richlistbox.children;
  // 1 extra for the current search engine match
  is(results.length, 11, "Should get 11 results");
  is_selected(0);

  info("Key Down to select the next item");
  EventUtils.synthesizeKey("VK_DOWN", {});
  is_selected(1);

  info("Key Down 11 times should wrap around all the way around");
  repeat(11, () => EventUtils.synthesizeKey("VK_DOWN", {}));
  is_selected(1);

  info("Key Up 11 times should wrap around the other way");
  repeat(11, () => EventUtils.synthesizeKey("VK_UP", {}));
  is_selected(1);

  info("Page Up will go up the list, but not wrap");
  EventUtils.synthesizeKey("VK_PAGE_UP", {})
  is_selected(0);

  info("Page Up again will wrap around to the end of the list");
  EventUtils.synthesizeKey("VK_PAGE_UP", {})
  is_selected(10);

  EventUtils.synthesizeKey("VK_ESCAPE", {});
  yield promisePopupHidden(gURLBar.popup);
});
