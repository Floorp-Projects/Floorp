/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let params = {};
let steps = [];

add_getBrowserUpdateStatus_task(
  params,
  steps,
  "updateDisabledByPolicy",
  async () => {
    if (!Services.policies) {
      return true;
    }

    const { EnterprisePolicyTesting, PoliciesPrefTracker } = ChromeUtils.import(
      "resource://testing-common/EnterprisePolicyTesting.jsm",
      null
    );

    PoliciesPrefTracker.start();
    await EnterprisePolicyTesting.setupPolicyEngineWithJson({
      policies: {
        DisableAppUpdate: true,
      },
    });
    registerCleanupFunction(async () => {
      if (Services.policies.status != Ci.nsIEnterprisePolicies.INACTIVE) {
        await EnterprisePolicyTesting.setupPolicyEngineWithJson("");
      }
      EnterprisePolicyTesting.resetRunOnceState();
      PoliciesPrefTracker.stop();
    });

    return false;
  }
);
