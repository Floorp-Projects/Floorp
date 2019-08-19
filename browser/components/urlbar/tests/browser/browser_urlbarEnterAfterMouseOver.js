/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that enter works correctly after a mouse over.
 */

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

function assertSelected(index) {
  Assert.equal(
    UrlbarTestUtils.getSelectedIndex(window),
    index,
    "Should have the correct index selected"
  );
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

  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    gMaxResults,
    "Should have got the correct amount of results"
  );

  let initiallySelected = UrlbarTestUtils.getSelectedIndex(window);

  info("Key Down to select the next item");
  EventUtils.synthesizeKey("KEY_ArrowDown");
  assertSelected(initiallySelected + 1);

  let result = await UrlbarTestUtils.getDetailsOfResultAt(
    window,
    initiallySelected + 1
  );
  let expectedURL = result.url;

  Assert.equal(
    gURLBar.untrimmedValue,
    expectedURL,
    "Value in the URL bar should be updated by keyboard selection"
  );

  // Verify that what we're about to do changes the selectedIndex:
  Assert.notEqual(
    initiallySelected + 1,
    3,
    "Shouldn't be changing the selectedIndex to the same index we keyboard-selected."
  );

  let element = await UrlbarTestUtils.waitForAutocompleteResultAt(window, 3);
  EventUtils.synthesizeMouseAtCenter(element, { type: "mousemove" });

  await UrlbarTestUtils.promisePopupClose(window, async () => {
    let openedExpectedPage = BrowserTestUtils.waitForDocLoadAndStopIt(
      expectedURL,
      gBrowser.selectedBrowser
    );
    EventUtils.synthesizeKey("KEY_Enter");
    await openedExpectedPage;
  });

  gBrowser.removeCurrentTab();
});
