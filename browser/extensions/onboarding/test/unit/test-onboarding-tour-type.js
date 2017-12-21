/*
 * Test for onboarding tour type check.
 */

"use strict";

Cu.import("resource://onboarding/modules/OnboardingTourType.jsm");

add_task(async function() {
  info("Starting testcase: When New user open the browser first time");
  resetOnboardingDefaultState();
  OnboardingTourType.check();

  Assert.equal(Preferences.get(PREF_TOUR_TYPE), "new", "should show the new user tour");
  Assert.equal(Preferences.get(PREF_TOURSET_VERSION), TOURSET_VERSION,
    "tourset version should not change");
  Assert.equal(Preferences.get(PREF_SEEN_TOURSET_VERSION), TOURSET_VERSION,
    "seen tourset version should be set as the tourset version");
});

add_task(async function() {
  info("Starting testcase: When New user restart the browser");
  resetOnboardingDefaultState();
  Preferences.set(PREF_TOUR_TYPE, "new");
  Preferences.set(PREF_SEEN_TOURSET_VERSION, TOURSET_VERSION);
  OnboardingTourType.check();

  Assert.equal(Preferences.get(PREF_TOUR_TYPE), "new", "should show the new user tour");
  Assert.equal(Preferences.get(PREF_TOURSET_VERSION), TOURSET_VERSION,
    "tourset version should not change");
  Assert.equal(Preferences.get(PREF_SEEN_TOURSET_VERSION), TOURSET_VERSION,
    "seen tourset version should be set as the tourset version");
});

add_task(async function() {
  info("Starting testcase: When New User choosed to hide the overlay and restart the browser");
  resetOnboardingDefaultState();
  Preferences.set(PREF_TOUR_TYPE, "new");
  Preferences.set(PREF_SEEN_TOURSET_VERSION, TOURSET_VERSION);
  OnboardingTourType.check();

  Assert.equal(Preferences.get(PREF_TOUR_TYPE), "new", "should show the new user tour");
  Assert.equal(Preferences.get(PREF_TOURSET_VERSION), TOURSET_VERSION,
    "tourset version should not change");
  Assert.equal(Preferences.get(PREF_SEEN_TOURSET_VERSION), TOURSET_VERSION,
    "seen tourset version should be set as the tourset version");
});

add_task(async function() {
  info("Starting testcase: When New User updated to the next major version and restart the browser");
  resetOnboardingDefaultState();
  Preferences.set(PREF_TOURSET_VERSION, NEXT_TOURSET_VERSION);
  Preferences.set(PREF_TOUR_TYPE, "new");
  Preferences.set(PREF_SEEN_TOURSET_VERSION, TOURSET_VERSION);
  OnboardingTourType.check();

  Assert.equal(Preferences.get(PREF_TOUR_TYPE), "update", "should show the update user tour");
  Assert.equal(Preferences.get(PREF_TOURSET_VERSION), NEXT_TOURSET_VERSION,
    "tourset version should not change");
  Assert.equal(Preferences.get(PREF_SEEN_TOURSET_VERSION), NEXT_TOURSET_VERSION,
    "seen tourset version should be set as the tourset version");
});

add_task(async function() {
  info("Starting testcase: When New User prefer hide the tour, then updated to the next major version and restart the browser");
  resetOnboardingDefaultState();
  Preferences.set(PREF_TOURSET_VERSION, NEXT_TOURSET_VERSION);
  Preferences.set(PREF_TOUR_TYPE, "new");
  Preferences.set(PREF_SEEN_TOURSET_VERSION, TOURSET_VERSION);
  OnboardingTourType.check();

  Assert.equal(Preferences.get(PREF_TOUR_TYPE), "update", "should show the update user tour");
  Assert.equal(Preferences.get(PREF_TOURSET_VERSION), NEXT_TOURSET_VERSION,
    "tourset version should not change");
  Assert.equal(Preferences.get(PREF_SEEN_TOURSET_VERSION), NEXT_TOURSET_VERSION,
    "seen tourset version should be set as the tourset version");
});

add_task(async function() {
  info("Starting testcase: When User update from browser version < 56");
  resetOldProfileDefaultState();
  OnboardingTourType.check();

  Assert.equal(Preferences.get(PREF_TOUR_TYPE), "update", "should show the update user tour");
  Assert.equal(Preferences.get(PREF_TOURSET_VERSION), TOURSET_VERSION,
    "tourset version should not change");
  Assert.equal(Preferences.get(PREF_SEEN_TOURSET_VERSION), TOURSET_VERSION,
    "seen tourset version should be set as the tourset version");
});
