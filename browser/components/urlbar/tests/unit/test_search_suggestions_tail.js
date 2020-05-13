/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that tailed search engine suggestions are returned by
 * UrlbarProviderSearchSuggestions when available.
 */

const { FormHistory } = ChromeUtils.import(
  "resource://gre/modules/FormHistory.jsm"
);

const ENGINE_NAME = "engine-tail-suggestions.xml";
const SUGGEST_PREF = "browser.urlbar.suggest.searches";
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
  Services.prefs.setCharPref(
    "browser.urlbar.matchBuckets",
    "general:5,suggestion:Infinity"
  );

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
  });
  Services.search.setDefault(engine);
  Services.prefs.setBoolPref(PRIVATE_SEARCH_PREF, false);
  Services.prefs.setBoolPref(TAIL_SUGGESTIONS_PREF, true);
  Services.prefs.setBoolPref(SUGGEST_ENABLED_PREF, true);

  // We must make sure the FormHistoryStartup component is initialized.
  Cc["@mozilla.org/satchel/form-history-startup;1"]
    .getService(Ci.nsIObserver)
    .observe(null, "profile-after-change", null);
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
        engineName: "engine-suggestions.xml",
        heuristic: true,
      }),
      makeSearchResult(context, {
        engineName: "engine-suggestions.xml",
        suggestion: query + " foo",
      }),
      makeSearchResult(context, {
        engineName: "engine-suggestions.xml",
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
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        suggestion: query + "oronto",
        tail: "… toronto",
      }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        suggestion: query + "unisia",
        tail: "… tunisia",
      }),
    ],
  });
  await cleanUpSuggestions();
});

/**
 * Tests a suggestions provider that returns both normal and tail suggestions.
 */
add_task(async function mixed_results() {
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
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        suggestion: "what is the time today texas",
        tail: undefined,
      }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        suggestion: query + "oronto",
        tail: "… toronto",
      }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        suggestion: query + "unisia",
        tail: "… tunisia",
      }),
    ],
  });
  await cleanUpSuggestions();
});

/**
 * Tests that tail suggestions are deduped if their full-text form is a dupe of
 * a local search suggestion.
 */
add_task(async function dedupe_local() {
  Services.prefs.setIntPref("browser.urlbar.maxHistoricalSearchSuggestions", 1);
  await updateSearchHistory("bump", "what time is it in toronto");

  const query = "what time is it in t";
  let context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        suggestion: query + "oronto",
        tail: undefined,
      }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        suggestion: query + "unisia",
        tail: "… tunisia",
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
  const query = "what time is it in t";
  let context = createContext(query, { isPrivate: false });
  context.maxResults = 2;
  await check_results({
    context,
    matches: [
      makeSearchResult(context, { engineName: ENGINE_NAME, heuristic: true }),
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        suggestion: query + "oronto",
        tail: "… toronto",
      }),
    ],
  });
  await cleanUpSuggestions();
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
