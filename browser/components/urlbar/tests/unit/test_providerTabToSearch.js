/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests UrlbarProviderTabToSearch. See also
 * browser/components/urlbar/tests/browser/browser_tabToSearch.js
 */

"use strict";

let testEngine;

add_task(async function init() {
  // Disable search suggestions for a less verbose test.
  Services.prefs.setBoolPref("browser.search.suggest.enabled", false);
  // Enable tab-to-search.
  Services.prefs.setBoolPref("browser.urlbar.update2", true);
  Services.prefs.setBoolPref("browser.urlbar.update2.tabToComplete", true);
  testEngine = await Services.search.addEngineWithDetails("Test", {
    template: "https://example.com/?search={searchTerms}",
  });

  registerCleanupFunction(async () => {
    await Services.search.removeEngine(testEngine);
    Services.prefs.clearUserPref("browser.urlbar.update2");
    Services.prefs.clearUserPref("browser.urlbar.update2.tabToComplete");
    Services.prefs.clearUserPref("browser.search.suggest.enabled");
  });
});

// Tests that tab-to-search results appear when the engine's result domain is
// autofilled.
add_task(async function basic() {
  await PlacesTestUtils.addVisits(["https://example.com/"]);
  let context = createContext("examp", { isPrivate: false });
  await check_results({
    context,
    autofilled: "example.com/",
    completed: "https://example.com/",
    matches: [
      makeVisitResult(context, {
        uri: "https://example.com/",
        title: "https://example.com",
        heuristic: true,
        providerName: "Autofill",
      }),
      makeSearchResult(context, {
        engineName: testEngine.name,
        uri: testEngine.getResultDomain(),
        keywordOffer: UrlbarUtils.KEYWORD_OFFER.SHOW,
        query: "",
        providerName: "TabToSearch",
      }),
    ],
  });
  await cleanupPlaces();
});

// Tests that tab-to-search results aren't shown when the typed string matches
// an engine domain but there is no autofill.
add_task(async function noAutofill() {
  // Note we are not adding any history visits.
  let context = createContext("examp", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: Services.search.defaultEngine.name,
        engineIconUri: Services.search.defaultEngine.iconURI?.spec,
        heuristic: true,
        providerName: "HeuristicFallback",
      }),
    ],
  });
});

// Tests that tab-to-search results are not shown when the typed string matches
// an engine domain, but something else is being autofilled.
add_task(async function autofillDoesNotMatchEngine() {
  await PlacesTestUtils.addVisits(["https://example.ca/"]);
  let context = createContext("example.c", { isPrivate: false });
  await check_results({
    context,
    autofilled: "example.ca/",
    completed: "https://example.ca/",
    matches: [
      makeVisitResult(context, {
        uri: "https://example.ca/",
        title: "https://example.ca",
        heuristic: true,
        providerName: "Autofill",
      }),
    ],
  });

  await cleanupPlaces();
});

// Tests that www. is ignored for the purposes of matching autofill to
// tab-to-search.
add_task(async function ignoreWww() {
  // The history result has www., the engine does not.
  await PlacesTestUtils.addVisits(["https://www.example.com/"]);
  let context = createContext("www.examp", { isPrivate: false });
  await check_results({
    context,
    autofilled: "www.example.com/",
    completed: "https://www.example.com/",
    matches: [
      makeVisitResult(context, {
        uri: "https://www.example.com/",
        title: "https://www.example.com",
        heuristic: true,
        providerName: "Autofill",
      }),
      makeSearchResult(context, {
        engineName: testEngine.name,
        uri: testEngine.getResultDomain(),
        keywordOffer: UrlbarUtils.KEYWORD_OFFER.SHOW,
        query: "",
        providerName: "TabToSearch",
      }),
    ],
  });
  await cleanupPlaces();

  // The engine has www., the history result does not.
  await PlacesTestUtils.addVisits(["https://foo.bar/"]);
  let wwwTestEngine = await Services.search.addEngineWithDetails("TestWww", {
    template: "https://www.foo.bar/?search={searchTerms}",
  });
  context = createContext("foo", { isPrivate: false });
  await check_results({
    context,
    autofilled: "foo.bar/",
    completed: "https://foo.bar/",
    matches: [
      makeVisitResult(context, {
        uri: "https://foo.bar/",
        title: "https://foo.bar",
        heuristic: true,
        providerName: "Autofill",
      }),
      makeSearchResult(context, {
        engineName: wwwTestEngine.name,
        uri: wwwTestEngine.getResultDomain(),
        keywordOffer: UrlbarUtils.KEYWORD_OFFER.SHOW,
        query: "",
        providerName: "TabToSearch",
      }),
    ],
  });
  await cleanupPlaces();

  // Both the engine and the history result have www.
  await PlacesTestUtils.addVisits(["https://www.foo.bar/"]);
  context = createContext("foo", { isPrivate: false });
  await check_results({
    context,
    autofilled: "foo.bar/",
    completed: "https://www.foo.bar/",
    matches: [
      makeVisitResult(context, {
        uri: "https://www.foo.bar/",
        title: "https://www.foo.bar",
        heuristic: true,
        providerName: "Autofill",
      }),
      makeSearchResult(context, {
        engineName: wwwTestEngine.name,
        uri: wwwTestEngine.getResultDomain(),
        keywordOffer: UrlbarUtils.KEYWORD_OFFER.SHOW,
        query: "",
        providerName: "TabToSearch",
      }),
    ],
  });
  await cleanupPlaces();

  await Services.search.removeEngine(wwwTestEngine);
});
