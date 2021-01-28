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

// Tests interaction between showSearchSuggestionsFirst and matchBuckets.
add_task(function showSearchSuggestionsFirst() {
  // Check initial values.
  Assert.equal(
    UrlbarPrefs.get("showSearchSuggestionsFirst"),
    true,
    "showSearchSuggestionsFirst is true initially"
  );
  Assert.equal(
    Services.prefs.getCharPref("browser.urlbar.matchBuckets", ""),
    "",
    "matchBuckets is empty initially"
  );

  // Set showSearchSuggestionsFirst = false.
  UrlbarPrefs.set("showSearchSuggestionsFirst", false);
  Assert.equal(
    Services.prefs.getCharPref("browser.urlbar.matchBuckets"),
    "general:5,suggestion:Infinity",
    "matchBuckets is updated after setting showSearchSuggestionsFirst = false"
  );

  // Set showSearchSuggestionsFirst = true.
  UrlbarPrefs.set("showSearchSuggestionsFirst", true);
  Assert.equal(
    Services.prefs.getCharPref("browser.urlbar.matchBuckets"),
    "suggestion:4,general:Infinity",
    "matchBuckets is updated after setting showSearchSuggestionsFirst = true"
  );

  // Set showSearchSuggestionsFirst = false again so we can clear it next.
  UrlbarPrefs.set("showSearchSuggestionsFirst", false);
  Assert.equal(
    Services.prefs.getCharPref("browser.urlbar.matchBuckets"),
    "general:5,suggestion:Infinity",
    "matchBuckets is updated after setting showSearchSuggestionsFirst = false"
  );

  // Clear showSearchSuggestionsFirst.
  Services.prefs.clearUserPref("browser.urlbar.showSearchSuggestionsFirst");
  Assert.equal(
    Services.prefs.getCharPref("browser.urlbar.matchBuckets"),
    "suggestion:4,general:Infinity",
    "matchBuckets is updated immediately after clearing showSearchSuggestionsFirst"
  );
  Assert.equal(
    UrlbarPrefs.get("showSearchSuggestionsFirst"),
    true,
    "showSearchSuggestionsFirst defaults to true after clearing it"
  );
  Assert.equal(
    Services.prefs.getCharPref("browser.urlbar.matchBuckets"),
    "suggestion:4,general:Infinity",
    "matchBuckets remains correct updated after getting showSearchSuggestionsFirst"
  );
});

// Tests UrlbarPrefs.initializeShowSearchSuggestionsFirstPref().
add_task(function initializeShowSearchSuggestionsFirstPref() {
  // For each of the following tests, we set the matchBuckets pref, call
  // initializeShowSearchSuggestionsFirstPref, and then check the actual value
  // of showSearchSuggestionsFirst against the expected value.
  //
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
    Services.prefs.setCharPref("browser.urlbar.matchBuckets", matchBuckets);
    UrlbarPrefs.initializeShowSearchSuggestionsFirstPref();
    Assert.equal(
      Services.prefs.getBoolPref("browser.urlbar.showSearchSuggestionsFirst"),
      expectedValue,
      "showSearchSuggestionsFirst has the expected value"
    );
    Assert.equal(
      Services.prefs.getCharPref("browser.urlbar.matchBuckets"),
      expectedValue
        ? "suggestion:4,general:Infinity"
        : "general:5,suggestion:Infinity",
      "matchBuckets value should be updated with the appropriate default"
    );
  }
});
