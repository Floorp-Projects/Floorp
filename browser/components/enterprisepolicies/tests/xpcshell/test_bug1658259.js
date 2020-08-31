/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_bug1658259_1() {
  await setupPolicyEngineWithJson({
    policies: {
      OfferToSaveLogins: false,
      OfferToSaveLoginsDefault: true,
    },
  });
  checkLockedPref("signon.rememberSignons", false);
});

add_task(async function test_bug1658259_2() {
  await setupPolicyEngineWithJson({
    policies: {
      OfferToSaveLogins: true,
      OfferToSaveLoginsDefault: false,
    },
  });
  checkLockedPref("signon.rememberSignons", true);
});

add_task(async function test_bug1658259_3() {
  await setupPolicyEngineWithJson({
    policies: {
      OfferToSaveLoginsDefault: true,
      OfferToSaveLogins: false,
    },
  });
  checkLockedPref("signon.rememberSignons", false);
});

add_task(async function test_bug1658259_4() {
  await setupPolicyEngineWithJson({
    policies: {
      OfferToSaveLoginsDefault: false,
      OfferToSaveLogins: true,
    },
  });
  checkLockedPref("signon.rememberSignons", true);
});
