/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
});

let ENABLED_PREF = "recentsearches.featureGate";
let EXPIRE_PREF = "recentsearches.expirationMs";
let SUGGESTS_PREF = "suggest.recentsearches";

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
  // Add the searches sequentially so they get a new timestamp
  // and we can order by the time added.
  for (let search of searches) {
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    await new Promise(r => setTimeout(r, 10));
    await UrlbarTestUtils.formHistory.add([
      { value: search, source: defaultEngine.name },
    ]);
  }
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
    UrlbarPrefs.clear(ENABLED_PREF);
    UrlbarPrefs.clear(SUGGESTS_PREF);
  });
});

add_task(async function test_enabled() {
  UrlbarPrefs.set(ENABLED_PREF, true);
  UrlbarPrefs.set(SUGGESTS_PREF, true);
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
  UrlbarPrefs.set(ENABLED_PREF, false);
  UrlbarPrefs.set(SUGGESTS_PREF, false);
  await addSearches();
  await check_results({
    context: createContext("", { isPrivate: false }),
    matches: [],
  });
});

add_task(async function test_most_recent_shown() {
  UrlbarPrefs.set(ENABLED_PREF, true);
  UrlbarPrefs.set(SUGGESTS_PREF, true);

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
  UrlbarPrefs.set(ENABLED_PREF, true);
  UrlbarPrefs.set(SUGGESTS_PREF, true);

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

  defaultEngine = oldEngine;
  await Services.search.setDefault(
    defaultEngine,
    Ci.nsISearchService.CHANGE_REASON_ADDON_INSTALL
  );

  info("We only show searches made since last default engine change");
  context = createContext("", { isPrivate: false });
  await check_results({
    context,
    matches: [],
  });
  await UrlbarTestUtils.formHistory.clear();
});

add_task(async function test_expiry() {
  UrlbarPrefs.set(ENABLED_PREF, true);
  UrlbarPrefs.set(SUGGESTS_PREF, true);
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

  let shortExpiration = 100;
  UrlbarPrefs.set(EXPIRE_PREF, shortExpiration.toString());
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(r => setTimeout(r, shortExpiration * 2));

  await check_results({
    context: createContext("", { isPrivate: false }),
    matches: [],
  });
});
