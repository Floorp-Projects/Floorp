/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test for bug 401869 to allow multiple words separated by spaces to match in
 * the page title, page url, or bookmark title to be considered a match. All
 * terms must match but not all terms need to be in the title, etc.
 *
 * Test bug 424216 by making sure bookmark titles are always shown if one is
 * available. Also bug 425056 makes sure matches aren't found partially in the
 * page title and partially in the bookmark.
 */

testEngine_setup();

add_task(async function test_match_beginning() {
  Services.prefs.setBoolPref("browser.urlbar.suggest.searches", false);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("browser.urlbar.suggest.searches");
  });

  let uri1 = Services.io.newURI("http://a.b.c/d-e_f/h/t/p");
  let uri2 = Services.io.newURI("http://d.e.f/g-h_i/h/t/p");
  let uri3 = Services.io.newURI("http://g.h.i/j-k_l/h/t/p");
  let uri4 = Services.io.newURI("http://j.k.l/m-n_o/h/t/p");
  await PlacesTestUtils.addVisits([
    { uri: uri4, title: "f(o)o b<a>r" },
    { uri: uri3, title: "f(o)o b<a>r" },
    { uri: uri2, title: "b(a)r b<a>z" },
    { uri: uri1, title: "f(o)o b<a>r" },
  ]);
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: uri3,
    title: "f(o)o b<a>r",
  });
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: uri4,
    title: "b(a)r b<a>z",
  });

  info("Match 2 terms all in url");
  let context = createContext("c d", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeVisitResult(context, { uri: uri1.spec, title: "f(o)o b<a>r" }),
    ],
  });

  info("Match 1 term in url and 1 term in title");
  context = createContext("b e", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeVisitResult(context, { uri: uri1.spec, title: "f(o)o b<a>r" }),
      makeVisitResult(context, { uri: uri2.spec, title: "b(a)r b<a>z" }),
    ],
  });

  info("Match 3 terms all in title; display bookmark title if matched");
  context = createContext("b a z", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeBookmarkResult(context, { uri: uri4.spec, title: "b(a)r b<a>z" }),
      makeVisitResult(context, { uri: uri2.spec, title: "b(a)r b<a>z" }),
    ],
  });

  info(
    "Match 2 terms in url and 1 in title; make sure bookmark title is used for search"
  );
  context = createContext("k f t", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeBookmarkResult(context, { uri: uri3.spec, title: "f(o)o b<a>r" }),
    ],
  });

  info("Match 3 terms in url and 1 in title");
  context = createContext("d i g z", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeVisitResult(context, { uri: uri2.spec, title: "b(a)r b<a>z" }),
    ],
  });

  info("Match nothing");
  context = createContext("m o z i", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
    ],
  });

  await cleanupPlaces();
});
