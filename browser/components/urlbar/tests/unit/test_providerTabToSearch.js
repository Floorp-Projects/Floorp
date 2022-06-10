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
  // Disable tab-to-search onboarding results. Those are covered in
  // browser/components/urlbar/tests/browser/browser_tabToSearch.js.
  Services.prefs.setIntPref(
    "browser.urlbar.tabToSearch.onboard.interactionsLeft",
    0
  );
  await SearchTestUtils.installSearchExtension({ name: "Test" });
  testEngine = await Services.search.getEngineByName("Test");

  registerCleanupFunction(async () => {
    Services.prefs.clearUserPref(
      "browser.urlbar.tabToSearch.onboard.interactionsLeft"
    );
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
    hasAutofillTitle: true,
    matches: [
      makeVisitResult(context, {
        uri: "https://example.com/",
        title: "test visit for https://example.com/",
        heuristic: true,
        providerName: "Autofill",
      }),
      makeSearchResult(context, {
        engineName: testEngine.name,
        engineIconUri: UrlbarUtils.ICON.SEARCH_GLASS,
        uri: UrlbarUtils.stripPublicSuffixFromHost(
          testEngine.getResultDomain()
        ),
        providesSearchMode: true,
        query: "",
        providerName: "TabToSearch",
      }),
    ],
  });

  info("Repeat the search but with tab-to-search disabled through pref.");
  Services.prefs.setBoolPref("browser.urlbar.suggest.engines", false);
  await check_results({
    context,
    autofilled: "example.com/",
    completed: "https://example.com/",
    hasAutofillTitle: true,
    matches: [
      makeVisitResult(context, {
        uri: "https://example.com/",
        title: "test visit for https://example.com/",
        heuristic: true,
        providerName: "Autofill",
      }),
    ],
  });
  Services.prefs.clearUserPref("browser.urlbar.suggest.engines");

  await cleanupPlaces();
});

// Tests that tab-to-search results are shown when the typed string matches an
// engine domain even when there is no autofill.
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
      makeSearchResult(context, {
        engineName: testEngine.name,
        engineIconUri: UrlbarUtils.ICON.SEARCH_GLASS,
        uri: UrlbarUtils.stripPublicSuffixFromHost(
          testEngine.getResultDomain()
        ),
        providesSearchMode: true,
        query: "",
        providerName: "TabToSearch",
      }),
    ],
  });
});

