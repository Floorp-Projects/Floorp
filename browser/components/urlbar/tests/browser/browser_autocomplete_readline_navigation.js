/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests navigation between results using ctrl-n/p.
 */

function repeat(limit, func) {
  for (let i = 0; i < limit; i++) {
    func(i);
  }
}

function assertSelected(index) {
  Assert.equal(
    UrlbarTestUtils.getSelectedRowIndex(window),
    index,
    "Should have the correct item selected"
  );

  // This is true because although both the listbox and the one-offs can have
  // selections, the test doesn't check that.
  Assert.equal(
    UrlbarTestUtils.getOneOffSearchButtons(window).selectedButton,
    null,
    "A result is selected, so the one-offs should not have a selection"
  );
}

add_task(async function() {
  let maxResults = Services.prefs.getIntPref("browser.urlbar.maxRichResults");
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:mozilla"
  );
  registerCleanupFunction(async function() {
    await PlacesUtils.history.clear();
    BrowserTestUtils.removeTab(tab);
  });

  let visits = [];
  repeat(maxResults, i => {
    visits.push({
      uri: makeURI("http://example.com/autocomplete/?" + i),
    });
  });
  await PlacesTestUtils.addVisits(visits);

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "example.com/autocomplete",
  });
  await UrlbarTestUtils.waitForAutocompleteResultAt(window, maxResults - 1);

  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    maxResults,
    "Should get maxResults=" + maxResults + " results"
  );
  assertSelected(0);

  info("Ctrl-n to select the next item");
  EventUtils.synthesizeKey("n", { ctrlKey: true });
  assertSelected(1);

  info("Ctrl-p to select the previous item");
  EventUtils.synthesizeKey("p", { ctrlKey: true });
  assertSelected(0);
});
