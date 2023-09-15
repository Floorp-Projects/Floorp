/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file tests browser.engagement.navigation.urlbar_persisted and the
 * event navigation.search.urlbar_persisted
 */

"use strict";

const { SearchSERPTelemetry } = ChromeUtils.importESModule(
  "resource:///modules/SearchSERPTelemetry.sys.mjs"
);

const SCALAR_URLBAR_PERSISTED =
  "browser.engagement.navigation.urlbar_persisted";

const SEARCH_STRING = "chocolate";

let testEngine;
add_setup(async () => {
  Services.telemetry.clearScalars();
  Services.telemetry.clearEvents();
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.showSearchTerms.featureGate", true]],
  });

  await SearchTestUtils.installSearchExtension(
    {
      name: "MozSearch",
      search_url: "https://www.example.com/",
      search_url_get_params: "q={searchTerms}&pc=fake_code",
    },
    { setAsDefault: true }
  );

  testEngine = Services.search.getEngineByName("MozSearch");

  // Enable event recording for the events.
  Services.telemetry.setEventRecordingEnabled("navigation", true);

  registerCleanupFunction(async function () {
    await PlacesUtils.history.clear();
    Services.telemetry.clearScalars();
    Services.telemetry.clearEvents();
    Services.telemetry.setEventRecordingEnabled("navigation", false);
  });
});

async function searchForString(searchString, tab) {
  info(`Search for string: ${searchString}.`);
  let [expectedSearchUrl] = UrlbarUtils.getSearchQueryUrl(
    testEngine,
    searchString
  );
  let browserLoadedPromise = BrowserTestUtils.browserLoaded(
    tab.linkedBrowser,
    false,
    expectedSearchUrl
  );
  gURLBar.focus();
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus,
    value: searchString,
    fireInputEvent: true,
  });
  EventUtils.synthesizeKey("KEY_Enter");
  await browserLoadedPromise;
  info("Finished loading search.");
  return expectedSearchUrl;
}

async function gotoUrl(url, tab) {
  let browserLoadedPromise = BrowserTestUtils.browserLoaded(
    tab.linkedBrowser,
    false,
    url
  );
  BrowserTestUtils.startLoadingURIString(tab.linkedBrowser, url);
  await browserLoadedPromise;
  info(`Loaded page: ${url}`);
}

async function goBack(browser) {
  let pageShowPromise = BrowserTestUtils.waitForContentEvent(
    browser,
    "pageshow"
  );
  browser.goBack();
  await pageShowPromise;
  info("Go back a page.");
}

async function goForward(browser) {
  let pageShowPromise = BrowserTestUtils.waitForContentEvent(
    browser,
    "pageshow"
  );
  browser.goForward();
  await pageShowPromise;
  info("Go forward a page.");
}

function assertScalarSearchEnter(number) {
  let scalars = TelemetryTestUtils.getProcessScalars("parent", true, true);
  TelemetryTestUtils.assertKeyedScalar(
    scalars,
    SCALAR_URLBAR_PERSISTED,
    "search_enter",
    number
  );
}

function assertScalarDoesNotExist(scalar) {
  let scalars = TelemetryTestUtils.getProcessScalars("parent", true, false);
  Assert.ok(!(scalar in scalars), scalar + " must not be recorded.");
}

function assertTelemetryEvents() {
  TelemetryTestUtils.assertEvents(
    [
      [
        "navigation",
        "search",
        "urlbar",
        "enter",
        { engine: "other-MozSearch" },
      ],
      [
        "navigation",
        "search",
        "urlbar_persisted",
        "enter",
        { engine: "other-MozSearch" },
      ],
    ],
    {
      category: "navigation",
      method: "search",
    }
  );
}

// A user making a search after making a search should result
// in the telemetry being recorded.
add_task(async function search_after_search() {
  let search_hist =
    TelemetryTestUtils.getAndClearKeyedHistogram("SEARCH_COUNTS");

  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  await searchForString(SEARCH_STRING, tab);

  // Scalar should not exist from a blank page, only when a search
  // is conducted from a default SERP.
  await assertScalarDoesNotExist(SCALAR_URLBAR_PERSISTED);

  // After the first search, we should expect the SAP to change
  // because the search term should show up on the SERP.
  await searchForString(SEARCH_STRING, tab);
  assertScalarSearchEnter(1);

  // Check search counts.
  TelemetryTestUtils.assertKeyedHistogramSum(
    search_hist,
    "other-MozSearch.urlbar-persisted",
    1
  );

  // Check events.
  assertTelemetryEvents();

  BrowserTestUtils.removeTab(tab);
});

// A user going to a tab that contains a SERP should
// trigger the telemetry when conducting a search.
add_task(async function switch_to_tab_and_search() {
  let search_hist =
    TelemetryTestUtils.getAndClearKeyedHistogram("SEARCH_COUNTS");

  const tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  await searchForString(SEARCH_STRING, tab1);

  const tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  await gotoUrl("https://www.example.com/some-place", tab2);

  await BrowserTestUtils.switchTab(gBrowser, tab1);
  await searchForString(SEARCH_STRING, tab1);
  assertScalarSearchEnter(1);

  // Check search count.
  TelemetryTestUtils.assertKeyedHistogramSum(
    search_hist,
    "other-MozSearch.urlbar-persisted",
    1
  );

  // Check events.
  assertTelemetryEvents();

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
});

// When a user reverts the Urlbar after the search terms persist,
// conducting another search should still be registered as a
// urlbar-persisted SAP.
add_task(async function handle_revert() {
  let search_hist =
    TelemetryTestUtils.getAndClearKeyedHistogram("SEARCH_COUNTS");

  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  await searchForString(SEARCH_STRING, tab);

  gURLBar.handleRevert();
  await searchForString(SEARCH_STRING, tab);

  assertScalarSearchEnter(1);

  // Check search count.
  TelemetryTestUtils.assertKeyedHistogramSum(
    search_hist,
    "other-MozSearch.urlbar-persisted",
    1
  );

  // Check events.
  assertTelemetryEvents();

  BrowserTestUtils.removeTab(tab);
});

// A user going back and forth in history should trigger
// urlbar-persisted telemetry when returning to a SERP
// and conducting a search.
add_task(async function back_and_forth() {
  let search_hist =
    TelemetryTestUtils.getAndClearKeyedHistogram("SEARCH_COUNTS");

  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  // Create three pages in history: a page, a SERP, and a page.
  await gotoUrl("https://www.example.com/some-place", tab);
  await searchForString(SEARCH_STRING, tab);
  await gotoUrl("https://www.example.com/another-page", tab);

  // Go back to the SERP by using both back and forward.
  await goBack(tab.linkedBrowser);
  await goBack(tab.linkedBrowser);
  await goForward(tab.linkedBrowser);
  await assertScalarDoesNotExist(SCALAR_URLBAR_PERSISTED);

  // Then do a search.
  await searchForString(SEARCH_STRING, tab);
  assertScalarSearchEnter(1);

  // Check search count.
  TelemetryTestUtils.assertKeyedHistogramSum(
    search_hist,
    "other-MozSearch.urlbar-persisted",
    1
  );

  // Check events.
  assertTelemetryEvents();

  BrowserTestUtils.removeTab(tab);
});
