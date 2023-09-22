/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

testEngine_setup();

const engineDomain = "s.example.com";
add_setup(async function () {
  Services.prefs.setBoolPref("browser.urlbar.restyleSearches", true);
  await SearchTestUtils.installSearchExtension({
    name: "MozSearch",
    search_url: `https://${engineDomain}/search`,
  });
  registerCleanupFunction(async () => {
    Services.prefs.clearUserPref("browser.urlbar.restyleSearches");
  });
});

add_task(async function test_searchEngine() {
  let uri = Services.io.newURI(`https://${engineDomain}/search?q=Terms`);
  await PlacesTestUtils.addVisits({
    uri,
    title: "Terms - SearchEngine Search",
  });

  info("Past search terms should be styled.");
  let context = createContext("term", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeFormHistoryResult(context, {
        engineName: "MozSearch",
        suggestion: "Terms",
      }),
    ],
  });

  info(
    "Searching for a superset of the search string in history should not restyle."
  );
  context = createContext("Terms Foo", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
    ],
  });

  info("Bookmarked past searches should not be restyled");
  await PlacesTestUtils.addBookmarkWithDetails({
    uri,
    title: "Terms - SearchEngine Search",
  });

  context = createContext("term", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeBookmarkResult(context, {
        uri: uri.spec,
        title: "Terms - SearchEngine Search",
      }),
    ],
  });

  await PlacesUtils.bookmarks.eraseEverything();

  info("Past search terms should not be styled if restyling is disabled");
  Services.prefs.setBoolPref("browser.urlbar.restyleSearches", false);
  context = createContext("term", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: uri.spec,
        title: "Terms - SearchEngine Search",
      }),
    ],
  });
  Services.prefs.setBoolPref("browser.urlbar.restyleSearches", true);

  await cleanupPlaces();
});

add_task(async function test_extraneousParameters() {
  info("SERPs in history with extraneous parameters should not be restyled.");
  let uri = Services.io.newURI(
    `https://${engineDomain}/search?q=Terms&p=2&type=img`
  );
  await PlacesTestUtils.addVisits({
    uri,
    title: "Terms - SearchEngine Search",
  });

  let context = createContext("term", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: uri.spec,
        title: "Terms - SearchEngine Search",
      }),
    ],
  });
});
