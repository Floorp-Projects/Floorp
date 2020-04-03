/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { ExperimentAPI, TEST_REFERENCE_RECIPE } = ChromeUtils.import(
  "resource://activity-stream/aboutwelcome/lib/AboutWelcomeExperimentAPI.jsm"
);

const SLUG_PREF = "browser.aboutwelcome.temp.testExperiment.slug";
const BRANCH_PREF = "browser.aboutwelcome.temp.testExperiment.branch";

add_task(async function test_getValue() {
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref(SLUG_PREF);
    Services.prefs.clearUserPref(BRANCH_PREF);
  });

  // Return empty array by default
  Assert.deepEqual(
    ExperimentAPI.getValue(),
    {},
    "should return empty data by default"
  );

  // Get control branch for a slug if no branch is set
  Services.prefs.setCharPref(SLUG_PREF, "about_welcome_test");
  Assert.deepEqual(
    ExperimentAPI.getValue(),
    TEST_REFERENCE_RECIPE.branches[0].value,
    "should return control branch for a given test experiment if defined in the slug pref"
  );

  // Get recipe for a slug and branch
  Services.prefs.setCharPref(SLUG_PREF, "about_welcome_test");
  Services.prefs.setCharPref(BRANCH_PREF, "variant");
  Assert.deepEqual(
    ExperimentAPI.getValue(),
    TEST_REFERENCE_RECIPE.branches[1].value,
    "should return variant branch for a given test experiment if defined in prefs"
  );
});

add_task(async function test_getExperiment() {
  Services.prefs.clearUserPref(SLUG_PREF);
  Services.prefs.clearUserPref(BRANCH_PREF);

  // Return empty array by default
  Assert.deepEqual(
    ExperimentAPI.getExperiment(),
    {},
    "should return empty data by default"
  );

  // Get control branch for a slug if no branch is set
  Services.prefs.setCharPref(SLUG_PREF, "about_welcome_test");

  Assert.deepEqual(
    ExperimentAPI.getExperiment().slug,
    "about_welcome_test",
    "should return test experiment slug"
  );

  Assert.deepEqual(
    ExperimentAPI.getExperiment().branch,
    TEST_REFERENCE_RECIPE.branches[0],
    "should return control branch for a given test experiment if defined in the slug pref"
  );

  // Get recipe for a slug and branch
  Services.prefs.setCharPref(SLUG_PREF, "about_welcome_test");
  Services.prefs.setCharPref(BRANCH_PREF, "variant");
  Assert.deepEqual(
    ExperimentAPI.getExperiment().branch,
    TEST_REFERENCE_RECIPE.branches[1],
    "should return variant branch for a given test experiment if defined in prefs"
  );

  registerCleanupFunction(() => {
    Services.prefs.clearUserPref(SLUG_PREF);
    Services.prefs.clearUserPref(BRANCH_PREF);
  });
});
