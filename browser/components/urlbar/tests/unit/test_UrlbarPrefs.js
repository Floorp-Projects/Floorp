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

// Tests UrlbarPrefs.makeResultGroups({ showSearchSuggestionsFirst: true }).
add_task(function makeResultGroups_true() {
  Assert.deepEqual(
    UrlbarPrefs.makeResultGroups({ showSearchSuggestionsFirst: true }),
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
            { group: UrlbarUtils.RESULT_GROUP.HEURISTIC_TOKEN_ALIAS_ENGINE },
            { group: UrlbarUtils.RESULT_GROUP.HEURISTIC_HISTORY_URL },
            { group: UrlbarUtils.RESULT_GROUP.HEURISTIC_FALLBACK },
          ],
        },
        // extensions using the omnibox API
        {
          group: UrlbarUtils.RESULT_GROUP.OMNIBOX,
        },
        // main group
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
                      flex: 2,
                      group: UrlbarUtils.RESULT_GROUP.RECENT_SEARCH,
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

// Tests UrlbarPrefs.makeResultGroups({ showSearchSuggestionsFirst: false }).
add_task(function makeResultGroups_false() {
  Assert.deepEqual(
    UrlbarPrefs.makeResultGroups({ showSearchSuggestionsFirst: false }),

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
            { group: UrlbarUtils.RESULT_GROUP.HEURISTIC_TOKEN_ALIAS_ENGINE },
            { group: UrlbarUtils.RESULT_GROUP.HEURISTIC_HISTORY_URL },
            { group: UrlbarUtils.RESULT_GROUP.HEURISTIC_FALLBACK },
          ],
        },
        // extensions using the omnibox API
        {
          group: UrlbarUtils.RESULT_GROUP.OMNIBOX,
        },
        // main group
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
                      flex: 2,
                      group: UrlbarUtils.RESULT_GROUP.RECENT_SEARCH,
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
  Assert.deepEqual(
    UrlbarPrefs.resultGroups,
    UrlbarPrefs.makeResultGroups({ showSearchSuggestionsFirst: true }),
    "resultGroups is the same as the groups for which howSearchSuggestionsFirst is true"
  );

  // Set showSearchSuggestionsFirst = false.
  UrlbarPrefs.set("showSearchSuggestionsFirst", false);
  Assert.deepEqual(
    UrlbarPrefs.resultGroups,
    UrlbarPrefs.makeResultGroups({ showSearchSuggestionsFirst: false }),
    "resultGroups is updated after setting showSearchSuggestionsFirst = false"
  );

  // Set showSearchSuggestionsFirst = true.
  UrlbarPrefs.set("showSearchSuggestionsFirst", true);
  Assert.deepEqual(
    UrlbarPrefs.resultGroups,
    UrlbarPrefs.makeResultGroups({ showSearchSuggestionsFirst: true }),
    "resultGroups is updated after setting showSearchSuggestionsFirst = true"
  );

  // Set showSearchSuggestionsFirst = false again so we can clear it next.
  UrlbarPrefs.set("showSearchSuggestionsFirst", false);
  Assert.deepEqual(
    UrlbarPrefs.resultGroups,
    UrlbarPrefs.makeResultGroups({ showSearchSuggestionsFirst: false }),
    "resultGroups is updated after setting showSearchSuggestionsFirst = false"
  );

  // Clear showSearchSuggestionsFirst.
  Services.prefs.clearUserPref("browser.urlbar.showSearchSuggestionsFirst");
  Assert.deepEqual(
    UrlbarPrefs.resultGroups,
    UrlbarPrefs.makeResultGroups({ showSearchSuggestionsFirst: true }),
    "resultGroups is updated immediately after clearing showSearchSuggestionsFirst"
  );
  Assert.equal(
    UrlbarPrefs.get("showSearchSuggestionsFirst"),
    true,
    "showSearchSuggestionsFirst defaults to true after clearing it"
  );
  Assert.deepEqual(
    UrlbarPrefs.resultGroups,
    UrlbarPrefs.makeResultGroups({ showSearchSuggestionsFirst: true }),
    "resultGroups remains correct after getting showSearchSuggestionsFirst"
  );
});

