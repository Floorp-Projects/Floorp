/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for the following data of engagement telemetry.
// - selected_result
// - selected_result_subtype
// - selected_position
// - provider
// - results

ChromeUtils.defineESModuleGetters(this, {
  UrlbarProviderClipboard:
    "resource:///modules/UrlbarProviderClipboard.sys.mjs",
  SearchUtils: "resource://gre/modules/SearchUtils.sys.mjs",
});

// This test has many subtests and can time out in verify mode.
requestLongerTimeout(5);

add_setup(async function () {
  await setup();
});

add_task(async function selected_result_autofill_about() {
  await doTest(async () => {
    await openPopup("about:about");
    await doEnter();

    assertEngagementTelemetry([
      {
        selected_result: "autofill_about",
        selected_result_subtype: "",
        selected_position: 1,
        provider: "Autofill",
        results: "autofill_about",
      },
    ]);
  });
});

add_task(async function selected_result_autofill_adaptive() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.autoFill.adaptiveHistory.enabled", true]],
  });

  await doTest(async () => {
    await PlacesTestUtils.addVisits("https://example.com/test");
    await UrlbarUtils.addToInputHistory("https://example.com/test", "exa");
    await openPopup("exa");
    await doEnter();

    assertEngagementTelemetry([
      {
        selected_result: "autofill_adaptive",
        selected_result_subtype: "",
        selected_position: 1,
        provider: "Autofill",
        results: "autofill_adaptive",
      },
    ]);
  });

  await SpecialPowers.popPrefEnv();
});

add_task(async function selected_result_autofill_origin() {
  await doTest(async () => {
    await PlacesTestUtils.addVisits("https://example.com/test");
    await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();
    await openPopup("exa");
    await doEnter();

    assertEngagementTelemetry([
      {
        selected_result: "autofill_origin",
        selected_result_subtype: "",
        selected_position: 1,
        provider: "Autofill",
        results: "autofill_origin,history",
      },
    ]);
  });
});

add_task(async function selected_result_autofill_url() {
  await doTest(async () => {
    await PlacesTestUtils.addVisits("https://example.com/test");
    await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();
    await openPopup("https://example.com/test");
    await doEnter();

    assertEngagementTelemetry([
      {
        selected_result: "autofill_url",
        selected_result_subtype: "",
        selected_position: 1,
        provider: "Autofill",
        results: "autofill_url",
      },
    ]);
  });
});

add_task(async function selected_result_bookmark() {
  await doTest(async () => {
    await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.unfiledGuid,
      url: "https://example.com/bookmark",
      title: "bookmark",
    });

    await openPopup("bookmark");
    await selectRowByURL("https://example.com/bookmark");
    await doEnter();

    assertEngagementTelemetry([
      {
        selected_result: "bookmark",
        selected_result_subtype: "",
        selected_position: 2,
        provider: "Places",
        results: "search_engine,bookmark",
      },
    ]);
  });
});

add_task(async function selected_result_history() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.autoFill", false]],
  });

  await doTest(async () => {
    await PlacesTestUtils.addVisits("https://example.com/test");

    await openPopup("example");
    await selectRowByURL("https://example.com/test");
    await doEnter();

    assertEngagementTelemetry([
      {
        selected_result: "history",
        selected_result_subtype: "",
        selected_position: 2,
        provider: "Places",
        results: "search_engine,history",
      },
    ]);
  });

  await SpecialPowers.popPrefEnv();
});

add_task(async function selected_result_keyword() {
  await doTest(async () => {
    await PlacesUtils.keywords.insert({
      keyword: "keyword",
      url: "https://example.com/?q=%s",
    });

    await openPopup("keyword test");
    await doEnter();

    assertEngagementTelemetry([
      {
        selected_result: "keyword",
        selected_result_subtype: "",
        selected_position: 1,
        provider: "BookmarkKeywords",
        results: "keyword",
      },
    ]);

    await PlacesUtils.keywords.remove("keyword");
  });
});

