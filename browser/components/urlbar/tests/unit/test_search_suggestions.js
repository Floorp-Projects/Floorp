/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that search engine suggestions are returned by
 * UrlbarProviderSearchSuggestions.
 */

const { FormHistory } = ChromeUtils.import(
  "resource://gre/modules/FormHistory.jsm"
);

const ENGINE_NAME = "engine-suggestions.xml";
// This is fixed to match the port number in engine-suggestions.xml.
const SERVER_PORT = 9000;
const SUGGEST_PREF = "browser.urlbar.suggest.searches";
const SUGGEST_ENABLED_PREF = "browser.search.suggest.enabled";
const PRIVATE_ENABLED_PREF = "browser.search.suggest.enabled.private";
const PRIVATE_SEARCH_PREF = "browser.search.separatePrivateDefault.ui.enabled";
const MAX_RICH_RESULTS_PREF = "browser.urlbar.maxRichResults";

var suggestionsFn;
var previousSuggestionsFn;

/**
 * Set the current suggestion funciton.
 * @param {function} fn
 *   A function that that a search string and returns an array of strings that
 *   will be used as search suggestions.
 *   Note: `fn` should return > 1 suggestion in most cases. Otherwise, you may
 *         encounter unexceptede behaviour with UrlbarProviderSuggestion's
 *         _lastLowResultsSearchSuggestion safeguard.
 */
function setSuggestionsFn(fn) {
  previousSuggestionsFn = suggestionsFn;
  suggestionsFn = fn;
}

async function cleanup() {
  Services.prefs.clearUserPref("browser.urlbar.autoFill");
  Services.prefs.clearUserPref("browser.urlbar.autoFill.searchEngines");
  Services.prefs.clearUserPref(SUGGEST_PREF);
  Services.prefs.clearUserPref(SUGGEST_ENABLED_PREF);
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesUtils.history.clear();
}

async function cleanUpSuggestions() {
  await cleanup();
  if (previousSuggestionsFn) {
    suggestionsFn = previousSuggestionsFn;
    previousSuggestionsFn = null;
  }
}

add_task(async function setup() {
  Services.prefs.setCharPref(
    "browser.urlbar.matchBuckets",
    "general:5,suggestion:Infinity"
  );

  let engine = await addTestSuggestionsEngine(searchStr => {
    return suggestionsFn(searchStr);
  });
  setSuggestionsFn(searchStr => {
    let suffixes = ["foo", "bar"];
    return [searchStr].concat(suffixes.map(s => searchStr + " " + s));
  });

  // Install the test engine.
  let oldDefaultEngine = await Services.search.getDefault();
  registerCleanupFunction(async () => {
    Services.search.setDefault(oldDefaultEngine);
    Services.prefs.clearUserPref(PRIVATE_SEARCH_PREF);
  });
  Services.search.setDefault(engine);
  Services.prefs.setBoolPref(PRIVATE_SEARCH_PREF, false);

  // We must make sure the FormHistoryStartup component is initialized.
  Cc["@mozilla.org/satchel/form-history-startup;1"]
    .getService(Ci.nsIObserver)
    .observe(null, "profile-after-change", null);
  await updateSearchHistory("bump", "hello Fred!");
  await updateSearchHistory("bump", "hello Barney!");
});

add_task(async function disabled_urlbarSuggestions() {
  Services.prefs.setBoolPref(SUGGEST_PREF, false);
  Services.prefs.setBoolPref(SUGGEST_ENABLED_PREF, true);
  let context = createContext("hello", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
    ],
  });
  await cleanUpSuggestions();
});

add_task(async function disabled_allSuggestions() {
  Services.prefs.setBoolPref(SUGGEST_PREF, true);
  Services.prefs.setBoolPref(SUGGEST_ENABLED_PREF, false);
  let context = createContext("hello", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
    ],
  });
  await cleanUpSuggestions();
});

add_task(async function disabled_privateWindow() {
  Services.prefs.setBoolPref(SUGGEST_PREF, true);
  Services.prefs.setBoolPref(SUGGEST_ENABLED_PREF, true);
  Services.prefs.setBoolPref(PRIVATE_ENABLED_PREF, false);
  let context = createContext("hello", { isPrivate: true });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
    ],
  });
  await cleanUpSuggestions();
});