// Tests UrlbarPrefs.initializeShowSearchSuggestionsFirstPref() and the
// interaction between matchGroups, showSearchSuggestionsFirst, and
// resultGroups.  It's a little complex, but the flow is:
//
// 1. The old matchGroups pref has some value
// 2. UrlbarPrefs.initializeShowSearchSuggestionsFirstPref() is called to
//    translate matchGroups into the newer showSearchSuggestionsFirst pref
// 3. The update to showSearchSuggestionsFirst causes the new resultGroups
//    pref to be set
add_task(function initializeShowSearchSuggestionsFirstPref() {
  // Each value in `tests`: [matchGroups, expectedShowSearchSuggestionsFirst]
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
    ["bogus groups", true],
  ];

  for (let [matchGroups, expectedValue] of tests) {
    info("Running test: " + JSON.stringify({ matchGroups, expectedValue }));
    Services.prefs.clearUserPref("browser.urlbar.showSearchSuggestionsFirst");

    // Set matchGroups.
    Services.prefs.setCharPref("browser.urlbar.matchGroups", matchGroups);

    // Call initializeShowSearchSuggestionsFirstPref.
    UrlbarPrefs.initializeShowSearchSuggestionsFirstPref();

    // Both showSearchSuggestionsFirst and resultGroups should be updated.
    Assert.equal(
      Services.prefs.getBoolPref("browser.urlbar.showSearchSuggestionsFirst"),
      expectedValue,
      "showSearchSuggestionsFirst has the expected value"
    );
    Assert.deepEqual(
      UrlbarPrefs.resultGroups,
      UrlbarPrefs.makeResultGroups({
        showSearchSuggestionsFirst: expectedValue,
      }),
      "resultGroups should be updated with the appropriate default"
    );
  }

  Services.prefs.clearUserPref("browser.urlbar.matchGroups");
});

// Tests whether observer.onNimbusChanged works.
add_task(async function onNimbusChanged() {
  Services.prefs.setBoolPref(
    "browser.urlbar.autoFill.adaptiveHistory.enabled",
    false
  );

  // Add an observer that throws an Error and an observer that does not define
  // anything to check whether the other observers can get notifications.
  UrlbarPrefs.addObserver({
    onPrefChanged(pref) {
      throw new Error("From onPrefChanged");
    },
    onNimbusChanged(pref) {
      throw new Error("From onNimbusChanged");
    },
  });
  UrlbarPrefs.addObserver({});

  const observer = {
    onPrefChanged(pref) {
      this.prefChangedList.push(pref);
    },
    onNimbusChanged(pref) {
      this.nimbusChangedList.push(pref);
    },
  };
  observer.prefChangedList = [];
  observer.nimbusChangedList = [];
  UrlbarPrefs.addObserver(observer);

  const doCleanup = await UrlbarTestUtils.initNimbusFeature({
    autoFillAdaptiveHistoryEnabled: true,
  });
  Assert.equal(observer.prefChangedList.length, 0);
  Assert.ok(
    observer.nimbusChangedList.includes("autoFillAdaptiveHistoryEnabled")
  );
  doCleanup();
});

// Tests whether observer.onPrefChanged works.
add_task(async function onPrefChanged() {
  const doCleanup = await UrlbarTestUtils.initNimbusFeature({
    autoFillAdaptiveHistoryEnabled: false,
  });
  Services.prefs.setBoolPref(
    "browser.urlbar.autoFill.adaptiveHistory.enabled",
    false
  );

  // Add an observer that throws an Error and an observer that does not define
  // anything to check whether the other observers can get notifications.
  UrlbarPrefs.addObserver({
    onPrefChanged(pref) {
      throw new Error("From onPrefChanged");
    },
    onNimbusChanged(pref) {
      throw new Error("From onNimbusChanged");
    },
  });
  UrlbarPrefs.addObserver({});

  const deferred = Promise.withResolvers();
  const observer = {
    onPrefChanged(pref) {
      this.prefChangedList.push(pref);
      deferred.resolve();
    },
    onNimbusChanged(pref) {
      this.nimbusChangedList.push(pref);
      deferred.resolve();
    },
  };
  observer.prefChangedList = [];
  observer.nimbusChangedList = [];
  UrlbarPrefs.addObserver(observer);

  Services.prefs.setBoolPref(
    "browser.urlbar.autoFill.adaptiveHistory.enabled",
    true
  );
  await deferred.promise;
  Assert.equal(observer.prefChangedList.length, 1);
  Assert.equal(observer.prefChangedList[0], "autoFill.adaptiveHistory.enabled");
  Assert.equal(observer.nimbusChangedList.length, 0);

  Services.prefs.clearUserPref(
    "browser.urlbar.autoFill.adaptiveHistory.enabled"
  );
  doCleanup();
});