add_task(async function selected_result_search_engine() {
  await doTest(async () => {
    await openPopup("x");
    await doEnter();

    assertEngagementTelemetry([
      {
        selected_result: "search_engine",
        selected_result_subtype: "",
        selected_position: 1,
        provider: "HeuristicFallback",
        results: "search_engine",
      },
    ]);
  });
});

add_task(async function selected_result_search_suggest() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.suggest.searches", true],
      ["browser.urlbar.maxHistoricalSearchSuggestions", 2],
    ],
  });

  await doTest(async () => {
    await openPopup("foo");
    await selectRowByURL("http://mochi.test:8888/?terms=foofoo");
    await doEnter();

    assertEngagementTelemetry([
      {
        selected_result: "search_suggest",
        selected_result_subtype: "",
        selected_position: 2,
        provider: "SearchSuggestions",
        results: "search_engine,search_suggest,search_suggest",
      },
    ]);
  });

  await SpecialPowers.popPrefEnv();
});

add_task(async function selected_result_search_history() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.suggest.searches", true],
      ["browser.urlbar.maxHistoricalSearchSuggestions", 2],
    ],
  });

  await doTest(async () => {
    await UrlbarTestUtils.formHistory.add(["foofoo", "foobar"]);

    await openPopup("foo");
    await selectRowByURL("http://mochi.test:8888/?terms=foofoo");
    await doEnter();

    assertEngagementTelemetry([
      {
        selected_result: "search_history",
        selected_result_subtype: "",
        selected_position: 3,
        provider: "SearchSuggestions",
        results: "search_engine,search_history,search_history",
      },
    ]);
  });

  await SpecialPowers.popPrefEnv();
});

add_task(async function selected_result_url() {
  await doTest(async () => {
    await openPopup("https://example.com/");
    await doEnter();

    assertEngagementTelemetry([
      {
        selected_result: "url",
        selected_result_subtype: "",
        selected_position: 1,
        provider: "HeuristicFallback",
        results: "url",
      },
    ]);
  });
});

add_task(async function selected_result_tab() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.secondaryActions.featureGate", false]],
  });

  const tab = BrowserTestUtils.addTab(gBrowser, "https://example.com/");

  await doTest(async () => {
    await openPopup("example");
    await selectRowByProvider("Places");
    EventUtils.synthesizeKey("KEY_Enter");
    await BrowserTestUtils.waitForCondition(() => gBrowser.selectedTab === tab);

    assertEngagementTelemetry([
      {
        selected_result: "tab",
        selected_result_subtype: "",
        selected_position: 4,
        provider: "Places",
        results: "search_engine,search_suggest,search_suggest,tab",
      },
    ]);
  });

  await SpecialPowers.popPrefEnv();
  BrowserTestUtils.removeTab(tab);
});

add_task(async function selected_result_remote_tab() {
  const remoteTab = await loadRemoteTab("https://example.com");

  await doTest(async () => {
    await openPopup("example");
    await selectRowByProvider("RemoteTabs");
    await doEnter();

    assertEngagementTelemetry([
      {
        selected_result: "remote_tab",
        selected_result_subtype: "",
        selected_position: 2,
        provider: "RemoteTabs",
        results: "search_engine,remote_tab",
      },
    ]);
  });

  await remoteTab.unload();
});

add_task(async function selected_result_addon() {
  const addon = loadOmniboxAddon({ keyword: "omni" });
  await addon.startup();

  await doTest(async () => {
    await openPopup("omni test");
    await doEnter();

    assertEngagementTelemetry([
      {
        selected_result: "addon",
        selected_result_subtype: "",
        selected_position: 1,
        provider: "Omnibox",
        results: "addon",
      },
    ]);
  });

  await addon.unload();
});

add_task(async function selected_result_tab_to_search() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.tabToSearch.onboard.interactionsLeft", 0]],
  });

  await SearchTestUtils.installSearchExtension({
    name: "mozengine",
    search_url: "https://mozengine/",
  });

  await doTest(async () => {
    for (let i = 0; i < 3; i++) {
      await PlacesTestUtils.addVisits(["https://mozengine/"]);
    }

    await openPopup("moze");
    await selectRowByProvider("TabToSearch");
    const onComplete = UrlbarTestUtils.promiseSearchComplete(window);
    EventUtils.synthesizeKey("KEY_Enter");
    await onComplete;

    assertEngagementTelemetry([
      {
        selected_result: "tab_to_search",
        selected_result_subtype: "",
        selected_position: 2,
        provider: "TabToSearch",
        results: "search_engine,tab_to_search,history",
      },
    ]);
  });

  await SpecialPowers.popPrefEnv();
});

