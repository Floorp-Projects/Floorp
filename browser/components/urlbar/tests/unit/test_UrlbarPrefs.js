/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(function test() {
  Assert.throws(
    () => UrlbarPrefs.get("browser.migration.version"),
    /Trying to access an unknown pref/,
    "Should throw when passing an untracked pref"
  );

  Assert.throws(
    () => UrlbarPrefs.set("browser.migration.version", 100),
    /Trying to access an unknown pref/,
    "Should throw when passing an untracked pref"
  );
  Assert.throws(
    () => UrlbarPrefs.set("maxRichResults", "10"),
    /Invalid value/,
    "Should throw when passing an invalid value type"
  );

  Assert.deepEqual(UrlbarPrefs.get("formatting.enabled"), true);
  UrlbarPrefs.set("formatting.enabled", false);
  Assert.deepEqual(UrlbarPrefs.get("formatting.enabled"), false);

  Assert.deepEqual(UrlbarPrefs.get("maxRichResults"), 10);
  UrlbarPrefs.set("maxRichResults", 6);
  Assert.deepEqual(UrlbarPrefs.get("maxRichResults"), 6);

  Assert.deepEqual(UrlbarPrefs.get("autoFill.stddevMultiplier"), 0.0);
  UrlbarPrefs.set("autoFill.stddevMultiplier", 0.01);
  // Due to rounding errors, floats are slightly imprecise, so we can't
  // directly compare what we set to what we retrieve.
  Assert.deepEqual(
    parseFloat(UrlbarPrefs.get("autoFill.stddevMultiplier").toFixed(2)),
    0.01
  );
});

// Tests UrlbarPrefs.makeResultBuckets({ showSearchSuggestionsFirst: true }).
add_task(function makeResultBuckets_true() {
  Assert.deepEqual(
    UrlbarPrefs.makeResultBuckets({ showSearchSuggestionsFirst: true }),
    {
      children: [
        // heuristic
        {
          maxResultCount: 1,
          children: [
            { group: UrlbarUtils.RESULT_GROUP.HEURISTIC_TEST },
            { group: UrlbarUtils.RESULT_GROUP.HEURISTIC_EXTENSION },
            { group: UrlbarUtils.RESULT_GROUP.HEURISTIC_SEARCH_TIP },
            { group: UrlbarUtils.RESULT_GROUP.HEURISTIC_OMNIBOX },
            { group: UrlbarUtils.RESULT_GROUP.HEURISTIC_ENGINE_ALIAS },
            { group: UrlbarUtils.RESULT_GROUP.HEURISTIC_BOOKMARK_KEYWORD },
            { group: UrlbarUtils.RESULT_GROUP.HEURISTIC_AUTOFILL },
            { group: UrlbarUtils.RESULT_GROUP.HEURISTIC_PRELOADED },
            { group: UrlbarUtils.RESULT_GROUP.HEURISTIC_TOKEN_ALIAS_ENGINE },
            { group: UrlbarUtils.RESULT_GROUP.HEURISTIC_FALLBACK },
          ],
        },
        // extensions using the omnibox API
        {
          group: UrlbarUtils.RESULT_GROUP.OMNIBOX,
          availableSpan: UrlbarUtils.MAX_OMNIBOX_RESULT_COUNT - 1,
        },
        // main bucket
        {
          flexChildren: true,
          children: [
            // suggestions
            {
              flex: 2,
              children: [
                {
                  flexChildren: true,
                  children: [
                    {
                      flex: 2,
                      group: UrlbarUtils.RESULT_GROUP.FORM_HISTORY,
                    },
                    {
                      flex: 4,
                      group: UrlbarUtils.RESULT_GROUP.REMOTE_SUGGESTION,
                    },
                  ],
                },
                {
                  group: UrlbarUtils.RESULT_GROUP.TAIL_SUGGESTION,
                },
              ],
            },
            // general
            {
              group: UrlbarUtils.RESULT_GROUP.GENERAL_PARENT,
              flex: 1,
              children: [
                {
                  availableSpan: 3,
                  group: UrlbarUtils.RESULT_GROUP.INPUT_HISTORY,
                },
                {
                  flexChildren: true,
                  children: [
                    {
                      flex: 1,
                      group: UrlbarUtils.RESULT_GROUP.REMOTE_TAB,
                    },
                    {
                      flex: 2,
                      group: UrlbarUtils.RESULT_GROUP.GENERAL,
                    },
                    {
                      flex: 2,
                      group: UrlbarUtils.RESULT_GROUP.ABOUT_PAGES,
                    },
                    {
                      flex: 1,
                      group: UrlbarUtils.RESULT_GROUP.PRELOADED,
                    },
                  ],
                },
                {
                  group: UrlbarUtils.RESULT_GROUP.INPUT_HISTORY,
                },
              ],
            },
          ],
        },
      ],
    }
  );
});

