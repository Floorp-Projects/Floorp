/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let RECENTSEARCHES_ENABLED_PREF = "browser.urlbar.recentsearches.featureGate";
let RECENTSEARCHES_SUGGESTS_PREF = "browser.urlbar.suggest.recentsearches";

let TEST_SEARCHES = ["Bob Vylan", "Glasgow Weather", "Joy Formidable"];
let defaultEngine;

function makeRecentSearchResult(context, engine, suggestion) {
  let result = makeFormHistoryResult(context, {
    suggestion,
    engineName: engine.name,
  });
  delete result.payload.lowerCaseSuggestion;
  return result;
}

add_setup(async () => {
  defaultEngine = await addTestSuggestionsEngine();
  await Services.search.setDefault(
    defaultEngine,
    Ci.nsISearchService.CHANGE_REASON_ADDON_INSTALL
  );

  let oldCurrentEngine = Services.search.defaultEngine;

  await UrlbarTestUtils.formHistory.add(
    TEST_SEARCHES.map(value => ({
      value,
      source: defaultEngine.name,
    }))
  );

  registerCleanupFunction(() => {
    Services.search.defaultEngine = oldCurrentEngine;
  });
});

add_task(async function test_enabled() {
  Services.prefs.setBoolPref(RECENTSEARCHES_ENABLED_PREF, true);
  Services.prefs.setBoolPref(RECENTSEARCHES_SUGGESTS_PREF, true);
  let context = createContext("", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeRecentSearchResult(context, defaultEngine, "Joy Formidable"),
      makeRecentSearchResult(context, defaultEngine, "Glasgow Weather"),
      makeRecentSearchResult(context, defaultEngine, "Bob Vylan"),
    ],
  });
  Services.prefs.clearUserPref(RECENTSEARCHES_ENABLED_PREF);
  Services.prefs.clearUserPref(RECENTSEARCHES_SUGGESTS_PREF);
});

add_task(async function test_disabled() {
  Services.prefs.setBoolPref(RECENTSEARCHES_ENABLED_PREF, false);
  Services.prefs.setBoolPref(RECENTSEARCHES_SUGGESTS_PREF, false);
  await check_results({
    context: createContext("", { isPrivate: false }),
    matches: [],
  });
});