add_task(async function selected_result_top_site() {
  await doTest(async () => {
    await addTopSites("https://example.com/");
    await showResultByArrowDown();
    await selectRowByURL("https://example.com/");
    await doEnter();

    assertEngagementTelemetry([
      {
        selected_result: "top_site",
        selected_result_subtype: "",
        selected_position: 1,
        provider: "UrlbarProviderTopSites",
        results: "top_site",
      },
    ]);
  });
});

add_task(async function selected_result_calc() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.suggest.calculator", true]],
  });

  await doTest(async () => {
    await openPopup("8*8");
    await selectRowByProvider("calculator");
    await SimpleTest.promiseClipboardChange("64", () => {
      EventUtils.synthesizeKey("KEY_Enter");
    });

    assertEngagementTelemetry([
      {
        selected_result: "calc",
        selected_result_subtype: "",
        selected_position: 2,
        provider: "calculator",
        results: "search_engine,calc",
      },
    ]);
  });

  await SpecialPowers.popPrefEnv();
});

add_task(async function selected_result_clipboard() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.clipboard.featureGate", true],
      ["browser.urlbar.suggest.clipboard", true],
    ],
  });
  SpecialPowers.clipboardCopyString(
    "https://example.com/selected_result_clipboard"
  );

  await doTest(async () => {
    await openPopup("");
    await selectRowByProvider("UrlbarProviderClipboard");
    await doEnter();

    assertEngagementTelemetry([
      {
        selected_result: "clipboard",
        selected_result_subtype: "",
        selected_position: 1,
        provider: "UrlbarProviderClipboard",
        results: "clipboard",
      },
    ]);
  });

  SpecialPowers.clipboardCopyString("");
  UrlbarProviderClipboard.setPreviousClipboardValue("");
  await SpecialPowers.popPrefEnv();
});

add_task(async function selected_result_unit() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.unitConversion.enabled", true]],
  });

  await doTest(async () => {
    await openPopup("1m to cm");
    await selectRowByProvider("UnitConversion");
    await SimpleTest.promiseClipboardChange("100 cm", () => {
      EventUtils.synthesizeKey("KEY_Enter");
    });

    assertEngagementTelemetry([
      {
        selected_result: "unit",
        selected_result_subtype: "",
        selected_position: 2,
        provider: "UnitConversion",
        results: "search_engine,unit",
      },
    ]);
  });

  await SpecialPowers.popPrefEnv();
});

add_task(async function selected_result_rs_adm_sponsored() {
  const cleanupQuickSuggest = await ensureQuickSuggestInit({
    prefs: [["quicksuggest.rustEnabled", false]],
  });

  await doTest(async () => {
    await openPopup("sponsored");
    await selectRowByURL("https://example.com/sponsored");
    await doEnter();

    assertEngagementTelemetry([
      {
        selected_result: "rs_adm_sponsored",
        selected_result_subtype: "",
        selected_position: 2,
        provider: "UrlbarProviderQuickSuggest",
        results: "search_engine,rs_adm_sponsored",
      },
    ]);
  });

  await cleanupQuickSuggest();
});

add_task(async function selected_result_rs_adm_nonsponsored() {
  const cleanupQuickSuggest = await ensureQuickSuggestInit({
    prefs: [["quicksuggest.rustEnabled", false]],
  });

  await doTest(async () => {
    await openPopup("nonsponsored");
    await selectRowByURL("https://example.com/nonsponsored");
    await doEnter();

    assertEngagementTelemetry([
      {
        selected_result: "rs_adm_nonsponsored",
        selected_result_subtype: "",
        selected_position: 2,
        provider: "UrlbarProviderQuickSuggest",
        results: "search_engine,rs_adm_nonsponsored",
      },
    ]);
  });

  await cleanupQuickSuggest();
});

