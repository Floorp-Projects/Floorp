/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// This test makes sure that deleting all text in the input doesn't mess up
// subsequent searches.

"use strict";

add_task(async function test() {
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesUtils.history.clear();
  await PlacesTestUtils.addVisits([
    "http://example.com/",
    "http://mozilla.org/",
  ]);

  // Do an initial search for "x".
  await promiseAutocompleteResultPopup("x", window, true);
  await checkResults();

  // Backspace.  The popup should close.
  await UrlbarTestUtils.promisePopupClose(window,
    () => EventUtils.synthesizeKey("KEY_Backspace"));

  // Type "x".  A new search should start.  Don't use
  // promiseAutocompleteResultPopup, which has some logic that starts the search
  // manually in certain conditions.  We want to specifically check that the
  // input event causes UrlbarInput to start a new search on its own.  If it
  // doesn't, then the test will hang here on promiseSearchComplete.
  EventUtils.synthesizeKey("x");
  await UrlbarTestUtils.promiseSearchComplete(window);
  await checkResults();

  // Now repeat the backspace + x two more times.  Same thing should happen.
  for (let i = 0; i < 2; i++) {
    await UrlbarTestUtils.promisePopupClose(window,
      () => EventUtils.synthesizeKey("KEY_Backspace"));
    EventUtils.synthesizeKey("x");
    await UrlbarTestUtils.promiseSearchComplete(window);
    await checkResults();
  }

  // Finally, backspace to close the popup.
  await UrlbarTestUtils.promisePopupClose(window,
    () => EventUtils.synthesizeKey("KEY_Backspace"));
});

async function checkResults() {
  Assert.equal(await UrlbarTestUtils.getResultCount(window), 2);
  let details = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.equal(details.type, UrlbarUtils.RESULT_TYPE.SEARCH);
  Assert.equal(details.searchParams.query, "x");
  details = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(details.type, UrlbarUtils.RESULT_TYPE.URL);
  Assert.equal(details.url, "http://example.com/");
}