add_task(async function enabled_by_pref_privateWindow() {
  Services.prefs.setBoolPref(SUGGEST_PREF, true);
  Services.prefs.setBoolPref(SUGGEST_ENABLED_PREF, true);
  Services.prefs.setBoolPref(PRIVATE_ENABLED_PREF, true);
  const query = "hello";
  let context = createContext(query, { isPrivate: true });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        suggestion: query + " foo",
      }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        suggestion: query + " bar",
      }),
    ],
  });
  await cleanUpSuggestions();

  Services.prefs.clearUserPref(PRIVATE_ENABLED_PREF);
});

add_task(async function singleWordQuery() {
  Services.prefs.setBoolPref(SUGGEST_PREF, true);
  Services.prefs.setBoolPref(SUGGEST_ENABLED_PREF, true);
  const query = "hello";
  let context = createContext(query, { isPrivate: false });

  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        suggestion: query + " foo",
      }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        suggestion: query + " bar",
      }),
    ],
  });

  await cleanUpSuggestions();
});

add_task(async function multiWordQuery() {
  Services.prefs.setBoolPref(SUGGEST_PREF, true);
  Services.prefs.setBoolPref(SUGGEST_ENABLED_PREF, true);
  const query = "hello world";
  let context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        suggestion: query + " foo",
      }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        suggestion: query + " bar",
      }),
    ],
  });

  await cleanUpSuggestions();
});

add_task(async function suffixMatch() {
  Services.prefs.setBoolPref(SUGGEST_PREF, true);
  Services.prefs.setBoolPref(SUGGEST_ENABLED_PREF, true);

  setSuggestionsFn(searchStr => {
    let prefixes = ["baz", "quux"];
    return prefixes.map(p => p + " " + searchStr);
  });

  const query = "hello";
  let context = createContext(query, { isPrivate: false });

  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        suggestion: "baz " + query,
      }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        suggestion: "quux " + query,
      }),
    ],
  });

  await cleanUpSuggestions();
});

add_task(async function queryIsNotASubstring() {
  Services.prefs.setBoolPref(SUGGEST_PREF, true);

  setSuggestionsFn(searchStr => {
    return ["aaa", "bbb"];
  });

  const query = "hello";
  let context = createContext(query, { isPrivate: false });

  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        suggestion: "aaa",
      }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        suggestion: "bbb",
      }),
    ],
  });

  await cleanUpSuggestions();
});

add_task(async function restrictToken() {
  Services.prefs.setBoolPref(SUGGEST_PREF, true);
  Services.prefs.setBoolPref(SUGGEST_ENABLED_PREF, true);

  // Add a visit and a bookmark.  Actually, make the bookmark visited too so
  // that it's guaranteed, with its higher frecency, to appear above the search
  // suggestions.
  await PlacesTestUtils.addVisits([
    {
      uri: Services.io.newURI("http://example.com/hello-visit"),
      title: "hello visit",
    },
    {
      uri: Services.io.newURI("http://example.com/hello-bookmark"),
      title: "hello bookmark",
    },
  ]);

  await PlacesTestUtils.addBookmarkWithDetails({
    uri: Services.io.newURI("http://example.com/hello-bookmark"),
    title: "hello bookmark",
  });

  const query = "hello";
  let context = createContext(query, { isPrivate: false });

  // Do an unrestricted search to make sure everything appears in it, including
  // the visit and bookmark.
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
      makeBookmarkResult(context, {
        uri: "http://example.com/hello-bookmark",
        title: "hello bookmark",
      }),
      makeVisitResult(context, {
        uri: "http://example.com/hello-visit",
        title: "hello visit",
      }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        suggestion: query + " foo",
      }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        suggestion: query + " bar",
      }),
    ],
  });

  // Now do a restricted search to make sure only suggestions appear.
  context = createContext(`${UrlbarTokenizer.RESTRICT.SEARCH} ${query}`, {
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        query,
        heuristic: true,
      }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        query,
        suggestion: query + " foo",
      }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        query,
        suggestion: query + " bar",
      }),
    ],
  });

  // Typing the search restriction char shows only the Search Engine entry with
  // no query.
  context = createContext(UrlbarTokenizer.RESTRICT.SEARCH, {
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        query: "",
        heuristic: true,
      }),
    ],
  });
  // Also if followed by multiple spaces.
  context = createContext(`${UrlbarTokenizer.RESTRICT.SEARCH}  `, {
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        query: "",
        heuristic: true,
      }),
    ],
  });
  // Also if followed by a single char.
  context = createContext(`${UrlbarTokenizer.RESTRICT.SEARCH}a`, {
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        query: "a",
        heuristic: true,
      }),
    ],
  });
  // Also if followed by a space and single char.
  context = createContext(`${UrlbarTokenizer.RESTRICT.SEARCH} a`, {
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        query: "a",
        heuristic: true,
      }),
    ],
  });
  // Any other restriction char allows to search for it.
  context = createContext(UrlbarTokenizer.RESTRICT.OPENPAGE, {
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
    ],
  });

  await cleanUpSuggestions();
});