add_task(async function selected_result_input_field() {
  const expected = [
    {
      selected_result: "input_field",
      selected_result_subtype: "",
      selected_position: 0,
      provider: null,
      results: "",
    },
  ];

  await doTest(async () => {
    await doDropAndGo("example.com");

    assertEngagementTelemetry(expected);
  });

  await doTest(async () => {
    await expectNoConsoleErrors(async () => {
      await doPasteAndGo("example.com");
    });
  });
});

add_task(async function selected_result_weather() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.quickactions.enabled", false]],
  });

  const cleanupQuickSuggest = await ensureQuickSuggestInit();
  await MerinoTestUtils.initWeather();

  let provider = UrlbarPrefs.get("quickSuggestRustEnabled")
    ? "UrlbarProviderQuickSuggest"
    : "Weather";
  await doTest(async () => {
    await openPopup(MerinoTestUtils.WEATHER_KEYWORD);
    await selectRowByProvider(provider);
    await doEnter();

    assertEngagementTelemetry([
      {
        selected_result: "weather",
        selected_result_subtype: "",
        selected_position: 2,
        provider,
        results: "search_engine,weather",
      },
    ]);
  });

  await cleanupQuickSuggest();
  await SpecialPowers.popPrefEnv();
});

add_task(async function selected_result_navigational() {
  const cleanupQuickSuggest = await ensureQuickSuggestInit({
    merinoSuggestions: [
      {
        title: "Navigational suggestion",
        url: "https://example.com/navigational-suggestion",
        provider: "top_picks",
        is_sponsored: false,
        score: 0.25,
        block_id: 0,
        is_top_pick: true,
      },
    ],
  });

  await doTest(async () => {
    await openPopup("only match the Merino suggestion");
    await selectRowByProvider("UrlbarProviderQuickSuggest");
    await doEnter();

    assertEngagementTelemetry([
      {
        selected_result: "merino_top_picks",
        selected_result_subtype: "",
        selected_position: 2,
        provider: "UrlbarProviderQuickSuggest",
        results: "search_engine,merino_top_picks",
      },
    ]);
  });

  await cleanupQuickSuggest();
});

add_task(async function selected_result_dynamic_wikipedia() {
  const cleanupQuickSuggest = await ensureQuickSuggestInit({
    merinoSuggestions: [
      {
        block_id: 1,
        url: "https://example.com/dynamic-wikipedia",
        title: "Dynamic Wikipedia suggestion",
        click_url: "https://example.com/click",
        impression_url: "https://example.com/impression",
        advertiser: "dynamic-wikipedia",
        provider: "wikipedia",
        iab_category: "5 - Education",
      },
    ],
  });

  await doTest(async () => {
    await openPopup("only match the Merino suggestion");
    await selectRowByProvider("UrlbarProviderQuickSuggest");
    await doEnter();

    assertEngagementTelemetry([
      {
        selected_result: "merino_wikipedia",
        selected_result_subtype: "",
        selected_position: 2,
        provider: "UrlbarProviderQuickSuggest",
        results: "search_engine,merino_wikipedia",
      },
    ]);
  });

  await cleanupQuickSuggest();
});

add_task(async function selected_result_search_shortcut_button() {
  await doTest(async () => {
    const oneOffSearchButtons = UrlbarTestUtils.getOneOffSearchButtons(window);
    await openPopup("x");
    Assert.ok(!oneOffSearchButtons.selectedButton);

    // Select oneoff button added for test in setup().
    for (;;) {
      EventUtils.synthesizeKey("KEY_ArrowDown");
      if (!oneOffSearchButtons.selectedButton) {
        continue;
      }

      if (
        oneOffSearchButtons.selectedButton.engine.name.includes(
          "searchSuggestionEngine.xml"
        )
      ) {
        break;
      }
    }

    // Search immediately.
    await doEnter({ shiftKey: true });

    assertEngagementTelemetry([
      {
        selected_result: "search_shortcut_button",
        selected_result_subtype: "",
        selected_position: 0,
        provider: null,
        results: "search_engine",
      },
    ]);
  });
});

