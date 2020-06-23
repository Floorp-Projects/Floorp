/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that results with a suggestedIndex property end up in the expected
// position.

add_task(async function suggestedIndex() {
  let result1 = new UrlbarResult(
    UrlbarUtils.RESULT_TYPE.URL,
    UrlbarUtils.RESULT_SOURCE.HISTORY,
    { url: "http://mozilla.org/1" }
  );
  result1.suggestedIndex = 2;
  let result2 = new UrlbarResult(
    UrlbarUtils.RESULT_TYPE.URL,
    UrlbarUtils.RESULT_SOURCE.HISTORY,
    { url: "http://mozilla.org/2" }
  );
  result2.suggestedIndex = 6;

  let provider = new UrlbarTestUtils.TestProvider({
    results: [result1, result2],
  });
  UrlbarProvidersManager.registerProvider(provider);
  async function clean() {
    UrlbarProvidersManager.unregisterProvider(provider);
    await PlacesUtils.history.clear();
  }
  registerCleanupFunction(clean);

  let urls = [];
  let maxResults = UrlbarPrefs.get("maxRichResults");
  // Add more results, so that the sum of these results plus the above ones,
  // will be greater than maxResults.
  for (let i = 0; i < maxResults; ++i) {
    urls.push("http://example.com/foo" + i);
  }
  await PlacesTestUtils.addVisits(urls);

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "foo",
  });

  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    maxResults,
    `There should be ${maxResults} results in the view.`
  );

  urls.reverse();
  urls.unshift(
    (await Services.search.getDefault()).getSubmission("foo").uri.spec
  );
  urls.splice(result1.suggestedIndex, 0, result1.payload.url);
  urls.splice(result2.suggestedIndex, 0, result2.payload.url);
  urls = urls.slice(0, maxResults);

  let expected = [];
  for (let i = 0; i < maxResults; ++i) {
    let url = (await UrlbarTestUtils.getDetailsOfResultAt(window, i)).url;
    expected.push(url);
  }
  // Check all the results.
  Assert.deepEqual(expected, urls);

  await clean();
  await UrlbarTestUtils.promisePopupClose(window);
});

add_task(async function suggestedIndex_append() {
  // When suggestedIndex is greater than the number of results the result is
  // appended.
  let result = new UrlbarResult(
    UrlbarUtils.RESULT_TYPE.URL,
    UrlbarUtils.RESULT_SOURCE.HISTORY,
    { url: "http://mozilla.org/append/" }
  );
  result.suggestedIndex = 4;

  let provider = new UrlbarTestUtils.TestProvider({ results: [result] });
  UrlbarProvidersManager.registerProvider(provider);
  async function clean() {
    UrlbarProvidersManager.unregisterProvider(provider);
    await PlacesUtils.history.clear();
  }
  registerCleanupFunction(clean);

  await PlacesTestUtils.addVisits("http://example.com/bar");

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "bar",
  });

  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    3,
    `There should be 3 results in the view.`
  );

  let urls = [
    (await Services.search.getDefault()).getSubmission("bar").uri.spec,
    "http://example.com/bar",
    "http://mozilla.org/append/",
  ];

  let expected = [];
  for (let i = 0; i < 3; ++i) {
    let url = (await UrlbarTestUtils.getDetailsOfResultAt(window, i)).url;
    expected.push(url);
  }
  // Check all the results.
  Assert.deepEqual(expected, urls);

  await clean();
  await UrlbarTestUtils.promisePopupClose(window);
});
