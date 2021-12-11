/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test bug 418257 by making sure tags are returned with the title as part of
 * the "comment" if there are tags even if we didn't match in the tags. They
 * are separated from the title by a endash.
 */

testEngine_setup();

add_task(async function test() {
  Services.prefs.setBoolPref("browser.urlbar.suggest.searches", false);
  Services.prefs.setBoolPref("browser.urlbar.autoFill", false);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("browser.urlbar.suggest.searches");
    Services.prefs.clearUserPref("browser.urlbar.autoFill");
  });

  let uri1 = Services.io.newURI("http://page1");
  let uri2 = Services.io.newURI("http://page2");
  let uri3 = Services.io.newURI("http://page3");
  let uri4 = Services.io.newURI("http://page4");
  await PlacesTestUtils.addVisits([
    { uri: uri1, title: "tagged" },
    { uri: uri2, title: "tagged" },
    { uri: uri3, title: "tagged" },
    { uri: uri4, title: "tagged" },
  ]);
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: uri1,
    title: "tagged",
    tags: ["tag1"],
  });
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: uri2,
    title: "tagged",
    tags: ["tag1", "tag2"],
  });
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: uri3,
    title: "tagged",
    tags: ["tag1", "tag3"],
  });
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: uri4,
    title: "tagged",
    tags: ["tag1", "tag2", "tag3"],
  });
  info("Make sure tags come back in the title when matching tags");
  let context = createContext("page1 tag", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeBookmarkResult(context, {
        uri: uri1.spec,
        title: "tagged",
        tags: ["tag1"],
      }),
    ],
  });

  info("Check tags in title for page2");
  context = createContext("page2 tag", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeBookmarkResult(context, {
        uri: uri2.spec,
        title: "tagged",
        tags: ["tag1", "tag2"],
      }),
    ],
  });

  info("Tags do not appear when not matching the tag");
  context = createContext("page3", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeBookmarkResult(context, {
        uri: uri3.spec,
        title: "tagged",
        tags: [],
      }),
    ],
  });

  info("Extra test just to make sure we match the title");
  context = createContext("tag2", { isPrivate: true });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeBookmarkResult(context, {
        uri: uri4.spec,
        title: "tagged",
        tags: ["tag2"],
      }),
      makeBookmarkResult(context, {
        uri: uri2.spec,
        title: "tagged",
        tags: ["tag2"],
      }),
    ],
  });

  await cleanupPlaces();
});