add_task(async function selected_result_trending() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.suggest.searches", true],
      ["browser.urlbar.trending.featureGate", true],
      ["browser.urlbar.trending.requireSearchMode", false],
      ["browser.urlbar.trending.maxResultsNoSearchMode", 1],
      ["browser.urlbar.weather.featureGate", false],
    ],
  });

  let defaultEngine = await Services.search.getDefault();
  let extension;
  if (!SearchUtils.newSearchConfigEnabled) {
    extension = await SearchTestUtils.installSearchExtension(
      {
        name: "mozengine",
        search_url: "https://example.org/",
      },
      { setAsDefault: true, skipUnload: true }
    );
  }

  SearchTestUtils.useMockIdleService();
  await SearchTestUtils.updateRemoteSettingsConfig(
    SearchUtils.newSearchConfigEnabled
      ? [
          {
            recordType: "engine",
            identifier: "mozengine",
            base: {
              name: "mozengine",
              urls: {
                search: {
                  base: "https://example.org/",
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
            globalDefault: "mozengine",
            specificDefaults: [],
          },
          {
            recordType: "engineOrders",
            orders: [],
          },
        ]
      : [
          {
            webExtension: { id: "mozengine@tests.mozilla.org" },
            urls: {
              trending: {
                fullPath:
                  "https://example.com/browser/browser/components/search/test/browser/trendingSuggestionEngine.sjs",
                query: "",
              },
            },
            appliesTo: [{ included: { everywhere: true } }],
            default: "yes",
          },
        ]
  );

  if (SearchUtils.newSearchConfigEnabled) {
    let engine = Services.search.getEngineByName("mozengine");
    await Services.search.setDefault(
      engine,
      Ci.nsISearchService.CHANGE_REASON_UNKNOWN
    );
  }

  await doTest(async () => {
    await openPopup("");
    await selectRowByProvider("SearchSuggestions");
    await doEnter();

    assertEngagementTelemetry([
      {
        selected_result: "trending_search",
        selected_result_subtype: "",
        selected_position: 1,
        provider: "SearchSuggestions",
        results: "trending_search",
      },
    ]);
  });

  if (SearchUtils.newSearchConfigEnabled) {
    let engine = Services.search.getEngineByName("mozengine");
    await Services.search.removeEngine(engine);
  } else {
    await extension.unload();
  }

  await Services.search.setDefault(
    defaultEngine,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );
  let settingsWritten = SearchTestUtils.promiseSearchNotification(
    "write-settings-to-disk-complete"
  );
  await SearchTestUtils.updateRemoteSettingsConfig();
  await settingsWritten;
  await PlacesUtils.history.clear();
  await SpecialPowers.popPrefEnv();
});

add_task(async function selected_result_trending_rich() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.richSuggestions.featureGate", true],
      ["browser.urlbar.suggest.searches", true],
      ["browser.urlbar.trending.featureGate", true],
      ["browser.urlbar.trending.requireSearchMode", false],
      ["browser.urlbar.trending.maxResultsNoSearchMode", 1],
      ["browser.urlbar.weather.featureGate", false],
    ],
  });

  let defaultEngine = await Services.search.getDefault();
  let extension;
  if (!SearchUtils.newSearchConfigEnabled) {
    extension = await SearchTestUtils.installSearchExtension(
      {
        name: "mozengine",
        search_url: "https://example.org/",
      },
      { setAsDefault: true, skipUnload: true }
    );
  }

  SearchTestUtils.useMockIdleService();
  await SearchTestUtils.updateRemoteSettingsConfig(
    SearchUtils.newSearchConfigEnabled
      ? [
          {
            recordType: "engine",
            identifier: "mozengine",
            base: {
              name: "mozengine",
              urls: {
                search: {
                  base: "https://example.org/",
                  searchTermParamName: "q",
                },
                trending: {
                  base: "https://example.com/browser/browser/components/search/test/browser/trendingSuggestionEngine.sjs",
                  method: "GET",
                  params: [
                    {
                      name: "richsuggestions",
                      value: "true",
                    },
                  ],
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
            globalDefault: "mozengine",
            specificDefaults: [],
          },
          {
            recordType: "engineOrders",
            orders: [],
          },
        ]
      : [
          {
            webExtension: { id: "mozengine@tests.mozilla.org" },
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
        ]
  );

  if (SearchUtils.newSearchConfigEnabled) {
    let engine = Services.search.getEngineByName("mozengine");
    await Services.search.setDefault(
      engine,
      Ci.nsISearchService.CHANGE_REASON_UNKNOWN
    );
  }

  await doTest(async () => {
    await openPopup("");
    await selectRowByProvider("SearchSuggestions");
    await doEnter();

    assertEngagementTelemetry([
      {
        selected_result: "trending_search_rich",
        selected_result_subtype: "",
        selected_position: 1,
        provider: "SearchSuggestions",
        results: "trending_search_rich",
      },
    ]);
  });

  if (SearchUtils.newSearchConfigEnabled) {
    let engine = Services.search.getEngineByName("mozengine");
    await Services.search.removeEngine(engine);
  } else {
    await extension.unload();
  }

  await Services.search.setDefault(
    defaultEngine,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );
  let settingsWritten = SearchTestUtils.promiseSearchNotification(
    "write-settings-to-disk-complete"
  );
  await SearchTestUtils.updateRemoteSettingsConfig();
  await settingsWritten;
  await PlacesUtils.history.clear();
  await SpecialPowers.popPrefEnv();
});

add_task(async function selected_result_addons() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.addons.featureGate", true],
      ["browser.urlbar.suggest.searches", false],
    ],
  });

  const cleanupQuickSuggest = await ensureQuickSuggestInit({
    merinoSuggestions: [
      {
        provider: "amo",
        icon: "https://example.com/good-addon.svg",
        url: "https://example.com/good-addon",
        title: "Good Addon",
        description: "This is a good addon",
        custom_details: {
          amo: {
            rating: "4.8",
            number_of_ratings: "1234567",
            guid: "good@addon",
          },
        },
        is_top_pick: true,
      },
    ],
  });

  await doTest(async () => {
    await openPopup("only match the Merino suggestion");
    await selectRowByProvider("UrlbarProviderQuickSuggest");
    await doEnter();

    assertEngagementTelemetry([
      {
        selected_result: "merino_amo",
        selected_result_subtype: "",
        selected_position: 2,
        provider: "UrlbarProviderQuickSuggest",
        results: "search_engine,merino_amo",
      },
    ]);
  });

  await cleanupQuickSuggest();
  await SpecialPowers.popPrefEnv();
});

