/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that visit-url and search engine heuristic results are returned by
 * UrlbarProviderHeuristicFallback.
 */

const SUGGEST_PREF = "browser.urlbar.suggest.searches";
const QUICKACTIONS_PREF = "browser.urlbar.suggest.quickactions";
const SUGGEST_ENABLED_PREF = "browser.search.suggest.enabled";
const PRIVATE_SEARCH_PREF = "browser.search.separatePrivateDefault.ui.enabled";

// We make sure that restriction tokens and search terms are correctly
// recognized when they are separated by each of these different types of spaces
// and combinations of spaces.  U+3000 is the ideographic space in CJK and is
// commonly used by CJK speakers.
const TEST_SPACES = [" ", "\u3000", " \u3000", "\u3000 "];

add_task(async function setup() {
  // Install a test engine.
  let engine = await addTestSuggestionsEngine();

  let oldDefaultEngine = await Services.search.getDefault();
  registerCleanupFunction(async () => {
    Services.search.setDefault(oldDefaultEngine);
    Services.prefs.clearUserPref(SUGGEST_PREF);
    Services.prefs.clearUserPref(QUICKACTIONS_PREF);
    Services.prefs.clearUserPref(SUGGEST_ENABLED_PREF);
    Services.prefs.clearUserPref(PRIVATE_SEARCH_PREF);
    Services.prefs.clearUserPref("keyword.enabled");
  });
  Services.search.setDefault(engine);
  Services.prefs.setBoolPref(SUGGEST_PREF, false);
  Services.prefs.setBoolPref(QUICKACTIONS_PREF, false);
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
        engineName: SUGGESTIONS_ENGINE_NAME,
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
        engineName: SUGGESTIONS_ENGINE_NAME,
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
        engineName: SUGGESTIONS_ENGINE_NAME,
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
        engineName: SUGGESTIONS_ENGINE_NAME,
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
        engineName: SUGGESTIONS_ENGINE_NAME,
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
        engineName: SUGGESTIONS_ENGINE_NAME,
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
        engineName: SUGGESTIONS_ENGINE_NAME,
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
        engineName: SUGGESTIONS_ENGINE_NAME,
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
        engineName: SUGGESTIONS_ENGINE_NAME,
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
        engineName: SUGGESTIONS_ENGINE_NAME,
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
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
    ],
  });

  info("change default engine");
  let originalTestEngine = Services.search.getEngineByName(
    SUGGESTIONS_ENGINE_NAME
  );
  await SearchTestUtils.installSearchExtension({
    name: "AliasEngine",
    keyword: "alias",
  });
  let engine2 = Services.search.getEngineByName("AliasEngine");
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

  info(
    "Leading search-mode restriction tokens are removed from the search result."
  );
  for (let token of UrlbarTokenizer.SEARCH_MODE_RESTRICT) {
    for (let spaces of TEST_SPACES) {
      query = token + spaces + "query";
      info("Testing: " + JSON.stringify({ query, spaces: codePoints(spaces) }));
      let expectedQuery = query.substring(1).trimStart();
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
        payload.engineName = SUGGESTIONS_ENGINE_NAME;
      }
      await check_results({
        context,
        matches: [makeSearchResult(context, payload)],
      });
    }
  }

  info(
    "Leading search-mode restriction tokens are removed from the search result with keyword.enabled = false."
  );
  Services.prefs.setBoolPref("keyword.enabled", false);
  for (let token of UrlbarTokenizer.SEARCH_MODE_RESTRICT) {
    for (let spaces of TEST_SPACES) {
      query = token + spaces + "query";
      info("Testing: " + JSON.stringify({ query, spaces: codePoints(spaces) }));
      let expectedQuery = query.substring(1).trimStart();
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
        payload.engineName = SUGGESTIONS_ENGINE_NAME;
      }
      await check_results({
        context,
        matches: [makeSearchResult(context, payload)],
      });
    }
  }
  Services.prefs.clearUserPref("keyword.enabled");

  info(
    "Leading non-search-mode restriction tokens are not removed from the search result."
  );
  for (let token of Object.values(UrlbarTokenizer.RESTRICT)) {
    if (UrlbarTokenizer.SEARCH_MODE_RESTRICT.has(token)) {
      continue;
    }
    for (let spaces of TEST_SPACES) {
      query = token + spaces + "query";
      info("Testing: " + JSON.stringify({ query, spaces: codePoints(spaces) }));
      let expectedQuery = query;
      context = createContext(query, { isPrivate: false });
      info(`Searching for "${query}", expecting "${expectedQuery}"`);
      await check_results({
        context,
        matches: [
          makeSearchResult(context, {
            heuristic: true,
            query: expectedQuery,
            engineName: SUGGESTIONS_ENGINE_NAME,
          }),
        ],
      });
    }
  }

  info(
    "Test the format inputed is user@host, and the host is in domainwhitelist"
  );
  Services.prefs.setBoolPref("browser.fixup.domainwhitelist.test-host", true);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("browser.fixup.domainwhitelist.test-host");
  });

  query = "any@test-host";
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
        engineName: SUGGESTIONS_ENGINE_NAME,
      }),
    ],
  });

  info(
    "Test the format inputed is user@host, but the host is not in domainwhitelist"
  );
  query = "any@not-host";
  context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        heuristic: true,
        query,
        engineName: SUGGESTIONS_ENGINE_NAME,
      }),
    ],
  });

  info(
    "Test if the format of user:pass@host is handled as visit even if the host is not in domainwhitelist"
  );
  query = "user:pass@not-host";
  context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        uri: "http://user:pass@not-host/",
        title: "http://user:pass@not-host/",
        heuristic: true,
      }),
    ],
  });

  info("Test if the format of user@ipaddress is handled as visit");
  query = "user@192.168.0.1";
  context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        uri: "http://user@192.168.0.1/",
        title: "http://user@192.168.0.1/",
        heuristic: true,
      }),
      makeSearchResult(context, {
        heuristic: false,
        query,
        engineName: SUGGESTIONS_ENGINE_NAME,
      }),
    ],
  });
});

/**
 * Returns an array of code points in the given string.  Each code point is
 * returned as a hexidecimal string.
 *
 * @param {string} str
 *   The code points of this string will be returned.
 * @returns {array}
 *   Array of code points in the string, where each is a hexidecimal string.
 */
function codePoints(str) {
  return str.split("").map(s => s.charCodeAt(0).toString(16));
}
