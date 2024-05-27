/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ActionsProviderContextualSearch } = ChromeUtils.importESModule(
  "resource:///modules/ActionsProviderContextualSearch.sys.mjs"
);

const { AddonTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/AddonTestUtils.sys.mjs"
);

add_setup(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.scotchBonnet.enableOverride", true]],
  });

  let ext = await SearchTestUtils.installSearchExtension({
    name: "Contextual",
    search_url: "https://example.com/browser",
  });
  await AddonTestUtils.waitForSearchProviderStartup(ext);
});

add_task(async function test_no_engine() {
  const ENGINE_TEST_URL = "https://example.org/";
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

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "test",
  });

  Assert.ok(
    UrlbarTestUtils.getResultCount(window) > 0,
    "At least one result is shown"
  );
});

add_task(async function test_selectContextualSearchResult_already_installed() {
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

  info("Focus and select the contextual search result");
  let onLoad = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    expectedUrl
  );

  EventUtils.synthesizeKey("KEY_Tab");
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

  info("Focus and select the contextual search result");
  let onLoad = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    EXPECTED_URL
  );
  EventUtils.synthesizeKey("KEY_Tab");
  EventUtils.synthesizeKey("KEY_Enter");
  await onLoad;

  Assert.equal(
    gBrowser.selectedBrowser.currentURI.spec,
    EXPECTED_URL,
    "Selecting the contextual search result opens the search URL"
  );

  ActionsProviderContextualSearch.resetForTesting();
});
