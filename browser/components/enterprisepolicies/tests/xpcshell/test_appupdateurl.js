/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let { Checker } = ChromeUtils.import(
  "resource://gre/modules/UpdateService.jsm"
);

add_task(async function test_app_update_URL() {
  await setupPolicyEngineWithJson({
    policies: {
      AppUpdateURL: "https://www.example.com/",
    },
  });

  equal(
    Services.policies.status,
    Ci.nsIEnterprisePolicies.ACTIVE,
    "Engine is active"
  );

  let checker = new Checker();
  let expected = await checker.getUpdateURL();

  equal("https://www.example.com/", expected, "Correct app update URL");
});
