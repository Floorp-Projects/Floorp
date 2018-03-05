/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_policy_disable_fxaccounts() {
  is(gSync.SYNC_ENABLED, true, "Sync is enabled before setting the policy.");

  await setupPolicyEngineWithJson({
    "policies": {
      "DisableFirefoxAccounts": true
    }
  });

  is(gSync.SYNC_ENABLED, false, "Sync is disabled after setting the policy.");

  // Manually clean-up the change made by the policy engine.
  // This is needed in case this test runs twice in a row
  // (such as in test-verify), in order for the first check
  // to pass again.
  Services.prefs.unlockPref("identity.fxaccounts.enabled");
  Services.prefs.getDefaultBranch("").setBoolPref("identity.fxaccounts.enabled", true);
});
