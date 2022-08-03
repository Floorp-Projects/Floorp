/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

// Search engine origins are autofilled normally when they get over the
// threshold, though certain origins redirect to localized subdomains, that
// the user is unlikely to type, for example wikipedia.org => en.wikipedia.org.
// We should get a tab to search result also for these cases, where a normal
// autofill wouldn't happen.

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  UrlbarProviderAutofill: "resource:///modules/UrlbarProviderAutofill.sys.mjs",
});

add_task(async function setup() {
  Services.prefs.setBoolPref("browser.urlbar.suggest.searches", false);
  Services.prefs.setBoolPref("browser.urlbar.suggest.quickactions", false);
  // Disable tab-to-search onboarding results.
  Services.prefs.setIntPref(
    "browser.urlbar.tabToSearch.onboard.interactionsLeft",
    0
  );
  Services.prefs.setBoolPref(
    "browser.search.separatePrivateDefault.ui.enabled",
    false
  );

  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("browser.urlbar.suggest.searches");
    Services.prefs.clearUserPref("browser.urlbar.suggest.quickactions");
    Services.prefs.clearUserPref(
      "browser.search.separatePrivateDefault.ui.enabled"
    );
    Services.prefs.clearUserPref(
      "browser.urlbar.tabToSearch.onboard.interactionsLeft"
    );
  });
});

add_task(async function test() {
  let url = "https://en.example.com/";
  await SearchTestUtils.installSearchExtension({
    name: "TestEngine",
    search_url: url,
  });
  let engine = Services.search.getEngineByName("TestEngine");
  let defaultEngine = await Services.search.getDefault();
  await Services.search.setDefault(engine);
  registerCleanupFunction(async () => {
    await Services.search.setDefault(defaultEngine);
  });
  // Make sure the engine domain would be autofilled.
  await PlacesUtils.bookmarks.insert({
    url,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "bookmark",
  });

  info("Test matching cases");

  for (let searchStr of ["ex", "example.c"]) {
    info("Searching for " + searchStr);
    let context = createContext(searchStr, { isPrivate: false });
    await check_results({
      context,
      matches: [
        makeSearchResult(context, {
          engineName: Services.search.defaultEngine.name,
          providerName: "HeuristicFallback",
          heuristic: true,
        }),
        makeSearchResult(context, {
          engineName: engine.name,
          engineIconUri: UrlbarUtils.ICON.SEARCH_GLASS,
          uri: "en.example.",
          providesSearchMode: true,
          query: "",
          providerName: "TabToSearch",
          satisfiesAutofillThreshold: true,
        }),
        makeBookmarkResult(context, {
          uri: url,
          title: "bookmark",
        }),
      ],
    });
  }

  info("Test a www engine");
  let url2 = "https://www.it.mochi.com/";
  await SearchTestUtils.installSearchExtension({
    name: "TestEngine2",
    search_url: url2,
  });

  let engine2 = Services.search.getEngineByName("TestEngine2");
  // Make sure the engine domain would be autofilled.
  await PlacesUtils.bookmarks.insert({
    url: url2,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "bookmark",
  });

  for (let searchStr of ["mo", "mochi.c"]) {
    info("Searching for " + searchStr);
    let context = createContext(searchStr, { isPrivate: false });
    await check_results({
      context,
      matches: [
        makeSearchResult(context, {
          engineName: Services.search.defaultEngine.name,
          providerName: "HeuristicFallback",
          heuristic: true,
        }),
        makeSearchResult(context, {
          engineName: engine2.name,
          engineIconUri: UrlbarUtils.ICON.SEARCH_GLASS,
          uri: "www.it.mochi.",
          providesSearchMode: true,
          query: "",
          providerName: "TabToSearch",
          satisfiesAutofillThreshold: true,
        }),
        makeBookmarkResult(context, {
          uri: url2,
          title: "bookmark",
        }),
      ],
    });
  }

  info("Test non-matching cases");

  for (let searchStr of ["www.en", "www.ex", "https://ex"]) {
    info("Searching for " + searchStr);
    let context = createContext(searchStr, { isPrivate: false });
    // We don't want to generate all the possible results here, just check
    // the heuristic result is not autofill.
    let controller = UrlbarTestUtils.newMockController();
    await UrlbarProvidersManager.startQuery(context, controller);
    Assert.ok(context.results[0].heuristic, "Check heuristic result");
    Assert.notEqual(context.results[0].providerName, "Autofill");
  }

  info("Tab-to-search is not shown when an unrelated site is autofilled.");
  let wikiUrl = "https://wikipedia.org/";
  await SearchTestUtils.installSearchExtension({
    name: "FakeWikipedia",
    search_url: url,
  });
  let wikiEngine = Services.search.getEngineByName("TestEngine");

  // Make sure that wikiUrl will pass getTopHostOverThreshold.
  await PlacesUtils.bookmarks.insert({
    url: wikiUrl,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "Wikipedia",
  });

  // Make sure an unrelated www site is autofilled.
  let wwwUrl = "https://www.example.com";
  await PlacesUtils.bookmarks.insert({
    url: wwwUrl,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "Example",
  });

  let searchStr = "w";
  let context = createContext(searchStr, {
    isPrivate: false,
    sources: [UrlbarUtils.RESULT_SOURCE.BOOKMARKS],
  });
  let host = await UrlbarProviderAutofill.getTopHostOverThreshold(context, [
    wikiEngine.getResultDomain(),
  ]);
  Assert.equal(
    host,
    wikiEngine.getResultDomain(),
    "The search satisfies the autofill threshold requirement."
  );
  await check_results({
    context,
    autofilled: "www.example.com/",
    completed: "https://www.example.com/",
    hasAutofillTitle: true,
    matches: [
      makeVisitResult(context, {
        uri: `${wwwUrl}/`,
        title: "Example",
        heuristic: true,
        providerName: "Autofill",
      }),
      // Note that tab-to-search is not shown.
      makeBookmarkResult(context, {
        uri: wikiUrl,
        title: "Wikipedia",
      }),
      makeBookmarkResult(context, {
        uri: url2,
        title: "bookmark",
      }),
    ],
  });

  info("Restricting to history should not autofill our bookmark");
  context = createContext("ex", {
    isPrivate: false,
    sources: [UrlbarUtils.RESULT_SOURCE.HISTORY],
  });
  let controller = UrlbarTestUtils.newMockController();
  await UrlbarProvidersManager.startQuery(context, controller);
  Assert.ok(context.results[0].heuristic, "Check heuristic result");
  Assert.notEqual(context.results[0].providerName, "Autofill");

  await cleanupPlaces();
});
