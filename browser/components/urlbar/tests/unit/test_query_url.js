/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const PLACES_PROVIDERNAME = "Places";

testEngine_setup();

add_task(async function test_no_slash() {
  info("Searching for host match without slash should match host");
  await PlacesTestUtils.addVisits([
    { uri: "http://file.org/test/" },
    { uri: "file:///c:/test.html" },
  ]);
  let context = createContext("file", { isPrivate: false });
  await check_results({
    context,
    autofilled: "file.org/",
    completed: "http://file.org/",
    hasAutofillTitle: false,
    matches: [
      makeVisitResult(context, {
        uri: "http://file.org/",
        title: "file.org",
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: "file:///c:/test.html",
        title: "test visit for file:///c:/test.html",
        iconUri: UrlbarUtils.ICON.DEFAULT,
        providerName: PLACES_PROVIDERNAME,
      }),
      makeVisitResult(context, {
        uri: "http://file.org/test/",
        title: "test visit for http://file.org/test/",
        providerName: PLACES_PROVIDERNAME,
      }),
    ],
  });
  await cleanupPlaces();
});

add_task(async function test_w_slash() {
  info("Searching match with slash at the end should match url");
  await PlacesTestUtils.addVisits(
    {
      uri: Services.io.newURI("http://file.org/test/"),
    },
    {
      uri: Services.io.newURI("file:///c:/test.html"),
    }
  );
  let context = createContext("file.org/", { isPrivate: false });
  await check_results({
    context,
    autofilled: "file.org/",
    completed: "http://file.org/",
    hasAutofillTitle: false,
    matches: [
      makeVisitResult(context, {
        uri: "http://file.org/",
        title: "file.org/",
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: "http://file.org/test/",
        title: "test visit for http://file.org/test/",
        providerName: PLACES_PROVIDERNAME,
      }),
    ],
  });
  await cleanupPlaces();
});

add_task(async function test_middle() {
  info("Searching match with slash in the middle should match url");
  await PlacesTestUtils.addVisits(
    {
      uri: Services.io.newURI("http://file.org/test/"),
    },
    {
      uri: Services.io.newURI("file:///c:/test.html"),
    }
  );
  let context = createContext("file.org/t", { isPrivate: false });
  await check_results({
    context,
    autofilled: "file.org/test/",
    completed: "http://file.org/test/",
    hasAutofillTitle: true,
    matches: [
      makeVisitResult(context, {
        uri: "http://file.org/test/",
        title: "test visit for http://file.org/test/",
        heuristic: true,
      }),
    ],
  });
  await cleanupPlaces();
});

add_task(async function test_nonhost() {
  info("Searching for non-host match without slash should not match url");
  await PlacesTestUtils.addVisits({
    uri: Services.io.newURI("file:///c:/test.html"),
  });
  let context = createContext("file", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: "file:///c:/test.html",
        title: "test visit for file:///c:/test.html",
        iconUri: UrlbarUtils.ICON.DEFAULT,
        providerName: PLACES_PROVIDERNAME,
      }),
    ],
  });
  await cleanupPlaces();
});
