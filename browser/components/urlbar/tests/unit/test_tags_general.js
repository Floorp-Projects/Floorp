/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
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

var uri1 = "http://site.tld/1/aaa";
var uri2 = "http://site.tld/2/bbb";
var uri3 = "http://site.tld/3/aaa";
var uri4 = "http://site.tld/4/bbb";
var uri5 = "http://site.tld/5/aaa";
var uri6 = "http://site.tld/6/bbb";

var tests = [
  () =>
    ensure_tag_results(
      [
        [uri1, ["foo"]],
        [uri4, ["foo bar"]],
        [uri6, ["foo bar cheese"]],
      ],
      "foo"
    ),
  () => ensure_tag_results([[uri1, ["foo"]]], "foo aaa"),
  () =>
    ensure_tag_results(
      [
        [uri4, ["foo bar"]],
        [uri6, ["foo bar cheese"]],
      ],
      "foo bbb"
    ),
  () =>
    ensure_tag_results(
      [
        [uri2, ["bar"]],
        [uri4, ["foo bar"]],
        [uri5, ["bar cheese"]],
        [uri6, ["foo bar cheese"]],
      ],
      "bar"
    ),
  () => ensure_tag_results([[uri5, ["bar cheese"]]], "bar aaa"),
  () =>
    ensure_tag_results(
      [
        [uri2, ["bar"]],
        [uri4, ["foo bar"]],
        [uri6, ["foo bar cheese"]],
      ],
      "bar bbb"
    ),
  () =>
    ensure_tag_results(
      [
        [uri3, ["cheese"]],
        [uri5, ["bar cheese"]],
        [uri6, ["foo bar cheese"]],
      ],
      "cheese"
    ),
  () =>
    ensure_tag_results(
      [
        [uri3, ["cheese"]],
        [uri5, ["bar cheese"]],
      ],
      "chees aaa"
    ),
  () => ensure_tag_results([[uri6, ["foo bar cheese"]]], "chees bbb"),
  () =>
    ensure_tag_results(
      [
        [uri4, ["foo bar"]],
        [uri6, ["foo bar cheese"]],
      ],
      "fo bar"
    ),
  () => ensure_tag_results([], "fo bar aaa"),
  () =>
    ensure_tag_results(
      [
        [uri4, ["foo bar"]],
        [uri6, ["foo bar cheese"]],
      ],
      "fo bar bbb"
    ),
  () =>
    ensure_tag_results(
      [
        [uri4, ["foo bar"]],
        [uri6, ["foo bar cheese"]],
      ],
      "ba foo"
    ),
  () => ensure_tag_results([], "ba foo aaa"),
  () =>
    ensure_tag_results(
      [
        [uri4, ["foo bar"]],
        [uri6, ["foo bar cheese"]],
      ],
      "ba foo bbb"
    ),
  () =>
    ensure_tag_results(
      [
        [uri5, ["bar cheese"]],
        [uri6, ["foo bar cheese"]],
      ],
      "ba chee"
    ),
  () => ensure_tag_results([[uri5, ["bar cheese"]]], "ba chee aaa"),
  () => ensure_tag_results([[uri6, ["foo bar cheese"]]], "ba chee bbb"),
  () =>
    ensure_tag_results(
      [
        [uri5, ["bar cheese"]],
        [uri6, ["foo bar cheese"]],
      ],
      "cheese bar"
    ),
  () => ensure_tag_results([[uri5, ["bar cheese"]]], "cheese bar aaa"),
  () => ensure_tag_results([[uri6, ["foo bar cheese"]]], "chees bar bbb"),
  () => ensure_tag_results([[uri6, ["foo bar cheese"]]], "cheese bar foo"),
  () => ensure_tag_results([], "foo bar cheese aaa"),
  () => ensure_tag_results([[uri6, ["foo bar cheese"]]], "foo bar cheese bbb"),
];

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
 * Test history autocomplete
 */
add_task(async function test_history_autocomplete_tags() {
  // always search in history + bookmarks, no matter what the default is
  Services.prefs.setBoolPref("browser.urlbar.suggest.history", true);
  Services.prefs.setBoolPref("browser.urlbar.suggest.bookmarks", true);
  Services.prefs.setBoolPref("browser.urlbar.suggest.searches", false);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("browser.urlbar.suggest.history");
    Services.prefs.clearUserPref("browser.urlbar.suggest.bookmarks");
    Services.prefs.clearUserPref("browser.urlbar.suggest.searches");
  });

  await tagURI(uri6, ["foo bar cheese"]);
  await tagURI(uri5, ["bar cheese"]);
  await tagURI(uri4, ["foo bar"]);
  await tagURI(uri3, ["cheese"]);
  await tagURI(uri2, ["bar"]);
  await tagURI(uri1, ["foo"]);

  for (let tagTest of tests) {
    await tagTest();
  }
});
