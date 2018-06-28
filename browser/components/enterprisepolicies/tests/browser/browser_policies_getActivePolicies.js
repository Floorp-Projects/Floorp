/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_active_policies() {
  await setupPolicyEngineWithJson({
    "policies": {
      "DisablePrivateBrowsing": true
    }
  });

  let expected = {
    "DisablePrivateBrowsing": true
  };

  Assert.deepEqual(await Services.policies.getActivePolicies(), expected, "Active policies parsed correctly");
});

add_task(async function test_wrong_policies() {
  await setupPolicyEngineWithJson({
    "policies": {
      "BlockAboutSupport": [true]
    }
  });

  let expected = {};

  Assert.deepEqual(await Services.policies.getActivePolicies(), expected, "Wrong policies ignored");
});

add_task(async function test_content_process() {
  await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    try {
      Services.policies.getActivePolicies();
    } catch (ex) {
      is(ex.result, Cr.NS_ERROR_NOT_AVAILABLE, "Function getActivePolicies() doesn't have a valid definition in the content process");
    }
  });
});
