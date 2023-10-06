/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const CONFIG_DEFAULT = [
  {
    webExtension: { id: "basic@search.mozilla.org" },
    urls: {
      trending: {
        fullPath:
          "https://example.com/browser/browser/components/search/test/browser/trendingSuggestionEngine.sjs?richsuggestions=true",
        query: "",
      },
    },
    appliesTo: [{ included: { everywhere: true } }],
    default: "yes",
  },
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
      ["browser.urlbar.trending.featureGate", true],
      ["browser.urlbar.trending.requireSearchMode", false],
      // Bug 1775917: Disable the persisted-search-terms search tip because if
      // not dismissed, it can cause issues with other search tests.
      ["browser.urlbar.tipShownCount.searchTip_persist", 999],
    ],
  });

  SearchTestUtils.useMockIdleService();
  await SearchTestUtils.updateRemoteSettingsConfig(CONFIG_DEFAULT);

  registerCleanupFunction(async () => {
    let settingsWritten = SearchTestUtils.promiseSearchNotification(
      "write-settings-to-disk-complete"
    );
    await SearchTestUtils.updateRemoteSettingsConfig();
    await settingsWritten;
  });
});

add_task(async function test_trending_results() {
  await check_results({ featureEnabled: true });
  await check_results({ featureEnabled: false });
});

async function check_results({ featureEnabled = false }) {
  Services.telemetry.clearEvents();
  Services.telemetry.clearScalars();
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.richSuggestions.featureGate", featureEnabled]],
  });

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "",
    waitForFocus: SimpleTest.waitForFocus,
  });

  let numResults = UrlbarTestUtils.getResultCount(window);

  for (let i = 0; i < numResults; i++) {
    let { result } = await UrlbarTestUtils.getDetailsOfResultAt(window, i);
    Assert.equal(result.type, UrlbarUtils.RESULT_TYPE.SEARCH);
    Assert.equal(result.providerName, "SearchSuggestions");
    Assert.equal(result.payload.engine, "basic");
    Assert.equal(result.isRichSuggestion, featureEnabled);
    if (featureEnabled) {
      Assert.equal(typeof result.payload.description, "string");
      Assert.ok(result.payload.icon.startsWith("data:"));
    }
  }

  info("Select first remote search suggestion & hit Enter.");
  EventUtils.synthesizeKey("KEY_ArrowDown", {}, window);
  EventUtils.synthesizeKey("VK_RETURN", {}, window);

  let scalars = TelemetryTestUtils.getProcessScalars("parent", false, true);
  TelemetryTestUtils.assertScalar(scalars, "urlbar.engagement", 1);

  await SpecialPowers.popPrefEnv();
}

add_task(async function test_richsuggestion_deduplication() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.richSuggestions.featureGate", true]],
  });

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "test0",
    waitForFocus: SimpleTest.waitForFocus,
  });

  let { result: heuristicResult } = await UrlbarTestUtils.getDetailsOfResultAt(
    window,
    0
  );
  let { result: richResult } = await UrlbarTestUtils.getDetailsOfResultAt(
    window,
    1
  );

  // The Rich Suggestion that points to the same query as the Hueristic result
  // should not be deduplicated.
  Assert.equal(heuristicResult.type, UrlbarUtils.RESULT_TYPE.SEARCH);
  Assert.equal(heuristicResult.providerName, "HeuristicFallback");
  Assert.equal(richResult.type, UrlbarUtils.RESULT_TYPE.SEARCH);
  Assert.equal(richResult.providerName, "SearchSuggestions");
  Assert.equal(
    heuristicResult.payload.query,
    richResult.payload.lowerCaseSuggestion
  );

  await UrlbarTestUtils.promisePopupClose(window);
});
