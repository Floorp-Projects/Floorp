/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the view results are cleared and the view is closed, when an empty
// result set arrives after a non-empty one.

add_task(async function() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus: SimpleTest.waitForFocus,
    value: "foo",
  });
  Assert.ok(
    UrlbarTestUtils.getResultCount(window) > 0,
    `There should be some results in the view.`
  );
  Assert.ok(gURLBar.view.isOpen, `The view should be open.`);

  // Register an high priority empty result provider.
  let provider = new UrlbarTestUtils.TestProvider({
    results: [],
    priority: 999,
  });
  UrlbarProvidersManager.registerProvider(provider);
  registerCleanupFunction(async function() {
    UrlbarProvidersManager.unregisterProvider(provider);
    await PlacesUtils.history.clear();
  });

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus: SimpleTest.waitForFocus,
    value: "foo",
  });
  Assert.ok(
    UrlbarTestUtils.getResultCount(window) == 0,
    `There should be no results in the view.`
  );
  Assert.ok(!gURLBar.view.isOpen, `The view should have been closed.`);
});
