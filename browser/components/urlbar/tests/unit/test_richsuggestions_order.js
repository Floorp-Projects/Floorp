/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that rich suggestion results are ordered in the
 * same order they were returned from the API.
 */

const SUGGEST_ENABLED_PREF = "browser.search.suggest.enabled";
const RICH_SUGGESTIONS_PREF = "browser.urlbar.richSuggestions.featureGate";

var suggestionsFn;
var previousSuggestionsFn;

/**
 * Set the current suggestion funciton.
 *
 * @param {Function} fn
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

async function cleanUpSuggestions() {
  if (previousSuggestionsFn) {
    suggestionsFn = previousSuggestionsFn;
    previousSuggestionsFn = null;
  }
}

add_setup(async function () {
  let engine = await addTestTailSuggestionsEngine(searchStr => {
    return suggestionsFn(searchStr);
  });
  setSuggestionsFn(searchStr => {
    let suffixes = ["toronto", "tunisia", "tacoma", "taipei"];
    return [
      "what time is it in t",
      suffixes.map(s => searchStr + s.slice(1)),
      [],
      {
        "google:irrelevantparameter": [],
        "google:suggestdetail": suffixes.map((suffix, i) => {
          // Set every other suggestion as a rich suggestion so we can
          // test how they are handled and ordered when interleaved.
          if (i % 2) {
            return {};
          }
          return {
            a: "description",
            dc: "#FFFFFF",
            i: "data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw==",
            t: "Title",
          };
        }),
      },
    ];
  });

  // Install the test engine.
  let oldDefaultEngine = await Services.search.getDefault();
  registerCleanupFunction(async () => {
    Services.search.setDefault(
      oldDefaultEngine,
      Ci.nsISearchService.CHANGE_REASON_UNKNOWN
    );
    Services.prefs.clearUserPref(RICH_SUGGESTIONS_PREF);
    Services.prefs.clearUserPref(SUGGEST_ENABLED_PREF);
  });
  Services.search.setDefault(engine, Ci.nsISearchService.CHANGE_REASON_UNKNOWN);
  Services.prefs.setBoolPref(RICH_SUGGESTIONS_PREF, true);
  Services.prefs.setBoolPref(SUGGEST_ENABLED_PREF, true);
});

/**
 * Tests that non-tail suggestion providers still return results correctly when
 * the tailSuggestions pref is enabled.
 */
add_task(async function test_richsuggestions_order() {
  const query = "what time is it in t";
  let context = createContext(query, { isPrivate: false });

  let defaultRichResult = {
    engineName: TAIL_SUGGESTIONS_ENGINE_NAME,
    tail: "Title",
    isRichSuggestion: true,
  };

  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: TAIL_SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeSearchResult(
        context,
        Object.assign(defaultRichResult, {
          suggestion: query + "oronto",
        })
      ),
      makeSearchResult(context, {
        engineName: TAIL_SUGGESTIONS_ENGINE_NAME,
        suggestion: query + "unisia",
      }),
      makeSearchResult(
        context,
        Object.assign(defaultRichResult, {
          suggestion: query + "acoma",
        })
      ),
      makeSearchResult(context, {
        engineName: TAIL_SUGGESTIONS_ENGINE_NAME,
        suggestion: query + "aipei",
      }),
    ],
  });

  await cleanUpSuggestions();
});
