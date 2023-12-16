/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// We should not autofill when the search string contains spaces.

testEngine_setup();

add_setup(async () => {
  Services.prefs.setBoolPref("browser.urlbar.suggest.searches", false);
  await PlacesTestUtils.addVisits({
    uri: Services.io.newURI("http://mozilla.org/link/"),
  });

  registerCleanupFunction(async () => {
    Services.prefs.clearUserPref("browser.urlbar.suggest.searches");
    await cleanupPlaces();
  });
});

add_task(async function test_not_autofill_ws_1() {
  info("Do not autofill whitespaced entry 1");
  let context = createContext("mozilla.org ", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        uri: "http://mozilla.org/",
        fallbackTitle: "http://mozilla.org/",
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: "http://mozilla.org/link/",
        title: "test visit for http://mozilla.org/link/",
      }),
    ],
  });
});

add_task(async function test_not_autofill_ws_2() {
  info("Do not autofill whitespaced entry 2");
  let context = createContext("mozilla.org/ ", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        uri: "http://mozilla.org/",
        fallbackTitle: "http://mozilla.org/",
        iconUri: "page-icon:http://mozilla.org/",
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: "http://mozilla.org/link/",
        title: "test visit for http://mozilla.org/link/",
      }),
    ],
  });
});

add_task(async function test_not_autofill_ws_3() {
  info("Do not autofill whitespaced entry 3");
  let context = createContext("mozilla.org/link ", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        uri: "http://mozilla.org/link",
        fallbackTitle: "http://mozilla.org/link",
        iconUri: "page-icon:http://mozilla.org/",
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: "http://mozilla.org/link/",
        title: "test visit for http://mozilla.org/link/",
      }),
    ],
  });
});

add_task(async function test_not_autofill_ws_4() {
  info(
    "Do not autofill whitespaced entry 4, but UrlbarProviderPlaces provides heuristic result"
  );
  let context = createContext("mozilla.org/link/ ", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        uri: "http://mozilla.org/link/",
        title: "test visit for http://mozilla.org/link/",
        iconUri: "page-icon:http://mozilla.org/link/",
        source: UrlbarUtils.RESULT_SOURCE.HISTORY,
        heuristic: true,
      }),
    ],
  });
});

add_task(async function test_not_autofill_ws_5() {
  info("Do not autofill whitespaced entry 5");
  let context = createContext("moz illa ", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        // query is made explict so makeSearchResult doesn't trim it.
        query: "moz illa ",
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: "http://mozilla.org/link/",
        title: "test visit for http://mozilla.org/link/",
      }),
    ],
  });
});

add_task(async function test_not_autofill_ws_6() {
  info("Do not autofill whitespaced entry 6");
  let context = createContext(" mozilla", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        // query is made explict so makeSearchResult doesn't trim it.
        query: " mozilla",
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: "http://mozilla.org/link/",
        title: "test visit for http://mozilla.org/link/",
      }),
    ],
  });
});
