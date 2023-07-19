/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// This test makes sure that when the view updates itself it doesn't try to
// update a row with a new result from a different provider. We avoid that
// because it's a cause of results flickering.

"use strict";

add_task(async function test() {
  // This slow provider is used to delay the end of the query.
  let slowProvider = new UrlbarTestUtils.TestProvider({
    results: [],
    priority: 10,
    addTimeout: 1000,
  });

  // We'll run a first query with this provider to generate results, that should
  // be overriden by results from the second provider.
  let firstProvider = new UrlbarTestUtils.TestProvider({
    results: [
      new UrlbarResult(
        UrlbarUtils.RESULT_TYPE.URL,
        UrlbarUtils.RESULT_SOURCE.HISTORY,
        { url: "https://mozilla.org/c" }
      ),
      new UrlbarResult(
        UrlbarUtils.RESULT_TYPE.URL,
        UrlbarUtils.RESULT_SOURCE.HISTORY,
        { url: "https://mozilla.org/d" }
      ),
    ],
    priority: 10,
  });

  // Then we'll run a second query with this provider, the results should not
  // immediately replace the ones from the first provider, but rather be
  // appended, until the query is done or the stale timer elapses.
  let secondProvider = new UrlbarTestUtils.TestProvider({
    results: [
      new UrlbarResult(
        UrlbarUtils.RESULT_TYPE.URL,
        UrlbarUtils.RESULT_SOURCE.HISTORY,
        { url: "https://mozilla.org/c" }
      ),
      new UrlbarResult(
        UrlbarUtils.RESULT_TYPE.URL,
        UrlbarUtils.RESULT_SOURCE.HISTORY,
        { url: "https://mozilla.org/d" }
      ),
    ],
    priority: 10,
  });

  UrlbarProvidersManager.registerProvider(slowProvider);
  UrlbarProvidersManager.registerProvider(firstProvider);
  function cleanup() {
    UrlbarProvidersManager.unregisterProvider(slowProvider);
    UrlbarProvidersManager.unregisterProvider(firstProvider);
    UrlbarProvidersManager.unregisterProvider(secondProvider);
  }
  registerCleanupFunction(cleanup);

  // Execute the first query.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "moz",
  });

  // Now run the second query but don't wait for it to finish, we want to
  // observe the view contents along the way.
  UrlbarProvidersManager.unregisterProvider(firstProvider);
  UrlbarProvidersManager.registerProvider(secondProvider);
  let hasAtLeast4Children = BrowserTestUtils.waitForMutationCondition(
    UrlbarTestUtils.getResultsContainer(window),
    { childList: true },
    () => UrlbarTestUtils.getResultCount(window) == 4
  );
  let queryPromise = UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "moz",
  });
  await hasAtLeast4Children;
  // At this point we have the old results marked as "stale", and the new ones.
  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    4,
    "There should be 4 results"
  );
  Assert.ok(
    UrlbarTestUtils.getRowAt(window, 0).hasAttribute("stale"),
    "Should be stale"
  );
  Assert.ok(
    UrlbarTestUtils.getRowAt(window, 1).hasAttribute("stale"),
    "Should be stale"
  );
  Assert.ok(
    !UrlbarTestUtils.getRowAt(window, 2).hasAttribute("stale"),
    "Should not be stale"
  );
  Assert.ok(
    !UrlbarTestUtils.getRowAt(window, 3).hasAttribute("stale"),
    "Should not be stale"
  );

  // Now wait for the query end, this should remove stale results.
  await queryPromise;

  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    2,
    "There should be 2 results"
  );
  Assert.ok(
    !UrlbarTestUtils.getRowAt(window, 0).hasAttribute("stale"),
    "Should not be stale"
  );
  Assert.ok(
    !UrlbarTestUtils.getRowAt(window, 1).hasAttribute("stale"),
    "Should not be stale"
  );

  await UrlbarTestUtils.promisePopupClose(window);
  gURLBar.handleRevert();

  cleanup();
});
