/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { UrlbarProviderContextualSearch } = ChromeUtils.importESModule(
  "resource:///modules/UrlbarProviderContextualSearch.sys.mjs"
);

add_setup(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.contextualSearch.enabled", true]],
  });
});

add_task(async function test_selectContextualSearchResult_already_installed() {
  await SearchTestUtils.installSearchExtension({
    name: "Contextual",
    search_url: "https://example.com/browser",
  });

  const ENGINE_TEST_URL = "https://example.com/";
  let onLoaded = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    ENGINE_TEST_URL
  );
  BrowserTestUtils.startLoadingURIString(
    gBrowser.selectedBrowser,
    ENGINE_TEST_URL
  );
  await onLoaded;

  const query = "search";
  let engine = Services.search.getEngineByName("Contextual");
  const [expectedUrl] = UrlbarUtils.getSearchQueryUrl(engine, query);

  Assert.ok(
    expectedUrl.includes(`?q=${query}`),
    "Expected URL should be a search URL"
  );

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: query,
  });
  const resultIndex = UrlbarTestUtils.getResultCount(window) - 1;
  const result = await UrlbarTestUtils.getDetailsOfResultAt(
    window,
    resultIndex
  );

  is(
    result.dynamicType,
    "contextualSearch",
    "Second last result is a contextual search result"
  );

  info("Focus and select the contextual search result");
  UrlbarTestUtils.setSelectedRowIndex(window, resultIndex);
  let onLoad = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    expectedUrl
  );
  EventUtils.synthesizeKey("KEY_Enter");
  await onLoad;

  Assert.equal(
    gBrowser.selectedBrowser.currentURI.spec,
    expectedUrl,
    "Selecting the contextual search result opens the search URL"
  );
});

add_task(async function test_selectContextualSearchResult_not_installed() {
  const ENGINE_TEST_URL =
    "http://mochi.test:8888/browser/browser/components/search/test/browser/opensearch.html";
  const EXPECTED_URL =
    "http://mochi.test:8888/browser/browser/components/search/test/browser/?search&test=search";
  let onLoaded = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    ENGINE_TEST_URL
  );
  BrowserTestUtils.startLoadingURIString(
    gBrowser.selectedBrowser,
    ENGINE_TEST_URL
  );
  await onLoaded;

  const query = "search";

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: query,
  });
  const resultIndex = UrlbarTestUtils.getResultCount(window) - 1;
  const result = await UrlbarTestUtils.getDetailsOfResultAt(
    window,
    resultIndex
  );

  Assert.equal(
    result.dynamicType,
    "contextualSearch",
    "Second last result is a contextual search result"
  );

  info("Focus and select the contextual search result");
  UrlbarTestUtils.setSelectedRowIndex(window, resultIndex);
  let onLoad = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    EXPECTED_URL
  );
  EventUtils.synthesizeKey("KEY_Enter");
  await onLoad;

  Assert.equal(
    gBrowser.selectedBrowser.currentURI.spec,
    EXPECTED_URL,
    "Selecting the contextual search result opens the search URL"
  );
});