add_task(async function mixup_frecency() {
  Services.prefs.setBoolPref(SUGGEST_PREF, true);
  // At most, we should have 12 results in this subtest. We set this to 20 to
  // make we're not cutting off any results and we are actually getting 12.
  Services.prefs.setIntPref(MAX_RICH_RESULTS_PREF, 20);

  // Add a visit and a bookmark.  Actually, make the bookmark visited too so
  // that it's guaranteed, with its higher frecency, to appear above the search
  // suggestions.
  await PlacesTestUtils.addVisits([
    {
      uri: Services.io.newURI("http://example.com/lo0"),
      title: "low frecency 0",
    },
    {
      uri: Services.io.newURI("http://example.com/lo1"),
      title: "low frecency 1",
    },
    {
      uri: Services.io.newURI("http://example.com/lo2"),
      title: "low frecency 2",
    },
    {
      uri: Services.io.newURI("http://example.com/lo3"),
      title: "low frecency 3",
    },
    {
      uri: Services.io.newURI("http://example.com/lo4"),
      title: "low frecency 4",
    },
  ]);

  for (let i = 0; i < 5; i++) {
    await PlacesTestUtils.addVisits([
      {
        uri: Services.io.newURI("http://example.com/hi0"),
        title: "high frecency 0",
        transition: Ci.nsINavHistoryService.TRANSITION_TYPED,
      },
      {
        uri: Services.io.newURI("http://example.com/hi1"),
        title: "high frecency 1",
        transition: Ci.nsINavHistoryService.TRANSITION_TYPED,
      },
      {
        uri: Services.io.newURI("http://example.com/hi2"),
        title: "high frecency 2",
        transition: Ci.nsINavHistoryService.TRANSITION_TYPED,
      },
      {
        uri: Services.io.newURI("http://example.com/hi3"),
        title: "high frecency 3",
        transition: Ci.nsINavHistoryService.TRANSITION_TYPED,
      },
    ]);
  }

  for (let i = 0; i < 4; i++) {
    let href = `http://example.com/hi${i}`;
    await PlacesTestUtils.addBookmarkWithDetails({
      uri: href,
      title: `high frecency ${i}`,
    });
  }

  // Do an unrestricted search to make sure everything appears in it, including
  // the visit and bookmark.
  const query = "frecency";
  let context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
      makeBookmarkResult(context, {
        uri: "http://example.com/hi3",
        title: "high frecency 3",
      }),
      makeBookmarkResult(context, {
        uri: "http://example.com/hi2",
        title: "high frecency 2",
      }),
      makeBookmarkResult(context, {
        uri: "http://example.com/hi1",
        title: "high frecency 1",
      }),
      makeBookmarkResult(context, {
        uri: "http://example.com/hi0",
        title: "high frecency 0",
      }),
      makeVisitResult(context, {
        uri: "http://example.com/lo4",
        title: "low frecency 4",
      }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        suggestion: query + " foo",
      }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        suggestion: query + " bar",
      }),
      makeVisitResult(context, {
        uri: "http://example.com/lo3",
        title: "low frecency 3",
      }),
      makeVisitResult(context, {
        uri: "http://example.com/lo2",
        title: "low frecency 2",
      }),
      makeVisitResult(context, {
        uri: "http://example.com/lo1",
        title: "low frecency 1",
      }),
      makeVisitResult(context, {
        uri: "http://example.com/lo0",
        title: "low frecency 0",
      }),
    ],
  });

  // Change the "general" context mixup.
  Services.prefs.setCharPref(
    "browser.urlbar.matchBuckets",
    "suggestion:1,general:5,suggestion:1"
  );

  // Do an unrestricted search to make sure everything appears in it, including
  // the visits and bookmarks.
  context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        suggestion: query + " foo",
      }),
      makeBookmarkResult(context, {
        uri: "http://example.com/hi3",
        title: "high frecency 3",
      }),
      makeBookmarkResult(context, {
        uri: "http://example.com/hi2",
        title: "high frecency 2",
      }),
      makeBookmarkResult(context, {
        uri: "http://example.com/hi1",
        title: "high frecency 1",
      }),
      makeBookmarkResult(context, {
        uri: "http://example.com/hi0",
        title: "high frecency 0",
      }),
      makeVisitResult(context, {
        uri: "http://example.com/lo4",
        title: "low frecency 4",
      }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        suggestion: query + " bar",
      }),
      makeVisitResult(context, {
        uri: "http://example.com/lo3",
        title: "low frecency 3",
      }),
      makeVisitResult(context, {
        uri: "http://example.com/lo2",
        title: "low frecency 2",
      }),
      makeVisitResult(context, {
        uri: "http://example.com/lo1",
        title: "low frecency 1",
      }),
      makeVisitResult(context, {
        uri: "http://example.com/lo0",
        title: "low frecency 0",
      }),
    ],
  });

  // Change the "search" context mixup.
  Services.prefs.setCharPref(
    "browser.urlbar.matchBucketsSearch",
    "suggestion:2,general:4"
  );

  context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        suggestion: query + " foo",
      }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        suggestion: query + " bar",
      }),
      makeBookmarkResult(context, {
        uri: "http://example.com/hi3",
        title: "high frecency 3",
      }),
      makeBookmarkResult(context, {
        uri: "http://example.com/hi2",
        title: "high frecency 2",
      }),
      makeBookmarkResult(context, {
        uri: "http://example.com/hi1",
        title: "high frecency 1",
      }),
      makeBookmarkResult(context, {
        uri: "http://example.com/hi0",
        title: "high frecency 0",
      }),
      makeVisitResult(context, {
        uri: "http://example.com/lo4",
        title: "low frecency 4",
      }),
      makeVisitResult(context, {
        uri: "http://example.com/lo3",
        title: "low frecency 3",
      }),
      makeVisitResult(context, {
        uri: "http://example.com/lo2",
        title: "low frecency 2",
      }),
      makeVisitResult(context, {
        uri: "http://example.com/lo1",
        title: "low frecency 1",
      }),
      makeVisitResult(context, {
        uri: "http://example.com/lo0",
        title: "low frecency 0",
      }),
    ],
  });

  Services.prefs.setCharPref(
    "browser.urlbar.matchBuckets",
    "general:5,suggestion:Infinity"
  );
  Services.prefs.clearUserPref("browser.urlbar.matchBucketsSearch");
  Services.prefs.clearUserPref(MAX_RICH_RESULTS_PREF);
  await cleanUpSuggestions();
});

