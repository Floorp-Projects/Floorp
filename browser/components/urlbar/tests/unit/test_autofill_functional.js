/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Functional tests for inline autocomplete

add_setup(async function () {
  registerCleanupFunction(async () => {
    Services.prefs.clearUserPref("browser.urlbar.suggest.searches");
    Services.prefs.clearUserPref("browser.urlbar.suggest.quickactions");
  });

  Services.prefs.setBoolPref("browser.urlbar.suggest.searches", false);
  Services.prefs.setBoolPref("browser.urlbar.suggest.quickactions", false);
});

add_task(async function test_urls_order() {
  info("Add urls, check for correct order");
  let places = [
    { uri: Services.io.newURI("http://visit1.mozilla.org") },
    { uri: Services.io.newURI("http://visit2.mozilla.org") },
  ];
  await PlacesTestUtils.addVisits(places);
  let context = createContext("vis", { isPrivate: false });
  await check_results({
    context,
    autofilled: "visit2.mozilla.org/",
    completed: "http://visit2.mozilla.org/",
    matches: [
      makeVisitResult(context, {
        uri: "http://visit2.mozilla.org/",
        title: "test visit for http://visit2.mozilla.org/",
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: "http://visit1.mozilla.org/",
        title: "test visit for http://visit1.mozilla.org/",
      }),
    ],
  });
  await cleanupPlaces();
});

add_task(async function test_bookmark_first() {
  info("With a bookmark and history, the query result should be the bookmark");
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: Services.io.newURI("http://bookmark1.mozilla.org/"),
  });
  await PlacesTestUtils.addVisits(
    Services.io.newURI("http://bookmark1.mozilla.org/foo")
  );
  let context = createContext("bookmark", { isPrivate: false });
  await check_results({
    context,
    autofilled: "bookmark1.mozilla.org/",
    completed: "http://bookmark1.mozilla.org/",
    matches: [
      makeVisitResult(context, {
        uri: "http://bookmark1.mozilla.org/",
        title: "A bookmark",
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: "http://bookmark1.mozilla.org/foo",
        title: "test visit for http://bookmark1.mozilla.org/foo",
      }),
    ],
  });
  await cleanupPlaces();
});

add_task(async function test_complete_querystring() {
  info("Check to make sure we autocomplete after ?");
  await PlacesTestUtils.addVisits(
    Services.io.newURI("http://smokey.mozilla.org/foo?bacon=delicious")
  );
  let context = createContext("smokey.mozilla.org/foo?", { isPrivate: false });
  await check_results({
    context,
    autofilled: "smokey.mozilla.org/foo?bacon=delicious",
    completed: "http://smokey.mozilla.org/foo?bacon=delicious",
    matches: [
      makeVisitResult(context, {
        uri: "http://smokey.mozilla.org/foo?bacon=delicious",
        title: "test visit for http://smokey.mozilla.org/foo?bacon=delicious",
        heuristic: true,
      }),
    ],
  });
  await cleanupPlaces();
});

add_task(async function test_complete_fragment() {
  info("Check to make sure we autocomplete after #");
  await PlacesTestUtils.addVisits(
    Services.io.newURI("http://smokey.mozilla.org/foo?bacon=delicious#bar")
  );
  let context = createContext("smokey.mozilla.org/foo?bacon=delicious#bar", {
    isPrivate: false,
  });
  await check_results({
    context,
    autofilled: "smokey.mozilla.org/foo?bacon=delicious#bar",
    completed: "http://smokey.mozilla.org/foo?bacon=delicious#bar",
    matches: [
      makeVisitResult(context, {
        uri: "http://smokey.mozilla.org/foo?bacon=delicious#bar",
        title:
          "test visit for http://smokey.mozilla.org/foo?bacon=delicious#bar",
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

  info("Should still autofill after a search is cancelled immediately");
  let context = createContext("mozi", { isPrivate: false });
  await check_results({
    context,
    incompleteSearch: "moz",
    autofilled: "mozilla.org/",
    completed: "http://mozilla.org/",
    matches: [
      makeVisitResult(context, {
        uri: "http://mozilla.org/",
        fallbackTitle: UrlbarTestUtils.trimURL("http://mozilla.org"),
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
