/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(async function test_clean_slate() {
  await startWithCleanSlate();
});

add_task(async function test_broken_json() {
  await setupPolicyEngineWithJson("config_broken_json.json");

  is(Services.policies.status, Ci.nsIEnterprisePolicies.FAILED, "Engine was correctly set to the error state");
});
