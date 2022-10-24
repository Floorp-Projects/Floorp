/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

testEngine_setup();

/**
 * Checks the results of a search for `searchTerm`.
 *
 * @param {Array} uris
 *   A 2-element array containing [{string} uri, {array} tags}], where `tags`
 *   is a comma-separated list of the tags expected to appear in the search.
 * @param {string} searchTerm
 *   The term to search for
 */
async function ensure_tag_results(uris, searchTerm) {
  print("Searching for '" + searchTerm + "'");
  let context = createContext(searchTerm, { isPrivate: false });
  let urlbarResults = [];
  for (let [uri, tags] of uris) {
    urlbarResults.push(
      makeBookmarkResult(context, {
        uri,
        title: "A title",
        tags,
      })
    );
  }
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        heuristic: true,
      }),
      ...urlbarResults,
    ],
  });
}

const uri1 = "http://site.tld/1";
const uri2 = "http://site.tld/2";
const uri3 = "http://site.tld/3";
const uri4 = "http://site.tld/4";
const uri5 = "http://site.tld/5";
const uri6 = "http://site.tld/6";

/**
 * Properly tags a uri adding it to bookmarks.
 *
 * @param {string} url
 *        The URI to tag.
 * @param {Array} tags
 *        The tags to add.
 */
async function tagURI(url, tags) {
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url,
    title: "A title",
  });
  PlacesUtils.tagging.tagURI(Services.io.newURI(url), tags);
}

/**
 * Test bug #408221
 */
add_task(async function test_tags_search_case_insensitivity() {
  // always search in history + bookmarks, no matter what the default is
  Services.prefs.setBoolPref("browser.urlbar.suggest.history", true);
  Services.prefs.setBoolPref("browser.urlbar.suggest.bookmarks", true);
  Services.prefs.setBoolPref("browser.urlbar.suggest.searches", false);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("browser.urlbar.suggest.history");
    Services.prefs.clearUserPref("browser.urlbar.suggest.bookmarks");
    Services.prefs.clearUserPref("browser.urlbar.suggest.searches");
  });

  await tagURI(uri6, ["muD"]);
  await tagURI(uri6, ["baR"]);
  await tagURI(uri5, ["mud"]);
  await tagURI(uri5, ["bar"]);
  await tagURI(uri4, ["MUD"]);
  await tagURI(uri4, ["BAR"]);
  await tagURI(uri3, ["foO"]);
  await tagURI(uri2, ["FOO"]);
  await tagURI(uri1, ["Foo"]);

  await ensure_tag_results(
    [
      [uri1, ["Foo"]],
      [uri2, ["Foo"]],
      [uri3, ["Foo"]],
    ],
    "foo"
  );
  await ensure_tag_results(
    [
      [uri1, ["Foo"]],
      [uri2, ["Foo"]],
      [uri3, ["Foo"]],
    ],
    "Foo"
  );
  await ensure_tag_results(
    [
      [uri1, ["Foo"]],
      [uri2, ["Foo"]],
      [uri3, ["Foo"]],
    ],
    "foO"
  );
  await ensure_tag_results(
    [
      [uri4, ["BAR", "MUD"]],
      [uri5, ["BAR", "MUD"]],
      [uri6, ["BAR", "MUD"]],
    ],
    "bar mud"
  );
  await ensure_tag_results(
    [
      [uri4, ["BAR", "MUD"]],
      [uri5, ["BAR", "MUD"]],
      [uri6, ["BAR", "MUD"]],
    ],
    "BAR MUD"
  );
  await ensure_tag_results(
    [
      [uri4, ["BAR", "MUD"]],
      [uri5, ["BAR", "MUD"]],
      [uri6, ["BAR", "MUD"]],
    ],
    "Bar Mud"
  );
});
