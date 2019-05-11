/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_app_update_URL() {
  await setupPolicyEngineWithJson({
    "policies": {
      "AppUpdateURL": "https://www.example.com/",
    },
  });

  equal(Services.policies.status, Ci.nsIEnterprisePolicies.ACTIVE, "Engine is active");

  // The app.update.url preference is read from the default preferences.
  let expected = Services.prefs.getDefaultBranch(null).getCharPref("app.update.url", undefined);

  equal("https://www.example.com/", expected, "Correct app update URL");
});
