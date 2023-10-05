/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests `Blob` icon management in the view.

"use strict";

// `URL.createObjectURL()` should be called the first time a blob icon is shown
// while the view is open, and `revokeObjectURL()` should be called when the
// view is closed.
add_task(async function test() {
  let sandbox = sinon.createSandbox();
  registerCleanupFunction(() => sandbox.restore());

  // Spy on `URL.createObjectURL()` and `revokeObjectURL()`.
  let spies = ["createObjectURL", "revokeObjectURL"].reduce((memo, name) => {
    memo[name] = sandbox.spy(Cu.getGlobalForObject(gURLBar.view).URL, name);
    return memo;
  }, {});

  // Do a search and close the view.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "test",
  });
  await UrlbarTestUtils.promisePopupClose(window);

  // No blob URLs should have been created or revoked since no results that have
  // blob icons were matched.
  checkCallCounts(spies, {
    createObjectURL: 0,
    revokeObjectURL: 0,
  });

  // Create a test provider that returns a result with a blob icon.
  let provider = new UrlbarTestUtils.TestProvider({
    results: [
      new UrlbarResult(
        UrlbarUtils.RESULT_TYPE.URL,
        UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        {
          url: "https://example.com/",
          iconBlob: new Blob([new Uint8Array([])]),
        }
      ),
    ],
  });
  UrlbarProvidersManager.registerProvider(provider);

  // Do some searches.
  await doSearches(provider, spies, {
    createObjectURL: 1,
    revokeObjectURL: 0,
  });

  // Closing the view should cause `revokeObjectURL()` to be called.
  await UrlbarTestUtils.promisePopupClose(window);
  checkCallCounts(spies, {
    createObjectURL: 1,
    revokeObjectURL: 1,
  });

  // Do some more searches.
  await doSearches(provider, spies, {
    createObjectURL: 2,
    revokeObjectURL: 1,
  });

  // Close the view.
  await UrlbarTestUtils.promisePopupClose(window);
  checkCallCounts(spies, {
    createObjectURL: 2,
    revokeObjectURL: 2,
  });

  // Remove the provider, do another search, and close the view. Since no
  // results with blob icons are matched, the call counts should not change.
  UrlbarProvidersManager.unregisterProvider(provider);
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "test",
  });
  await UrlbarTestUtils.promisePopupClose(window);
  checkCallCounts(spies, {
    createObjectURL: 2,
    revokeObjectURL: 2,
  });

  sandbox.restore();
});

async function doSearches(provider, spies, expectedCountsByName) {
  let previousImage;
  for (let i = 0; i < 3; i++) {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "test " + i,
    });

    let result = await getTestResult(provider);
    Assert.ok(result, "Test result should be present");
    Assert.ok(result.image, "Row has an icon with a src");
    Assert.ok(result.image.startsWith("blob:"), "Row icon src is a blob URL");
    if (i > 0) {
      Assert.equal(
        result.image,
        previousImage,
        "Blob URL should be the same as in previous searches"
      );
    }
    previousImage = result.image;

    // `createObjectURL()` should be called only once across all searches since
    // the view remains open the whole time.
    checkCallCounts(spies, expectedCountsByName);
  }
}

async function getTestResult(provider) {
  for (let i = 0; i < UrlbarTestUtils.getResultCount(window); i++) {
    let result = await UrlbarTestUtils.getDetailsOfResultAt(window, i);
    if (result.result.providerName == provider.name) {
      return result;
    }
  }
  return null;
}

function checkCallCounts(spies, expectedCountsByName) {
  for (let [name, count] of Object.entries(expectedCountsByName)) {
    Assert.strictEqual(spies[name].callCount, count, "Spy call count: " + name);
  }
}
