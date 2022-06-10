/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const AUTOFILL_PROVIDERNAME = "Autofill";
const HEURISTIC_FALLBACK_PROVIDERNAME = "HeuristicFallback";
const PLACES_PROVIDERNAME = "Places";

testEngine_setup();

add_task(async function test_casing_1() {
  info("Searching for cased entry 1");
  await PlacesTestUtils.addVisits({
    uri: Services.io.newURI("http://mozilla.org/test/"),
  });
  let context = createContext("MOZ", { isPrivate: false });
  await check_results({
    context,
    autofilled: "MOZilla.org/",
    completed: "http://mozilla.org/",
    hasAutofillTitle: false,
    matches: [
      makeVisitResult(context, {
        uri: "http://mozilla.org/",
        title: "mozilla.org",
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: "http://mozilla.org/test/",
        title: "test visit for http://mozilla.org/test/",
        providerName: PLACES_PROVIDERNAME,
      }),
    ],
  });
  await cleanupPlaces();
});

add_task(async function test_casing_2() {
  info("Searching for cased entry 2");
  await PlacesTestUtils.addVisits({
    uri: Services.io.newURI("http://mozilla.org/test/"),
  });
  let context = createContext("mozilla.org/T", { isPrivate: false });
  await check_results({
    context,
    autofilled: "mozilla.org/Test/",
    completed: "http://mozilla.org/test/",
    hasAutofillTitle: true,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.HISTORY,
        uri: "http://mozilla.org/test/",
        title: "test visit for http://mozilla.org/test/",
        iconUri: "page-icon:http://mozilla.org/test/",
        heuristic: true,
        providerName: AUTOFILL_PROVIDERNAME,
      }),
    ],
  });
  await cleanupPlaces();
});

add_task(async function test_casing_3() {
  info("Searching for cased entry 3");
  await PlacesTestUtils.addVisits({
    uri: Services.io.newURI("http://mozilla.org/Test/"),
  });
  let context = createContext("mozilla.org/T", { isPrivate: false });
  await check_results({
    context,
    autofilled: "mozilla.org/Test/",
    completed: "http://mozilla.org/Test/",
    hasAutofillTitle: true,
    matches: [
      makeVisitResult(context, {
        uri: "http://mozilla.org/Test/",
        title: "test visit for http://mozilla.org/Test/",
        heuristic: true,
      }),
    ],
  });
  await cleanupPlaces();
});

add_task(async function test_casing_4() {
  info("Searching for cased entry 4");
  await PlacesTestUtils.addVisits({
    uri: Services.io.newURI("http://mozilla.org/Test/"),
  });
  let context = createContext("mOzilla.org/t", { isPrivate: false });
  await check_results({
    context,
    autofilled: "mOzilla.org/test/",
    completed: "http://mozilla.org/Test/",
    hasAutofillTitle: true,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.HISTORY,
        uri: "http://mozilla.org/Test/",
        title: "test visit for http://mozilla.org/Test/",
        iconUri: "page-icon:http://mozilla.org/Test/",
        heuristic: true,
        providerName: AUTOFILL_PROVIDERNAME,
      }),
    ],
  });
  await cleanupPlaces();
});

add_task(async function test_casing_5() {
  info("Searching for cased entry 5");
  await PlacesTestUtils.addVisits({
    uri: Services.io.newURI("http://mozilla.org/Test/"),
  });
  let context = createContext("mOzilla.org/T", { isPrivate: false });
  await check_results({
    context,
    autofilled: "mOzilla.org/Test/",
    completed: "http://mozilla.org/Test/",
    hasAutofillTitle: true,
    matches: [
      makeVisitResult(context, {
        uri: "http://mozilla.org/Test/",
        title: "test visit for http://mozilla.org/Test/",
        heuristic: true,
      }),
    ],
  });
  await cleanupPlaces();
});

add_task(async function test_untrimmed_casing() {
  info("Searching for untrimmed cased entry");
  await PlacesTestUtils.addVisits({
    uri: Services.io.newURI("http://mozilla.org/Test/"),
  });
  let context = createContext("http://mOz", { isPrivate: false });
  await check_results({
    context,
    autofilled: "http://mOzilla.org/",
    completed: "http://mozilla.org/",
    hasAutofillTitle: false,
    matches: [
      makeVisitResult(context, {
        uri: "http://mozilla.org/",
        title: "mozilla.org",
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: "http://mozilla.org/Test/",
        title: "test visit for http://mozilla.org/Test/",
        providerName: PLACES_PROVIDERNAME,
      }),
    ],
  });
  await cleanupPlaces();
});

add_task(async function test_untrimmed_www_casing() {
  info("Searching for untrimmed cased entry with www");
  await PlacesTestUtils.addVisits({
    uri: Services.io.newURI("http://www.mozilla.org/Test/"),
  });
  let context = createContext("http://www.mOz", { isPrivate: false });
  await check_results({
    context,
    autofilled: "http://www.mOzilla.org/",
    completed: "http://www.mozilla.org/",
    hasAutofillTitle: false,
    matches: [
      makeVisitResult(context, {
        uri: "http://www.mozilla.org/",
        title: "www.mozilla.org",
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: "http://www.mozilla.org/Test/",
        title: "test visit for http://www.mozilla.org/Test/",
        providerName: PLACES_PROVIDERNAME,
      }),
    ],
  });
  await cleanupPlaces();
});

