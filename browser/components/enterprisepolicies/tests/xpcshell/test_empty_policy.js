/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_empty_policy() {
  await setupPolicyEngineWithJson({
    policies: {
      Certificates: {},
    },
  });

  equal(
    Services.policies.status,
    Ci.nsIEnterprisePolicies.INACTIVE,
    "Engine is not active"
  );
});

add_task(async function test_empty_array() {
  await setupPolicyEngineWithJson({
    policies: {
      RequestedLocales: [],
    },
  });

  equal(
    Services.policies.status,
    Ci.nsIEnterprisePolicies.ACTIVE,
    "Engine is active"
  );
});
