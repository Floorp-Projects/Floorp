/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test for bug 395161 that allows special searches that restrict results to
 * history/bookmark/tagged items and title/url matches.
 *
 * Test 485122 by making sure results don't have tags when restricting result
 * to just history either by default behavior or dynamic query restrict.
 */

testEngine_setup();

function setSuggestPrefsToFalse() {
  Services.prefs.setBoolPref("browser.urlbar.suggest.history", false);
  Services.prefs.setBoolPref("browser.urlbar.suggest.bookmark", false);
}

const TRANSITION_TYPED = PlacesUtils.history.TRANSITION_TYPED;

add_task(async function test_special_searches() {
  let uri1 = Services.io.newURI("http://url/");
  let uri2 = Services.io.newURI("http://url/2");
  let uri3 = Services.io.newURI("http://foo.bar/");
  let uri4 = Services.io.newURI("http://foo.bar/2");
  let uri5 = Services.io.newURI("http://url/star");
  let uri6 = Services.io.newURI("http://url/star/2");
  let uri7 = Services.io.newURI("http://foo.bar/star");
  let uri8 = Services.io.newURI("http://foo.bar/star/2");
  let uri9 = Services.io.newURI("http://url/tag");
  let uri10 = Services.io.newURI("http://url/tag/2");
  let uri11 = Services.io.newURI("http://foo.bar/tag");
  let uri12 = Services.io.newURI("http://foo.bar/tag/2");
  await PlacesTestUtils.addVisits([
    { uri: uri11, title: "title", transition: TRANSITION_TYPED },
    { uri: uri6, title: "foo.bar" },
    { uri: uri4, title: "foo.bar", transition: TRANSITION_TYPED },
    { uri: uri3, title: "title" },
    { uri: uri2, title: "foo.bar" },
    { uri: uri1, title: "title", transition: TRANSITION_TYPED },
  ]);

  await PlacesTestUtils.addBookmarkWithDetails({
    uri: uri12,
    title: "foo.bar",
    tags: ["foo.bar"],
  });
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: uri11,
    title: "title",
    tags: ["foo.bar"],
  });
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: uri10,
    title: "foo.bar",
    tags: ["foo.bar"],
  });
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: uri9,
    title: "title",
    tags: ["foo.bar"],
  });
  await PlacesTestUtils.addBookmarkWithDetails({ uri: uri8, title: "foo.bar" });
  await PlacesTestUtils.addBookmarkWithDetails({ uri: uri7, title: "title" });
  await PlacesTestUtils.addBookmarkWithDetails({ uri: uri6, title: "foo.bar" });
  await PlacesTestUtils.addBookmarkWithDetails({ uri: uri5, title: "title" });

  await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();

  // Order of frecency when not restricting, descending:
  // uri11
  // uri1
  // uri4
  // uri6
  // uri5
  // uri7
  // uri8
  // uri9
  // uri10
  // uri12
  // uri2
  // uri3

  // Test restricting searches.

  info("History restrict");
  let context = createContext(UrlbarTokenizer.RESTRICT.HISTORY, {
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeVisitResult(context, { uri: uri11.spec, title: "title" }),
      makeVisitResult(context, { uri: uri1.spec, title: "title" }),
      makeVisitResult(context, { uri: uri4.spec, title: "foo.bar" }),
      makeVisitResult(context, { uri: uri6.spec, title: "foo.bar" }),
      makeVisitResult(context, { uri: uri2.spec, title: "foo.bar" }),
      makeVisitResult(context, { uri: uri3.spec, title: "title" }),
    ],
  });

  info("Star restrict");
  context = createContext(UrlbarTokenizer.RESTRICT.BOOKMARK, {
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeBookmarkResult(context, {
        uri: uri11.spec,
        title: "title",
      }),
      makeBookmarkResult(context, { uri: uri6.spec, title: "foo.bar" }),
      makeBookmarkResult(context, { uri: uri5.spec, title: "title" }),
      makeBookmarkResult(context, { uri: uri7.spec, title: "title" }),
      makeBookmarkResult(context, { uri: uri8.spec, title: "foo.bar" }),
      makeBookmarkResult(context, {
        uri: uri9.spec,
        title: "title",
      }),
      makeBookmarkResult(context, {
        uri: uri10.spec,
        title: "foo.bar",
      }),
      makeBookmarkResult(context, {
        uri: uri12.spec,
        title: "foo.bar",
      }),
    ],
  });

  info("Tag restrict");
  context = createContext(UrlbarTokenizer.RESTRICT.TAG, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeBookmarkResult(context, {
        uri: uri11.spec,
        title: "title",
      }),
      makeBookmarkResult(context, {
        uri: uri9.spec,
        title: "title",
      }),
      makeBookmarkResult(context, {
        uri: uri10.spec,
        title: "foo.bar",
      }),
      makeBookmarkResult(context, {
        uri: uri12.spec,
        title: "foo.bar",
      }),
    ],
  });

  info("Special as first word");
  context = createContext(`${UrlbarTokenizer.RESTRICT.HISTORY} foo bar`, {
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        query: "foo bar",
        alias: UrlbarTokenizer.RESTRICT.HISTORY,
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        heuristic: true,
      }),
      makeVisitResult(context, { uri: uri11.spec, title: "title" }),
      makeVisitResult(context, { uri: uri4.spec, title: "foo.bar" }),
      makeVisitResult(context, { uri: uri6.spec, title: "foo.bar" }),
      makeVisitResult(context, { uri: uri2.spec, title: "foo.bar" }),
      makeVisitResult(context, { uri: uri3.spec, title: "title" }),
    ],
  });

  info("Special as last word");
  context = createContext(`foo bar ${UrlbarTokenizer.RESTRICT.HISTORY}`, {
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeVisitResult(context, { uri: uri11.spec, title: "title" }),
      makeVisitResult(context, { uri: uri4.spec, title: "foo.bar" }),
      makeVisitResult(context, { uri: uri6.spec, title: "foo.bar" }),
      makeVisitResult(context, { uri: uri2.spec, title: "foo.bar" }),
      makeVisitResult(context, { uri: uri3.spec, title: "title" }),
    ],
  });

  // Test restricting and matching searches with a term.

  info(`foo ${UrlbarTokenizer.RESTRICT.HISTORY} -> history`);
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
      makeVisitResult(context, { uri: uri11.spec, title: "title" }),
      makeVisitResult(context, { uri: uri4.spec, title: "foo.bar" }),
      makeVisitResult(context, { uri: uri6.spec, title: "foo.bar" }),
      makeVisitResult(context, { uri: uri2.spec, title: "foo.bar" }),
      makeVisitResult(context, { uri: uri3.spec, title: "title" }),
    ],
  });

  info(`foo ${UrlbarTokenizer.RESTRICT.BOOKMARK} -> is star`);
  context = createContext(`foo ${UrlbarTokenizer.RESTRICT.BOOKMARK}`, {
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeBookmarkResult(context, {
        uri: uri11.spec,
        title: "title",
        tags: ["foo.bar"],
      }),
      makeBookmarkResult(context, { uri: uri6.spec, title: "foo.bar" }),
      makeBookmarkResult(context, { uri: uri7.spec, title: "title" }),
      makeBookmarkResult(context, { uri: uri8.spec, title: "foo.bar" }),
      makeBookmarkResult(context, {
        uri: uri9.spec,
        title: "title",
        tags: ["foo.bar"],
      }),
      makeBookmarkResult(context, {
        uri: uri10.spec,
        title: "foo.bar",
        tags: ["foo.bar"],
      }),
      makeBookmarkResult(context, {
        uri: uri12.spec,
        title: "foo.bar",
        tags: ["foo.bar"],
      }),
    ],
  });

  info(`foo ${UrlbarTokenizer.RESTRICT.TITLE} -> in title`);
  context = createContext(`foo ${UrlbarTokenizer.RESTRICT.TITLE}`, {
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeVisitResult(context, { uri: uri11.spec, title: "title" }),
      makeVisitResult(context, { uri: uri4.spec, title: "foo.bar" }),
      makeBookmarkResult(context, { uri: uri6.spec, title: "foo.bar" }),
      makeBookmarkResult(context, { uri: uri8.spec, title: "foo.bar" }),
      makeVisitResult(context, { uri: uri9.spec, title: "title" }),
      makeVisitResult(context, { uri: uri10.spec, title: "foo.bar" }),
      makeVisitResult(context, { uri: uri12.spec, title: "foo.bar" }),
      makeVisitResult(context, { uri: uri2.spec, title: "foo.bar" }),
    ],
  });

  info(`foo ${UrlbarTokenizer.RESTRICT.URL} -> in url`);
  context = createContext(`foo ${UrlbarTokenizer.RESTRICT.URL}`, {
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeVisitResult(context, { uri: uri11.spec, title: "title" }),
      makeVisitResult(context, { uri: uri4.spec, title: "foo.bar" }),
      makeBookmarkResult(context, { uri: uri7.spec, title: "title" }),
      makeBookmarkResult(context, { uri: uri8.spec, title: "foo.bar" }),
      makeVisitResult(context, { uri: uri12.spec, title: "foo.bar" }),
      makeVisitResult(context, { uri: uri3.spec, title: "title" }),
    ],
  });

  info(`foo ${UrlbarTokenizer.RESTRICT.TAG} -> is tag`);
  context = createContext(`foo ${UrlbarTokenizer.RESTRICT.TAG}`, {
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeBookmarkResult(context, {
        uri: uri11.spec,
        title: "title",
        tags: ["foo.bar"],
      }),
      makeBookmarkResult(context, {
        uri: uri9.spec,
        title: "title",
        tags: ["foo.bar"],
      }),
      makeBookmarkResult(context, {
        uri: uri10.spec,
        title: "foo.bar",
        tags: ["foo.bar"],
      }),
      makeBookmarkResult(context, {
        uri: uri12.spec,
        title: "foo.bar",
        tags: ["foo.bar"],
      }),
    ],
  });

  // Test conflicting restrictions.

  info(
    `conflict ${UrlbarTokenizer.RESTRICT.TITLE} ${UrlbarTokenizer.RESTRICT.URL} -> url wins`
  );
  await PlacesTestUtils.addVisits([
    {
      uri: `http://conflict.com/${UrlbarTokenizer.RESTRICT.TITLE}`,
      title: "test",
    },
    {
      uri: "http://conflict.com/",
      title: `test${UrlbarTokenizer.RESTRICT.TITLE}`,
    },
  ]);
  context = createContext(
    `conflict ${UrlbarTokenizer.RESTRICT.TITLE} ${UrlbarTokenizer.RESTRICT.URL}`,
    { isPrivate: false }
  );
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: `http://conflict.com/${UrlbarTokenizer.RESTRICT.TITLE}`,
        title: "test",
      }),
    ],
  });

  info(
    `conflict ${UrlbarTokenizer.RESTRICT.HISTORY} ${UrlbarTokenizer.RESTRICT.BOOKMARK} -> bookmark wins`
  );
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: "http://bookmark.conflict.com/",
    title: `conflict ${UrlbarTokenizer.RESTRICT.HISTORY}`,
  });
  await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();
  context = createContext(
    `conflict ${UrlbarTokenizer.RESTRICT.HISTORY} ${UrlbarTokenizer.RESTRICT.BOOKMARK}`,
    { isPrivate: false }
  );
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeBookmarkResult(context, {
        uri: "http://bookmark.conflict.com/",
        title: `conflict ${UrlbarTokenizer.RESTRICT.HISTORY}`,
      }),
    ],
  });

  info(
    `conflict ${UrlbarTokenizer.RESTRICT.BOOKMARK} ${UrlbarTokenizer.RESTRICT.TAG} -> tag wins`
  );
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: "http://tag.conflict.com/",
    title: `conflict ${UrlbarTokenizer.RESTRICT.BOOKMARK}`,
    tags: ["one"],
  });
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: "http://nontag.conflict.com/",
    title: `conflict ${UrlbarTokenizer.RESTRICT.BOOKMARK}`,
  });
  await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();
  context = createContext(
    `conflict ${UrlbarTokenizer.RESTRICT.BOOKMARK} ${UrlbarTokenizer.RESTRICT.TAG}`,
    { isPrivate: false }
  );
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeBookmarkResult(context, {
        uri: "http://tag.conflict.com/",
        title: `conflict ${UrlbarTokenizer.RESTRICT.BOOKMARK}`,
      }),
    ],
  });

  // Disable autoFill for the next tests, see test_autoFill_default_behavior.js
  // for specific tests.
  Services.prefs.setBoolPref("browser.urlbar.autoFill", false);

  // Test default usage by setting certain browser.urlbar.suggest.* prefs
  info("foo -> default history");
  setSuggestPrefsToFalse();
  Services.prefs.setBoolPref("browser.urlbar.suggest.history", true);
  context = createContext("foo", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeVisitResult(context, { uri: uri11.spec, title: "title" }),
      makeVisitResult(context, { uri: uri4.spec, title: "foo.bar" }),
      makeVisitResult(context, { uri: uri6.spec, title: "foo.bar" }),
      makeVisitResult(context, { uri: uri2.spec, title: "foo.bar" }),
      makeVisitResult(context, { uri: uri3.spec, title: "title" }),
    ],
  });

  info("foo -> default history, is star");
  setSuggestPrefsToFalse();
  Services.prefs.setBoolPref("browser.urlbar.suggest.history", true);
  Services.prefs.setBoolPref("browser.urlbar.suggest.bookmark", true);
  // The purpose of this test is to verify what is being sent by ProviderPlaces.
  // It will send 10 results, but the heuristic result pushes the last result
  // out of the panel. We set maxRichResults to a high value to test the full
  // output of ProviderPlaces.
  Services.prefs.setIntPref("browser.urlbar.maxRichResults", 20);
  context = createContext("foo", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeBookmarkResult(context, {
        uri: uri11.spec,
        title: "title",
        tags: ["foo.bar"],
      }),
      makeVisitResult(context, { uri: uri4.spec, title: "foo.bar" }),
      makeBookmarkResult(context, { uri: uri6.spec, title: "foo.bar" }),
      makeBookmarkResult(context, { uri: uri7.spec, title: "title" }),
      makeBookmarkResult(context, { uri: uri8.spec, title: "foo.bar" }),
      makeBookmarkResult(context, {
        uri: uri9.spec,
        title: "title",
        tags: ["foo.bar"],
      }),
      makeBookmarkResult(context, {
        uri: uri10.spec,
        title: "foo.bar",
        tags: ["foo.bar"],
      }),
      makeBookmarkResult(context, {
        uri: uri12.spec,
        title: "foo.bar",
        tags: ["foo.bar"],
      }),
      makeVisitResult(context, { uri: uri2.spec, title: "foo.bar" }),
      makeVisitResult(context, { uri: uri3.spec, title: "title" }),
    ],
  });
  Services.prefs.clearUserPref("browser.urlbar.maxRichResults");

  info("foo -> is star");
  setSuggestPrefsToFalse();
  Services.prefs.setBoolPref("browser.urlbar.suggest.history", false);
  Services.prefs.setBoolPref("browser.urlbar.suggest.bookmark", true);
  context = createContext("foo", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeBookmarkResult(context, {
        uri: uri11.spec,
        title: "title",
        tags: ["foo.bar"],
      }),
      makeBookmarkResult(context, { uri: uri6.spec, title: "foo.bar" }),
      makeBookmarkResult(context, { uri: uri7.spec, title: "title" }),
      makeBookmarkResult(context, { uri: uri8.spec, title: "foo.bar" }),
      makeBookmarkResult(context, {
        uri: uri9.spec,
        title: "title",
        tags: ["foo.bar"],
      }),
      makeBookmarkResult(context, {
        uri: uri10.spec,
        title: "foo.bar",
        tags: ["foo.bar"],
      }),
      makeBookmarkResult(context, {
        uri: uri12.spec,
        title: "foo.bar",
        tags: ["foo.bar"],
      }),
    ],
  });

  await cleanupPlaces();
});
