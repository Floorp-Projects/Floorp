/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// This is a simple test to check the UnifiedComplete provider works, it is not
// intended to check all the edge cases, because that component is already
// covered by a good amount of tests.

const { AddonTestUtils } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm"
);

const SUGGEST_PREF = "browser.urlbar.suggest.searches";
const SUGGEST_ENABLED_PREF = "browser.search.suggest.enabled";

AddonTestUtils.init(this, false);
AddonTestUtils.overrideCertDB();
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "1",
  "42"
);

add_task(async function setup() {
  await AddonTestUtils.promiseStartupManager();
});

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

  let controller = UrlbarTestUtils.newMockController();
  // Also check case insensitivity.
  let searchString = "MoZ oRg";
  let context = createContext(searchString, { isPrivate: false });

  // Add entries from multiple sources.
  await PlacesUtils.bookmarks.insert({
    url: "https://bookmark.mozilla.org/",
    title: "Test bookmark",
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
  });
  PlacesUtils.tagging.tagURI(
    Services.io.newURI("https://bookmark.mozilla.org/"),
    ["mozilla", "org", "ham", "moz", "bacon"]
  );
  await PlacesTestUtils.addVisits([
    { uri: "https://history.mozilla.org/", title: "Test history" },
    { uri: "https://tab.mozilla.org/", title: "Test tab" },
  ]);
  UrlbarProviderOpenTabs.registerOpenTab("https://tab.mozilla.org/", 0);

  await controller.startQuery(context);

  info(
    "Results:\n" +
      context.results.map(m => `${m.title} - ${m.payload.url}`).join("\n")
  );
  Assert.equal(
    context.results.length,
    6,
    "Found the expected number of matches"
  );

  Assert.deepEqual(
    [
      UrlbarUtils.RESULT_TYPE.SEARCH,
      UrlbarUtils.RESULT_TYPE.SEARCH,
      UrlbarUtils.RESULT_TYPE.SEARCH,
      UrlbarUtils.RESULT_TYPE.URL,
      UrlbarUtils.RESULT_TYPE.TAB_SWITCH,
      UrlbarUtils.RESULT_TYPE.URL,
    ],
    context.results.map(m => m.type),
    "Check result types"
  );

  Assert.deepEqual(
    [
      searchString,
      searchString + " foo",
      searchString + " bar",
      "Test bookmark",
      "Test tab",
      "Test history",
    ],
    context.results.map(m => m.title),
    "Check match titles"
  );

  Assert.deepEqual(
    context.results[3].payload.tags,
    ["moz", "mozilla", "org"],
    "Check tags"
  );

  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.eraseEverything();
  UrlbarProviderOpenTabs.unregisterOpenTab("https://tab.mozilla.org/", 0);
});

add_task(async function test_bookmarkBehaviorDisabled_tagged() {
  Services.prefs.setBoolPref(SUGGEST_PREF, false);
  Services.prefs.setBoolPref(SUGGEST_ENABLED_PREF, false);

  // Disable the bookmark behavior in UnifiedComplete.
  Services.prefs.setBoolPref("browser.urlbar.suggest.bookmark", false);

  let controller = UrlbarTestUtils.newMockController();
  // Also check case insensitivity.
  let searchString = "MoZ oRg";
  let context = createContext(searchString, { isPrivate: false });

  // Add a tagged bookmark that's also visited.
  await PlacesUtils.bookmarks.insert({
    url: "https://bookmark.mozilla.org/",
    title: "Test bookmark",
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
  });
  PlacesUtils.tagging.tagURI(
    Services.io.newURI("https://bookmark.mozilla.org/"),
    ["mozilla", "org", "ham", "moz", "bacon"]
  );
  await PlacesTestUtils.addVisits("https://bookmark.mozilla.org/");

  await controller.startQuery(context);

  info(
    "Results:\n" +
      context.results.map(m => `${m.title} - ${m.payload.url}`).join("\n")
  );
  Assert.equal(
    context.results.length,
    2,
    "Found the expected number of matches"
  );

  Assert.deepEqual(
    [UrlbarUtils.RESULT_TYPE.SEARCH, UrlbarUtils.RESULT_TYPE.URL],
    context.results.map(m => m.type),
    "Check result types"
  );

  Assert.deepEqual(
    [searchString, "Test bookmark"],
    context.results.map(m => m.title),
    "Check match titles"
  );

  Assert.deepEqual(context.results[1].payload.tags, [], "Check tags");

  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.eraseEverything();
});

add_task(async function test_bookmarkBehaviorDisabled_untagged() {
  Services.prefs.setBoolPref(SUGGEST_PREF, false);
  Services.prefs.setBoolPref(SUGGEST_ENABLED_PREF, false);

  // Disable the bookmark behavior in UnifiedComplete.
  Services.prefs.setBoolPref("browser.urlbar.suggest.bookmark", false);

  let controller = UrlbarTestUtils.newMockController();
  // Also check case insensitivity.
  let searchString = "MoZ oRg";
  let context = createContext(searchString, { isPrivate: false });

  // Add an *untagged* bookmark that's also visited.
  await PlacesUtils.bookmarks.insert({
    url: "https://bookmark.mozilla.org/",
    title: "Test bookmark",
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
  });
  await PlacesTestUtils.addVisits("https://bookmark.mozilla.org/");

  await controller.startQuery(context);

  info(
    "Results:\n" +
      context.results.map(m => `${m.title} - ${m.payload.url}`).join("\n")
  );
  Assert.equal(
    context.results.length,
    2,
    "Found the expected number of matches"
  );

  Assert.deepEqual(
    [UrlbarUtils.RESULT_TYPE.SEARCH, UrlbarUtils.RESULT_TYPE.URL],
    context.results.map(m => m.type),
    "Check result types"
  );

  Assert.deepEqual(
    [searchString, "Test bookmark"],
    context.results.map(m => m.title),
    "Check match titles"
  );

  Assert.deepEqual(context.results[1].payload.tags, [], "Check tags");

  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.eraseEverything();
});
