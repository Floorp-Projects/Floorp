/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URL = "https://example.com/autocomplete";

add_setup(async function () {
  await PlacesTestUtils.addVisits(TEST_URL);
  registerCleanupFunction(async function () {
    await PlacesUtils.history.clear();
  });
});

add_task(async function test_click_row_border() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "example.com/autocomplete",
  });
  let resultRow = await UrlbarTestUtils.waitForAutocompleteResultAt(window, 0);
  let loaded = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    TEST_URL
  );
  info("Clicking on the result's top pixel row");
  EventUtils.synthesizeMouse(
    resultRow,
    parseInt(getComputedStyle(resultRow).borderTopLeftRadius) * 2,
    1,
    {}
  );
  info("Waiting for page to load");
  await loaded;
  ok(true, "Page loaded");
});
