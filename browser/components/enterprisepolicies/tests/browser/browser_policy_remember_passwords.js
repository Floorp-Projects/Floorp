/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_policy_remember_passwords() {
  await setupPolicyEngineWithJson({
    "policies": {
      "RememberPasswords": false
    }
  });

  is(Services.prefs.getBoolPref("signon.rememberSignons"), false, "Logins & Passwords have been disabled");
  is(Services.prefs.prefIsLocked("signon.rememberSignons"), true, "Logins & Passwords pref has been locked");


  await setupPolicyEngineWithJson({
    "policies": {
      "RememberPasswords": true
    }
  });

  is(Services.prefs.getBoolPref("signon.rememberSignons"), true, "Logins & Passwords have been enabled");
  is(Services.prefs.prefIsLocked("signon.rememberSignons"), true, "Logins & Passwords pref has been locked");
});
