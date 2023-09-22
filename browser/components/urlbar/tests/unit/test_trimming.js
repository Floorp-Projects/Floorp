/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

add_setup(async function () {
  registerCleanupFunction(async () => {
    Services.prefs.clearUserPref("browser.urlbar.suggest.searches");
  });
  Services.prefs.setBoolPref("browser.urlbar.suggest.searches", false);
});

add_task(async function test_untrimmed_secure_www() {
  info("Searching for untrimmed https://www entry");
  await PlacesTestUtils.addVisits({
    uri: Services.io.newURI("https://www.mozilla.org/test/"),
  });
  let context = createContext("mo", { isPrivate: false });
  await check_results({
    context,
    autofilled: "mozilla.org/",
    completed: "https://www.mozilla.org/",
    matches: [
      makeVisitResult(context, {
        uri: "https://www.mozilla.org/",
        fallbackTitle: "https://www.mozilla.org",
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: "https://www.mozilla.org/test/",
        title: "test visit for https://www.mozilla.org/test/",
      }),
    ],
  });
  await cleanupPlaces();
});

add_task(async function test_untrimmed_secure_www_path() {
  info("Searching for untrimmed https://www entry with path");
  await PlacesTestUtils.addVisits({
    uri: Services.io.newURI("https://www.mozilla.org/test/"),
  });
  let context = createContext("mozilla.org/t", { isPrivate: false });
  await check_results({
    context,
    autofilled: "mozilla.org/test/",
    completed: "https://www.mozilla.org/test/",
    matches: [
      makeVisitResult(context, {
        uri: "https://www.mozilla.org/test/",
        title: "test visit for https://www.mozilla.org/test/",
        heuristic: true,
      }),
    ],
  });
  await cleanupPlaces();
});

add_task(async function test_untrimmed_secure() {
  info("Searching for untrimmed https:// entry");
  await PlacesTestUtils.addVisits({
    uri: Services.io.newURI("https://mozilla.org/test/"),
  });
  let context = createContext("mo", { isPrivate: false });
  await check_results({
    context,
    autofilled: "mozilla.org/",
    completed: "https://mozilla.org/",
    matches: [
      makeVisitResult(context, {
        uri: "https://mozilla.org/",
        fallbackTitle: "https://mozilla.org",
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: "https://mozilla.org/test/",
        title: "test visit for https://mozilla.org/test/",
      }),
    ],
  });
  await cleanupPlaces();
});

add_task(async function test_untrimmed_secure_path() {
  info("Searching for untrimmed https:// entry with path");
  await PlacesTestUtils.addVisits({
    uri: Services.io.newURI("https://mozilla.org/test/"),
  });
  let context = createContext("mozilla.org/t", { isPrivate: false });
  await check_results({
    context,
    autofilled: "mozilla.org/test/",
    completed: "https://mozilla.org/test/",
    matches: [
      makeVisitResult(context, {
        uri: "https://mozilla.org/test/",
        title: "test visit for https://mozilla.org/test/",
        heuristic: true,
      }),
    ],
  });
  await cleanupPlaces();
});

add_task(async function test_untrimmed_www() {
  info("Searching for untrimmed http://www entry");
  await PlacesTestUtils.addVisits({
    uri: Services.io.newURI("http://www.mozilla.org/test/"),
  });
  let context = createContext("mo", { isPrivate: false });
  await check_results({
    context,
    autofilled: "mozilla.org/",
    completed: "http://www.mozilla.org/",
    matches: [
      makeVisitResult(context, {
        uri: "http://www.mozilla.org/",
        fallbackTitle: "www.mozilla.org",
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: "http://www.mozilla.org/test/",
        title: "test visit for http://www.mozilla.org/test/",
      }),
    ],
  });
  await cleanupPlaces();
});

add_task(async function test_untrimmed_www_path() {
  info("Searching for untrimmed http://www entry with path");
  await PlacesTestUtils.addVisits({
    uri: Services.io.newURI("http://www.mozilla.org/test/"),
  });
  let context = createContext("mozilla.org/t", { isPrivate: false });
  await check_results({
    context,
    autofilled: "mozilla.org/test/",
    completed: "http://www.mozilla.org/test/",
    matches: [
      makeVisitResult(context, {
        uri: "http://www.mozilla.org/test/",
        title: "test visit for http://www.mozilla.org/test/",
        heuristic: true,
      }),
    ],
  });
  await cleanupPlaces();
});

add_task(async function test_escaped_chars() {
  info("Searching for URL with characters that are normally escaped");
  await PlacesTestUtils.addVisits({
    uri: Services.io.newURI("https://www.mozilla.org/啊-test"),
  });
  let context = createContext("https://www.mozilla.org/啊-test", {
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.HISTORY,
        uri: "https://www.mozilla.org/%E5%95%8A-test",
        title: "test visit for https://www.mozilla.org/%E5%95%8A-test",
        iconUri: "page-icon:https://www.mozilla.org/%E5%95%8A-test",
        heuristic: true,
      }),
    ],
  });
  await cleanupPlaces();
});