add_task(async function prohibit_suggestions() {
  Services.prefs.setBoolPref(SUGGEST_PREF, true);
  Services.prefs.setBoolPref("browser.fixup.domainwhitelist.localhost", false);

  const query = "localhost";
  let context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        suggestion: query + " foo",
      }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        suggestion: query + " bar",
      }),
    ],
  });

  Services.prefs.setBoolPref("browser.fixup.domainwhitelist.localhost", true);
  registerCleanupFunction(() => {
    Services.prefs.setBoolPref(
      "browser.fixup.domainwhitelist.localhost",
      false
    );
  });
  context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        uri: "http://localhost/",
        title: "http://localhost/",
        iconUri: "",
        heuristic: true,
      }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        heuristic: false,
      }),
    ],
  });

  // When using multiple words, we should still get suggestions:
  context = createContext(`${query} other`, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        suggestion: `${query} other foo`,
      }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        suggestion: `${query} other bar`,
      }),
    ],
  });

  // Clear the whitelist for localhost, and try preferring DNS for any single
  // word instead:
  Services.prefs.setBoolPref("browser.fixup.domainwhitelist.localhost", false);
  Services.prefs.setBoolPref("browser.fixup.dns_first_for_single_words", true);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("browser.fixup.dns_first_for_single_words");
  });

  context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        uri: "http://localhost/",
        title: "http://localhost/",
        iconUri: "",
        heuristic: true,
      }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        heuristic: false,
      }),
    ],
  });

  context = createContext("somethingelse", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        uri: "http://somethingelse/",
        title: "http://somethingelse/",
        iconUri: "",
        heuristic: true,
      }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        heuristic: false,
      }),
    ],
  });

  // When using multiple words, we should still get suggestions:
  context = createContext(`${query} other`, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        suggestion: `${query} other foo`,
      }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        suggestion: `${query} other bar`,
      }),
    ],
  });

  Services.prefs.clearUserPref("browser.fixup.dns_first_for_single_words");

  context = createContext("http://1.2.3.4/", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        uri: "http://1.2.3.4/",
        title: "http://1.2.3.4/",
        heuristic: true,
      }),
    ],
  });

  context = createContext("[2001::1]:30", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        uri: "http://[2001::1]:30/",
        title: "http://[2001::1]:30/",
        iconUri: "",
        heuristic: true,
      }),
    ],
  });

  context = createContext("user:pass@test", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        uri: "http://user:pass@test/",
        title: "http://user:pass@test/",
        iconUri: "",
        heuristic: true,
      }),
    ],
  });

  context = createContext("test/test", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        uri: "http://test/test",
        title: "http://test/test",
        iconUri: "page-icon:http://test/",
        heuristic: true,
      }),
    ],
  });

  context = createContext("data:text/plain,Content", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        uri: "data:text/plain,Content",
        title: "data:text/plain,Content",
        iconUri: "",
        heuristic: true,
      }),
    ],
  });

  context = createContext("a", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
    ],
  });

  await cleanUpSuggestions();
});

