/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Test bug 416211 to make sure results that match the tag show the bookmark
 * title instead of the page title.
 */

testEngine_setup();

add_task(async function test_tag_match_has_bookmark_title() {
  Services.prefs.setBoolPref("browser.urlbar.suggest.searches", false);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("browser.urlbar.suggest.searches");
  });

  info("Make sure the tag match gives the bookmark title");
  let uri = Services.io.newURI("http://theuri/");
  await PlacesTestUtils.addVisits({ uri, title: "Page title" });
  await PlacesTestUtils.addBookmarkWithDetails({
    uri,
    title: "Bookmark title",
    tags: ["superTag"],
  });
  let context = createContext("superTag", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeBookmarkResult(context, {
        uri: uri.spec,
        title: "Bookmark title",
        tags: ["superTag"],
      }),
    ],
  });
  await cleanupPlaces();
});
