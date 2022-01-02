/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_duplicates() {
  const TEST_URL = "https://history.mozilla.org/";
  await PlacesTestUtils.addVisits([
    { uri: TEST_URL, title: "Test history" },
    { uri: TEST_URL + "?#", title: "Test history" },
    { uri: TEST_URL + "#", title: "Test history" },
  ]);

  let controller = UrlbarTestUtils.newMockController();
  let searchString = "^Hist";
  let context = createContext(searchString, { isPrivate: false });
  await controller.startQuery(context);

  // The first result will be a search heuristic, which we don't care about for
  // this test.
  info(
    "Results:\n" +
      context.results.map(m => `${m.title} - ${m.payload.url}`).join("\n")
  );
  Assert.equal(
    context.results.length,
    2,
    "Found the expected number of matches"
  );
  Assert.equal(
    context.results[1].type,
    UrlbarUtils.RESULT_TYPE.URL,
    "Should have a history  result"
  );
  Assert.equal(
    context.results[1].payload.url,
    TEST_URL + "#",
    "Check result URL"
  );

  await PlacesUtils.history.clear();
});
