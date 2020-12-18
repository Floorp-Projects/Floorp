/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This test checks that search values longer than
 * SearchSuggestionController.SEARCH_HISTORY_MAX_VALUE_LENGTH are not added to
 * search history.
 */

"use strict";

const { SearchSuggestionController } = ChromeUtils.import(
  "resource://gre/modules/SearchSuggestionController.jsm"
);

let gEngine;

add_task(async function setup() {
  gEngine = await Services.search.addEngineWithDetails("TestLimit", {
    template: "http://example.com/?search={searchTerms}",
  });
  let oldDefaultEngine = await Services.search.getDefault();
  await Services.search.setDefault(gEngine);
  await UrlbarTestUtils.formHistory.clear();

  registerCleanupFunction(async function() {
    await Services.search.setDefault(oldDefaultEngine);
    await Services.search.removeEngine(gEngine);
    await UrlbarTestUtils.formHistory.clear();
  });
});

add_task(async function sanityCheckShortString() {
  const shortString = new Array(
    SearchSuggestionController.SEARCH_HISTORY_MAX_VALUE_LENGTH
  )
    .fill("a")
    .join("");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: shortString,
  });
  let url = gEngine.getSubmission(shortString).uri.spec;
  let loadPromise = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    url
  );
  let addPromise = UrlbarTestUtils.formHistory.promiseChanged("add");
  EventUtils.synthesizeKey("VK_RETURN");
  await Promise.all([loadPromise, addPromise]);

  let formHistory = (
    await UrlbarTestUtils.formHistory.search({ source: gEngine.name })
  ).map(entry => entry.value);
  Assert.deepEqual(
    formHistory,
    [shortString],
    "Should find form history after adding it"
  );

  await UrlbarTestUtils.formHistory.clear();
});

add_task(async function urlbar_checkLongString() {
  const longString = new Array(
    SearchSuggestionController.SEARCH_HISTORY_MAX_VALUE_LENGTH + 1
  )
    .fill("a")
    .join("");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: longString,
  });
  let url = gEngine.getSubmission(longString).uri.spec;
  let loadPromise = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    url
  );
  EventUtils.synthesizeKey("VK_RETURN");
  await loadPromise;
  // There's nothing we can wait for, since addition should not be happening.
  /* eslint-disable mozilla/no-arbitrary-setTimeout */
  await new Promise(resolve => setTimeout(resolve, 500));
  let formHistory = (
    await UrlbarTestUtils.formHistory.search({ source: gEngine.name })
  ).map(entry => entry.value);
  Assert.deepEqual(formHistory, [], "Should not find form history");

  await UrlbarTestUtils.formHistory.clear();
});