// Tests UrlbarPrefs.makeResultBuckets({ showSearchSuggestionsFirst: false }).
add_task(function makeResultBuckets_false() {
  Assert.deepEqual(
    UrlbarPrefs.makeResultBuckets({ showSearchSuggestionsFirst: false }),

    {
      children: [
        // heuristic
        {
          maxResultCount: 1,
          children: [
            { group: UrlbarUtils.RESULT_GROUP.HEURISTIC_TEST },
            { group: UrlbarUtils.RESULT_GROUP.HEURISTIC_EXTENSION },
            { group: UrlbarUtils.RESULT_GROUP.HEURISTIC_SEARCH_TIP },
            { group: UrlbarUtils.RESULT_GROUP.HEURISTIC_OMNIBOX },
            { group: UrlbarUtils.RESULT_GROUP.HEURISTIC_ENGINE_ALIAS },
            { group: UrlbarUtils.RESULT_GROUP.HEURISTIC_BOOKMARK_KEYWORD },
            { group: UrlbarUtils.RESULT_GROUP.HEURISTIC_AUTOFILL },
            { group: UrlbarUtils.RESULT_GROUP.HEURISTIC_PRELOADED },
            { group: UrlbarUtils.RESULT_GROUP.HEURISTIC_TOKEN_ALIAS_ENGINE },
            { group: UrlbarUtils.RESULT_GROUP.HEURISTIC_FALLBACK },
          ],
        },
        // extensions using the omnibox API
        {
          group: UrlbarUtils.RESULT_GROUP.OMNIBOX,
          availableSpan: UrlbarUtils.MAX_OMNIBOX_RESULT_COUNT - 1,
        },
        // main bucket
        {
          flexChildren: true,
          children: [
            // general
            {
              group: UrlbarUtils.RESULT_GROUP.GENERAL_PARENT,
              flex: 2,
              children: [
                {
                  availableSpan: 3,
                  group: UrlbarUtils.RESULT_GROUP.INPUT_HISTORY,
                },
                {
                  flexChildren: true,
                  children: [
                    {
                      flex: 1,
                      group: UrlbarUtils.RESULT_GROUP.REMOTE_TAB,
                    },
                    {
                      flex: 2,
                      group: UrlbarUtils.RESULT_GROUP.GENERAL,
                    },
                    {
                      flex: 2,
                      group: UrlbarUtils.RESULT_GROUP.ABOUT_PAGES,
                    },
                    {
                      flex: 1,
                      group: UrlbarUtils.RESULT_GROUP.PRELOADED,
                    },
                  ],
                },
                {
                  group: UrlbarUtils.RESULT_GROUP.INPUT_HISTORY,
                },
              ],
            },
            // suggestions
            {
              flex: 1,
              children: [
                {
                  flexChildren: true,
                  children: [
                    {
                      flex: 2,
                      group: UrlbarUtils.RESULT_GROUP.FORM_HISTORY,
                    },
                    {
                      flex: 4,
                      group: UrlbarUtils.RESULT_GROUP.REMOTE_SUGGESTION,
                    },
                  ],
                },
                {
                  group: UrlbarUtils.RESULT_GROUP.TAIL_SUGGESTION,
                },
              ],
            },
          ],
        },
      ],
    }
  );
});

