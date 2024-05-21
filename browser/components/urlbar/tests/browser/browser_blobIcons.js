/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests `Blob` icon management in the view.

"use strict";

// A test blob that should be unique in the entire browser.
const TEST_ICON_BLOB = new Blob([new Uint8Array([5, 11, 2013])]);

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

  // No blob URLs should have been created since there were no results with blob
  // icons.
  await checkCallCounts(spies, null, {
    createObjectURL: 0,
  });

  // Create a test provider that returns a result with a blob icon.
  let provider = new UrlbarTestUtils.TestProvider({
    results: [
      new UrlbarResult(
        UrlbarUtils.RESULT_TYPE.URL,
        UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        {
          url: "https://example.com/",
          iconBlob: TEST_ICON_BLOB,
        }
      ),
    ],
  });
  UrlbarProvidersManager.registerProvider(provider);

  // Do some searches.
  let blobUrl = await doSearches(provider, spies);

  // Closing the view should cause `revokeObjectURL()` to be called.
  await UrlbarTestUtils.promisePopupClose(window);
  await checkCallCounts(spies, blobUrl, {
    createObjectURL: 0,
    revokeObjectURL: 1,
  });
  resetSpies(spies);

  // Do some more searches and close the view again.
  blobUrl = await doSearches(provider, spies);
  await UrlbarTestUtils.promisePopupClose(window);
  await checkCallCounts(spies, blobUrl, {
    createObjectURL: 0,
    revokeObjectURL: 1,
  });
  resetSpies(spies);

  // Remove the provider, do another search, and close the view. Since no
  // results with blob icons are matched, the call counts should not change.
  UrlbarProvidersManager.unregisterProvider(provider);
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "test",
  });
  await UrlbarTestUtils.promisePopupClose(window);
  await checkCallCounts(spies, blobUrl, {
    createObjectURL: 0,
    revokeObjectURL: 0,
  });

  sandbox.restore();
});

async function doSearches(provider, spies) {
  let previousBlobUrl;
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
        previousBlobUrl,
        "Blob URL should be the same as in previous searches"
      );
    }
    previousBlobUrl = result.image;

    // `createObjectURL()` should be called only once across all searches
    // performed in this function since there's only one result with a blob and
    // the view remains open the whole time. The URL should be created and
    // cached on the first search, and the cached URL should be used for the
    // later searches.
    await checkCallCounts(spies, result.image, {
      createObjectURL: 1,
      revokeObjectURL: 0,
    });
  }

  resetSpies(spies);

  return previousBlobUrl;
}

function resetSpies(spies) {
  for (let spy of Object.values(spies)) {
    spy.resetHistory();
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

/**
 * Checks calls to `createObjectURL()` and `revokeObjectURL()`.
 *
 * @param {object} spies
 *   An object that maps spy names to Sinon spies, one entry per function being
 *   spied on: `{ createObjectURL, revokeObjectURL }`
 * @param {Array} knownBlobUrl
 *   The blob URL that has been created and not yet verified as being revoked,
 *   or null if no blob URL has been created.
 * @param {object} expectedCountsByName
 *   An object that maps function names to the expected number of times they
 *   should have been called since spies were last reset:
 *   `{ createObjectURL, revokeObjectURL }`
 */
async function checkCallCounts(spies, knownBlobUrl, expectedCountsByName) {
  // Other parts of the browser may also create and revoke blob URLs, so first
  // we filter `createObjectURL()` calls that were passed our test blob.
  let createCalls = [];
  for (let call of spies.createObjectURL.getCalls()) {
    if (await areBlobsEqual(call.args[0], TEST_ICON_BLOB)) {
      createCalls.push(call);
    }
  }
  Assert.strictEqual(
    createCalls.length,
    expectedCountsByName.createObjectURL,
    "createObjectURL spy call count"
  );

  // Similarly there may be other callers of `revokeObjectURL()`, so first we
  // filter calls that were passed the known blob URL.
  if (knownBlobUrl) {
    let calls = spies.revokeObjectURL
      .getCalls()
      .filter(call => call.args[0] == knownBlobUrl);
    Assert.strictEqual(
      calls.length,
      expectedCountsByName.revokeObjectURL,
      "revokeObjectURL spy call count"
    );
  }
}

async function areBlobsEqual(blob1, blob2) {
  let buf1 = new Uint8Array(await blob1.arrayBuffer());
  let buf2 = new Uint8Array(await blob2.arrayBuffer());
  return (
    buf1.length == buf2.length && buf1.every((element, i) => element == buf2[i])
  );
}
