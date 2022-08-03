/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test to make sure matches against the url, title, tags are first made on word
 * boundaries, instead of in the middle of words, and later are extended to the
 * whole words. For this test it is critical to check sorting of the matches.
 *
 * Make sure we don't try matching one after a CamelCase because the upper-case
 * isn't really a word boundary. (bug 429498)
 */

testEngine_setup();

var katakana = ["\u30a8", "\u30c9"]; // E, Do
var ideograph = ["\u4efb", "\u5929", "\u5802"]; // Nin Ten Do

add_task(async function test_escape() {
  Services.prefs.setBoolPref("browser.urlbar.autoFill.searchEngines", false);
  Services.prefs.setBoolPref("browser.urlbar.autoFill", false);
  Services.prefs.setBoolPref("browser.urlbar.suggest.searches", false);
  Services.prefs.setBoolPref("browser.urlbar.suggest.quickactions", false);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("browser.urlbar.suggest.searches");
    Services.prefs.clearUserPref("browser.urlbar.suggest.quickactions");
    Services.prefs.clearUserPref("browser.urlbar.autoFill");
    Services.prefs.clearUserPref("browser.urlbar.autoFill.searchEngines");
  });

  await PlacesTestUtils.addVisits([
    { uri: "http://matchme/", title: "title1" },
    { uri: "http://dontmatchme/", title: "title1" },
    { uri: "http://title/1", title: "matchme2" },
    { uri: "http://title/2", title: "dontmatchme3" },
    { uri: "http://tag/1", title: "title1" },
    { uri: "http://tag/2", title: "title1" },
    { uri: "http://crazytitle/", title: "!@#$%^&*()_+{}|:<>?word" },
    { uri: "http://katakana/", title: katakana.join("") },
    { uri: "http://ideograph/", title: ideograph.join("") },
    { uri: "http://camel/pleaseMatchMe/", title: "title1" },
  ]);
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: "http://tag/1",
    title: "title1",
    tags: ["matchme2"],
  });
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: "http://tag/2",
    title: "title1",
    tags: ["dontmatchme3"],
  });

  info("Match 'match' at the beginning or after / or on a CamelCase");
  let context = createContext("match", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeBookmarkResult(context, {
        uri: "http://tag/1",
        title: "title1",
        tags: ["matchme2"],
      }),
      makeVisitResult(context, {
        uri: "http://camel/pleaseMatchMe/",
        title: "title1",
      }),
      makeVisitResult(context, { uri: "http://title/1", title: "matchme2" }),
      makeVisitResult(context, { uri: "http://matchme/", title: "title1" }),
      makeBookmarkResult(context, {
        uri: "http://tag/2",
        title: "title1",
        tags: ["dontmatchme3"],
      }),
      makeVisitResult(context, {
        uri: "http://title/2",
        title: "dontmatchme3",
      }),
      makeVisitResult(context, { uri: "http://dontmatchme/", title: "title1" }),
    ],
  });

  info("Match 'dont' at the beginning or after /");
  context = createContext("dont", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeBookmarkResult(context, {
        uri: "http://tag/2",
        title: "title1",
        tags: ["dontmatchme3"],
      }),
      makeVisitResult(context, {
        uri: "http://title/2",
        title: "dontmatchme3",
      }),
      makeVisitResult(context, { uri: "http://dontmatchme/", title: "title1" }),
    ],
  });

  info("Match 'match' at the beginning or after / or on a CamelCase");
  context = createContext("2", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeBookmarkResult(context, {
        uri: "http://tag/2",
        title: "title1",
      }),
      makeBookmarkResult(context, {
        uri: "http://tag/1",
        title: "title1",
        tags: ["matchme2"],
      }),
      makeVisitResult(context, {
        uri: "http://title/2",
        title: "dontmatchme3",
      }),
      makeVisitResult(context, { uri: "http://title/1", title: "matchme2" }),
    ],
  });

  info("Match 't' at the beginning or after /");
  context = createContext("t", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeBookmarkResult(context, {
        uri: "http://tag/2",
        title: "title1",
        tags: ["dontmatchme3"],
      }),
      makeBookmarkResult(context, {
        uri: "http://tag/1",
        title: "title1",
        tags: ["matchme2"],
      }),
      makeVisitResult(context, {
        uri: "http://camel/pleaseMatchMe/",
        title: "title1",
      }),
      makeVisitResult(context, {
        uri: "http://title/2",
        title: "dontmatchme3",
      }),
      makeVisitResult(context, { uri: "http://title/1", title: "matchme2" }),
      makeVisitResult(context, { uri: "http://dontmatchme/", title: "title1" }),
      makeVisitResult(context, { uri: "http://matchme/", title: "title1" }),
      makeVisitResult(context, {
        uri: "http://katakana/",
        title: katakana.join(""),
      }),
      makeVisitResult(context, {
        uri: "http://crazytitle/",
        title: "!@#$%^&*()_+{}|:<>?word",
      }),
    ],
  });

  info("Match 'word' after many consecutive word boundaries");
  context = createContext("word", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: "http://crazytitle/",
        title: "!@#$%^&*()_+{}|:<>?word",
      }),
    ],
  });

  info("Match a word boundary '/' for everything");
  context = createContext("/", { isPrivate: false });
  // UNIX platforms can search for a file:// URL by typing a forward slash.
  let heuristicSlashResult =
    AppConstants.platform == "win"
      ? makeSearchResult(context, {
          engineName: SUGGESTIONS_ENGINE_NAME,
          heuristic: true,
        })
      : makeVisitResult(context, {
          uri: "file:///",
          title: "file:///",
          source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
          heuristic: true,
        });
  await check_results({
    context,
    matches: [
      heuristicSlashResult,
      makeBookmarkResult(context, {
        uri: "http://tag/2",
        title: "title1",
      }),
      makeBookmarkResult(context, {
        uri: "http://tag/1",
        title: "title1",
      }),
      makeVisitResult(context, {
        uri: "http://camel/pleaseMatchMe/",
        title: "title1",
      }),
      makeVisitResult(context, {
        uri: "http://ideograph/",
        title: ideograph.join(""),
      }),
      makeVisitResult(context, {
        uri: "http://katakana/",
        title: katakana.join(""),
      }),
      makeVisitResult(context, {
        uri: "http://crazytitle/",
        title: "!@#$%^&*()_+{}|:<>?word",
      }),
      makeVisitResult(context, {
        uri: "http://title/2",
        title: "dontmatchme3",
      }),
      makeVisitResult(context, { uri: "http://title/1", title: "matchme2" }),
      makeVisitResult(context, { uri: "http://dontmatchme/", title: "title1" }),
    ],
  });

  info("Match word boundaries '()_' that are among word boundaries");
  context = createContext("()_", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: "http://crazytitle/",
        title: "!@#$%^&*()_+{}|:<>?word",
      }),
    ],
  });

  info("Katakana characters form a string, so match the beginning");
  context = createContext(katakana[0], { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: "http://katakana/",
        title: katakana.join(""),
      }),
    ],
  });

  /*
  info("Middle of a katakana word shouldn't be matched");
  await check_autocomplete({
    search: katakana[1],
    matches: [ ],
  });
*/

  info("Ideographs are treated as words so 'nin' is one word");
  context = createContext(ideograph[0], { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: "http://ideograph/",
        title: ideograph.join(""),
      }),
    ],
  });

  info("Ideographs are treated as words so 'ten' is another word");
  context = createContext(ideograph[1], { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: "http://ideograph/",
        title: ideograph.join(""),
      }),
    ],
  });

  info("Ideographs are treated as words so 'do' is yet another word");
  context = createContext(ideograph[2], { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: "http://ideograph/",
        title: ideograph.join(""),
      }),
    ],
  });

  info("Match in the middle. Should just be sorted by frecency.");
  context = createContext("ch", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeBookmarkResult(context, {
        uri: "http://tag/2",
        title: "title1",
        tags: ["dontmatchme3"],
      }),
      makeBookmarkResult(context, {
        uri: "http://tag/1",
        title: "title1",
        tags: ["matchme2"],
      }),
      makeVisitResult(context, {
        uri: "http://camel/pleaseMatchMe/",
        title: "title1",
      }),
      makeVisitResult(context, {
        uri: "http://title/2",
        title: "dontmatchme3",
      }),
      makeVisitResult(context, { uri: "http://title/1", title: "matchme2" }),
      makeVisitResult(context, { uri: "http://dontmatchme/", title: "title1" }),
      makeVisitResult(context, { uri: "http://matchme/", title: "title1" }),
    ],
  });

  // Also this test should just be sorted by frecency.
  info(
    "Don't match one character after a camel-case word boundary (bug 429498). Should just be sorted by frecency."
  );
  context = createContext("atch", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeBookmarkResult(context, {
        uri: "http://tag/2",
        title: "title1",
        tags: ["dontmatchme3"],
      }),
      makeBookmarkResult(context, {
        uri: "http://tag/1",
        title: "title1",
        tags: ["matchme2"],
      }),
      makeVisitResult(context, {
        uri: "http://camel/pleaseMatchMe/",
        title: "title1",
      }),
      makeVisitResult(context, {
        uri: "http://title/2",
        title: "dontmatchme3",
      }),
      makeVisitResult(context, { uri: "http://title/1", title: "matchme2" }),
      makeVisitResult(context, { uri: "http://dontmatchme/", title: "title1" }),
      makeVisitResult(context, { uri: "http://matchme/", title: "title1" }),
    ],
  });

  await cleanupPlaces();
});
