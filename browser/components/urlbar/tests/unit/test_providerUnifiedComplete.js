/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// This is a simple test to check the UnifiedComplete provider works, it is not
// intended to check all the edge cases, because that component is already
// covered by a good amount of tests.

const SUGGEST_PREF = "browser.urlbar.suggest.searches";
const SUGGEST_ENABLED_PREF = "browser.search.suggest.enabled";

add_task(async function test_unifiedComplete() {
  Services.prefs.setBoolPref(SUGGEST_PREF, true);
  Services.prefs.setBoolPref(SUGGEST_ENABLED_PREF, true);
  let engine = await addTestSuggestionsEngine();
  Services.search.defaultEngine = engine;
  let oldCurrentEngine = Services.search.defaultEngine;
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref(SUGGEST_PREF);
    Services.prefs.clearUserPref(SUGGEST_ENABLED_PREF);
    Services.search.defaultEngine = oldCurrentEngine;
  });

  let controller = new UrlbarController({
    browserWindow: {
      location: {
        href: AppConstants.BROWSER_CHROME_URL,
      },
    },
  });
  let context = createContext("moz", {isPrivate: false});

  // Add entries from multiple sources.
  await PlacesTestUtils.addVisits("https://history.mozilla.org/");
  await PlacesUtils.bookmarks.insert({
    url: "https://bookmark.mozilla.org/",
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
  });
  await PlacesTestUtils.addVisits("https://tab.mozilla.org/");
  UrlbarProviderOpenTabs.registerOpenTab("https://tab.mozilla.org/", 0);

  await controller.startQuery(context);
  dump(context.results.map(m => m.title + " " + m.payload.url) + "\n");
  Assert.deepEqual([
    UrlbarUtils.MATCH_TYPE.SEARCH,
    UrlbarUtils.MATCH_TYPE.SEARCH,
    UrlbarUtils.MATCH_TYPE.SEARCH,
    UrlbarUtils.MATCH_TYPE.URL,
    UrlbarUtils.MATCH_TYPE.TAB_SWITCH,
    UrlbarUtils.MATCH_TYPE.URL,
  ], context.results.map(m => m.type), "Check matches");
  Assert.equal(context.results.length, 6, "Found the expected number of matches");
});
