/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const UI_VERSION = 26;
const TOPIC_BROWSERGLUE_TEST = "browser-glue-test";
const TOPICDATA_BROWSERGLUE_TEST = "force-ui-migration";
const DEFAULT_BEHAVIOR_PREF = "browser.urlbar.default.behavior";
const AUTOCOMPLETE_PREF = "browser.urlbar.autocomplete.enabled";

var gBrowserGlue = Cc["@mozilla.org/browser/browserglue;1"]
                     .getService(Ci.nsIObserver);
var gGetBoolPref = Services.prefs.getBoolPref;

function run_test() {
  run_next_test();
}

do_register_cleanup(cleanup);

function cleanup() {
  let prefix = "browser.urlbar.suggest.";
  for (let type of ["history", "bookmark", "openpage", "history.onlyTyped"]) {
    Services.prefs.clearUserPref(prefix + type);
  }
  Services.prefs.clearUserPref("browser.migration.version");
  Services.prefs.clearUserPref(AUTOCOMPLETE_PREF);
}

function setupBehaviorAndMigrate(aDefaultBehavior, aAutocompleteEnabled = true) {
  cleanup();
  // Migrate browser.urlbar.default.behavior preference.
  Services.prefs.setIntPref("browser.migration.version", UI_VERSION - 1);
  Services.prefs.setIntPref(DEFAULT_BEHAVIOR_PREF, aDefaultBehavior);
  Services.prefs.setBoolPref(AUTOCOMPLETE_PREF, aAutocompleteEnabled);
  // Simulate a migration.
  gBrowserGlue.observe(null, TOPIC_BROWSERGLUE_TEST, TOPICDATA_BROWSERGLUE_TEST);
}

add_task(function*() {
  do_print("Migrate default.behavior = 0");
  setupBehaviorAndMigrate(0);

  Assert.ok(gGetBoolPref("browser.urlbar.suggest.history"),
    "History preference should be true.");
  Assert.ok(gGetBoolPref("browser.urlbar.suggest.bookmark"),
    "Bookmark preference should be true.");
  Assert.ok(gGetBoolPref("browser.urlbar.suggest.openpage"),
    "Openpage preference should be true.");
  Assert.equal(gGetBoolPref("browser.urlbar.suggest.history.onlyTyped"), false,
    "Typed preference should be false.");
});

add_task(function*() {
  do_print("Migrate default.behavior = 1");
  setupBehaviorAndMigrate(1);

  Assert.ok(gGetBoolPref("browser.urlbar.suggest.history"),
    "History preference should be true.");
  Assert.equal(gGetBoolPref("browser.urlbar.suggest.bookmark"), false,
    "Bookmark preference should be false.");
  Assert.equal(gGetBoolPref("browser.urlbar.suggest.openpage"), false,
    "Openpage preference should be false");
  Assert.equal(gGetBoolPref("browser.urlbar.suggest.history.onlyTyped"), false,
    "Typed preference should be false");
});

add_task(function*() {
  do_print("Migrate default.behavior = 2");
  setupBehaviorAndMigrate(2);

  Assert.equal(gGetBoolPref("browser.urlbar.suggest.history"), false,
    "History preference should be false.");
  Assert.ok(gGetBoolPref("browser.urlbar.suggest.bookmark"),
    "Bookmark preference should be true.");
  Assert.equal(gGetBoolPref("browser.urlbar.suggest.openpage"), false,
    "Openpage preference should be false");
  Assert.equal(gGetBoolPref("browser.urlbar.suggest.history.onlyTyped"), false,
    "Typed preference should be false");
});

add_task(function*() {
  do_print("Migrate default.behavior = 3");
  setupBehaviorAndMigrate(3);

  Assert.ok(gGetBoolPref("browser.urlbar.suggest.history"),
    "History preference should be true.");
  Assert.ok(gGetBoolPref("browser.urlbar.suggest.bookmark"),
    "Bookmark preference should be true.");
  Assert.equal(gGetBoolPref("browser.urlbar.suggest.openpage"), false,
    "Openpage preference should be false");
  Assert.equal(gGetBoolPref("browser.urlbar.suggest.history.onlyTyped"), false,
    "Typed preference should be false");
});

add_task(function*() {
  do_print("Migrate default.behavior = 19");
  setupBehaviorAndMigrate(19);

  Assert.ok(gGetBoolPref("browser.urlbar.suggest.history"),
    "History preference should be true.");
  Assert.ok(gGetBoolPref("browser.urlbar.suggest.bookmark"),
    "Bookmark preference should be true.");
  Assert.equal(gGetBoolPref("browser.urlbar.suggest.openpage"), false,
    "Openpage preference should be false");
  Assert.equal(gGetBoolPref("browser.urlbar.suggest.history.onlyTyped"), false,
    "Typed preference should be false");
});

add_task(function*() {
  do_print("Migrate default.behavior = 33");
  setupBehaviorAndMigrate(33);

  Assert.ok(gGetBoolPref("browser.urlbar.suggest.history"),
    "History preference should be true.");
  Assert.equal(gGetBoolPref("browser.urlbar.suggest.bookmark"), false,
    "Bookmark preference should be false.");
  Assert.equal(gGetBoolPref("browser.urlbar.suggest.openpage"), false,
    "Openpage preference should be false");
  Assert.ok(gGetBoolPref("browser.urlbar.suggest.history.onlyTyped"),
    "Typed preference should be true");
});

add_task(function*() {
  do_print("Migrate default.behavior = 129");
  setupBehaviorAndMigrate(129);

  Assert.ok(gGetBoolPref("browser.urlbar.suggest.history"),
    "History preference should be true.");
  Assert.equal(gGetBoolPref("browser.urlbar.suggest.bookmark"), false,
    "Bookmark preference should be false.");
  Assert.ok(gGetBoolPref("browser.urlbar.suggest.openpage"),
    "Openpage preference should be true");
  Assert.equal(gGetBoolPref("browser.urlbar.suggest.history.onlyTyped"), false,
    "Typed preference should be false");
});

add_task(function*() {
  do_print("Migrate default.behavior = 0, autocomplete.enabled = false");
  setupBehaviorAndMigrate(0, false);

  Assert.equal(gGetBoolPref("browser.urlbar.suggest.history"), false,
    "History preference should be false.");
  Assert.equal(gGetBoolPref("browser.urlbar.suggest.bookmark"), false,
    "Bookmark preference should be false.");
  Assert.equal(gGetBoolPref("browser.urlbar.suggest.openpage"), false,
    "Openpage preference should be false");
  Assert.equal(gGetBoolPref("browser.urlbar.suggest.history.onlyTyped"), false,
    "Typed preference should be false");
});
