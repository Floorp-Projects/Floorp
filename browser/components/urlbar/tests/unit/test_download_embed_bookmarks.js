/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim:set ts=2 sw=2 sts=2 et:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Tests bug 449406 to ensure that TRANSITION_DOWNLOAD, TRANSITION_EMBED and
 * TRANSITION_FRAMED_LINK bookmarked uri's show up in the location bar.
 */

testEngine_setup();

const TRANSITION_EMBED = PlacesUtils.history.TRANSITION_EMBED;
const TRANSITION_FRAMED_LINK = PlacesUtils.history.TRANSITION_FRAMED_LINK;
const TRANSITION_DOWNLOAD = PlacesUtils.history.TRANSITION_DOWNLOAD;

add_task(async function test_download_embed_bookmarks() {
  Services.prefs.setBoolPref("browser.urlbar.suggest.searches", false);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("browser.urlbar.suggest.searches");
  });

  let uri1 = Services.io.newURI("http://download/bookmarked");
  let uri2 = Services.io.newURI("http://embed/bookmarked");
  let uri3 = Services.io.newURI("http://framed/bookmarked");
  let uri4 = Services.io.newURI("http://download");
  let uri5 = Services.io.newURI("http://embed");
  let uri6 = Services.io.newURI("http://framed");
  await PlacesTestUtils.addVisits([
    { uri: uri1, title: "download-bookmark", transition: TRANSITION_DOWNLOAD },
    { uri: uri2, title: "embed-bookmark", transition: TRANSITION_EMBED },
    { uri: uri3, title: "framed-bookmark", transition: TRANSITION_FRAMED_LINK },
    { uri: uri4, title: "download2", transition: TRANSITION_DOWNLOAD },
    { uri: uri5, title: "embed2", transition: TRANSITION_EMBED },
    { uri: uri6, title: "framed2", transition: TRANSITION_FRAMED_LINK },
  ]);
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: uri1,
    title: "download-bookmark",
  });
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: uri2,
    title: "embed-bookmark",
  });
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: uri3,
    title: "framed-bookmark",
  });

  info("Searching for bookmarked download uri matches");
  let context = createContext("download-bookmark", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeBookmarkResult(context, {
        uri: uri1.spec,
        title: "download-bookmark",
      }),
    ],
  });

  info("Searching for bookmarked embed uri matches");
  context = createContext("embed-bookmark", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeBookmarkResult(context, {
        uri: uri2.spec,
        title: "embed-bookmark",
      }),
    ],
  });

  info("Searching for bookmarked framed uri matches");
  context = createContext("framed-bookmark", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      makeBookmarkResult(context, {
        uri: uri3.spec,
        title: "framed-bookmark",
      }),
    ],
  });

  info("Searching for download uri does not match");
  context = createContext("download2", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
    ],
  });

  info("Searching for embed uri does not match");
  context = createContext("embed2", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
    ],
  });

  info("Searching for framed uri does not match");
  context = createContext("framed2", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
    ],
  });

  await cleanupPlaces();
});