add_task(async function test_untrimmed_path_casing() {
  info("Searching for untrimmed cased entry with path");
  await PlacesTestUtils.addVisits({
    uri: Services.io.newURI("http://mozilla.org/Test/"),
  });
  let context = createContext("http://mOzilla.org/t", { isPrivate: false });
  await check_results({
    context,
    autofilled: "http://mOzilla.org/test/",
    completed: "http://mozilla.org/Test/",
    hasAutofillTitle: true,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.HISTORY,
        uri: "http://mozilla.org/Test/",
        title: "test visit for http://mozilla.org/Test/",
        iconUri: "page-icon:http://mozilla.org/Test/",
        heuristic: true,
        providerName: AUTOFILL_PROVIDERNAME,
      }),
    ],
  });
  await cleanupPlaces();
});

add_task(async function test_untrimmed_path_casing_2() {
  info("Searching for untrimmed cased entry with path 2");
  await PlacesTestUtils.addVisits({
    uri: Services.io.newURI("http://mozilla.org/Test/"),
  });
  let context = createContext("http://mOzilla.org/T", { isPrivate: false });
  await check_results({
    context,
    autofilled: "http://mOzilla.org/Test/",
    completed: "http://mozilla.org/Test/",
    hasAutofillTitle: true,
    matches: [
      makeVisitResult(context, {
        uri: "http://mozilla.org/Test/",
        title: "test visit for http://mozilla.org/Test/",
        heuristic: true,
      }),
    ],
  });
  await cleanupPlaces();
});

add_task(async function test_untrimmed_path_www_casing() {
  info("Searching for untrimmed cased entry with www and path");
  await PlacesTestUtils.addVisits({
    uri: Services.io.newURI("http://www.mozilla.org/Test/"),
  });
  let context = createContext("http://www.mOzilla.org/t", { isPrivate: false });
  await check_results({
    context,
    autofilled: "http://www.mOzilla.org/test/",
    completed: "http://www.mozilla.org/Test/",
    hasAutofillTitle: true,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.HISTORY,
        uri: "http://www.mozilla.org/Test/",
        title: "test visit for http://www.mozilla.org/Test/",
        iconUri: "page-icon:http://www.mozilla.org/Test/",
        heuristic: true,
        providerName: AUTOFILL_PROVIDERNAME,
      }),
    ],
  });
  await cleanupPlaces();
});

add_task(async function test_untrimmed_path_www_casing_2() {
  info("Searching for untrimmed cased entry with www and path 2");
  await PlacesTestUtils.addVisits({
    uri: Services.io.newURI("http://www.mozilla.org/Test/"),
  });
  let context = createContext("http://www.mOzilla.org/T", { isPrivate: false });
  await check_results({
    context,
    autofilled: "http://www.mOzilla.org/Test/",
    completed: "http://www.mozilla.org/Test/",
    hasAutofillTitle: true,
    matches: [
      makeVisitResult(context, {
        uri: "http://www.mozilla.org/Test/",
        title: "test visit for http://www.mozilla.org/Test/",
        heuristic: true,
      }),
    ],
  });
  await cleanupPlaces();
});

add_task(async function test_searching() {
  let uri1 = Services.io.newURI("http://dummy/1/");
  let uri2 = Services.io.newURI("http://dummy/2/");
  let uri3 = Services.io.newURI("http://dummy/3/");
  let uri4 = Services.io.newURI("http://dummy/4/");
  let uri5 = Services.io.newURI("http://dummy/5/");

  await PlacesTestUtils.addVisits([
    { uri: uri1, title: "uppercase lambda \u039B" },
    { uri: uri2, title: "lowercase lambda \u03BB" },
    { uri: uri3, title: "symbol \u212A" }, // kelvin
    { uri: uri4, title: "uppercase K" },
    { uri: uri5, title: "lowercase k" },
  ]);

  info("Search for lowercase lambda");
  let context = createContext("\u03BB", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: uri2.spec,
        title: "lowercase lambda \u03BB",
      }),
      makeVisitResult(context, {
        uri: uri1.spec,
        title: "uppercase lambda \u039B",
      }),
    ],
  });

  info("Search for uppercase lambda");
  context = createContext("\u039B", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: uri2.spec,
        title: "lowercase lambda \u03BB",
      }),
      makeVisitResult(context, {
        uri: uri1.spec,
        title: "uppercase lambda \u039B",
      }),
    ],
  });

  info("Search for kelvin sign");
  context = createContext("\u212A", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeVisitResult(context, { uri: uri5.spec, title: "lowercase k" }),
      makeVisitResult(context, { uri: uri4.spec, title: "uppercase K" }),
      makeVisitResult(context, { uri: uri3.spec, title: "symbol \u212A" }),
    ],
  });

  info("Search for lowercase k");
  context = createContext("k", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeVisitResult(context, { uri: uri5.spec, title: "lowercase k" }),
      makeVisitResult(context, { uri: uri4.spec, title: "uppercase K" }),
      makeVisitResult(context, { uri: uri3.spec, title: "symbol \u212A" }),
    ],
  });

  info("Search for uppercase k");

  context = createContext("K", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeVisitResult(context, { uri: uri5.spec, title: "lowercase k" }),
      makeVisitResult(context, { uri: uri4.spec, title: "uppercase K" }),
      makeVisitResult(context, { uri: uri3.spec, title: "symbol \u212A" }),
    ],
  });

  await cleanupPlaces();
});