// Tests interaction between showSearchSuggestionsFirst and resultGroups.
add_task(function showSearchSuggestionsFirst_resultGroups() {
  // Check initial values.
  Assert.equal(
    UrlbarPrefs.get("showSearchSuggestionsFirst"),
    true,
    "showSearchSuggestionsFirst is true initially"
  );
  Assert.equal(
    Services.prefs.getCharPref("browser.urlbar.resultGroups", ""),
    "",
    "resultGroups is empty initially"
  );

  // Set showSearchSuggestionsFirst = false.
  UrlbarPrefs.set("showSearchSuggestionsFirst", false);
  Assert.ok(
    Services.prefs.getCharPref("browser.urlbar.resultGroups", ""),
    "resultGroups should exist after setting showSearchSuggestionsFirst"
  );
  Assert.deepEqual(
    JSON.parse(Services.prefs.getCharPref("browser.urlbar.resultGroups")),
    UrlbarPrefs.makeResultBuckets({ showSearchSuggestionsFirst: false }),
    "resultGroups is updated after setting showSearchSuggestionsFirst = false"
  );

  // Set showSearchSuggestionsFirst = true.
  UrlbarPrefs.set("showSearchSuggestionsFirst", true);
  Assert.deepEqual(
    JSON.parse(Services.prefs.getCharPref("browser.urlbar.resultGroups")),
    UrlbarPrefs.makeResultBuckets({ showSearchSuggestionsFirst: true }),
    "resultGroups is updated after setting showSearchSuggestionsFirst = true"
  );

  // Set showSearchSuggestionsFirst = false again so we can clear it next.
  UrlbarPrefs.set("showSearchSuggestionsFirst", false);
  Assert.deepEqual(
    JSON.parse(Services.prefs.getCharPref("browser.urlbar.resultGroups")),
    UrlbarPrefs.makeResultBuckets({ showSearchSuggestionsFirst: false }),
    "resultGroups is updated after setting showSearchSuggestionsFirst = false"
  );

  // Clear showSearchSuggestionsFirst.
  Services.prefs.clearUserPref("browser.urlbar.showSearchSuggestionsFirst");
  Assert.deepEqual(
    JSON.parse(Services.prefs.getCharPref("browser.urlbar.resultGroups")),
    UrlbarPrefs.makeResultBuckets({ showSearchSuggestionsFirst: true }),
    "resultGroups is updated immediately after clearing showSearchSuggestionsFirst"
  );
  Assert.equal(
    UrlbarPrefs.get("showSearchSuggestionsFirst"),
    true,
    "showSearchSuggestionsFirst defaults to true after clearing it"
  );
  Assert.deepEqual(
    JSON.parse(Services.prefs.getCharPref("browser.urlbar.resultGroups")),
    UrlbarPrefs.makeResultBuckets({ showSearchSuggestionsFirst: true }),
    "resultGroups remains correct after getting showSearchSuggestionsFirst"
  );
});

// Tests UrlbarPrefs.initializeShowSearchSuggestionsFirstPref() and the
// interaction between matchBuckets, showSearchSuggestionsFirst, and
// resultGroups.  It's a little complex, but the flow is:
//
// 1. The old matchBuckets pref has some value
// 2. UrlbarPrefs.initializeShowSearchSuggestionsFirstPref() is called to
//    translate matchBuckets into the newer showSearchSuggestionsFirst pref
// 3. The update to showSearchSuggestionsFirst causes the new resultGroups
//    pref to be set
add_task(function initializeShowSearchSuggestionsFirstPref() {
  // Each value in `tests`: [matchBuckets, expectedShowSearchSuggestionsFirst]
  let tests = [
    ["suggestion:4,general:Infinity", true],
    ["suggestion:4,general:5", true],
    ["suggestion:1,general:5,suggestion:Infinity", true],
    ["suggestion:Infinity", true],
    ["suggestion:4", true],

    ["foo:1,suggestion:4,general:Infinity", true],
    ["foo:2,suggestion:4,general:5", true],
    ["foo:3,suggestion:1,general:5,suggestion:Infinity", true],
    ["foo:4,suggestion:Infinity", true],
    ["foo:5,suggestion:4", true],

    ["general:5,suggestion:Infinity", false],
    ["general:5,suggestion:4", false],
    ["general:1,suggestion:4,general:Infinity", false],
    ["general:Infinity", false],
    ["general:5", false],

    ["foo:1,general:5,suggestion:Infinity", false],
    ["foo:2,general:5,suggestion:4", false],
    ["foo:3,general:1,suggestion:4,general:Infinity", false],
    ["foo:4,general:Infinity", false],
    ["foo:5,general:5", false],

    ["", true],
    ["bogus buckets", true],
  ];

  for (let [matchBuckets, expectedValue] of tests) {
    info("Running test: " + JSON.stringify({ matchBuckets, expectedValue }));
    Services.prefs.clearUserPref("browser.urlbar.showSearchSuggestionsFirst");

    // Set matchBuckets.
    Services.prefs.setCharPref("browser.urlbar.matchBuckets", matchBuckets);

    // Call initializeShowSearchSuggestionsFirstPref.
    UrlbarPrefs.initializeShowSearchSuggestionsFirstPref();

    // Both showSearchSuggestionsFirst and resultGroups should be updated.
    Assert.equal(
      Services.prefs.getBoolPref("browser.urlbar.showSearchSuggestionsFirst"),
      expectedValue,
      "showSearchSuggestionsFirst has the expected value"
    );
    Assert.deepEqual(
      JSON.parse(Services.prefs.getCharPref("browser.urlbar.resultGroups")),
      UrlbarPrefs.makeResultBuckets({
        showSearchSuggestionsFirst: expectedValue,
      }),
      "resultGroups should be updated with the appropriate default"
    );
  }

  Services.prefs.clearUserPref("browser.urlbar.matchBuckets");
});
