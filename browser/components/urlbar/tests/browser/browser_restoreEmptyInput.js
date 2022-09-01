/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// When the input is empty and the view is opened, keying down through the
// results and then out of the results should restore the empty input.

"use strict";

add_task(async function test() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.suggest.quickactions", false]],
  });

  for (let i = 0; i < 5; i++) {
    await PlacesTestUtils.addVisits("http://example.com/");
  }
  // Update Top Sites to make sure the last Top Site is a URL. Otherwise, it
  // would be a search shortcut and thus would not fill the Urlbar when
  // selected.
  await updateTopSites(sites => {
    return (
      sites &&
      sites[sites.length - 1] &&
      sites[sites.length - 1].url == "http://example.com/"
    );
  });

  registerCleanupFunction(async function() {
    await PlacesUtils.history.clear();
  });

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "",
    fireInputEvent: true,
  });

  Assert.equal(
    UrlbarTestUtils.getSelectedRowIndex(window),
    -1,
    "Nothing selected"
  );

  let resultCount = UrlbarTestUtils.getResultCount(window);
  Assert.greater(resultCount, 0, "At least one result");

  for (let i = 0; i < resultCount; i++) {
    EventUtils.synthesizeKey("KEY_ArrowDown");
  }
  Assert.equal(
    UrlbarTestUtils.getSelectedRowIndex(window),
    resultCount - 1,
    "Last result selected"
  );
  Assert.notEqual(gURLBar.value, "", "Input should not be empty");

  EventUtils.synthesizeKey("KEY_ArrowDown");
  Assert.equal(
    UrlbarTestUtils.getSelectedRowIndex(window),
    -1,
    "Nothing selected"
  );
  Assert.equal(gURLBar.value, "", "Input should be empty");
});
