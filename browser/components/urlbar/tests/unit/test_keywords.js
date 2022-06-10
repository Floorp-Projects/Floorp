/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const ENGINE_NAME = "engine-suggestions.xml";

testEngine_setup();

add_task(async function test_non_keyword() {
  info("Searching for non-keyworded entry should autoFill it");
  await PlacesTestUtils.addVisits({
    uri: Services.io.newURI("http://mozilla.org/test/"),
  });
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: Services.io.newURI("http://mozilla.org/test/"),
  });
  let context = createContext("moz", { isPrivate: false });
  await check_results({
    context,
    autofilled: "mozilla.org/",
    completed: "http://mozilla.org/",
    hasAutofillTitle: false,
    matches: [
      makeVisitResult(context, {
        uri: "http://mozilla.org/",
        title: "mozilla.org",
        heuristic: true,
      }),
      makeBookmarkResult(context, {
        uri: "http://mozilla.org/test/",
        title: "A bookmark",
      }),
    ],
  });
  await cleanupPlaces();
});

add_task(async function test_keyword() {
  info("Searching for keyworded entry should not autoFill it");
  await PlacesTestUtils.addVisits({
    uri: Services.io.newURI("http://mozilla.org/test/"),
  });
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: Services.io.newURI("http://mozilla.org/test/"),
    keyword: "moz",
  });
  let context = createContext("moz", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeKeywordSearchResult(context, {
        uri: "http://mozilla.org/test/",
        title: "http://mozilla.org/test/",
        keyword: "moz",
        heuristic: true,
      }),
    ],
  });
  await cleanupPlaces();
});

add_task(async function test_more_than_keyword() {
  info("Searching for more than keyworded entry should autoFill it");
  await PlacesTestUtils.addVisits({
    uri: Services.io.newURI("http://mozilla.org/test/"),
  });
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: Services.io.newURI("http://mozilla.org/test/"),
    keyword: "moz",
  });
  let context = createContext("mozi", { isPrivate: false });
  await check_results({
    context,
    autofilled: "mozilla.org/",
    completed: "http://mozilla.org/",
    hasAutofillTitle: false,
    matches: [
      makeVisitResult(context, {
        uri: "http://mozilla.org/",
        title: "mozilla.org",
        heuristic: true,
      }),
      makeBookmarkResult(context, {
        uri: "http://mozilla.org/test/",
        title: "A bookmark",
      }),
    ],
  });
  await cleanupPlaces();
});

add_task(async function test_less_than_keyword() {
  info("Searching for less than keyworded entry should autoFill it");
  await PlacesTestUtils.addVisits({
    uri: Services.io.newURI("http://mozilla.org/test/"),
  });
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: Services.io.newURI("http://mozilla.org/test/"),
    keyword: "moz",
  });
  let context = createContext("mo", { isPrivate: false });
  await check_results({
    context,
    search: "mo",
    autofilled: "mozilla.org/",
    completed: "http://mozilla.org/",
    hasAutofillTitle: false,
    matches: [
      makeVisitResult(context, {
        uri: "http://mozilla.org/",
        title: "mozilla.org",
        heuristic: true,
      }),
      makeBookmarkResult(context, {
        uri: "http://mozilla.org/test/",
        title: "A bookmark",
      }),
    ],
  });
  await cleanupPlaces();
});

add_task(async function test_keyword_casing() {
  info("Searching for keyworded entry is case-insensitive");
  await PlacesTestUtils.addVisits({
    uri: Services.io.newURI("http://mozilla.org/test/"),
  });
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: Services.io.newURI("http://mozilla.org/test/"),
    keyword: "moz",
  });
  let context = createContext("MoZ", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeKeywordSearchResult(context, {
        uri: "http://mozilla.org/test/",
        title: "http://mozilla.org/test/",
        keyword: "MoZ",
        heuristic: true,
      }),
    ],
  });
  await cleanupPlaces();
});

add_task(async function test_less_then_equal_than_keyword_bug_1124238() {
  info("Searching for less than keyworded entry should autoFill it");
  await PlacesTestUtils.addVisits({
    uri: Services.io.newURI("http://mozilla.org/test/"),
  });
  await PlacesTestUtils.addVisits("http://mozilla.com/");
  PlacesTestUtils.addBookmarkWithDetails({
    uri: Services.io.newURI("http://mozilla.com/"),
    keyword: "moz",
  });

  let context = createContext("mo", { isPrivate: false });
  await check_results({
    context,
    search: "mo",
    autofilled: "mozilla.com/",
    completed: "http://mozilla.com/",
    hasAutofillTitle: true,
    matches: [
      makeVisitResult(context, {
        uri: "http://mozilla.com/",
        title: "test visit for http://mozilla.com/",
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: "http://mozilla.org/test/",
        title: "test visit for http://mozilla.org/test/",
      }),
    ],
  });

  // Search with an additional character. As the input matches a keyword, the
  // completion should equal the keyword and not the URI as before.
  context = createContext("moz", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeKeywordSearchResult(context, {
        uri: "http://mozilla.com/",
        title: "http://mozilla.com/",
        keyword: "moz",
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: "http://mozilla.org/test/",
        title: "test visit for http://mozilla.org/test/",
      }),
    ],
  });

  // Search with an additional character. The input doesn't match a keyword
  // anymore, it should be autofilled.
  context = createContext("mozi", { isPrivate: false });
  await check_results({
    context,
    autofilled: "mozilla.com/",
    completed: "http://mozilla.com/",
    hasAutofillTitle: true,
    matches: [
      makeVisitResult(context, {
        uri: "http://mozilla.com/",
        title: "A bookmark",
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: "http://mozilla.org/test/",
        title: "test visit for http://mozilla.org/test/",
      }),
    ],
  });

  await cleanupPlaces();
});
