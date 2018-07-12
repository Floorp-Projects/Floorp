/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_app_update_URL() {
  await setupPolicyEngineWithJson({
    "policies": {
      "AppUpdateURL": "https://www.example.com/"
    }
  });

  is(Services.policies.status, Ci.nsIEnterprisePolicies.ACTIVE, "Engine is active");

  let expected = Services.prefs.getStringPref("app.update.url", undefined);

  is("https://www.example.com/", expected, "Correct app update URL");
});
