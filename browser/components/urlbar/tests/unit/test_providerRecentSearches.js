/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  UrlbarProviderRecentSearches:
    "resource:///modules/UrlbarProviderRecentSearches.sys.mjs",
});

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

async function addSearches(searches = TEST_SEARCHES) {
  await UrlbarTestUtils.formHistory.add(
    searches.map(value => ({
      value,
      source: defaultEngine.name,
    }))
  );
}

add_setup(async () => {
  defaultEngine = await addTestSuggestionsEngine();
  await Services.search.setDefault(
    defaultEngine,
    Ci.nsISearchService.CHANGE_REASON_ADDON_INSTALL
  );

  let oldCurrentEngine = Services.search.defaultEngine;

  registerCleanupFunction(() => {
    Services.search.defaultEngine = oldCurrentEngine;
    Services.prefs.clearUserPref(RECENTSEARCHES_ENABLED_PREF);
    Services.prefs.clearUserPref(RECENTSEARCHES_SUGGESTS_PREF);
  });
});

add_task(async function test_enabled() {
  Services.prefs.setBoolPref(RECENTSEARCHES_ENABLED_PREF, true);
  Services.prefs.setBoolPref(RECENTSEARCHES_SUGGESTS_PREF, true);
  await addSearches();
  let context = createContext("", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeRecentSearchResult(context, defaultEngine, "Joy Formidable"),
      makeRecentSearchResult(context, defaultEngine, "Glasgow Weather"),
      makeRecentSearchResult(context, defaultEngine, "Bob Vylan"),
    ],
  });
});

add_task(async function test_disabled() {
  Services.prefs.setBoolPref(RECENTSEARCHES_ENABLED_PREF, false);
  Services.prefs.setBoolPref(RECENTSEARCHES_SUGGESTS_PREF, false);
  await addSearches();
  await check_results({
    context: createContext("", { isPrivate: false }),
    matches: [],
  });
});

add_task(async function test_most_recent_shown() {
  Services.prefs.setBoolPref(RECENTSEARCHES_ENABLED_PREF, true);
  Services.prefs.setBoolPref(RECENTSEARCHES_SUGGESTS_PREF, true);

  await addSearches(Array.from(Array(10).keys()).map(i => `Search ${i}`));
  let context = createContext("", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeRecentSearchResult(context, defaultEngine, "Search 9"),
      makeRecentSearchResult(context, defaultEngine, "Search 8"),
      makeRecentSearchResult(context, defaultEngine, "Search 7"),
      makeRecentSearchResult(context, defaultEngine, "Search 6"),
      makeRecentSearchResult(context, defaultEngine, "Search 5"),
    ],
  });
  await UrlbarTestUtils.formHistory.clear();
});

add_task(async function test_per_engine() {
  Services.prefs.setBoolPref(RECENTSEARCHES_ENABLED_PREF, true);
  Services.prefs.setBoolPref(RECENTSEARCHES_SUGGESTS_PREF, true);

  let oldEngine = defaultEngine;
  await addSearches();

  defaultEngine = await addTestSuggestionsEngine(null, {
    name: "NewTestEngine",
  });
  await Services.search.setDefault(
    defaultEngine,
    Ci.nsISearchService.CHANGE_REASON_ADDON_INSTALL
  );

  await addSearches();

  let context = createContext("", {
    isPrivate: false,
    formHistoryName: "test",
  });
  await check_results({
    context,
    matches: [
      makeRecentSearchResult(context, defaultEngine, "Joy Formidable"),
      makeRecentSearchResult(context, defaultEngine, "Glasgow Weather"),
      makeRecentSearchResult(context, defaultEngine, "Bob Vylan"),
    ],
  });

  info("Delete one of the results");
  UrlbarProviderRecentSearches.onEngagement(
    null,
    context,
    { selType: "dismiss", result: context.results[0] },
    { removeResult: () => {} }
  );

  info("The result should be deleted");
  context = createContext("", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeRecentSearchResult(context, defaultEngine, "Glasgow Weather"),
      makeRecentSearchResult(context, defaultEngine, "Bob Vylan"),
    ],
  });

  defaultEngine = oldEngine;
  await Services.search.setDefault(
    defaultEngine,
    Ci.nsISearchService.CHANGE_REASON_ADDON_INSTALL
  );

  info("The same search term still exists on different engine");
  context = createContext("", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeRecentSearchResult(context, defaultEngine, "Joy Formidable"),
      makeRecentSearchResult(context, defaultEngine, "Glasgow Weather"),
      makeRecentSearchResult(context, defaultEngine, "Bob Vylan"),
    ],
  });
});
