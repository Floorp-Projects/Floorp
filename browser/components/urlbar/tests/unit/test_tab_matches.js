/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim:set ts=2 sw=2 sts=2 et:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

testEngine_setup();

add_task(async function test_tab_matches() {
  Services.prefs.setBoolPref("browser.urlbar.autoFill", false);
  Services.prefs.setBoolPref("browser.urlbar.suggest.searches", false);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("browser.urlbar.suggest.searches");
    Services.prefs.clearUserPref("browser.urlbar.autoFill");
  });

  let uri1 = Services.io.newURI("http://abc.com/");
  let uri2 = Services.io.newURI("http://xyz.net/");
  let uri3 = Services.io.newURI("about:mozilla");
  let uri4 = Services.io.newURI("data:text/html,test");
  let uri5 = Services.io.newURI("http://foobar.org");
  await PlacesTestUtils.addVisits([
    {
      uri: uri5,
      title: "foobar.org - much better than ABC, definitely better than XYZ",
    },
    { uri: uri2, title: "xyz.net - we're better than ABC" },
    { uri: uri1, title: "ABC rocks" },
  ]);
  await addOpenPages(uri1, 1);
  // Pages that cannot be registered in history.
  await addOpenPages(uri3, 1);
  await addOpenPages(uri4, 1);

  info("basic tab match");
  let context = createContext("abc.com", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.HISTORY,
        uri: "http://abc.com/",
        title: "ABC rocks",
        heuristic: true,
      }),
      makeTabSwitchResult(context, {
        uri: "http://abc.com/",
        title: "ABC rocks",
      }),
    ],
  });

  info("three results, one tab match");
  context = createContext("abc", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeTabSwitchResult(context, {
        uri: "http://abc.com/",
        title: "ABC rocks",
      }),
      makeVisitResult(context, {
        uri: uri2.spec,
        title: "xyz.net - we're better than ABC",
      }),
      makeVisitResult(context, {
        uri: uri5.spec,
        title: "foobar.org - much better than ABC, definitely better than XYZ",
      }),
    ],
  });

  info("three results, both normal results are tab matches");
  await addOpenPages(uri2, 1);
  context = createContext("abc", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeTabSwitchResult(context, {
        uri: "http://abc.com/",
        title: "ABC rocks",
      }),
      makeTabSwitchResult(context, {
        uri: "http://xyz.net/",
        title: "xyz.net - we're better than ABC",
      }),
      makeVisitResult(context, {
        uri: uri5.spec,
        title: "foobar.org - much better than ABC, definitely better than XYZ",
      }),
    ],
  });

  info("a container tab is not visible in 'switch to tab'");
  await addOpenPages(uri5, 1, /* userContextId: */ 3);
  context = createContext("abc", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeTabSwitchResult(context, {
        uri: "http://abc.com/",
        title: "ABC rocks",
      }),
      makeTabSwitchResult(context, {
        uri: "http://xyz.net/",
        title: "xyz.net - we're better than ABC",
      }),
      makeVisitResult(context, {
        uri: uri5.spec,
        title: "foobar.org - much better than ABC, definitely better than XYZ",
      }),
    ],
  });

  info(
    "a container tab should not see 'switch to tab' for other container tabs"
  );
  context = createContext("abc", { isPrivate: false, userContextId: 3 });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: uri1.spec,
        title: "ABC rocks",
      }),
      makeVisitResult(context, {
        uri: uri2.spec,
        title: "xyz.net - we're better than ABC",
      }),
      makeTabSwitchResult(context, {
        uri: "http://foobar.org/",
        title: "foobar.org - much better than ABC, definitely better than XYZ",
        userContextId: 3,
      }),
    ],
  });

  info("a different container tab should not see any 'switch to tab'");
  context = createContext("abc", { isPrivate: false, userContextId: 2 });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeVisitResult(context, { uri: uri1.spec, title: "ABC rocks" }),
      makeVisitResult(context, {
        uri: uri2.spec,
        title: "xyz.net - we're better than ABC",
      }),
      makeVisitResult(context, {
        uri: uri5.spec,
        title: "foobar.org - much better than ABC, definitely better than XYZ",
      }),
    ],
  });

  info(
    "three results, both normal results are tab matches, one has multiple tabs"
  );
  await addOpenPages(uri2, 5);
  context = createContext("abc", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeTabSwitchResult(context, {
        uri: "http://abc.com/",
        title: "ABC rocks",
      }),
      makeTabSwitchResult(context, {
        uri: "http://xyz.net/",
        title: "xyz.net - we're better than ABC",
      }),
      makeVisitResult(context, {
        uri: uri5.spec,
        title: "foobar.org - much better than ABC, definitely better than XYZ",
      }),
    ],
  });

  info("three results, no tab matches");
  await removeOpenPages(uri1, 1);
  await removeOpenPages(uri2, 6);
  context = createContext("abc", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: uri1.spec,
        title: "ABC rocks",
      }),
      makeVisitResult(context, {
        uri: uri2.spec,
        title: "xyz.net - we're better than ABC",
      }),
      makeVisitResult(context, {
        uri: uri5.spec,
        title: "foobar.org - much better than ABC, definitely better than XYZ",
      }),
    ],
  });

  info("tab match search with restriction character");
  await addOpenPages(uri1, 1);
  context = createContext(UrlbarTokenizer.RESTRICT.OPENPAGE + " abc", {
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        query: "abc",
        alias: UrlbarTokenizer.RESTRICT.OPENPAGE,
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        heuristic: true,
      }),
      makeTabSwitchResult(context, {
        uri: "http://abc.com/",
        title: "ABC rocks",
      }),
    ],
  });

  info("tab match with not-addable pages");
  context = createContext("mozilla", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeTabSwitchResult(context, {
        uri: "about:mozilla",
        title: "about:mozilla",
      }),
    ],
  });

  info("tab match with not-addable pages, no boundary search");
  context = createContext("ut:mo", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeTabSwitchResult(context, {
        uri: "about:mozilla",
        title: "about:mozilla",
      }),
    ],
  });

  info("tab match with not-addable pages and restriction character");
  context = createContext(UrlbarTokenizer.RESTRICT.OPENPAGE + " mozilla", {
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        query: "mozilla",
        alias: UrlbarTokenizer.RESTRICT.OPENPAGE,
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        heuristic: true,
      }),
      makeTabSwitchResult(context, {
        uri: "about:mozilla",
        title: "about:mozilla",
      }),
    ],
  });

  info("tab match with not-addable pages and only restriction character");
  context = createContext(UrlbarTokenizer.RESTRICT.OPENPAGE, {
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeTabSwitchResult(context, {
        uri: "data:text/html,test",
        title: "data:text/html,test",
        iconUri: UrlbarUtils.ICON.DEFAULT,
      }),
      makeTabSwitchResult(context, {
        uri: "about:mozilla",
        title: "about:mozilla",
      }),
      makeTabSwitchResult(context, {
        uri: "http://abc.com/",
        title: "ABC rocks",
      }),
    ],
  });

  info("tab match should not return tags as part of the title");
  // Bookmark one of the pages, and add tags to it, to check they don't appear
  // in the title.
  let bm = await PlacesUtils.bookmarks.insert({
    url: uri1,
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
  });
  PlacesUtils.tagging.tagURI(uri1, ["test-tag"]);
  context = createContext("abc.com", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.HISTORY,
        uri: "http://abc.com/",
        title: "ABC rocks",
        heuristic: true,
      }),
      makeTabSwitchResult(context, {
        uri: "http://abc.com/",
        title: "ABC rocks",
      }),
    ],
  });
  await PlacesUtils.bookmarks.remove(bm);

  await cleanupPlaces();
});
