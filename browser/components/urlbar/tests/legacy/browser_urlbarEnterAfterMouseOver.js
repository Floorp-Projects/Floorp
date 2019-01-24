function repeat(limit, func) {
  for (let i = 0; i < limit; i++) {
    func(i);
  }
}

async function promiseAutoComplete(inputText) {
  gURLBar.focus();
  gURLBar.value = inputText.slice(0, -1);
  EventUtils.sendString(inputText.slice(-1));
  await promiseSearchComplete();
}

function is_selected(index) {
  is(gURLBar.popup.richlistbox.selectedIndex, index, `Item ${index + 1} should be selected`);
}

let gMaxResults;

add_task(async function() {
  registerCleanupFunction(async function() {
    await PlacesUtils.history.clear();
  });

  await PlacesUtils.history.clear();

  gMaxResults = Services.prefs.getIntPref("browser.urlbar.maxRichResults");

  let visits = [];
  repeat(gMaxResults, i => {
    visits.push({
      uri: makeURI("http://example.com/autocomplete/?" + i),
    });
  });
  await PlacesTestUtils.addVisits(visits);

  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, "about:blank");
  await promiseAutoComplete("http://example.com/autocomplete/");

  let popup = gURLBar.popup;
  let results = popup.richlistbox.itemChildren;
  is(results.length, gMaxResults,
     "Should get gMaxResults=" + gMaxResults + " results");

  let initiallySelected = gURLBar.popup.richlistbox.selectedIndex;

  info("Key Down to select the next item");
  EventUtils.synthesizeKey("KEY_ArrowDown");
  is_selected(initiallySelected + 1);
  let expectedURL = gURLBar.controller.getFinalCompleteValueAt(initiallySelected + 1);

  is(gURLBar.value, gURLBar.controller.getValueAt(initiallySelected + 1),
     "Value in the URL bar should be updated by keyboard selection");

  // Verify that what we're about to do changes the selectedIndex:
  isnot(initiallySelected + 1, 3, "Shouldn't be changing the selectedIndex to the same index we keyboard-selected.");

  // Would love to use a synthetic mousemove event here, but that doesn't seem to do anything.
  // EventUtils.synthesizeMouseAtCenter(results[3], {type: "mousemove"});
  gURLBar.popup.richlistbox.selectedIndex = 3;
  is_selected(3);

  let autocompletePopupHidden = promisePopupHidden(gURLBar.popup);
  let openedExpectedPage = waitForDocLoadAndStopIt(expectedURL);
  EventUtils.synthesizeKey("KEY_Enter");
  await Promise.all([autocompletePopupHidden, openedExpectedPage]);

  gBrowser.removeCurrentTab();
});
