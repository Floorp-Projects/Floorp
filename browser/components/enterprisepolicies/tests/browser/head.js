/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {EnterprisePolicyTesting} = ChromeUtils.import("resource://testing-common/EnterprisePolicyTesting.jsm", {});

async function setupPolicyEngineWithJson(json, customSchema) {
  if (typeof(json) != "object") {
    let filePath = getTestFilePath(json ? json : "non-existing-file.json");
    return EnterprisePolicyTesting.setupPolicyEngineWithJson(filePath, customSchema);
  }
  return EnterprisePolicyTesting.setupPolicyEngineWithJson(json, customSchema);
}

function checkLockedPref(prefName, prefValue) {
  EnterprisePolicyTesting.checkPolicyPref(prefName, prefValue, true);
}

function checkUnlockedPref(prefName, prefValue) {
  EnterprisePolicyTesting.checkPolicyPref(prefName, prefValue, false);
}

add_task(async function policies_headjs_startWithCleanSlate() {
  if (Services.policies.status != Ci.nsIEnterprisePolicies.INACTIVE) {
    await setupPolicyEngineWithJson("");
  }
  is(Services.policies.status, Ci.nsIEnterprisePolicies.INACTIVE, "Engine is inactive at the start of the test");
});

registerCleanupFunction(async function policies_headjs_finishWithCleanSlate() {
  if (Services.policies.status != Ci.nsIEnterprisePolicies.INACTIVE) {
    await setupPolicyEngineWithJson("");
  }
  is(Services.policies.status, Ci.nsIEnterprisePolicies.INACTIVE, "Engine is inactive at the end of the test");

  EnterprisePolicyTesting.resetRunOnceState();
});
