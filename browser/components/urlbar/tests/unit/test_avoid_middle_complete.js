/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

testEngine_setup();

add_task(async function test_prefix_space_noautofill() {
  await PlacesTestUtils.addVisits({
    uri: Services.io.newURI("http://moz.org/test/"),
  });

  info("Should not try to autoFill if search string contains a space");
  let context = createContext(" mo", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        query: " mo",
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: "http://moz.org/test/",
        title: "test visit for http://moz.org/test/",
      }),
    ],
  });

  await cleanupPlaces();
});

add_task(async function test_trailing_space_noautofill() {
  await PlacesTestUtils.addVisits({
    uri: Services.io.newURI("http://moz.org/test/"),
  });

  info("Should not try to autoFill if search string contains a space");
  let context = createContext("mo ", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        query: "mo ",
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: "http://moz.org/test/",
        title: "test visit for http://moz.org/test/",
      }),
    ],
  });

  await cleanupPlaces();
});

add_task(async function test_searchEngine_autofill() {
  Services.prefs.setBoolPref("browser.urlbar.autoFill.searchEngines", true);
  await SearchTestUtils.installSearchExtension({
    name: "CakeSearch",
    search_url: "https://cake.search",
  });

  info(
    "Should autoFill search engine if search string does not contains a space"
  );
  let context = createContext("ca", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makePrioritySearchResult(context, {
        engineName: "CakeSearch",
        heuristic: true,
      }),
    ],
  });

  await cleanupPlaces();
});

add_task(async function test_searchEngine_prefix_space_noautofill() {
  Services.prefs.setBoolPref("browser.urlbar.autoFill.searchEngines", true);
  await SearchTestUtils.installSearchExtension({
    name: "CupcakeSearch",
    search_url: "https://cupcake.search",
  });

  info(
    "Should not try to autoFill search engine if search string contains a space"
  );
  let context = createContext(" cu", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        query: " cu",
        heuristic: true,
      }),
    ],
  });

  await cleanupPlaces();
});

add_task(async function test_searchEngine_trailing_space_noautofill() {
  Services.prefs.setBoolPref("browser.urlbar.autoFill.searchEngines", true);
  await SearchTestUtils.installSearchExtension({
    name: "BaconSearch",
    search_url: "https://bacon.search",
  });

  info(
    "Should not try to autoFill search engine if search string contains a space"
  );
  let context = createContext("ba ", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        query: "ba ",
        heuristic: true,
      }),
    ],
  });

  await cleanupPlaces();
});

add_task(async function test_searchEngine_www_noautofill() {
  Services.prefs.setBoolPref("browser.urlbar.autoFill.searchEngines", true);
  await SearchTestUtils.installSearchExtension({
    name: "HamSearch",
    search_url: "https://ham.search",
  });

  info(
    "Should not autoFill search engine if search string contains www. but engine doesn't"
  );
  let context = createContext("www.ham", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        uri: "http://www.ham/",
        title: "http://www.ham/",
        displayUrl: "http://www.ham",
        heuristic: true,
      }),
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        query: "www.ham",
      }),
    ],
  });

  await cleanupPlaces();
});

add_task(async function test_searchEngine_different_scheme_noautofill() {
  Services.prefs.setBoolPref("browser.urlbar.autoFill.searchEngines", true);
  await SearchTestUtils.installSearchExtension({
    name: "PieSearch",
    search_url: "https://pie.search",
  });

  info(
    "Should not autoFill search engine if search string has a different scheme."
  );
  let context = createContext("http://pie", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        uri: "http://pie/",
        title: "http://pie/",
        iconUri: "",
        heuristic: true,
      }),
    ],
  });

  await cleanupPlaces();
});

add_task(async function test_searchEngine_matching_prefix_autofill() {
  Services.prefs.setBoolPref("browser.urlbar.autoFill.searchEngines", true);
  await SearchTestUtils.installSearchExtension({
    name: "BeanSearch",
    search_url: "https://www.bean.search",
  });

  info("Should autoFill search engine if search string has matching prefix.");
  let context = createContext("https://www.be", { isPrivate: false });
  await check_results({
    context,
    autofilled: "https://www.bean.search/",
    matches: [
      makePrioritySearchResult(context, {
        engineName: "BeanSearch",
        heuristic: true,
      }),
    ],
  });

  info("Should autoFill search engine if search string has www prefix.");
  context = createContext("www.be", { isPrivate: false });
  await check_results({
    context,
    autofilled: "www.bean.search/",
    matches: [
      makePrioritySearchResult(context, {
        engineName: "BeanSearch",
        heuristic: true,
      }),
    ],
  });

  info("Should autoFill search engine if search string has matching scheme.");
  context = createContext("https://be", { isPrivate: false });
  await check_results({
    context,
    autofilled: "https://bean.search/",
    matches: [
      makePrioritySearchResult(context, {
        engineName: "BeanSearch",
        heuristic: true,
      }),
    ],
  });

  await cleanupPlaces();
});

add_task(async function test_prefix_autofill() {
  await PlacesTestUtils.addVisits({
    uri: Services.io.newURI("http://mozilla.org/test/"),
  });
  await PlacesTestUtils.addVisits({
    uri: Services.io.newURI("http://moz.org/test/"),
  });

  info(
    "Should not try to autoFill in-the-middle if a search is canceled immediately"
  );
  let context = createContext("mozi", { isPrivate: false });
  await check_results({
    context,
    incompleteSearch: "moz",
    autofilled: "mozilla.org/",
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
        providerName: "Places",
      }),
    ],
  });

  await cleanupPlaces();
});