add_task(async function avoid_url_suggestions() {
  Services.prefs.setBoolPref(SUGGEST_PREF, true);

  setSuggestionsFn(searchStr => {
    let suffixes = [".com", "/test", ":1]", "@test", ". com"];
    return suffixes.map(s => searchStr + s);
  });

  const query = "test";
  let context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        suggestion: `${query}. com`,
      }),
    ],
  });

  await cleanUpSuggestions();
});

add_task(async function avoid_url_suggestions() {
  Services.prefs.setBoolPref(SUGGEST_PREF, true);
  Services.prefs.setBoolPref("browser.urlbar.autoFill", false);

  setSuggestionsFn(searchStr => {
    let suffixes = ["ed", "eds"];
    return suffixes.map(s => searchStr + s);
  });

  let context = createContext("htt", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        suggestion: "htted",
      }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        suggestion: "htteds",
      }),
    ],
  });

  context = createContext("ftp", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        suggestion: "ftped",
      }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        suggestion: "ftpeds",
      }),
    ],
  });

  context = createContext("http", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        suggestion: "httped",
      }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        suggestion: "httpeds",
      }),
    ],
  });

  context = createContext("http:", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
    ],
  });

  context = createContext("https", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        suggestion: "httpsed",
      }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        suggestion: "httpseds",
      }),
    ],
  });

  context = createContext("https:", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
    ],
  });

  context = createContext("httpd", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        suggestion: "httpded",
      }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        suggestion: "httpdeds",
      }),
    ],
  });

  // Check FTP enabled
  Services.prefs.setBoolPref("network.ftp.enabled", true);
  registerCleanupFunction(() =>
    Services.prefs.clearUserPref("network.ftp.enabled")
  );

  context = createContext("ftp:", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
    ],
  });

  context = createContext("ftp:/", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
    ],
  });

  context = createContext("ftp://", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
    ],
  });

  context = createContext("ftp://test", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        uri: "ftp://test/",
        title: "ftp://test/",
        iconUri: "",
        heuristic: true,
      }),
    ],
  });

  // Check FTP disabled
  Services.prefs.setBoolPref("network.ftp.enabled", false);
  context = createContext("ftp:", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
    ],
  });

  context = createContext("ftp:/", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
    ],
  });

  context = createContext("ftp://", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
    ],
  });

  context = createContext("ftp://test", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        uri: "ftp://test/",
        title: "ftp://test/",
        iconUri: "",
        heuristic: true,
      }),
    ],
  });

  context = createContext("http:/", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
    ],
  });

  context = createContext("https:/", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
    ],
  });

  context = createContext("http://", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
    ],
  });

  context = createContext("https://", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
    ],
  });

  context = createContext("http://www", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        uri: "http://www/",
        title: "http://www/",
        iconUri: "",
        heuristic: true,
      }),
    ],
  });

  context = createContext("https://www", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        uri: "https://www/",
        title: "https://www/",
        iconUri: "",
        heuristic: true,
      }),
    ],
  });

  context = createContext("http://test", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        uri: "http://test/",
        title: "http://test/",
        iconUri: "",
        heuristic: true,
      }),
    ],
  });

  context = createContext("https://test", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        uri: "https://test/",
        title: "https://test/",
        iconUri: "",
        heuristic: true,
      }),
    ],
  });

  context = createContext("http://www.test", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        uri: "http://www.test/",
        title: "http://www.test/",
        iconUri: "",
        heuristic: true,
      }),
    ],
  });

  context = createContext("http://www.test.com", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        uri: "http://www.test.com/",
        title: "http://www.test.com/",
        iconUri: "",
        heuristic: true,
      }),
    ],
  });

  context = createContext("file", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        suggestion: "fileed",
      }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        suggestion: "fileeds",
      }),
    ],
  });

  context = createContext("file:", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
    ],
  });

  context = createContext("file:///Users", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        uri: "file:///Users",
        title: "file:///Users",
        iconUri: "",
        heuristic: true,
      }),
    ],
  });

  context = createContext("moz-test://", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
    ],
  });

  context = createContext("moz+test://", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
    ],
  });

  context = createContext("about", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        suggestion: "abouted",
      }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        suggestion: "abouteds",
      }),
    ],
  });

  context = createContext("about:", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
    ],
  });

  await cleanUpSuggestions();
});

