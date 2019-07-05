/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This test ensures that a search for "empty" strings doesn't break the urlbar.

add_task(async function test_setup() {
  await PlacesTestUtils.addVisits([
    {
      uri: `http://one.mozilla.org/`,
      transition: PlacesUtils.history.TRANSITIONS.TYPED,
    },
    {
      uri: `http://two.mozilla.org/`,
      transition: PlacesUtils.history.TRANSITIONS.TYPED,
    },
  ]);
  registerCleanupFunction(PlacesUtils.history.clear);
});

add_task(async function test_empty() {
  info("Test searching for nothing");
  await promiseAutocompleteResultPopup("", window, true);
  // The first search collects the results, the following ones check results
  // are the same.
  let results = [{}]; // Add a fake first result, to account for heuristic.
  for (let i = 0; i < (await UrlbarTestUtils.getResultCount(window)); ++i) {
    let result = await UrlbarTestUtils.getDetailsOfResultAt(window, i);
    // Don't compare the element part of the results.
    delete result.element;
    results.push(result);
  }

  for (let str of [" ", "  "]) {
    info(`Test searching for "${str}"`);
    await promiseAutocompleteResultPopup(str, window, true);
    // Skip the heuristic result.
    Assert.ok(
      (await UrlbarTestUtils.getDetailsOfResultAt(window, 0)).heuristic,
      "The first result is heuristic"
    );
    let length = await UrlbarTestUtils.getResultCount(window);
    Assert.equal(length, results.length, "Comparing results count");
    for (let i = 1; i < length; ++i) {
      let result = await UrlbarTestUtils.getDetailsOfResultAt(window, i);
      // Don't compare the element part of the results.
      delete result.element;
      Assert.deepEqual(result, results[i], `Comparing result at index ${i}`);
    }
  }
  await UrlbarTestUtils.promisePopupClose(window);
});

add_task(async function test_backspace_empty() {
  info("Testing that deleting the input value via backspace closes the popup");
  await promiseAutocompleteResultPopup(" ", window, true);
  await UrlbarTestUtils.promisePopupClose(window, () => {
    EventUtils.synthesizeKey("KEY_Backspace");
  });
});