add_task(async function selected_result_rust_adm_sponsored() {
  const cleanupQuickSuggest = await ensureQuickSuggestInit({
    prefs: [["quicksuggest.rustEnabled", true]],
  });

  await doTest(async () => {
    await openPopup("sponsored");
    await selectRowByURL("https://example.com/sponsored");
    await doEnter();

    assertEngagementTelemetry([
      {
        selected_result: "rust_adm_sponsored",
        selected_result_subtype: "",
        selected_position: 2,
        provider: "UrlbarProviderQuickSuggest",
        results: "search_engine,rust_adm_sponsored",
      },
    ]);
  });

  await cleanupQuickSuggest();
});

add_task(async function selected_result_rust_adm_nonsponsored() {
  const cleanupQuickSuggest = await ensureQuickSuggestInit({
    prefs: [["quicksuggest.rustEnabled", true]],
  });

  await doTest(async () => {
    await openPopup("nonsponsored");
    await selectRowByURL("https://example.com/nonsponsored");
    await doEnter();

    assertEngagementTelemetry([
      {
        selected_result: "rust_adm_nonsponsored",
        selected_result_subtype: "",
        selected_position: 2,
        provider: "UrlbarProviderQuickSuggest",
        results: "search_engine,rust_adm_nonsponsored",
      },
    ]);
  });

  await cleanupQuickSuggest();
});
