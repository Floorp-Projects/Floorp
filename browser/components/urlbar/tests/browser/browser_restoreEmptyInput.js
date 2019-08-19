/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// When the input is empty and the view is opened, keying down through the
// results and then out of the results should restore the empty input.

"use strict";

add_task(async function test() {
  await PlacesTestUtils.addVisits("http://example.com/");
  registerCleanupFunction(async function() {
    await PlacesUtils.history.clear();
  });

  await promiseAutocompleteResultPopup("", window, true);

  Assert.equal(
    UrlbarTestUtils.getSelectedIndex(window),
    -1,
    "Nothing selected"
  );

  let resultCount = UrlbarTestUtils.getResultCount(window);
  Assert.ok(resultCount > 0, "At least one result");

  for (let i = 0; i < resultCount; i++) {
    EventUtils.synthesizeKey("KEY_ArrowDown");
  }
  Assert.equal(
    UrlbarTestUtils.getSelectedIndex(window),
    resultCount - 1,
    "Last result selected"
  );
  Assert.notEqual(gURLBar.value, "", "Input should not be empty");

  EventUtils.synthesizeKey("KEY_ArrowDown");
  Assert.equal(
    UrlbarTestUtils.getSelectedIndex(window),
    -1,
    "Nothing selected"
  );
  Assert.equal(gURLBar.value, "", "Input should be empty");
});
