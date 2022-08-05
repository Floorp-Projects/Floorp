/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that tailed search engine suggestions are returned by
 * UrlbarProviderSearchSuggestions when available.
 */

const SUGGEST_ENABLED_PREF = "browser.search.suggest.enabled";
const PRIVATE_SEARCH_PREF = "browser.search.separatePrivateDefault.ui.enabled";
const TAIL_SUGGESTIONS_PREF = "browser.urlbar.richSuggestions.tail";

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
  let engine = await addTestTailSuggestionsEngine(searchStr => {
    return suggestionsFn(searchStr);
  });
  setSuggestionsFn(searchStr => {
    let suffixes = ["toronto", "tunisia"];
    return [
      "what time is it in t",
      suffixes.map(s => searchStr + s.slice(1)),
      [],
      {
        "google:irrelevantparameter": [],
        "google:suggestdetail": suffixes.map(s => ({
          mp: "… ",
          t: s,
        })),
      },
    ];
  });

  // Install the test engine.
  let oldDefaultEngine = await Services.search.getDefault();
  registerCleanupFunction(async () => {
    Services.search.setDefault(oldDefaultEngine);
    Services.prefs.clearUserPref(PRIVATE_SEARCH_PREF);
    Services.prefs.clearUserPref(TAIL_SUGGESTIONS_PREF);
    Services.prefs.clearUserPref(SUGGEST_ENABLED_PREF);
    UrlbarPrefs.clear("resultGroups");
  });
  Services.search.setDefault(engine);
  Services.prefs.setBoolPref(PRIVATE_SEARCH_PREF, false);
  Services.prefs.setBoolPref(TAIL_SUGGESTIONS_PREF, true);
  Services.prefs.setBoolPref(SUGGEST_ENABLED_PREF, true);
});

/**
 * Tests that non-tail suggestion providers still return results correctly when
 * the tailSuggestions pref is enabled.
 */
add_task(async function normal_suggestions_provider() {
  let engine = await addTestSuggestionsEngine();
  let tailEngine = await Services.search.getDefault();
  Services.search.setDefault(engine);

  const query = "hello world";
  let context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        suggestion: query + " foo",
      }),
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        suggestion: query + " bar",
      }),
    ],
  });

  Services.search.setDefault(tailEngine);
  await cleanUpSuggestions();
});

/**
 * Tests a suggestions provider that returns only tail suggestions.
 */
add_task(async function basic_tail() {
  const query = "what time is it in t";
  let context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: TAIL_SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeSearchResult(context, {
        engineName: TAIL_SUGGESTIONS_ENGINE_NAME,
        suggestion: query + "oronto",
        tail: "toronto",
      }),
      makeSearchResult(context, {
        engineName: TAIL_SUGGESTIONS_ENGINE_NAME,
        suggestion: query + "unisia",
        tail: "tunisia",
      }),
    ],
  });
  await cleanUpSuggestions();
});

/**
 * Tests a suggestions provider that returns both normal and tail suggestions.
 * Only normal results should be shown.
 */
add_task(async function mixed_suggestions() {
  // When normal suggestions are mixed with tail suggestions, they appear at the
  // correct position in the google:suggestdetail array as empty objects.
  setSuggestionsFn(searchStr => {
    let suffixes = ["toronto", "tunisia"];
    return [
      "what time is it in t",
      ["what is the time today texas"].concat(
        suffixes.map(s => searchStr + s.slice(1))
      ),
      [],
      {
        "google:irrelevantparameter": [],
        "google:suggestdetail": [{}].concat(
          suffixes.map(s => ({
            mp: "… ",
            t: s,
          }))
        ),
      },
    ];
  });

  const query = "what time is it in t";
  let context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: TAIL_SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeSearchResult(context, {
        engineName: TAIL_SUGGESTIONS_ENGINE_NAME,
        suggestion: "what is the time today texas",
        tail: undefined,
      }),
    ],
  });
  await cleanUpSuggestions();
});

/**
 * Tests a suggestions provider that returns both normal and tail suggestions,
 * with tail suggestions listed before normal suggestions.  In the real world
 * we don't expect that to happen, but we should handle it by showing only the
 * normal suggestions.
 */
