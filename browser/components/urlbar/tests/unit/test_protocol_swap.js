/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test bug 424717 to make sure searching with an existing location like
 * http://site/ also matches https://site/ or ftp://site/. Same thing for
 * ftp://site/ and https://site/.
 *
 * Test bug 461483 to make sure a search for "w" doesn't match the "www." from
 * site subdomains.
 */

testEngine_setup();

add_task(async function test_swap_protocol() {
  let uri1 = Services.io.newURI("http://www.site/");
  let uri2 = Services.io.newURI("http://site/");
  let uri3 = Services.io.newURI("ftp://ftp.site/");
  let uri4 = Services.io.newURI("ftp://site/");
  let uri5 = Services.io.newURI("https://www.site/");
  let uri6 = Services.io.newURI("https://site/");
  let uri7 = Services.io.newURI("http://woohoo/");
  let uri8 = Services.io.newURI("http://wwwwwwacko/");
  await PlacesTestUtils.addVisits([
    { uri: uri8, title: "title" },
    { uri: uri7, title: "title" },
    { uri: uri6, title: "title" },
    { uri: uri5, title: "title" },
    { uri: uri4, title: "title" },
    { uri: uri3, title: "title" },
    { uri: uri2, title: "title" },
    { uri: uri1, title: "title" },
  ]);

  // Disable autoFill to avoid handling the first result.
  Services.prefs.setBoolPref("browser.urlbar.autoFill", false);
  Services.prefs.setBoolPref("browser.urlbar.suggest.searches", false);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("browser.urlbar.suggest.searches");
  });

  info("http://www.site matches 'www.site' pages");
  let searchString = "http://www.site";
  let context = createContext(searchString, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        uri: `${searchString}/`,
        title: "title",
        source: UrlbarUtils.RESULT_SOURCE.HISTORY,
        heuristic: true,
      }),
      makeVisitResult(context, { uri: uri5.spec, title: "title" }),
    ],
  });

  info("http://site matches all sites");
  searchString = "http://site";
  context = createContext(searchString, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        uri: `${searchString}/`,
        title: "title",
        source: UrlbarUtils.RESULT_SOURCE.HISTORY,
        heuristic: true,
      }),
      makeVisitResult(context, { uri: uri3.spec, title: "title" }),
      makeVisitResult(context, { uri: uri4.spec, title: "title" }),
      makeVisitResult(context, { uri: uri6.spec, title: "title" }),
    ],
  });

  info("ftp://ftp.site matches itself");
  searchString = "ftp://ftp.site";
  context = createContext(searchString, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        uri: `${searchString}/`,
        fallbackTitle: `${searchString}/`,
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        heuristic: true,
      }),
      makeVisitResult(context, { uri: uri3.spec, title: "title" }),
    ],
  });

  info("ftp://site matches all sites");
  searchString = "ftp://site";
  context = createContext(searchString, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        uri: `${searchString}/`,
        fallbackTitle: `${searchString}/`,
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        heuristic: true,
      }),
      makeVisitResult(context, { uri: uri3.spec, title: "title" }),
      makeVisitResult(context, { uri: uri4.spec, title: "title" }),
      makeVisitResult(context, { uri: uri6.spec, title: "title" }),
    ],
  });

  info("https://www.site matches all sites");
  searchString = "https://www.sit";
  context = createContext(searchString, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        uri: `${searchString}/`,
        fallbackTitle: `${searchString}/`,
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        heuristic: true,
      }),
      makeVisitResult(context, { uri: uri5.spec, title: "title" }),
    ],
  });

  info("https://site matches all sites");
  searchString = "https://sit";
  context = createContext(searchString, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        uri: `${searchString}/`,
        fallbackTitle: `${searchString}/`,
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        heuristic: true,
      }),
      makeVisitResult(context, { uri: uri3.spec, title: "title" }),
      makeVisitResult(context, { uri: uri4.spec, title: "title" }),
      makeVisitResult(context, { uri: uri6.spec, title: "title" }),
    ],
  });

  info("www.site matches 'www.site' pages");
  searchString = "www.site";
  context = createContext(searchString, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        uri: `http://${searchString}/`,
        title: "title",
        source: UrlbarUtils.RESULT_SOURCE.HISTORY,
        heuristic: true,
      }),
      makeVisitResult(context, { uri: uri5.spec, title: "title" }),
    ],
  });

  info("w matches 'w' pages, including 'www'");
  context = createContext("w", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeVisitResult(context, { uri: uri5.spec, title: "title" }),
      makeVisitResult(context, { uri: uri7.spec, title: "title" }),
      makeVisitResult(context, { uri: uri8.spec, title: "title" }),
    ],
  });

  info("http://w matches 'w' pages, including 'www'");
  searchString = "http://w";
  context = createContext(searchString, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        uri: `${searchString}/`,
        fallbackTitle: `${searchString}/`,
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        heuristic: true,
      }),
      makeVisitResult(context, { uri: uri5.spec, title: "title" }),
      makeVisitResult(context, { uri: uri7.spec, title: "title" }),
      makeVisitResult(context, { uri: uri8.spec, title: "title" }),
    ],
  });

  info("http://www.w matches nothing");
  searchString = "http://www.w";
  context = createContext(searchString, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        uri: `${searchString}/`,
        fallbackTitle: `${searchString}/`,
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        heuristic: true,
      }),
    ],
  });

  info("ww matches no 'ww' pages, including 'www'");
  context = createContext("ww", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeVisitResult(context, { uri: uri5.spec, title: "title" }),
      makeVisitResult(context, { uri: uri8.spec, title: "title" }),
    ],
  });

  info("http://ww matches no 'ww' pages, including 'www'");
  searchString = "http://ww";
  context = createContext(searchString, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        uri: `${searchString}/`,
        fallbackTitle: `${searchString}/`,
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        heuristic: true,
      }),
      makeVisitResult(context, { uri: uri5.spec, title: "title" }),
      makeVisitResult(context, { uri: uri8.spec, title: "title" }),
    ],
  });

  info("http://www.ww matches nothing");
  searchString = "http://www.ww";
  context = createContext(searchString, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        uri: `${searchString}/`,
        fallbackTitle: `${searchString}/`,
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        heuristic: true,
      }),
    ],
  });

  info("www matches 'www' pages");
  context = createContext("www", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeVisitResult(context, { uri: uri5.spec, title: "title" }),
      makeVisitResult(context, { uri: uri8.spec, title: "title" }),
    ],
  });

  info("http://www matches 'www' pages");
  searchString = "http://www";
  context = createContext(searchString, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        uri: `${searchString}/`,
        fallbackTitle: `${searchString}/`,
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        heuristic: true,
      }),
      makeVisitResult(context, { uri: uri5.spec, title: "title" }),
      makeVisitResult(context, { uri: uri8.spec, title: "title" }),
    ],
  });

  info("http://www.www matches nothing");
  searchString = "http://www.www";
  context = createContext(searchString, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        uri: `${searchString}/`,
        fallbackTitle: `${searchString}/`,
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        heuristic: true,
      }),
    ],
  });

  await cleanupPlaces();
});
