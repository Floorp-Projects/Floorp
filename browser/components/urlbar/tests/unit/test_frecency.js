/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test for bug 406358 to make sure frecency works for empty input/search, but
 * this also tests for non-empty inputs as well. Because the interactions among
 * *DIFFERENT* visit counts and visit dates is not well defined, this test
 * holds one of the two values constant when modifying the other.
 *
 * Also test bug 419068 to make sure tagged pages don't necessarily have to be
 * first in the results.
 *
 * Also test bug 426166 to make sure that the results of autocomplete searches
 * are stable.  Note that failures of this test will be intermittent by nature
 * since we are testing to make sure that the unstable sort algorithm used
 * by SQLite is not changing the order of the results on us.
 */

testEngine_setup();

async function task_setCountDate(uri, count, date) {
  // We need visits so that frecency can be computed over multiple visits
  let visits = [];
  for (let i = 0; i < count; i++) {
    visits.push({
      uri,
      visitDate: date,
      transition: PlacesUtils.history.TRANSITION_TYPED,
    });
  }
  await PlacesTestUtils.addVisits(visits);
}

async function setBookmark(uri) {
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    url: uri,
    title: "bleh",
  });
}

async function tagURI(uri, tags) {
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: uri,
    title: "bleh",
  });
  PlacesUtils.tagging.tagURI(uri, tags);
}

var uri1 = Services.io.newURI("http://site.tld/1");
var uri2 = Services.io.newURI("http://site.tld/2");
var uri3 = Services.io.newURI("http://aaaaaaaaaa/1");
var uri4 = Services.io.newURI("http://aaaaaaaaaa/2");

// d1 is younger (should show up higher) than d2 (PRTime is in usecs not msec)
// Make sure the dates fall into different frecency groups
var d1 = new Date(Date.now() - 1000 * 60 * 60) * 1000;
var d2 = new Date(Date.now() - 1000 * 60 * 60 * 24 * 10) * 1000;
// c1 is larger (should show up higher) than c2
var c1 = 10;
var c2 = 1;

