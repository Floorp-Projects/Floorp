/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from head.js */

ChromeUtils.defineESModuleGetters(this, {
  UrlbarProviderClipboard:
    "resource:///modules/UrlbarProviderClipboard.sys.mjs",
});

async function doHeuristicsTest({ trigger, assert }) {
  await doTest(async browser => {
    await openPopup("x");

    await trigger();
    await assert();
  });
}

async function doAdaptiveHistoryTest({ trigger, assert }) {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.autoFill", false]],
  });

  await doTest(async browser => {
    await PlacesTestUtils.addVisits(["https://example.com/test"]);
    await UrlbarUtils.addToInputHistory("https://example.com/test", "examp");

    await openPopup("exa");
    await selectRowByURL("https://example.com/test");

    await trigger();
    await assert();
  });

  await SpecialPowers.popPrefEnv();
}

async function doSearchHistoryTest({ trigger, assert }) {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.suggest.searches", true],
      ["browser.urlbar.maxHistoricalSearchSuggestions", 2],
    ],
  });

  await doTest(async browser => {
    await UrlbarTestUtils.formHistory.add(["foofoo", "foobar"]);

    await openPopup("foo");
    await selectRowByURL("http://mochi.test:8888/?terms=foofoo");

    await trigger();
    await assert();
  });

  await SpecialPowers.popPrefEnv();
}

async function doRecentSearchTest({ trigger, assert }) {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.recentsearches.featureGate", true]],
  });

  await doTest(async browser => {
    await UrlbarTestUtils.formHistory.add([
      { value: "foofoo", source: Services.search.defaultEngine.name },
    ]);

    await openPopup("");
    await selectRowByURL("http://mochi.test:8888/?terms=foofoo");

    await trigger();
    await assert();
  });

  await SpecialPowers.popPrefEnv();
}

async function doSearchSuggestTest({ trigger, assert }) {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.suggest.searches", true],
      ["browser.urlbar.maxHistoricalSearchSuggestions", 2],
    ],
  });

  await doTest(async browser => {
    await openPopup("foo");
    await selectRowByURL("http://mochi.test:8888/?terms=foofoo");

    await trigger();
    await assert();
  });

  await SpecialPowers.popPrefEnv();
}

async function doTailSearchSuggestTest({ trigger, assert }) {
  const cleanup = await _useTailSuggestionsEngine();

  await doTest(async browser => {
    await openPopup("hello");
    await selectRowByProvider("SearchSuggestions");

    await trigger();
    await assert();
  });

  await cleanup();
}

async function doTopPickTest({ trigger, assert }) {
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

  await doTest(async browser => {
    await openPopup("navigational");
    await selectRowByURL("https://example.com/navigational-suggestion");

    await trigger();
    await assert();
  });

  await cleanupQuickSuggest();
}

async function doTopSiteTest({ trigger, assert }) {
  await doTest(async browser => {
    await addTopSites("https://example.com/");

    await showResultByArrowDown();
    await selectRowByURL("https://example.com/");

    await trigger();
    await assert();
  });
}

async function doClipboardTest({ trigger, assert }) {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.clipboard.featureGate", true],
      ["browser.urlbar.suggest.clipboard", true],
    ],
  });
  SpecialPowers.clipboardCopyString("https://example.com/clipboard");
  await doTest(async browser => {
    await showResultByArrowDown();
    await selectRowByURL("https://example.com/clipboard");

    await trigger();
    await assert();
  });
  SpecialPowers.clipboardCopyString("");
  UrlbarProviderClipboard.setPreviousClipboardValue("");
  await SpecialPowers.popPrefEnv();
}

async function doRemoteTabTest({ trigger, assert }) {
  const remoteTab = await loadRemoteTab("https://example.com");

  await doTest(async browser => {
    await openPopup("example");
    await selectRowByProvider("RemoteTabs");

    await trigger();
    await assert();
  });

  await remoteTab.unload();
}

async function doAddonTest({ trigger, assert }) {
  const addon = loadOmniboxAddon({ keyword: "omni" });
  await addon.startup();

  await doTest(async browser => {
    await openPopup("omni test");

    await trigger();
    await assert();
  });

  await addon.unload();
}

async function doGeneralBookmarkTest({ trigger, assert }) {
  await doTest(async browser => {
    await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.unfiledGuid,
      url: "https://example.com/bookmark",
      title: "bookmark",
    });

    await openPopup("bookmark");
    await selectRowByURL("https://example.com/bookmark");

    await trigger();
    await assert();
  });
}

async function doGeneralHistoryTest({ trigger, assert }) {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.autoFill", false]],
  });

  await doTest(async browser => {
    await PlacesTestUtils.addVisits("https://example.com/test");

    await openPopup("example");
    await selectRowByURL("https://example.com/test");

    await trigger();
    await assert();
  });

  await SpecialPowers.popPrefEnv();
}

async function doSuggestTest({ trigger, assert }) {
  const cleanupQuickSuggest = await ensureQuickSuggestInit();

  await doTest(async browser => {
    await openPopup("nonsponsored");
    await selectRowByURL("https://example.com/nonsponsored");

    await trigger();
    await assert();
  });

  await cleanupQuickSuggest();
}

async function doAboutPageTest({ trigger, assert }) {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.maxRichResults", 3]],
  });

  await doTest(async browser => {
    await openPopup("about:");
    await selectRowByURL("about:robots");

    await trigger();
    await assert();
  });

  await SpecialPowers.popPrefEnv();
}

async function doSuggestedIndexTest({ trigger, assert }) {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.unitConversion.enabled", true]],
  });

  await doTest(async browser => {
    await openPopup("1m to cm");
    await selectRowByProvider("UnitConversion");

    await trigger();
    await assert();
  });

  await SpecialPowers.popPrefEnv();
}

/**
 * Creates a search engine that returns tail suggestions and sets it as the
 * default engine.
 *
 * @returns {Function}
 *   A cleanup function that will revert the default search engine and stop http
 *   server.
 */
async function _useTailSuggestionsEngine() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.search.suggest.enabled", true],
      ["browser.urlbar.suggest.searches", true],
      ["browser.urlbar.richSuggestions.tail", true],
    ],
  });

  const engineName = "TailSuggestions";
  const httpServer = new HttpServer();
  httpServer.start(-1);
  httpServer.registerPathHandler("/suggest", (req, resp) => {
    const params = new URLSearchParams(req.queryString);
    const searchStr = params.get("q");
    const suggestions = [
      searchStr,
      [searchStr + "-tail"],
      [],
      {
        "google:suggestdetail": [{ t: "-tail", mp: "… " }],
      },
    ];
    resp.setHeader("Content-Type", "application/json", false);
    resp.write(JSON.stringify(suggestions));
  });

  await SearchTestUtils.installSearchExtension({
    name: engineName,
    search_url: `http://localhost:${httpServer.identity.primaryPort}/search`,
    suggest_url: `http://localhost:${httpServer.identity.primaryPort}/suggest`,
    suggest_url_get_params: "?q={searchTerms}",
    search_form: `http://localhost:${httpServer.identity.primaryPort}/search?q={searchTerms}`,
  });

  const tailEngine = Services.search.getEngineByName(engineName);
  const originalEngine = await Services.search.getDefault();
  Services.search.setDefault(
    tailEngine,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );

  return async () => {
    Services.search.setDefault(
      originalEngine,
      Ci.nsISearchService.CHANGE_REASON_UNKNOWN
    );
    httpServer.stop(() => {});
    await SpecialPowers.popPrefEnv();
  };
}
