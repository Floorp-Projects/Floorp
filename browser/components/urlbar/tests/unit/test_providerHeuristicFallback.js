/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that visit-url and search engine heuristic results are returned by
 * UrlbarProviderHeuristicFallback.
 */

const ENGINE_NAME = "engine-suggestions.xml";
const SUGGEST_PREF = "browser.urlbar.suggest.searches";
const SUGGEST_ENABLED_PREF = "browser.search.suggest.enabled";
const PRIVATE_SEARCH_PREF = "browser.search.separatePrivateDefault.ui.enabled";

add_task(async function setup() {
  // Install a test engine so we're sure of ENGINE_NAME.
  let engine = await addTestSuggestionsEngine();

  // Install the test engine.
  let oldDefaultEngine = await Services.search.getDefault();
  registerCleanupFunction(async () => {
    Services.search.setDefault(oldDefaultEngine);
    Services.prefs.clearUserPref(SUGGEST_PREF);
    Services.prefs.clearUserPref(SUGGEST_ENABLED_PREF);
    Services.prefs.clearUserPref(PRIVATE_SEARCH_PREF);
    Services.prefs.clearUserPref("keyword.enabled");
    Services.prefs.clearUserPref("browser.urlbar.update2");
  });
  Services.search.setDefault(engine);
  Services.prefs.setBoolPref(SUGGEST_PREF, false);
  Services.prefs.setBoolPref(SUGGEST_ENABLED_PREF, false);
  Services.prefs.setBoolPref(PRIVATE_SEARCH_PREF, false);
});

