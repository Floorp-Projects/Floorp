/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that rich suggestion results results are shown without
 * rich data if richSuggestions are disabled.
 */

const SUGGEST_ENABLED_PREF = "browser.search.suggest.enabled";
const RICH_SUGGESTIONS_PREF = "browser.urlbar.richSuggestions.featureGate";
const QUICKACTIONS_URLBAR_PREF = "quickactions.enabled";

add_setup(async function () {
  let engine = await addTestTailSuggestionsEngine(defaultRichSuggestionsFn);
  // Install the test engine.
  let oldDefaultEngine = await Services.search.getDefault();
  registerCleanupFunction(async () => {
    Services.search.setDefault(
      oldDefaultEngine,
      Ci.nsISearchService.CHANGE_REASON_UNKNOWN
    );
    Services.prefs.clearUserPref(RICH_SUGGESTIONS_PREF);
    Services.prefs.clearUserPref(SUGGEST_ENABLED_PREF);
    UrlbarPrefs.clear(QUICKACTIONS_URLBAR_PREF);
  });
  Services.search.setDefault(engine, Ci.nsISearchService.CHANGE_REASON_UNKNOWN);
  Services.prefs.setBoolPref(RICH_SUGGESTIONS_PREF, true);
  Services.prefs.setBoolPref(SUGGEST_ENABLED_PREF, true);
  UrlbarPrefs.set(QUICKACTIONS_URLBAR_PREF, false);
});

/**
 * Test that suggestions with rich data are still shown
 */
add_task(async function test_richsuggestions_disabled() {
  Services.prefs.setBoolPref(RICH_SUGGESTIONS_PREF, false);

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
      }),
      makeSearchResult(context, {
        engineName: TAIL_SUGGESTIONS_ENGINE_NAME,
        suggestion: query + "unisia",
      }),
      makeSearchResult(context, {
        engineName: TAIL_SUGGESTIONS_ENGINE_NAME,
        suggestion: query + "acoma",
      }),
      makeSearchResult(context, {
        engineName: TAIL_SUGGESTIONS_ENGINE_NAME,
        suggestion: query + "aipei",
      }),
    ],
  });
});