add_task(async function mixed_suggestions_tail_first() {
  setSuggestionsFn(searchStr => {
    let suffixes = ["toronto", "tunisia"];
    return [
      "what time is it in t",
      suffixes
        .map(s => searchStr + s.slice(1))
        .concat(["what is the time today texas"]),
      [],
      {
        "google:irrelevantparameter": [],
        "google:suggestdetail": suffixes
          .map(s => ({
            mp: "… ",
            t: s,
          }))
          .concat([{}]),
      },
    ];
  });

  const query = "what time is it in t";
  let context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: TAIL_SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeSearchResult(context, {
        engineName: TAIL_SUGGESTIONS_ENGINE_NAME,
        suggestion: "what is the time today texas",
        tail: undefined,
      }),
    ],
  });
  await cleanUpSuggestions();
});

/**
 * Tests a search that returns history results, bookmark results and tail
 * suggestions. Only the history and bookmark results should be shown.
 */
add_task(async function mixed_results() {
  await PlacesTestUtils.addVisits([
    {
      uri: Services.io.newURI("http://example.com/1"),
      title: "what time is",
    },
  ]);

  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: "http://example.com/2",
    title: "what time is",
  });

  // Tail suggestions should not be shown.
  const query = "what time is";
  let context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: TAIL_SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeBookmarkResult(context, {
        uri: "http://example.com/2",
        title: "what time is",
      }),
      makeVisitResult(context, {
        uri: "http://example.com/1",
        title: "what time is",
      }),
    ],
  });

  // Once we make the query specific enough to exclude the history and bookmark
  // results, we should show tail suggestions.
  const tQuery = "what time is it in t";
  context = createContext(tQuery, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: TAIL_SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeSearchResult(context, {
        engineName: TAIL_SUGGESTIONS_ENGINE_NAME,
        suggestion: tQuery + "oronto",
        tail: "toronto",
      }),
      makeSearchResult(context, {
        engineName: TAIL_SUGGESTIONS_ENGINE_NAME,
        suggestion: tQuery + "unisia",
        tail: "tunisia",
      }),
    ],
  });

  await cleanUpSuggestions();
});

/**
 * Tests that tail suggestions are deduped if their full-text form is a dupe of
 * a local search suggestion. Remaining tail suggestions should also not be
 * shown since we do not mix tail and non-tail suggestions.
 */
add_task(async function dedupe_local() {
  Services.prefs.setIntPref("browser.urlbar.maxHistoricalSearchSuggestions", 1);
  await UrlbarTestUtils.formHistory.add(["what time is it in toronto"]);

  const query = "what time is it in t";
  let context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: TAIL_SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeFormHistoryResult(context, {
        engineName: TAIL_SUGGESTIONS_ENGINE_NAME,
        suggestion: query + "oronto",
      }),
    ],
  });

  Services.prefs.clearUserPref("browser.urlbar.maxHistoricalSearchSuggestions");
  await cleanUpSuggestions();
});

/**
 * Tests that the correct number of suggestion results are displayed if
 * maxResults is limited, even when tail suggestions are returned.
 */
add_task(async function limit_results() {
  await UrlbarTestUtils.formHistory.clear();
  const query = "what time is it in t";
  let context = createContext(query, { isPrivate: false });
  context.maxResults = 2;
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: TAIL_SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeSearchResult(context, {
        engineName: TAIL_SUGGESTIONS_ENGINE_NAME,
        suggestion: query + "oronto",
        tail: "toronto",
      }),
    ],
  });
  await cleanUpSuggestions();
});

/**
 * Tests that tail suggestions are hidden if the pref is disabled.
 */
add_task(async function disable_pref() {
  let oldPrefValue = Services.prefs.getBoolPref(TAIL_SUGGESTIONS_PREF);
  Services.prefs.setBoolPref(TAIL_SUGGESTIONS_PREF, false);
  const query = "what time is it in t";
  let context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: TAIL_SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
    ],
  });

  Services.prefs.setBoolPref(TAIL_SUGGESTIONS_PREF, oldPrefValue);
  await cleanUpSuggestions();
});
