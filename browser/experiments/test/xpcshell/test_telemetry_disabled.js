/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource:///modules/experiments/Experiments.jsm");

add_test(function test_experiments_activation() {
  do_get_profile();
  loadAddonManager();

  Services.prefs.setBoolPref(PREF_EXPERIMENTS_ENABLED, true);
  Services.telemetry.canRecordExtended = false;

  let experiments = Experiments.instance();

  Assert.ok(!experiments.enabled, "Experiments must be disabled if Telemetry is disabled.");

  run_next_test();
});
