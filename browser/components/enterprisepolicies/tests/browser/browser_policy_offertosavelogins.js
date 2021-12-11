/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_policy_offertosavelogins() {
  await setupPolicyEngineWithJson({
    policies: {
      OfferToSaveLogins: false,
    },
  });

  await BrowserTestUtils.withNewTab(
    "about:preferences#privacy",
    async browser => {
      is(
        browser.contentDocument.getElementById("savePasswords").disabled,
        true,
        "Save passwords is disabled"
      );
    }
  );
});
