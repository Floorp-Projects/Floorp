/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test for bug 426864 that makes sure searching a space only shows typed pages
 * from history.
 */

testEngine_setup();

add_task(async function test_empty_search() {
  Services.prefs.setBoolPref("browser.urlbar.suggest.searches", false);
  Services.prefs.setBoolPref("browser.urlbar.suggest.quickactions", false);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("browser.urlbar.suggest.searches");
    Services.prefs.clearUserPref("browser.urlbar.suggest.quickactions");
  });

  let uri1 = Services.io.newURI("http://t.foo/1");
  let uri2 = Services.io.newURI("http://t.foo/2");
  let uri3 = Services.io.newURI("http://t.foo/3");
  let uri4 = Services.io.newURI("http://t.foo/4");
  let uri5 = Services.io.newURI("http://t.foo/5");
  let uri6 = Services.io.newURI("http://t.foo/6");
  let uri7 = Services.io.newURI("http://t.foo/7");

  await PlacesTestUtils.addVisits([
    { uri: uri7, title: "title" },
    { uri: uri6, title: "title" },
    { uri: uri4, title: "title" },
    { uri: uri3, title: "title" },
    { uri: uri2, title: "title" },
    { uri: uri1, title: "title" },
  ]);

  await PlacesTestUtils.addBookmarkWithDetails({ uri: uri6, title: "title" });
  await PlacesTestUtils.addBookmarkWithDetails({ uri: uri5, title: "title" });
  await PlacesTestUtils.addBookmarkWithDetails({ uri: uri4, title: "title" });
  await PlacesTestUtils.addBookmarkWithDetails({ uri: uri2, title: "title" });

  await addOpenPages(uri7, 1);

  // Now remove page 6 from history, so it is an unvisited bookmark.
  await PlacesUtils.history.remove(uri6);

  // With the changes above, the sites in descending order of frecency are:
  // uri2
  // uri4
  // uri5
  // uri6
  // uri1
  // uri3
  // uri7

  info("Match everything");
  let context = createContext("foo", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeBookmarkResult(context, {
        uri: uri2.spec,
        title: "title",
      }),
      makeBookmarkResult(context, {
        uri: uri4.spec,
        title: "title",
      }),
      makeBookmarkResult(context, {
        uri: uri5.spec,
        title: "title",
      }),
      makeBookmarkResult(context, {
        uri: uri6.spec,
        title: "title",
      }),
      makeVisitResult(context, { uri: uri1.spec, title: "title" }),
      makeVisitResult(context, { uri: uri3.spec, title: "title" }),
      makeTabSwitchResult(context, { uri: uri7.spec, title: "title" }),
    ],
  });

  info("Match only history");
  context = createContext(`foo ${UrlbarTokenizer.RESTRICT.HISTORY}`, {
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeVisitResult(context, { uri: uri2.spec, title: "title" }),
      makeVisitResult(context, { uri: uri4.spec, title: "title" }),
      makeVisitResult(context, { uri: uri1.spec, title: "title" }),
      makeVisitResult(context, { uri: uri3.spec, title: "title" }),
      makeVisitResult(context, { uri: uri7.spec, title: "title" }),
    ],
  });

  info("Drop-down empty search matches history sorted by frecency desc");
  context = createContext(" ", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        // query is made explict so makeSearchResult doesn't trim it.
        query: " ",
        heuristic: true,
      }),
      makeVisitResult(context, { uri: uri2.spec, title: "title" }),
      makeVisitResult(context, { uri: uri4.spec, title: "title" }),
      makeVisitResult(context, { uri: uri1.spec, title: "title" }),
      makeVisitResult(context, { uri: uri3.spec, title: "title" }),
      makeVisitResult(context, { uri: uri7.spec, title: "title" }),
    ],
  });

  info("Empty search matches only bookmarks when history is disabled");
  Services.prefs.setBoolPref("browser.urlbar.suggest.history", false);
  Services.prefs.setBoolPref("browser.urlbar.suggest.bookmark", true);
  context = createContext(" ", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        // query is made explict so makeSearchResult doesn't trim it.
        query: " ",
        heuristic: true,
      }),
      makeBookmarkResult(context, {
        uri: uri2.spec,
        title: "title",
      }),
      makeBookmarkResult(context, {
        uri: uri4.spec,
        title: "title",
      }),
      makeBookmarkResult(context, {
        uri: uri5.spec,
        title: "title",
      }),
      makeBookmarkResult(context, {
        uri: uri6.spec,
        title: "title",
      }),
    ],
  });

  info(
    "Empty search matches only open tabs when bookmarks and history are disabled"
  );
  Services.prefs.setBoolPref("browser.urlbar.suggest.bookmark", false);
  context = createContext(" ", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        // query is made explict so makeSearchResult doesn't trim it.
        query: " ",
        heuristic: true,
      }),
      makeTabSwitchResult(context, { uri: uri7.spec, title: "title" }),
    ],
  });

  Services.prefs.clearUserPref("browser.urlbar.suggest.history");
  Services.prefs.clearUserPref("browser.urlbar.suggest.bookmark");

  await cleanupPlaces();
});
