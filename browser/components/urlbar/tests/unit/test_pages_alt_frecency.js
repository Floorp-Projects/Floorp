/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is a basic autocomplete test to ensure enabling the alternative frecency
// algorithm doesn't break results and sorts them appropriately.
// A more comprehensive testing of the algorithm itself is not included since it
// is something that may change frequently according to experimentation results.
// Other existing tests will, of course, need to be adapted once an algorithm
// is promoted to be the default.

testEngine_setup();

add_task(async function test_autofill() {
  const searchString = "match";
  const singleVisitUrl = "https://singlevisit-match.org/";
  const singleVisitBookmarkedUrl = "https://singlevisitbookmarked-match.org/";
  const adaptiveVisitUrl = "https://adaptivevisit-match.org/";
  const adaptiveManyVisitsUrl = "https://adaptivemanyvisit-match.org/";
  const manyVisitsUrl = "https://manyvisits-match.org/";
  const sampledVisitsUrl = "https://sampledvisits-match.org/";
  const bookmarkedUrl = "https://bookmarked-match.org/";

  await PlacesUtils.bookmarks.insert({
    url: bookmarkedUrl,
    title: "bookmark",
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
  });
  await PlacesUtils.bookmarks.insert({
    url: singleVisitBookmarkedUrl,
    title: "visited bookmark",
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
  });
  await PlacesTestUtils.addVisits([
    singleVisitUrl,
    singleVisitBookmarkedUrl,
    adaptiveVisitUrl,
    ...new Array(10).fill(adaptiveManyVisitsUrl),
    ...new Array(100).fill(manyVisitsUrl),
    ...new Array(10).fill(sampledVisitsUrl),
  ]);
  await UrlbarUtils.addToInputHistory(adaptiveVisitUrl, searchString);
  await UrlbarUtils.addToInputHistory(adaptiveManyVisitsUrl, searchString);

  let context = createContext(searchString, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: "Suggestions",
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: adaptiveManyVisitsUrl,
        title: `test visit for ${adaptiveManyVisitsUrl}`,
      }),
      makeVisitResult(context, {
        uri: adaptiveVisitUrl,
        title: `test visit for ${adaptiveVisitUrl}`,
      }),
      makeVisitResult(context, {
        uri: manyVisitsUrl,
        title: `test visit for ${manyVisitsUrl}`,
      }),
      makeVisitResult(context, {
        uri: sampledVisitsUrl,
        title: `test visit for ${sampledVisitsUrl}`,
      }),
      makeBookmarkResult(context, {
        uri: singleVisitBookmarkedUrl,
        title: "visited bookmark",
      }),
      makeBookmarkResult(context, {
        uri: bookmarkedUrl,
        title: "bookmark",
      }),
      makeVisitResult(context, {
        uri: singleVisitUrl,
        title: `test visit for ${singleVisitUrl}`,
      }),
    ],
  });

  await PlacesUtils.history.clear();
});
