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
const MAX_FORM_HISTORY_PREF = "browser.urlbar.maxHistoricalSearchSuggestions";
const SEARCH_STRING = "hello";
const MATCH_BUCKETS_VALUE = "general:5,suggestion:Infinity";

var suggestionsFn;
var previousSuggestionsFn;

/**
 * Set the current suggestion funciton.
 * @param {function} fn
 *   A function that that a search string and returns an array of strings that
 *   will be used as search suggestions.
 *   Note: `fn` should return > 0 suggestions in most cases. Otherwise, you may
 *         encounter unexpected behaviour with UrlbarProviderSuggestion's
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

function makeExpectedFormHistoryResults(context, minCount = 0) {
  let count = Math.max(
    minCount,
    Services.prefs.getIntPref(MAX_FORM_HISTORY_PREF, 0)
  );
  let results = [];
  for (let i = 0; i < count; i++) {
    results.push(
      makeFormHistoryResult(context, {
        suggestion: `${SEARCH_STRING} world Form History ${i}`,
        engineName: ENGINE_NAME,
      })
    );
  }
  return results;
}

function makeExpectedRemoteSuggestionResults(
  context,
  { suggestionPrefix = SEARCH_STRING, query = undefined } = {}
) {
  return [
    makeSearchResult(context, {
      query,
      engineName: ENGINE_NAME,
      suggestion: suggestionPrefix + " foo",
    }),
    makeSearchResult(context, {
      query,
      engineName: ENGINE_NAME,
      suggestion: suggestionPrefix + " bar",
    }),
  ];
}

function makeExpectedSuggestionResults(
  context,
  { suggestionPrefix = SEARCH_STRING, query = undefined } = {}
) {
  return [
    ...makeExpectedFormHistoryResults(context),
    ...makeExpectedRemoteSuggestionResults(context, {
      suggestionPrefix,
      query,
    }),
  ];
}

add_task(async function setup() {
  Services.prefs.setCharPref(
    "browser.urlbar.matchBuckets",
    MATCH_BUCKETS_VALUE
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

  // Add some form history.
  let context = createContext(SEARCH_STRING, { isPrivate: false });
  let entries = makeExpectedFormHistoryResults(context, 2).map(r => ({
    value: r.payload.suggestion,
    source: ENGINE_NAME,
  }));
  await UrlbarTestUtils.formHistory.add(entries);
});

add_task(async function disabled_urlbarSuggestions() {
  Services.prefs.setBoolPref(SUGGEST_PREF, false);
  Services.prefs.setBoolPref(SUGGEST_ENABLED_PREF, true);
  let context = createContext(SEARCH_STRING, { isPrivate: false });
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
  let context = createContext(SEARCH_STRING, { isPrivate: false });
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
  let context = createContext(SEARCH_STRING, { isPrivate: true });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
    ],
  });
  await cleanUpSuggestions();
});

add_task(async function disabled_urlbarSuggestions_withRestrictionToken() {
  Services.prefs.setBoolPref(SUGGEST_PREF, false);
  Services.prefs.setBoolPref(SUGGEST_ENABLED_PREF, true);
  let context = createContext(
    `${UrlbarTokenizer.RESTRICT.SEARCH} ${SEARCH_STRING}`,
    { isPrivate: false }
  );
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        query: SEARCH_STRING,
        alias: UrlbarTokenizer.RESTRICT.SEARCH,
        engineName: ENGINE_NAME,
        heuristic: true,
      }),
      ...makeExpectedSuggestionResults(context, {
        query: SEARCH_STRING,
      }),
    ],
  });
  await cleanUpSuggestions();
});

add_task(
  async function disabled_urlbarSuggestions_withRestrictionToken_private() {
    Services.prefs.setBoolPref(SUGGEST_PREF, false);
    Services.prefs.setBoolPref(SUGGEST_ENABLED_PREF, true);
    Services.prefs.setBoolPref(PRIVATE_ENABLED_PREF, false);
    let context = createContext(
      `${UrlbarTokenizer.RESTRICT.SEARCH} ${SEARCH_STRING}`,
      { isPrivate: true }
    );
    await check_results({
      context,
      matches: [
        makeSearchResult(context, {
          query: SEARCH_STRING,
          alias: UrlbarTokenizer.RESTRICT.SEARCH,
          engineName: ENGINE_NAME,
          heuristic: true,
        }),
      ],
    });
    await cleanUpSuggestions();
  }
);

add_task(
  async function disabled_urlbarSuggestions_withRestrictionToken_private_enabled() {
    Services.prefs.setBoolPref(SUGGEST_PREF, false);
    Services.prefs.setBoolPref(SUGGEST_ENABLED_PREF, true);
    Services.prefs.setBoolPref(PRIVATE_ENABLED_PREF, true);
    let context = createContext(
      `${UrlbarTokenizer.RESTRICT.SEARCH} ${SEARCH_STRING}`,
      { isPrivate: true }
    );
    await check_results({
      context,
      matches: [
        makeSearchResult(context, {
          query: SEARCH_STRING,
          alias: UrlbarTokenizer.RESTRICT.SEARCH,
          engineName: ENGINE_NAME,
          heuristic: true,
        }),
        ...makeExpectedSuggestionResults(context, {
          query: SEARCH_STRING,
        }),
      ],
    });
    await cleanUpSuggestions();
  }
);

add_task(async function enabled_by_pref_privateWindow() {
  Services.prefs.setBoolPref(SUGGEST_PREF, true);
  Services.prefs.setBoolPref(SUGGEST_ENABLED_PREF, true);
  Services.prefs.setBoolPref(PRIVATE_ENABLED_PREF, true);
  let context = createContext(SEARCH_STRING, { isPrivate: true });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
      ...makeExpectedSuggestionResults(context),
    ],
  });
  await cleanUpSuggestions();

  Services.prefs.clearUserPref(PRIVATE_ENABLED_PREF);
});

add_task(async function singleWordQuery() {
  Services.prefs.setBoolPref(SUGGEST_PREF, true);
  Services.prefs.setBoolPref(SUGGEST_ENABLED_PREF, true);
  let context = createContext(SEARCH_STRING, { isPrivate: false });

  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
      ...makeExpectedSuggestionResults(context),
    ],
  });

  await cleanUpSuggestions();
});

add_task(async function multiWordQuery() {
  Services.prefs.setBoolPref(SUGGEST_PREF, true);
  Services.prefs.setBoolPref(SUGGEST_ENABLED_PREF, true);
  const query = `${SEARCH_STRING} world`;
  let context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
      ...makeExpectedSuggestionResults(context, { suggestionPrefix: query }),
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

  let context = createContext(SEARCH_STRING, { isPrivate: false });

  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
      ...makeExpectedFormHistoryResults(context),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        suggestion: "baz " + SEARCH_STRING,
      }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        suggestion: "quux " + SEARCH_STRING,
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

  let context = createContext(SEARCH_STRING, { isPrivate: false });

  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
      ...makeExpectedFormHistoryResults(context),
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
      uri: Services.io.newURI(`http://example.com/${SEARCH_STRING}-visit`),
      title: `${SEARCH_STRING} visit`,
    },
    {
      uri: Services.io.newURI(`http://example.com/${SEARCH_STRING}-bookmark`),
      title: `${SEARCH_STRING} bookmark`,
    },
  ]);

  await PlacesTestUtils.addBookmarkWithDetails({
    uri: Services.io.newURI(`http://example.com/${SEARCH_STRING}-bookmark`),
    title: `${SEARCH_STRING} bookmark`,
  });

  let context = createContext(SEARCH_STRING, { isPrivate: false });

  // Do an unrestricted search to make sure everything appears in it, including
  // the visit and bookmark.
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
      makeBookmarkResult(context, {
        uri: `http://example.com/${SEARCH_STRING}-bookmark`,
        title: `${SEARCH_STRING} bookmark`,
      }),
      makeVisitResult(context, {
        uri: `http://example.com/${SEARCH_STRING}-visit`,
        title: `${SEARCH_STRING} visit`,
      }),
      ...makeExpectedSuggestionResults(context),
    ],
  });

  // Now do a restricted search to make sure only suggestions appear.
  context = createContext(
    `${UrlbarTokenizer.RESTRICT.SEARCH} ${SEARCH_STRING}`,
    {
      isPrivate: false,
    }
  );
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        alias: UrlbarTokenizer.RESTRICT.SEARCH,
        query: SEARCH_STRING,
        heuristic: true,
      }),
      ...makeExpectedSuggestionResults(context, {
        suggestionPrefix: SEARCH_STRING,
        query: SEARCH_STRING,
      }),
    ],
  });

  // Typing the search restriction char shows the Search Engine entry and local
  // results.
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
      ...makeExpectedFormHistoryResults(context),
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
        alias: UrlbarTokenizer.RESTRICT.SEARCH,
        query: "",
        heuristic: true,
      }),
      ...makeExpectedFormHistoryResults(context),
    ],
  });

  // If followed by any char we should fetch suggestions.
  // Note this uses "h" to match form history.
  context = createContext(`${UrlbarTokenizer.RESTRICT.SEARCH}h`, {
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        query: "h",
        heuristic: true,
      }),
      ...makeExpectedSuggestionResults(context, {
        suggestionPrefix: "h",
        query: "h",
      }),
    ],
  });

  // Also if followed by a space and single char.
  context = createContext(`${UrlbarTokenizer.RESTRICT.SEARCH} h`, {
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        alias: UrlbarTokenizer.RESTRICT.SEARCH,
        query: "h",
        heuristic: true,
      }),
      ...makeExpectedSuggestionResults(context, {
        suggestionPrefix: "h",
        query: "h",
      }),
    ],
  });

  // With update2 disabled, any other restriction char allows to search for it.
  Services.prefs.setBoolPref("browser.urlbar.update2", false);
  context = createContext(UrlbarTokenizer.RESTRICT.OPENPAGE, {
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
    ],
  });
  Services.prefs.clearUserPref("browser.urlbar.update2");

  // Leading search-mode restriction tokens are removed.
  Services.prefs.setBoolPref("browser.urlbar.update2", true);
  context = createContext(
    `${UrlbarTokenizer.RESTRICT.BOOKMARK} ${SEARCH_STRING}`,
    { isPrivate: false }
  );
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        heuristic: true,
        query: SEARCH_STRING,
        alias: UrlbarTokenizer.RESTRICT.BOOKMARK,
      }),
      makeBookmarkResult(context, {
        uri: `http://example.com/${SEARCH_STRING}-bookmark`,
        title: `${SEARCH_STRING} bookmark`,
      }),
    ],
  });

  // Non-search-mode restriction tokens remain in the query and heuristic search
  // result.
  let token;
  for (let t of Object.values(UrlbarTokenizer.RESTRICT)) {
    if (!UrlbarTokenizer.SEARCH_MODE_RESTRICT.has(t)) {
      token = t;
      break;
    }
  }
  Assert.ok(
    token,
    "Non-search-mode restrict token exists -- if not, you can probably remove me!"
  );
  context = createContext(token, {
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
    ],
  });
  Services.prefs.clearUserPref("browser.urlbar.update2");

  await cleanUpSuggestions();
});

add_task(async function mixup_frecency() {
  Services.prefs.setBoolPref(SUGGEST_PREF, true);
  // At most, we should have 14 results in this subtest. We set this to 20 to
  // make we're not cutting off any results and we are actually getting 12.
  Services.prefs.setIntPref(MAX_RICH_RESULTS_PREF, 20);

  // Add a visit and a bookmark.  Actually, make the bookmark visited too so
  // that it's guaranteed, with its higher frecency, to appear above the search
  // suggestions.
  await PlacesTestUtils.addVisits([
    {
      uri: Services.io.newURI("http://example.com/lo0"),
      title: `${SEARCH_STRING} low frecency 0`,
    },
    {
      uri: Services.io.newURI("http://example.com/lo1"),
      title: `${SEARCH_STRING} low frecency 1`,
    },
    {
      uri: Services.io.newURI("http://example.com/lo2"),
      title: `${SEARCH_STRING} low frecency 2`,
    },
    {
      uri: Services.io.newURI("http://example.com/lo3"),
      title: `${SEARCH_STRING} low frecency 3`,
    },
    {
      uri: Services.io.newURI("http://example.com/lo4"),
      title: `${SEARCH_STRING} low frecency 4`,
    },
  ]);

  for (let i = 0; i < 5; i++) {
    await PlacesTestUtils.addVisits([
      {
        uri: Services.io.newURI("http://example.com/hi0"),
        title: `${SEARCH_STRING} high frecency 0`,
        transition: Ci.nsINavHistoryService.TRANSITION_TYPED,
      },
      {
        uri: Services.io.newURI("http://example.com/hi1"),
        title: `${SEARCH_STRING} high frecency 1`,
        transition: Ci.nsINavHistoryService.TRANSITION_TYPED,
      },
      {
        uri: Services.io.newURI("http://example.com/hi2"),
        title: `${SEARCH_STRING} high frecency 2`,
        transition: Ci.nsINavHistoryService.TRANSITION_TYPED,
      },
      {
        uri: Services.io.newURI("http://example.com/hi3"),
        title: `${SEARCH_STRING} high frecency 3`,
        transition: Ci.nsINavHistoryService.TRANSITION_TYPED,
      },
    ]);
  }

  for (let i = 0; i < 4; i++) {
    let href = `http://example.com/hi${i}`;
    await PlacesTestUtils.addBookmarkWithDetails({
      uri: href,
      title: `${SEARCH_STRING} high frecency ${i}`,
    });
  }

  // Do an unrestricted search to make sure everything appears in it, including
  // the visit and bookmark.
  let context = createContext(SEARCH_STRING, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
      makeBookmarkResult(context, {
        uri: "http://example.com/hi3",
        title: `${SEARCH_STRING} high frecency 3`,
      }),
      makeBookmarkResult(context, {
        uri: "http://example.com/hi2",
        title: `${SEARCH_STRING} high frecency 2`,
      }),
      makeBookmarkResult(context, {
        uri: "http://example.com/hi1",
        title: `${SEARCH_STRING} high frecency 1`,
      }),
      makeBookmarkResult(context, {
        uri: "http://example.com/hi0",
        title: `${SEARCH_STRING} high frecency 0`,
      }),
      makeVisitResult(context, {
        uri: "http://example.com/lo4",
        title: `${SEARCH_STRING} low frecency 4`,
      }),
      ...makeExpectedSuggestionResults(context),
      makeVisitResult(context, {
        uri: "http://example.com/lo3",
        title: `${SEARCH_STRING} low frecency 3`,
      }),
      makeVisitResult(context, {
        uri: "http://example.com/lo2",
        title: `${SEARCH_STRING} low frecency 2`,
      }),
      makeVisitResult(context, {
        uri: "http://example.com/lo1",
        title: `${SEARCH_STRING} low frecency 1`,
      }),
      makeVisitResult(context, {
        uri: "http://example.com/lo0",
        title: `${SEARCH_STRING} low frecency 0`,
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
  context = createContext(SEARCH_STRING, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
      ...makeExpectedSuggestionResults(context).slice(0, 1),
      makeBookmarkResult(context, {
        uri: "http://example.com/hi3",
        title: `${SEARCH_STRING} high frecency 3`,
      }),
      makeBookmarkResult(context, {
        uri: "http://example.com/hi2",
        title: `${SEARCH_STRING} high frecency 2`,
      }),
      makeBookmarkResult(context, {
        uri: "http://example.com/hi1",
        title: `${SEARCH_STRING} high frecency 1`,
      }),
      makeBookmarkResult(context, {
        uri: "http://example.com/hi0",
        title: `${SEARCH_STRING} high frecency 0`,
      }),
      makeVisitResult(context, {
        uri: "http://example.com/lo4",
        title: `${SEARCH_STRING} low frecency 4`,
      }),
      ...makeExpectedSuggestionResults(context).slice(1),
      makeVisitResult(context, {
        uri: "http://example.com/lo3",
        title: `${SEARCH_STRING} low frecency 3`,
      }),
      makeVisitResult(context, {
        uri: "http://example.com/lo2",
        title: `${SEARCH_STRING} low frecency 2`,
      }),
      makeVisitResult(context, {
        uri: "http://example.com/lo1",
        title: `${SEARCH_STRING} low frecency 1`,
      }),
      makeVisitResult(context, {
        uri: "http://example.com/lo0",
        title: `${SEARCH_STRING} low frecency 0`,
      }),
    ],
  });

  // Change the "search" context mixup.
  Services.prefs.setCharPref(
    "browser.urlbar.matchBucketsSearch",
    "suggestion:2,general:4"
  );

  context = createContext(SEARCH_STRING, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
      ...makeExpectedSuggestionResults(context).slice(0, 2),
      makeBookmarkResult(context, {
        uri: "http://example.com/hi3",
        title: `${SEARCH_STRING} high frecency 3`,
      }),
      makeBookmarkResult(context, {
        uri: "http://example.com/hi2",
        title: `${SEARCH_STRING} high frecency 2`,
      }),
      makeBookmarkResult(context, {
        uri: "http://example.com/hi1",
        title: `${SEARCH_STRING} high frecency 1`,
      }),
      makeBookmarkResult(context, {
        uri: "http://example.com/hi0",
        title: `${SEARCH_STRING} high frecency 0`,
      }),
      ...makeExpectedSuggestionResults(context).slice(2),
      makeVisitResult(context, {
        uri: "http://example.com/lo4",
        title: `${SEARCH_STRING} low frecency 4`,
      }),
      makeVisitResult(context, {
        uri: "http://example.com/lo3",
        title: `${SEARCH_STRING} low frecency 3`,
      }),
      makeVisitResult(context, {
        uri: "http://example.com/lo2",
        title: `${SEARCH_STRING} low frecency 2`,
      }),
      makeVisitResult(context, {
        uri: "http://example.com/lo1",
        title: `${SEARCH_STRING} low frecency 1`,
      }),
      makeVisitResult(context, {
        uri: "http://example.com/lo0",
        title: `${SEARCH_STRING} low frecency 0`,
      }),
    ],
  });

  Services.prefs.setCharPref(
    "browser.urlbar.matchBuckets",
    MATCH_BUCKETS_VALUE
  );
  Services.prefs.clearUserPref("browser.urlbar.matchBucketsSearch");
  Services.prefs.clearUserPref(MAX_RICH_RESULTS_PREF);
  await cleanUpSuggestions();
});

add_task(async function prohibit_suggestions() {
  Services.prefs.setBoolPref(SUGGEST_PREF, true);
  Services.prefs.setBoolPref(
    `browser.fixup.domainwhitelist.${SEARCH_STRING}`,
    false
  );

  let context = createContext(SEARCH_STRING, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
      ...makeExpectedSuggestionResults(context),
    ],
  });

  Services.prefs.setBoolPref(
    `browser.fixup.domainwhitelist.${SEARCH_STRING}`,
    true
  );
  registerCleanupFunction(() => {
    Services.prefs.setBoolPref(
      `browser.fixup.domainwhitelist.${SEARCH_STRING}`,
      false
    );
  });
  context = createContext(SEARCH_STRING, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        uri: `http://${SEARCH_STRING}/`,
        title: `http://${SEARCH_STRING}/`,
        iconUri: "",
        heuristic: true,
      }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        heuristic: false,
      }),
      ...makeExpectedFormHistoryResults(context),
    ],
  });

  // When using multiple words, we should still get suggestions:
  let query = `${SEARCH_STRING} world`;
  context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
      ...makeExpectedSuggestionResults(context, { suggestionPrefix: query }),
    ],
  });

  // Clear the whitelist for SEARCH_STRING and try preferring DNS for any single
  // word instead:
  Services.prefs.setBoolPref(
    `browser.fixup.domainwhitelist.${SEARCH_STRING}`,
    false
  );
  Services.prefs.setBoolPref("browser.fixup.dns_first_for_single_words", true);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("browser.fixup.dns_first_for_single_words");
  });

  context = createContext(SEARCH_STRING, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        uri: `http://${SEARCH_STRING}/`,
        title: `http://${SEARCH_STRING}/`,
        iconUri: "",
        heuristic: true,
      }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        heuristic: false,
      }),
      ...makeExpectedFormHistoryResults(context),
    ],
  });

  context = createContext("somethingelse", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
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
  query = `${SEARCH_STRING} world`;
  context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
      ...makeExpectedSuggestionResults(context, { suggestionPrefix: query }),
    ],
  });

  Services.prefs.clearUserPref("browser.fixup.dns_first_for_single_words");

  context = createContext("http://1.2.3.4/", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        uri: "http://1.2.3.4/",
        title: "http://1.2.3.4/",
        iconUri: "page-icon:http://1.2.3.4/",
        heuristic: true,
      }),
    ],
  });

  context = createContext("[2001::1]:30", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
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
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        uri: "http://user:pass@test/",
        title: "http://user:pass@test/",
        iconUri: "",
        heuristic: true,
      }),
    ],
  });

  context = createContext("data:text/plain,Content", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
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

add_task(async function uri_like_queries() {
  Services.prefs.setBoolPref(SUGGEST_PREF, true);
  Services.prefs.setBoolPref(SUGGEST_ENABLED_PREF, true);

  // We should not fetch any suggestions for an actual URL.
  let query = "mozilla.org";
  let context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        title: `http://${query}/`,
        uri: `http://${query}/`,
        iconUri: "",
        heuristic: true,
      }),
      makeSearchResult(context, { query, engineName: ENGINE_NAME }),
    ],
  });

  // We should also not fetch suggestions for a partially-typed URL.
  query = "mozilla.o";
  context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
    ],
  });

  // Now trying queries that could be confused for URLs. They should return
  // results.
  const uriLikeQueries = [
    "mozilla.org is a great website",
    "I like mozilla.org",
    "a/b testing",
    "he/him",
    "Google vs.",
    "5.8 cm",
  ];
  for (query of uriLikeQueries) {
    context = createContext(query, { isPrivate: false });
    await check_results({
      context,
      matches: [
        makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
        ...makeExpectedRemoteSuggestionResults(context, {
          suggestionPrefix: query,
        }),
      ],
    });
  }

  await cleanUpSuggestions();
});

add_task(async function avoid_remote_url_suggestions_1() {
  Services.prefs.setBoolPref(SUGGEST_PREF, true);
  Services.prefs.setIntPref(MAX_FORM_HISTORY_PREF, 1);

  setSuggestionsFn(searchStr => {
    let suffixes = [".com", "/test", ":1]", "@test", ". com"];
    return suffixes.map(s => searchStr + s);
  });

  const query = "test";

  await UrlbarTestUtils.formHistory.add([`${query}.com`]);

  let context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
      makeFormHistoryResult(context, {
        engineName: ENGINE_NAME,
        suggestion: `${query}.com`,
      }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        suggestion: `${query}. com`,
      }),
    ],
  });

  await cleanUpSuggestions();
  await UrlbarTestUtils.formHistory.remove([`${query}.com`]);
  Services.prefs.clearUserPref(MAX_FORM_HISTORY_PREF);
});

add_task(async function avoid_remote_url_suggestions_2() {
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
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
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
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
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
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
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
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
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
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
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
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
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
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
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
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
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
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
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

add_task(async function restrict_remote_suggestions_after_no_results() {
  // We don't fetch remote suggestions if a query with a length over
  // maxCharsForSearchSuggestions returns 0 results. We set it to 4 here to
  // avoid constructing a 100+ character string.
  Services.prefs.setIntPref("browser.urlbar.maxCharsForSearchSuggestions", 4);
  setSuggestionsFn(searchStr => {
    return [];
  });

  const query = SEARCH_STRING.substring(0, SEARCH_STRING.length - 1);
  let context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
      ...makeExpectedFormHistoryResults(context),
    ],
  });

  context = createContext(SEARCH_STRING, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
      ...makeExpectedFormHistoryResults(context),
      // Because the previous search returned no suggestions, we will not fetch
      // remote suggestions for this query that is just a longer version of the
      // previous query.
    ],
  });

  // Do one more search before resetting maxCharsForSearchSuggestions to reset
  // the search suggestion provider's _lastLowResultsSearchSuggestion property.
  // Otherwise it will be stuck at SEARCH_STRING, which interferes with
  // subsequent tests.
  context = createContext("not the search string", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
    ],
  });

  Services.prefs.clearUserPref("browser.urlbar.maxCharsForSearchSuggestions");

  await cleanUpSuggestions();
});

add_task(async function formHistory() {
  Services.prefs.setBoolPref(SUGGEST_PREF, true);
  Services.prefs.setBoolPref(SUGGEST_ENABLED_PREF, true);

  Services.prefs.setIntPref(MAX_FORM_HISTORY_PREF, 0);
  let context = createContext(SEARCH_STRING, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
      ...makeExpectedRemoteSuggestionResults(context),
      ...makeExpectedFormHistoryResults(context, 2),
    ],
  });

  Services.prefs.setIntPref(MAX_FORM_HISTORY_PREF, 1);
  context = createContext(SEARCH_STRING, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
      ...makeExpectedFormHistoryResults(context, 2).slice(0, 1),
      ...makeExpectedRemoteSuggestionResults(context),
      ...makeExpectedFormHistoryResults(context, 2).slice(1),
    ],
  });

  Services.prefs.setIntPref(MAX_FORM_HISTORY_PREF, 2);
  context = createContext(SEARCH_STRING, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
      ...makeExpectedFormHistoryResults(context, 2),
      ...makeExpectedRemoteSuggestionResults(context),
    ],
  });

  // Do a search for exactly the suggestion of the first form history result.
  // The heuristic's query should be the suggestion; the first form history
  // result should not be included since it dupes the heuristic; the second form
  // history result should not be included since it doesn't match; and both
  // remote suggestions should be included.
  let firstSuggestion = makeExpectedFormHistoryResults(context)[0].payload
    .suggestion;
  context = createContext(firstSuggestion, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
      ...makeExpectedRemoteSuggestionResults(context, {
        suggestionPrefix: firstSuggestion,
      }),
    ],
  });

  // Add these form history strings to use below.
  let formHistoryStrings = ["foo", "foobar", "fooquux"];
  await UrlbarTestUtils.formHistory.add(formHistoryStrings);

  // Search for "foo".  "foo" shouldn't be included since it dupes the
  // heuristic.  Both "foobar" and "fooquux" should be included even though the
  // max form history count is only two and there are three matching form
  // history results (including "foo").
  context = createContext("foo", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
      makeFormHistoryResult(context, {
        suggestion: "foobar",
        engineName: ENGINE_NAME,
      }),
      ...makeExpectedRemoteSuggestionResults(context, {
        suggestionPrefix: "foo",
      }),
      // Note that the second form history result appears after the remote
      // suggestions.  This isn't ideal because it should appear right after the
      // first form history result, but it doesn't because the actual first form
      // history result duped the heuristic, so the muxer discarded it.
      makeFormHistoryResult(context, {
        suggestion: "fooquux",
        engineName: ENGINE_NAME,
      }),
    ],
  });

  // Add a visit that matches "foo" and will autofill so that the heuristic is
  // not a search result.  Now the "foo" and "foobar" form history should be
  // included.
  await PlacesTestUtils.addVisits("http://foo.example.com/");
  context = createContext("foo", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.HISTORY,
        uri: "http://foo.example.com/",
        title: "foo.example.com",
        heuristic: true,
      }),
      makeFormHistoryResult(context, {
        suggestion: "foo",
        engineName: ENGINE_NAME,
      }),
      makeFormHistoryResult(context, {
        suggestion: "foobar",
        engineName: ENGINE_NAME,
      }),
      ...makeExpectedRemoteSuggestionResults(context, {
        suggestionPrefix: "foo",
      }),
      makeFormHistoryResult(context, {
        suggestion: "fooquux",
        engineName: ENGINE_NAME,
      }),
    ],
  });
  await PlacesUtils.history.clear();

  // Add SERPs for "foobar" and "food" and search for "foo".  The "foo" form
  // history should be excluded since it dupes the heuristic; the "foobar" and
  // "fooquux" form history should be included; the "food" SERP should be
  // included since it doesn't dupe either form history result; and the "foobar"
  // SERP depends on the match buckets, see below.
  let engine = await Services.search.getDefault();
  let [serpURL1] = UrlbarUtils.getSearchQueryUrl(engine, "foobar");
  let [serpURL2] = UrlbarUtils.getSearchQueryUrl(engine, "food");
  await PlacesTestUtils.addVisits([serpURL1, serpURL2]);

  // First, use the MATCH_BUCKETS_VALUE that the test set above.  General
  // results appear before suggestions, which means that the muxer visits the
  // "foobar" SERP before visiting the "foobar" form history, and so it doesn't
  // see that the SERP dupes the form history.  The "foobar" SERP is therefore
  // included.
  context = createContext("foo", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
      makeVisitResult(context, {
        uri: "http://localhost:9000/search?terms=food",
        title: "test visit for http://localhost:9000/search?terms=food",
      }),
      makeVisitResult(context, {
        uri: "http://localhost:9000/search?terms=foobar",
        title: "test visit for http://localhost:9000/search?terms=foobar",
      }),
      makeFormHistoryResult(context, {
        suggestion: "foobar",
        engineName: ENGINE_NAME,
      }),
      ...makeExpectedRemoteSuggestionResults(context, {
        suggestionPrefix: "foo",
      }),
      makeFormHistoryResult(context, {
        suggestion: "fooquux",
        engineName: ENGINE_NAME,
      }),
    ],
  });

  // Now use Firefox's default match buckets, where suggestions appear before
  // general results.  Now the muxer will see that the "foobar" SERP dupes the
  // "foobar" form history, so it will exclude the SERP.
  Services.prefs.setCharPref(
    "browser.urlbar.matchBuckets",
    "suggestion:4,general:Infinity"
  );
  context = createContext("foo", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
      // Note that the remote suggestions appear in between the two form history
      // results.  Ideally the form history would appear together before the
      // remote suggestions, but they don't because the actual first form
      // history result duped the heuristic, so the muxer discarded it.
      makeFormHistoryResult(context, {
        suggestion: "foobar",
        engineName: ENGINE_NAME,
      }),
      ...makeExpectedRemoteSuggestionResults(context, {
        suggestionPrefix: "foo",
      }),
      makeFormHistoryResult(context, {
        suggestion: "fooquux",
        engineName: ENGINE_NAME,
      }),
      makeVisitResult(context, {
        uri: "http://localhost:9000/search?terms=food",
        title: "test visit for http://localhost:9000/search?terms=food",
      }),
    ],
  });
  Services.prefs.setCharPref(
    "browser.urlbar.matchBuckets",
    MATCH_BUCKETS_VALUE
  );

  await PlacesUtils.history.clear();

  await UrlbarTestUtils.formHistory.remove(formHistoryStrings);

  await cleanUpSuggestions();
  await PlacesUtils.history.clear();
  Services.prefs.clearUserPref(MAX_FORM_HISTORY_PREF);
});