add_task(async function restrict_suggestions_after_low_results() {
  setSuggestionsFn(searchStr => {
    return [searchStr + "s"];
  });

  const query = "hello";
  let context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        suggestion: `${query}s`,
      }),
    ],
  });

  context = createContext(`${query}a`, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
      // Because the previous search only returned one suggestion, we will not
      // fetch suggestions for this query that is just a longer version of the
      // previous query.
    ],
  });

  await cleanUpSuggestions();
});

add_task(async function historicalSuggestion() {
  Services.prefs.setBoolPref(SUGGEST_PREF, true);
  Services.prefs.setBoolPref(SUGGEST_ENABLED_PREF, true);
  Services.prefs.setIntPref("browser.urlbar.maxHistoricalSearchSuggestions", 1);

  const query = "hello";
  let context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        suggestion: `${query} Barney!`,
      }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        suggestion: `${query} foo`,
      }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        suggestion: `${query} bar`,
      }),
    ],
  });

  await cleanUpSuggestions();
  Services.prefs.clearUserPref("browser.urlbar.maxHistoricalSearchSuggestions");
});

function updateSearchHistory(op, value) {
  return new Promise((resolve, reject) => {
    FormHistory.update(
      { op, fieldname: "searchbar-history", value },
      {
        handleError(error) {
          do_throw("Error occurred updating form history: " + error);
          reject(error);
        },
        handleCompletion(reason) {
          if (reason) {
            reject(reason);
          } else {
            resolve();
          }
        },
      }
    );
  });
}
