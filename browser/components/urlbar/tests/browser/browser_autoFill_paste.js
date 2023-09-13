/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// This test checks we don't autofill on paste.

"use strict";

add_task(async function test() {
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesUtils.history.clear();
  await PlacesTestUtils.addVisits(["http://example.com/"]);
  await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();
  registerCleanupFunction(async () => {
    await PlacesUtils.history.clear();
  });

  // Search for "e".  It should autofill to example.com/.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "e",
    fireInputEvent: true,
  });
  let details = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.ok(details.autofill);
  Assert.equal(gURLBar.value, "example.com/");
  Assert.equal(gURLBar.selectionStart, "e".length);
  Assert.equal(gURLBar.selectionEnd, "example.com/".length);

  // Now paste.
  await selectAndPaste("ex");

  // Nothing should have been autofilled.
  await UrlbarTestUtils.promiseSearchComplete(window);
  details = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.ok(!details.autofill);
  Assert.equal(gURLBar.value, "ex");
  Assert.equal(gURLBar.selectionStart, "ex".length);
  Assert.equal(gURLBar.selectionEnd, "ex".length);
});
