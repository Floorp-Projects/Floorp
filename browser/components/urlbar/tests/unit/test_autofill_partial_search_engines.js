/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

// Search engine origins are autofilled normally when they get over the
// threshold, though certain origins redirect to localized subdomains, that
// the user is unlikely to type, for example wikipedia.org => en.wikipedia.org.
// This checks that we can autofill the search engine origin even if the user
// types just "wiki".

"use strict";

add_task(async function setup() {
  Services.prefs.setBoolPref("browser.urlbar.suggest.searches", false);
  // Enable tab-to-search.
  Services.prefs.setBoolPref("browser.urlbar.update2", true);
  Services.prefs.setBoolPref("browser.urlbar.update2.tabToComplete", true);
  // Disable tab-to-search onboarding results.
  Services.prefs.setIntPref("browser.urlbar.tabToSearch.onboard.maxShown", 0);
  Services.prefs.setBoolPref(
    "browser.search.separatePrivateDefault.ui.enabled",
    false
  );

  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("browser.urlbar.suggest.searches");
    Services.prefs.clearUserPref(
      "browser.search.separatePrivateDefault.ui.enabled"
    );
    Services.prefs.clearUserPref("browser.urlbar.update2");
    Services.prefs.clearUserPref("browser.urlbar.update2.tabToComplete");
    Services.prefs.clearUserPref("browser.urlbar.tabToSearch.onboard.maxShown");
  });

  let url = "https://en.example.com/";
  let engine = await Services.search.addEngineWithDetails("TestEngine", {
    method: "GET",
    template: url,
    searchGetParams: "q={searchTerms}",
  });
  registerCleanupFunction(async () => {
    await Services.search.removeEngine(engine);
  });
  // Make sure the engine domain would be autofilled.
  await PlacesUtils.bookmarks.insert({
    url,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
  });

  info("Test matching cases");

  for (let searchStr of ["e", "en", "ex", "example.com"]) {
    info("Searching for " + searchStr);
    let context = createContext(searchStr, { isPrivate: false });
    let [title] = UrlbarUtils.stripPrefixAndTrim(url, {
      stripHttp: true,
      trimEmptyQuery: true,
      trimSlash: true,
    });
    await check_results({
      context,
      autofilled:
        searchStr + url.substring(url.indexOf(searchStr) + searchStr.length),
      completed: url,
      matches: [
        makeVisitResult(context, {
          uri: url,
          title,
          heuristic: true,
        }),
        makeSearchResult(context, {
          engineName: engine.name,
          engineIconUri: UrlbarUtils.ICON.SEARCH_GLASS_INVERTED,
          uri: "en.example.",
          keywordOffer: UrlbarUtils.KEYWORD_OFFER.SHOW,
          query: "",
          providerName: "TabToSearch",
        }),
      ],
    });
  }

  info("Test a www engine");
  let url2 = "https://www.it.mochi.com/";
  let engine2 = await Services.search.addEngineWithDetails("TestEngine2", {
    method: "GET",
    template: url2,
    searchGetParams: "q={searchTerms}",
  });
  registerCleanupFunction(async () => {
    await Services.search.removeEngine(engine2);
  });
  // Make sure the engine domain would be autofilled.
  await PlacesUtils.bookmarks.insert({
    url: url2,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
  });

  for (let searchStr of ["mo", "it", "it.m", "www.it"]) {
    info("Searching for " + searchStr);
    let context = createContext(searchStr, { isPrivate: false });
    let [title] = UrlbarUtils.stripPrefixAndTrim(url2, {
      stripHttp: true,
      trimEmptyQuery: true,
      trimSlash: true,
    });
    await check_results({
      context,
      autofilled:
        searchStr + url2.substring(url2.indexOf(searchStr) + searchStr.length),
      completed: url2,
      matches: [
        makeVisitResult(context, {
          uri: url2,
          title,
          heuristic: true,
        }),
        makeSearchResult(context, {
          engineName: engine2.name,
          engineIconUri: UrlbarUtils.ICON.SEARCH_GLASS_INVERTED,
          uri: "www.it.mochi.",
          keywordOffer: UrlbarUtils.KEYWORD_OFFER.SHOW,
          query: "",
          providerName: "TabToSearch",
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

  info("Restricting to history should not autofill our bookmark");
  let context = createContext("ex", {
    isPrivate: false,
    sources: [UrlbarUtils.RESULT_SOURCE.HISTORY],
  });
  let controller = UrlbarTestUtils.newMockController();
  await UrlbarProvidersManager.startQuery(context, controller);
  Assert.ok(context.results[0].heuristic, "Check heuristic result");
  Assert.notEqual(context.results[0].providerName, "Autofill");
});
