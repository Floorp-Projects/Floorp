/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Tests some cases where autofill should not happen.
 */

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
