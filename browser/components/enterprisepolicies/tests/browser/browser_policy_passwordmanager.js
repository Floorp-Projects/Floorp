/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_pwmanagerbutton() {
  await setupPolicyEngineWithJson({
    policies: {
      PasswordManagerEnabled: false,
    },
  });

  await BrowserTestUtils.withNewTab(
    "about:preferences#privacy",
    async browser => {
      is(
        browser.contentDocument.getElementById("showPasswords").disabled,
        true,
        "showPasswords should be disabled."
      );
    }
  );
});