var tests = [
  // test things without a search term
  async function() {
    info("Test 0: same count, different date");
    await task_setCountDate(uri1, c1, d1);
    await task_setCountDate(uri2, c1, d2);
    await tagURI(uri1, ["site"]);
    let context = createContext(" ", { isPrivate: false });
    await check_results({
      context,
      matches: [
        makeSearchResult(context, {
          engineName: SUGGESTIONS_ENGINE_NAME,
          heuristic: true,
          query: " ",
        }),
        // uri1 is a visit result despite being a tagged bookmark because we
        // are searching for the empty string. By default, the empty string
        // filters to history. uri1 will be displayed as a bookmark later in the
        // test when we are searching with a non-empty string.
        makeVisitResult(context, {
          uri: uri1.spec,
          title: "bleh",
        }),
        makeVisitResult(context, {
          uri: uri2.spec,
          title: `test visit for ${uri2.spec}`,
        }),
      ],
    });
  },
  async function() {
    info("Test 1: same count, different date");
    await task_setCountDate(uri1, c1, d2);
    await task_setCountDate(uri2, c1, d1);
    await tagURI(uri1, ["site"]);
    let context = createContext(" ", { isPrivate: false });
    await check_results({
      context,
      matches: [
        makeSearchResult(context, {
          engineName: SUGGESTIONS_ENGINE_NAME,
          heuristic: true,
          query: " ",
        }),
        makeVisitResult(context, {
          uri: uri2.spec,
          title: `test visit for ${uri2.spec}`,
        }),
        makeVisitResult(context, {
          uri: uri1.spec,
          title: "bleh",
        }),
      ],
    });
  },
  async function() {
    info("Test 2: different count, same date");
    await task_setCountDate(uri1, c1, d1);
    await task_setCountDate(uri2, c2, d1);
    await tagURI(uri1, ["site"]);
    let context = createContext(" ", { isPrivate: false });
    await check_results({
      context,
      matches: [
        makeSearchResult(context, {
          engineName: SUGGESTIONS_ENGINE_NAME,
          heuristic: true,
          query: " ",
        }),
        makeVisitResult(context, {
          uri: uri1.spec,
          title: "bleh",
        }),
        makeVisitResult(context, {
          uri: uri2.spec,
          title: `test visit for ${uri2.spec}`,
        }),
      ],
    });
  },
  async function() {
    info("Test 3: different count, same date");
    await task_setCountDate(uri1, c2, d1);
    await task_setCountDate(uri2, c1, d1);
    await tagURI(uri1, ["site"]);
    let context = createContext(" ", { isPrivate: false });
    await check_results({
      context,
      matches: [
        makeSearchResult(context, {
          engineName: SUGGESTIONS_ENGINE_NAME,
          heuristic: true,
          query: " ",
        }),
        makeVisitResult(context, {
          uri: uri2.spec,
          title: `test visit for ${uri2.spec}`,
        }),
        makeVisitResult(context, {
          uri: uri1.spec,
          title: "bleh",
        }),
      ],
    });
  },

  // test things with a search term
  async function() {
    info("Test 4: same count, different date");
    await task_setCountDate(uri1, c1, d1);
    await task_setCountDate(uri2, c1, d2);
    await tagURI(uri1, ["site"]);
    let context = createContext("site", { isPrivate: false });
    await check_results({
      context,
      matches: [
        makeSearchResult(context, {
          engineName: SUGGESTIONS_ENGINE_NAME,
          heuristic: true,
        }),
        makeBookmarkResult(context, {
          uri: uri1.spec,
          title: "bleh",
          tags: ["site"],
        }),
        makeVisitResult(context, {
          uri: uri2.spec,
          title: `test visit for ${uri2.spec}`,
        }),
      ],
    });
  },
  async function() {
    info("Test 5: same count, different date");
    await task_setCountDate(uri1, c1, d2);
    await task_setCountDate(uri2, c1, d1);
    await tagURI(uri1, ["site"]);
    let context = createContext("site", { isPrivate: false });
    await check_results({
      context,
      matches: [
        makeSearchResult(context, {
          engineName: SUGGESTIONS_ENGINE_NAME,
          heuristic: true,
        }),
        makeVisitResult(context, {
          uri: uri2.spec,
          title: `test visit for ${uri2.spec}`,
        }),
        makeBookmarkResult(context, {
          uri: uri1.spec,
          title: "bleh",
          tags: ["site"],
        }),
      ],
    });
  },
  async function() {
    info("Test 6: different count, same date");
    await task_setCountDate(uri1, c1, d1);
    await task_setCountDate(uri2, c2, d1);
    await tagURI(uri1, ["site"]);
    let context = createContext("site", { isPrivate: false });
    await check_results({
      context,
      matches: [
        makeSearchResult(context, {
          engineName: SUGGESTIONS_ENGINE_NAME,
          heuristic: true,
        }),
        makeBookmarkResult(context, {
          uri: uri1.spec,
          title: "bleh",
          tags: ["site"],
        }),
        makeVisitResult(context, {
          uri: uri2.spec,
          title: `test visit for ${uri2.spec}`,
        }),
      ],
    });
  },
  async function() {
    info("Test 7: different count, same date");
    await task_setCountDate(uri1, c2, d1);
    await task_setCountDate(uri2, c1, d1);
    await tagURI(uri1, ["site"]);
    let context = createContext("site", { isPrivate: false });
    await check_results({
      context,
      matches: [
        makeSearchResult(context, {
          engineName: SUGGESTIONS_ENGINE_NAME,
          heuristic: true,
        }),
        makeVisitResult(context, {
          uri: uri2.spec,
          title: `test visit for ${uri2.spec}`,
        }),
        makeBookmarkResult(context, {
          uri: uri1.spec,
          title: "bleh",
          tags: ["site"],
        }),
      ],
    });
  },
  // There are multiple tests for 8, hence the multiple functions
  // Bug 426166 section
  async function() {
    info("Test 8.1a: same count, same date");
    await setBookmark(uri3);
    await setBookmark(uri4);
    let context = createContext("a", { isPrivate: false });
    let bookmarkResults = [
      makeBookmarkResult(context, {
        uri: uri4.spec,
        title: "bleh",
      }),
      makeBookmarkResult(context, {
        uri: uri3.spec,
        title: "bleh",
      }),
    ];
    await check_results({
      context,
      matches: [
        makeSearchResult(context, {
          engineName: SUGGESTIONS_ENGINE_NAME,
          heuristic: true,
        }),
        ...bookmarkResults,
      ],
    });

    context = createContext("aa", { isPrivate: false });
    await check_results({
      context,
      matches: [
        // We need to continuously redefine the heuristic search result because it
        // is the only one that changes with the search string.
        makeSearchResult(context, {
          engineName: SUGGESTIONS_ENGINE_NAME,
          heuristic: true,
        }),
        ...bookmarkResults,
      ],
    });

    context = createContext("aaa", { isPrivate: false });
    await check_results({
      context,
      matches: [
        makeSearchResult(context, {
          engineName: SUGGESTIONS_ENGINE_NAME,
          heuristic: true,
        }),
        ...bookmarkResults,
      ],
    });

    context = createContext("aaaa", { isPrivate: false });
    await check_results({
      context,
      matches: [
        makeSearchResult(context, {
          engineName: SUGGESTIONS_ENGINE_NAME,
          heuristic: true,
        }),
        ...bookmarkResults,
      ],
    });

    context = createContext("aaa", { isPrivate: false });
    await check_results({
      context,
      matches: [
        makeSearchResult(context, {
          engineName: SUGGESTIONS_ENGINE_NAME,
          heuristic: true,
        }),
        ...bookmarkResults,
      ],
    });

    context = createContext("aa", { isPrivate: false });
    await check_results({
      context,
      matches: [
        makeSearchResult(context, {
          engineName: SUGGESTIONS_ENGINE_NAME,
          heuristic: true,
        }),
        ...bookmarkResults,
      ],
    });

    context = createContext("a", { isPrivate: false });
    await check_results({
      context,
      matches: [
        makeSearchResult(context, {
          engineName: SUGGESTIONS_ENGINE_NAME,
          heuristic: true,
        }),
        ...bookmarkResults,
      ],
    });
  },
];

add_task(async function test_frecency() {
  Services.prefs.setBoolPref("browser.urlbar.autoFill", false);
  // always search in history + bookmarks, no matter what the default is
  Services.prefs.setBoolPref("browser.urlbar.suggest.history", true);
  Services.prefs.setBoolPref("browser.urlbar.suggest.bookmark", true);
  Services.prefs.setBoolPref("browser.urlbar.suggest.openpage", false);
  Services.prefs.setBoolPref("browser.urlbar.suggest.searches", false);
  Services.prefs.setBoolPref("browser.urlbar.suggest.engines", false);
  Services.prefs.setBoolPref("browser.urlbar.suggest.quickactions", false);
  for (let test of tests) {
    await PlacesUtils.bookmarks.eraseEverything();
    await PlacesUtils.history.clear();

    await test();
  }
  for (let type of [
    "history",
    "bookmark",
    "openpage",
    "searches",
    "engines",
    "quickactions",
  ]) {
    Services.prefs.clearUserPref("browser.urlbar.suggest." + type);
    Services.prefs.clearUserPref("browser.urlbar.autoFill");
  }
});
