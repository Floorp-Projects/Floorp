/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_policy_disable_fxaccounts() {
  await setupPolicyEngineWithJson({});

  is(gSync.FXA_ENABLED, true, "Sync is enabled before setting the policy.");

  await setupPolicyEngineWithJson({
    policies: {
      DisableFirefoxAccounts: true,
    },
  });

  is(gSync.FXA_ENABLED, false, "Sync is disabled after setting the policy.");
});

add_task(async function test_policy_disable_accounts() {
  await setupPolicyEngineWithJson({});

  is(gSync.FXA_ENABLED, true, "Sync is enabled before setting the policy.");

  await setupPolicyEngineWithJson({
    policies: {
      DisableAccounts: true,
    },
  });

  is(gSync.FXA_ENABLED, false, "Sync is disabled after setting the policy.");
});

add_task(async function test_both_disable_accounts_policies() {
  await setupPolicyEngineWithJson({});

  is(gSync.FXA_ENABLED, true, "Sync is enabled before setting the policy.");

  // When both accounts policies are set, DisableAccounts should take precedence.
  await setupPolicyEngineWithJson({
    policies: {
      DisableFirefoxAccounts: false,
      DisableAccounts: true,
    },
  });

  is(gSync.FXA_ENABLED, false, "Sync is disabled after setting the policy.");
});
