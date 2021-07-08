/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Test autocomplete for non-English URLs that match the tag bug 416214. Also
 * test bug 417441 by making sure escaped ascii characters like "+" remain
 * escaped.
 *
 * - add a visit for a page with a non-English URL
 * - add a tag for the page
 * - search for the tag
 * - test number of matches (should be exactly one)
 * - make sure the url is decoded
 */

testEngine_setup();

add_task(async function test_tag_match_url() {
  Services.prefs.setBoolPref("browser.urlbar.suggest.searches", false);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("browser.urlbar.suggest.searches");
  });
  info(
    "Make sure tag matches return the right url as well as '+' remain escaped"
  );
  let uri1 = Services.io.newURI("http://escaped/ユニコード");
  let uri2 = Services.io.newURI("http://asciiescaped/blocking-firefox3%2B");
  await PlacesTestUtils.addVisits([
    { uri: uri1, title: "title" },
    { uri: uri2, title: "title" },
  ]);
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: uri1,
    title: "title",
    tags: ["superTag"],
    style: ["bookmark-tag"],
  });
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: uri2,
    title: "title",
    tags: ["superTag"],
    style: ["bookmark-tag"],
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
        uri: uri2.spec,
        title: "title",
        tags: ["superTag"],
      }),
      makeBookmarkResult(context, {
        uri: uri1.spec,
        title: "title",
        tags: ["superTag"],
      }),
    ],
  });
  await cleanupPlaces();
});
