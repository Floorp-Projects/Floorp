/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

testEngine_setup();

add_task(async function test_deduplication_for_switch_tab() {
  // Set up Places to think the tab is open locally.
  let uri = Services.io.newURI("http://example.com/");

  await PlacesTestUtils.addVisits({ uri, title: "An Example" });
  await addOpenPages(uri, 1);
  await UrlbarUtils.addToInputHistory("http://example.com/", "An");

  let query = "An";
  let context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeTabSwitchResult(context, {
        uri: "http://example.com/",
        title: "An Example",
      }),
    ],
  });

  await removeOpenPages(uri, 1);
  await cleanupPlaces();
});
