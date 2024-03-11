/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const CONFIG_DEFAULT = [
  {
    webExtension: { id: "basic@search.mozilla.org" },
    appliesTo: [{ included: { everywhere: true } }],
    urls: {
      trending: {
        fullPath:
          "https://example.com/browser/browser/components/search/test/browser/trendingSuggestionEngine.sjs",
        query: "",
      },
    },
    default: "yes",
  },
];

const CONFIG_DEFAULT_V2 = [
  {
    recordType: "engine",
    identifier: "basic",
    base: {
      name: "basic",
      urls: {
        search: {
          base: "https://example.com",
          searchTermParamName: "q",
        },
        trending: {
          base: "https://example.com/browser/browser/components/search/test/browser/trendingSuggestionEngine.sjs",
          method: "GET",
        },
      },
    },
    variants: [
      {
        environment: { allRegionsAndLocales: true },
      },
    ],
  },
  {
    recordType: "defaultEngines",
    globalDefault: "basic",
    specificDefaults: [],
  },
  {
    recordType: "engineOrders",
    orders: [],
  },
];

const TOP_SITES = [
  "https://example-1.com/",
  "https://example-2.com/",
  "https://example-3.com/",
];

SearchTestUtils.init(this);

add_setup(async () => {
  // Use engines in test directory
  let searchExtensions = getChromeDir(getResolvedURI(gTestPath));
  searchExtensions.append("search-engines");
  await SearchTestUtils.useMochitestEngines(searchExtensions);

  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.suggest.searches", true],
      ["browser.urlbar.suggest.recentsearches", true],
      ["browser.urlbar.recentsearches.featureGate", true],
      // Disable UrlbarProviderSearchTips
      [
        "browser.newtabpage.activity-stream.asrouter.userprefs.cfr.features",
        false,
      ],
    ],
  });

  SearchTestUtils.useMockIdleService();
  await SearchTestUtils.updateRemoteSettingsConfig(
    SearchUtils.newSearchConfigEnabled ? CONFIG_DEFAULT_V2 : CONFIG_DEFAULT
  );
  Services.telemetry.clearScalars();

  registerCleanupFunction(async () => {
    let settingsWritten = SearchTestUtils.promiseSearchNotification(
      "write-settings-to-disk-complete"
    );
    await SearchTestUtils.updateRemoteSettingsConfig();
    await settingsWritten;
    await UrlbarTestUtils.formHistory.clear();
  });
});

add_task(async () => {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    window.gBrowser,
    "data:text/html,"
  );

  info("Perform a search that will be added to search history.");
  let browserLoaded = BrowserTestUtils.browserLoaded(
    window.gBrowser.selectedBrowser
  );
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "Bob Vylan",
  });

  await UrlbarTestUtils.promisePopupClose(window, () => {
    EventUtils.synthesizeKey("KEY_Enter", {}, window);
  });
  await browserLoaded;

  info("Now check that is shown in search history.");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "",
  });

  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    1,
    "Previous search shown"
  );
  let { result } = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.equal(result.providerName, "RecentSearches");

  info("Selecting the recent search should be indicated in telemetry.");
  browserLoaded = BrowserTestUtils.browserLoaded(
    window.gBrowser.selectedBrowser
  );
  await UrlbarTestUtils.promisePopupClose(window, () => {
    EventUtils.synthesizeKey("KEY_ArrowDown", {}, window);
    EventUtils.synthesizeKey("KEY_Enter", {}, window);
  });
  await browserLoaded;

  let scalars = TelemetryTestUtils.getProcessScalars("parent", true, true);
  TelemetryTestUtils.assertKeyedScalar(
    scalars,
    "urlbar.picked.recent_search",
    0,
    1
  );
  await BrowserTestUtils.removeTab(tab);
});

// Ensure that top sites are shown above recent searches, even if trending
// suggestions are disabled.
add_task(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.suggest.trending", false],
      ["browser.urlbar.suggest.topsites", true],
      ["browser.newtabpage.activity-stream.default.sites", TOP_SITES.join(",")],
    ],
  });
  await updateTopSites(sites => sites && sites.length);

  let tab = await BrowserTestUtils.openNewForegroundTab(
    window.gBrowser,
    "data:text/html,"
  );

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "",
  });

  let count = UrlbarTestUtils.getResultCount(window);
  let { result } = await UrlbarTestUtils.getDetailsOfResultAt(
    window,
    count - 1
  );
  Assert.equal(result.providerName, "RecentSearches");

  await BrowserTestUtils.removeTab(tab);
  await SpecialPowers.popPrefEnv();
});

// Test that triggering the help menu of trending suggestions does not
// record that selection as a search.
add_task(async () => {
  await UrlbarTestUtils.formHistory.clear();
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.suggest.topsites", false],
      ["browser.urlbar.suggest.trending", true],
      ["browser.urlbar.trending.featureGate", true],
      ["browser.urlbar.trending.requireSearchMode", false],
      ["app.support.baseURL", "https://example.com"],
    ],
  });

  let tab = await BrowserTestUtils.openNewForegroundTab(
    window.gBrowser,
    "data:text/html,"
  );

  info("Open the urlbar and pick the help menu of a trending result.");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "",
  });

  await UrlbarTestUtils.openResultMenuAndClickItem(window, "help", {
    resultIndex: 1,
    openByMouse: true,
  });

  info("Open the urlbar and check that a recent search has not been added.");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "",
  });

  let { result } = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.notEqual(
    result.providerName,
    "RecentSearches",
    "Click on help URL did not record a search"
  );

  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
  await BrowserTestUtils.removeTab(tab);
  await SpecialPowers.popPrefEnv();
});
