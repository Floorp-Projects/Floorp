/*
 * Test for onboarding tour type check.
 */

"use strict";

add_task(async function() {
  do_print("Starting testcase: New user state");
  resetOnboardingDefaultState();

  do_check_eq(Preferences.get(PREF_TOURSET_VERSION), TOURSET_VERSION);
  do_check_eq(Preferences.get(PREF_ONBOARDING_HIDDEN), false);
});
