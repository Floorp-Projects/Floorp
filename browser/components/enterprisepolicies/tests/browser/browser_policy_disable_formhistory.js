/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_policy_disable_formhistory() {
  await setupPolicyEngineWithJson({
    "policies": {
      "DisableFormHistory": true
    }
  });

  is(Services.prefs.getBoolPref("browser.formfill.enable"), false, "FormHistory has been disabled");
  is(Services.prefs.prefIsLocked("browser.formfill.enable"), true, "FormHistory pref has been locked");
});
