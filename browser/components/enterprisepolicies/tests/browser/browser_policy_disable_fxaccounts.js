/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_policy_disable_fxaccounts() {
  is(gSync.SYNC_ENABLED, true, "Sync is enabled before setting the policy.");

  await setupPolicyEngineWithJson({
    "policies": {
      "DisableFirefoxAccounts": true,
    },
  });

  is(gSync.SYNC_ENABLED, false, "Sync is disabled after setting the policy.");
});