// Tests that tab-to-search results are not shown when the typed string matches
// an engine domain, but something else is being autofilled.
add_task(async function autofillDoesNotMatchEngine() {
  await PlacesTestUtils.addVisits(["https://example.test.ca/"]);
  let context = createContext("example", { isPrivate: false });
  await check_results({
    context,
    autofilled: "example.test.ca/",
    completed: "https://example.test.ca/",
    hasAutofillTitle: true,
    matches: [
      makeVisitResult(context, {
        uri: "https://example.test.ca/",
        title: "test visit for https://example.test.ca/",
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
    hasAutofillTitle: true,
    matches: [
      makeVisitResult(context, {
        uri: "https://www.example.com/",
        title: "test visit for https://www.example.com/",
        heuristic: true,
        providerName: "Autofill",
      }),
      makeSearchResult(context, {
        engineName: testEngine.name,
        engineIconUri: UrlbarUtils.ICON.SEARCH_GLASS,
        uri: UrlbarUtils.stripPublicSuffixFromHost(
          testEngine.getResultDomain()
        ),
        providesSearchMode: true,
        query: "",
        providerName: "TabToSearch",
      }),
    ],
  });
  await cleanupPlaces();

  // The engine has www., the history result does not.
  await PlacesTestUtils.addVisits(["https://foo.bar/"]);
  let extension = await SearchTestUtils.installSearchExtension(
    {
      name: "TestWww",
      search_url: "https://www.foo.bar/",
    },
    true
  );
  let wwwTestEngine = Services.search.getEngineByName("TestWww");
  context = createContext("foo", { isPrivate: false });
  await check_results({
    context,
    autofilled: "foo.bar/",
    completed: "https://foo.bar/",
    hasAutofillTitle: true,
    matches: [
      makeVisitResult(context, {
        uri: "https://foo.bar/",
        title: "test visit for https://foo.bar/",
        heuristic: true,
        providerName: "Autofill",
      }),
      makeSearchResult(context, {
        engineName: wwwTestEngine.name,
        engineIconUri: UrlbarUtils.ICON.SEARCH_GLASS,
        uri: UrlbarUtils.stripPublicSuffixFromHost(
          wwwTestEngine.getResultDomain()
        ),
        providesSearchMode: true,
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
    hasAutofillTitle: true,
    matches: [
      makeVisitResult(context, {
        uri: "https://www.foo.bar/",
        title: "test visit for https://www.foo.bar/",
        heuristic: true,
        providerName: "Autofill",
      }),
      makeSearchResult(context, {
        engineName: wwwTestEngine.name,
        engineIconUri: UrlbarUtils.ICON.SEARCH_GLASS,
        uri: UrlbarUtils.stripPublicSuffixFromHost(
          wwwTestEngine.getResultDomain()
        ),
        providesSearchMode: true,
        query: "",
        providerName: "TabToSearch",
      }),
    ],
  });
  await cleanupPlaces();

  await extension.unload();
});

// Tests that when a user's query causes autofill to replace one engine's domain
// with another, the correct tab-to-search results are shown.
add_task(async function conflictingEngines() {
  for (let i = 0; i < 3; i++) {
    await PlacesTestUtils.addVisits([
      "https://foobar.com/",
      "https://foo.com/",
    ]);
  }
  let extension1 = await SearchTestUtils.installSearchExtension(
    {
      name: "TestFooBar",
      search_url: "https://foobar.com/",
    },
    true
  );
  let extension2 = await SearchTestUtils.installSearchExtension(
    {
      name: "TestFoo",
      search_url: "https://foo.com/",
    },
    true
  );
  let fooBarTestEngine = Services.search.getEngineByName("TestFooBar");
  let fooTestEngine = Services.search.getEngineByName("TestFoo");

  // Search for "foo", autofilling foo.com. Observe that the foo.com
  // tab-to-search result is shown, even though the foobar.com engine was added
  // first (and thus enginesForDomainPrefix puts it earlier in its returned
  // array.)
  let context = createContext("foo", { isPrivate: false });
  await check_results({
    context,
    autofilled: "foo.com/",
    completed: "https://foo.com/",
    hasAutofillTitle: true,
    matches: [
      makeVisitResult(context, {
        uri: "https://foo.com/",
        title: "test visit for https://foo.com/",
        heuristic: true,
        providerName: "Autofill",
      }),
      makeSearchResult(context, {
        engineName: fooTestEngine.name,
        engineIconUri: UrlbarUtils.ICON.SEARCH_GLASS,
        uri: UrlbarUtils.stripPublicSuffixFromHost(
          fooTestEngine.getResultDomain()
        ),
        providesSearchMode: true,
        query: "",
        providerName: "TabToSearch",
      }),
      makeVisitResult(context, {
        uri: "https://foobar.com/",
        title: "test visit for https://foobar.com/",
        providerName: "Places",
      }),
    ],
  });

  // Search for "foob", autofilling foobar.com. Observe that the foo.com
  // tab-to-search result is replaced with the foobar.com tab-to-search result.
  context = createContext("foob", { isPrivate: false });
  await check_results({
    context,
    autofilled: "foobar.com/",
    completed: "https://foobar.com/",
    hasAutofillTitle: true,
    matches: [
      makeVisitResult(context, {
        uri: "https://foobar.com/",
        title: "test visit for https://foobar.com/",
        heuristic: true,
        providerName: "Autofill",
      }),
      makeSearchResult(context, {
        engineName: fooBarTestEngine.name,
        engineIconUri: UrlbarUtils.ICON.SEARCH_GLASS,
        uri: UrlbarUtils.stripPublicSuffixFromHost(
          fooBarTestEngine.getResultDomain()
        ),
        providesSearchMode: true,
        query: "",
        providerName: "TabToSearch",
      }),
    ],
  });

  await cleanupPlaces();
  await extension1.unload();
  await extension2.unload();
});

add_task(async function multipleEnginesForHostname() {
  info(
    "In case of multiple engines only one tab-to-search result should be returned"
  );
  let extension = await SearchTestUtils.installSearchExtension(
    {
      name: "TestMaps",
      search_url: "https://example.com/maps/",
    },
    true
  );

  let context = createContext("examp", { isPrivate: false });
  let maxResultCount = UrlbarPrefs.get("maxRichResults");

  // Add enough visits to autofill example.com.
  for (let i = 0; i < maxResultCount; i++) {
    await PlacesTestUtils.addVisits("https://example.com/");
  }

  // Add enough visits to other URLs matching our query to fill up the list of
  // results.
  let otherVisitResults = [];
  for (let i = 0; i < maxResultCount; i++) {
    let url = "https://mochi.test:8888/example/" + i;
    await PlacesTestUtils.addVisits(url);
    otherVisitResults.unshift(
      makeVisitResult(context, {
        uri: url,
        title: "test visit for " + url,
      })
    );
  }

  await check_results({
    context,
    autofilled: "example.com/",
    completed: "https://example.com/",
    hasAutofillTitle: true,
    matches: [
      makeVisitResult(context, {
        uri: "https://example.com/",
        title: "test visit for https://example.com/",
        heuristic: true,
        providerName: "Autofill",
      }),
      makeSearchResult(context, {
        engineName: testEngine.name,
        engineIconUri: UrlbarUtils.ICON.SEARCH_GLASS,
        uri: UrlbarUtils.stripPublicSuffixFromHost(
          testEngine.getResultDomain()
        ),
        providesSearchMode: true,
        query: "",
        providerName: "TabToSearch",
      }),
      // There should be `maxResultCount` - 2 other visit results. If this fails
      // because there are actually `maxResultCount` - 3 other results, then the
      // muxer is improperly including both TabToSearch results in its
      // calculation of the total available result span instead of only one, so
      // one fewer visit result appears than expected.
      ...otherVisitResults.slice(0, maxResultCount - 2),
    ],
  });
  await cleanupPlaces();
  await extension.unload();
});

add_task(async function test_casing() {
  info("Tab-to-search results appear also in case of different casing.");
  await PlacesTestUtils.addVisits(["https://example.com/"]);
  let context = createContext("eXAm", { isPrivate: false });
  await check_results({
    context,
    autofilled: "eXAmple.com/",
    completed: "https://example.com/",
    hasAutofillTitle: true,
    matches: [
      makeVisitResult(context, {
        uri: "https://example.com/",
        title: "test visit for https://example.com/",
        heuristic: true,
        providerName: "Autofill",
      }),
      makeSearchResult(context, {
        engineName: testEngine.name,
        engineIconUri: UrlbarUtils.ICON.SEARCH_GLASS,
        uri: UrlbarUtils.stripPublicSuffixFromHost(
          testEngine.getResultDomain()
        ),
        providesSearchMode: true,
        query: "",
        providerName: "TabToSearch",
      }),
    ],
  });
  await cleanupPlaces();
});

add_task(async function test_publicSuffix() {
  info("Tab-to-search results appear also in case of partial host match.");
  let extension = await SearchTestUtils.installSearchExtension(
    {
      name: "MyTest",
      search_url: "https://test.mytest.it/",
    },
    true
  );
  let engine = Services.search.getEngineByName("MyTest");
  await PlacesTestUtils.addVisits(["https://test.mytest.it/"]);
  let context = createContext("my", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: Services.search.defaultEngine.name,
        engineIconUri: Services.search.defaultEngine.iconURI?.spec,
        heuristic: true,
        providerName: "HeuristicFallback",
      }),
      makeSearchResult(context, {
        engineName: engine.name,
        engineIconUri: UrlbarUtils.ICON.SEARCH_GLASS,
        uri: UrlbarUtils.stripPublicSuffixFromHost(engine.getResultDomain()),
        providesSearchMode: true,
        query: "",
        providerName: "TabToSearch",
        satisfiesAutofillThreshold: true,
      }),
      makeVisitResult(context, {
        uri: "https://test.mytest.it/",
        title: "test visit for https://test.mytest.it/",
        providerName: "Places",
      }),
    ],
  });
  await cleanupPlaces();
  await extension.unload();
});

add_task(async function test_publicSuffixIsHost() {
  info("Tab-to-search results does not appear in case we autofill a suffix.");
  let extension = await SearchTestUtils.installSearchExtension(
    {
      name: "SuffixTest",
      search_url: "https://somesuffix.com.mx/",
    },
    true
  );

  // The top level domain will be autofilled, not the full domain.
  await PlacesTestUtils.addVisits(["https://com.mx/"]);
  let context = createContext("co", { isPrivate: false });
  await check_results({
    context,
    autofilled: "com.mx/",
    completed: "https://com.mx/",
    hasAutofillTitle: true,
    matches: [
      makeVisitResult(context, {
        uri: "https://com.mx/",
        title: "test visit for https://com.mx/",
        heuristic: true,
        providerName: "Autofill",
      }),
    ],
  });
  await cleanupPlaces();
  await extension.unload();
});

add_task(async function test_disabledEngine() {
  info("Tab-to-search results does not appear for a Pref-disabled engine.");
  let extension = await SearchTestUtils.installSearchExtension(
    {
      name: "Disabled",
      search_url: "https://disabled.com/",
    },
    true
  );
  let engine = Services.search.getEngineByName("Disabled");
  await PlacesTestUtils.addVisits(["https://disabled.com/"]);
  let context = createContext("dis", { isPrivate: false });

  info("Sanity check that the engine would appear.");
  await check_results({
    context,
    autofilled: "disabled.com/",
    completed: "https://disabled.com/",
    hasAutofillTitle: true,
    matches: [
      makeVisitResult(context, {
        uri: "https://disabled.com/",
        title: "test visit for https://disabled.com/",
        heuristic: true,
        providerName: "Autofill",
      }),
      makeSearchResult(context, {
        engineName: engine.name,
        engineIconUri: UrlbarUtils.ICON.SEARCH_GLASS,
        uri: UrlbarUtils.stripPublicSuffixFromHost(engine.getResultDomain()),
        providesSearchMode: true,
        query: "",
        providerName: "TabToSearch",
      }),
    ],
  });

  info("Now disable the engine.");
  Services.prefs.setCharPref("browser.search.hiddenOneOffs", engine.name);
  await check_results({
    context,
    autofilled: "disabled.com/",
    completed: "https://disabled.com/",
    hasAutofillTitle: true,
    matches: [
      makeVisitResult(context, {
        uri: "https://disabled.com/",
        title: "test visit for https://disabled.com/",
        heuristic: true,
        providerName: "Autofill",
      }),
    ],
  });
  Services.prefs.clearUserPref("browser.search.hiddenOneOffs");

  await cleanupPlaces();
  await extension.unload();
});
