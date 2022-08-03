/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test for bug 417798 to make sure javascript: URIs don't show up unless the
 * user searches for javascript: explicitly.
 */

testEngine_setup();

add_task(async function test_javascript_match() {
  Services.prefs.setBoolPref("browser.urlbar.autoFill.searchEngines", false);
  Services.prefs.setBoolPref("browser.urlbar.suggest.searches", false);
  Services.prefs.setBoolPref("browser.urlbar.suggest.engines", false);
  Services.prefs.setBoolPref("browser.urlbar.suggest.quickactions", false);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("browser.urlbar.autoFill.searchEngines");
    Services.prefs.clearUserPref("browser.urlbar.suggest.searches");
    Services.prefs.clearUserPref("browser.urlbar.suggest.engines");
    Services.prefs.clearUserPref("browser.urlbar.suggest.quickactions");
  });

  let uri1 = Services.io.newURI("http://abc/def");
  let uri2 = Services.io.newURI("javascript:5");
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: uri2,
    title: "Title with javascript:",
  });
  await PlacesTestUtils.addVisits([
    { uri: uri1, title: "Title with javascript:" },
  ]);

  info("Match non-javascript: with plain search");
  let context = createContext("a", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: uri1.spec,
        title: "Title with javascript:",
      }),
    ],
  });

  info("Match non-javascript: with 'javascript'");
  context = createContext("javascript", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: uri1.spec,
        title: "Title with javascript:",
      }),
    ],
  });

  info("Match non-javascript with 'javascript:'");
  context = createContext("javascript:", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: uri1.spec,
        title: "Title with javascript:",
      }),
    ],
  });

  info("Match nothing with '5 javascript:'");
  context = createContext("5 javascript:", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
    ],
  });

  info("Match non-javascript: with 'a javascript:'");
  context = createContext("a javascript:", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: uri1.spec,
        title: "Title with javascript:",
      }),
    ],
  });

  info("Match non-javascript: and javascript: with 'javascript: a'");
  context = createContext("javascript: a", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        uri: "javascript: a",
        title: "javascript: a",
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: uri1.spec,
        title: "Title with javascript:",
      }),
      makeBookmarkResult(context, {
        uri: uri2.spec,
        iconUri: "chrome://global/skin/icons/defaultFavicon.svg",
        title: "Title with javascript:",
      }),
    ],
  });

  info("Match javascript: with 'javascript: 5'");
  context = createContext("javascript: 5", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        uri: "javascript: 5",
        title: "javascript: 5",
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        heuristic: true,
      }),
      makeBookmarkResult(context, {
        uri: uri2.spec,
        iconUri: "chrome://global/skin/icons/defaultFavicon.svg",
        title: "Title with javascript:",
      }),
    ],
  });

  await cleanupPlaces();
});