add_task(async function() {
  info("visit url, no protocol");
  let query = "mozilla.org";
  let context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        uri: `http://${query}/`,
        title: `http://${query}/`,
        heuristic: true,
      }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
      }),
    ],
  });

  info("visit url, no protocol but with 2 dots");
  query = "www.mozilla.org";
  context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        uri: `http://${query}/`,
        title: `http://${query}/`,
        heuristic: true,
      }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
      }),
    ],
  });

  info("visit url, no protocol, e-mail like");
  query = "a@b.com";
  context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        uri: `http://${query}/`,
        title: `http://${query}/`,
        heuristic: true,
      }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
      }),
    ],
  });

  info("visit url, with protocol but with 2 dots");
  query = "https://www.mozilla.org";
  context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        uri: `${query}/`,
        title: `${query}/`,
        heuristic: true,
      }),
    ],
  });

  // info("visit url, with protocol but with 3 dots");
  query = "https://www.mozilla.org.tw";
  context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        uri: `${query}/`,
        title: `${query}/`,
        heuristic: true,
      }),
    ],
  });

  info("visit url, with protocol");
  query = "https://mozilla.org";
  context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        uri: `${query}/`,
        title: `${query}/`,
        heuristic: true,
      }),
    ],
  });

  info("visit url, about: protocol (no host)");
  query = "about:nonexistent";
  context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        uri: query,
        title: query,
        heuristic: true,
      }),
    ],
  });

  info("visit url, with non-standard whitespace");
  query = "https://mozilla.org";
  context = createContext(`${query}\u2028`, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        uri: `${query}/`,
        title: `${query}/`,
        heuristic: true,
      }),
    ],
  });

  // This is distinct because of how we predict being able to url autofill via
  // host lookups.
  info("visit url, host matching visited host but not visited url");
  await PlacesTestUtils.addVisits([
    {
      uri: Services.io.newURI("http://mozilla.org/wine/"),
      title: "Mozilla Wine",
      transition: PlacesUtils.history.TRANSITION_TYPED,
    },
  ]);
  query = "mozilla.org/rum";
  context = createContext(`${query}\u2028`, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        uri: `http://${query}`,
        title: `http://${query}`,
        iconUri: "page-icon:http://mozilla.org/",
        heuristic: true,
      }),
    ],
  });
  await PlacesUtils.history.clear();

  // And hosts with no dot in them are special, due to requiring safelisting.
  info("unknown host");
  query = "firefox";
  context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        heuristic: true,
      }),
    ],
  });

  info("string with known host");
  query = "firefox/get";
  context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        heuristic: true,
      }),
    ],
  });

  Services.prefs.setBoolPref("browser.fixup.domainwhitelist.firefox", true);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("browser.fixup.domainwhitelist.firefox");
  });

  info("known host");
  query = "firefox";
  context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        uri: `http://${query}/`,
        title: `http://${query}/`,
        heuristic: true,
      }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
      }),
    ],
  });

  info("url with known host");
  query = "firefox/get";
  context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        uri: `http://${query}`,
        title: `http://${query}`,
        iconUri: "page-icon:http://firefox/",
        heuristic: true,
      }),
    ],
  });

  info("visit url, host matching visited host but not visited url, known host");
  Services.prefs.setBoolPref("browser.fixup.domainwhitelist.mozilla", true);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("browser.fixup.domainwhitelist.mozilla");
  });
  query = "mozilla/rum";
  context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        uri: `http://${query}`,
        title: `http://${query}`,
        iconUri: "page-icon:http://mozilla/",
        heuristic: true,
      }),
    ],
  });

  // ipv4 and ipv6 literal addresses should offer to visit.
  info("visit url, ipv4 literal");
  query = "127.0.0.1";
  context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        uri: `http://${query}/`,
        title: `http://${query}/`,
        heuristic: true,
      }),
    ],
  });

  info("visit url, ipv6 literal");
  query = "[2001:db8::1]";
  context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        uri: `http://${query}/`,
        title: `http://${query}/`,
        heuristic: true,
      }),
    ],
  });

  // Setting keyword.enabled to false should always try to visit.
  let keywordEnabled = Services.prefs.getBoolPref("keyword.enabled");
  Services.prefs.setBoolPref("keyword.enabled", false);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("keyword.enabled");
  });
  info("visit url, keyword.enabled = false");
  query = "bacon";
  context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        uri: `http://${query}/`,
        title: `http://${query}/`,
        heuristic: true,
      }),
    ],
  });

  info("visit two word query, keyword.enabled = false");
  query = "bacon lovers";
  context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        uri: query,
        title: query,
        heuristic: true,
      }),
    ],
  });

  info("Forced search through a restriction token, keyword.enabled = false");
  query = "?bacon";
  context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        heuristic: true,
        query: "bacon",
      }),
    ],
  });

  Services.prefs.setBoolPref("keyword.enabled", true);
  info("visit two word query, keyword.enabled = true");
  query = "bacon lovers";
  context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        heuristic: true,
      }),
    ],
  });

  Services.prefs.setBoolPref("keyword.enabled", keywordEnabled);

  info("visit url, scheme+host");
  query = "http://example";
  context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        uri: `${query}/`,
        title: `${query}/`,
        heuristic: true,
      }),
    ],
  });

  info("visit url, scheme+host");
  query = "ftp://example";
  context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        uri: `${query}/`,
        title: `${query}/`,
        heuristic: true,
      }),
    ],
  });

  info("visit url, host+port");
  query = "example:8080";
  context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        uri: `http://${query}/`,
        title: `http://${query}/`,
        heuristic: true,
      }),
    ],
  });

  info("numerical operations that look like urls should search");
  query = "123/12";
  context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        heuristic: true,
      }),
    ],
  });

  info("numerical operations that look like urls should search");
  query = "123.12/12.1";
  context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        heuristic: true,
      }),
    ],
  });

  query = "resource:///modules";
  context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        uri: query,
        title: query,
        heuristic: true,
      }),
    ],
  });

  info("access resource://app/modules");
  query = "resource://app/modules";
  context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        uri: query,
        title: query,
        heuristic: true,
      }),
    ],
  });

  info("protocol with an extra slash");
  query = "http:///";
  context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        heuristic: true,
      }),
    ],
  });

  info("change default engine");
  let originalTestEngine = Services.search.getEngineByName(ENGINE_NAME);
  let engine2 = await Services.search.addEngineWithDetails("AliasEngine", {
    alias: "alias",
    method: "GET",
    template: "http://example.com/?q={searchTerms}",
  });
  Assert.notEqual(
    Services.search.defaultEngine,
    engine2,
    "New engine shouldn't be the current engine yet"
  );
  await Services.search.setDefault(engine2);
  query = "toronto";
  context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: "AliasEngine",
        heuristic: true,
      }),
    ],
  });
  await Services.search.setDefault(originalTestEngine);

  Services.prefs.setBoolPref("browser.urlbar.update2", false);
  info(
    "With update2 disabled, leading restriction tokens are not removed from the search result, apart from the search token."
  );
  // Note that we use the alias from AliasEngine in the query. Since we're using
  // a restriction token, we expect that the default engine be used.
  for (let token of Object.values(UrlbarTokenizer.RESTRICT)) {
    for (query of [`${token} alias query`, `query ${token}`]) {
      let expectedQuery =
        token == UrlbarTokenizer.RESTRICT.SEARCH &&
        query.startsWith(UrlbarTokenizer.RESTRICT.SEARCH)
          ? query.substring(2)
          : query;
      context = createContext(query, { isPrivate: false });
      info(`Searching for "${query}", expecting "${expectedQuery}"`);
      await check_results({
        context,
        matches: [
          makeSearchResult(context, {
            engineName: ENGINE_NAME,
            query: expectedQuery,
            heuristic: true,
          }),
        ],
      });
    }
  }
  Services.prefs.clearUserPref("browser.urlbar.update2");

  Services.prefs.setBoolPref("browser.urlbar.update2", true);
  info(
    "Leading search-mode restriction tokens are removed from the search result."
  );
  for (let token of UrlbarTokenizer.SEARCH_MODE_RESTRICT) {
    query = `${token} query`;
    let expectedQuery = query.substring(2);
    context = createContext(query, { isPrivate: false });
    info(`Searching for "${query}", expecting "${expectedQuery}"`);
    let payload = {
      source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
      heuristic: true,
      query: expectedQuery,
      alias: token,
    };
    if (token == UrlbarTokenizer.RESTRICT.SEARCH) {
      payload.source = UrlbarUtils.RESULT_SOURCE.SEARCH;
      payload.engineName = ENGINE_NAME;
    }
    await check_results({
      context,
      matches: [makeSearchResult(context, payload)],
    });
  }

  info(
    "Leading search-mode restriction tokens are removed from the search result with keyword.enabled = false."
  );
  Services.prefs.setBoolPref("keyword.enabled", false);
  for (let token of UrlbarTokenizer.SEARCH_MODE_RESTRICT) {
    query = `${token} query`;
    let expectedQuery = query.substring(2);
    context = createContext(query, { isPrivate: false });
    info(`Searching for "${query}", expecting "${expectedQuery}"`);
    let payload = {
      source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
      heuristic: true,
      query: expectedQuery,
      alias: token,
    };
    if (token == UrlbarTokenizer.RESTRICT.SEARCH) {
      payload.source = UrlbarUtils.RESULT_SOURCE.SEARCH;
      payload.engineName = ENGINE_NAME;
    }
    await check_results({
      context,
      matches: [makeSearchResult(context, payload)],
    });
  }
  Services.prefs.clearUserPref("keyword.enabled");

  info(
    "Leading non-search-mode restriction tokens are not removed from the search result."
  );
  for (let token of Object.values(UrlbarTokenizer.RESTRICT)) {
    if (UrlbarTokenizer.SEARCH_MODE_RESTRICT.has(token)) {
      continue;
    }
    query = `${token} query`;
    let expectedQuery = query;
    context = createContext(query, { isPrivate: false });
    info(`Searching for "${query}", expecting "${expectedQuery}"`);
    await check_results({
      context,
      matches: [
        makeSearchResult(context, {
          heuristic: true,
          query: expectedQuery,
          engineName: ENGINE_NAME,
        }),
      ],
    });
  }
  Services.prefs.clearUserPref("browser.urlbar.update2");

  await Services.search.removeEngine(engine2);
});
