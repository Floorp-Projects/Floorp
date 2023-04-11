/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let { LoginTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/LoginTestUtils.sys.mjs"
);

// Test that once a password is set, you can't unset it
add_task(async function test_policy_masterpassword_set() {
  await setupPolicyEngineWithJson({
    policies: {
      PrimaryPassword: true,
    },
  });

  LoginTestUtils.primaryPassword.enable();

  await BrowserTestUtils.withNewTab(
    "about:preferences#privacy",
    async browser => {
      is(
        browser.contentDocument.getElementById("useMasterPassword").disabled,
        true,
        "Master Password checkbox should be disabled"
      );
    }
  );

  LoginTestUtils.primaryPassword.disable();
});

// Test that password can't be removed in changemp.xhtml
add_task(async function test_policy_nochangemp() {
  await setupPolicyEngineWithJson({
    policies: {
      PrimaryPassword: true,
    },
  });

  LoginTestUtils.primaryPassword.enable();

  let changeMPWindow = window.openDialog(
    "chrome://mozapps/content/preferences/changemp.xhtml",
    "",
    ""
  );
  await BrowserTestUtils.waitForEvent(changeMPWindow, "load");

  is(
    changeMPWindow.document.getElementById("admin").hidden,
    true,
    "Admin message should not be visible because there is a password."
  );

  changeMPWindow.document.getElementById("oldpw").value =
    LoginTestUtils.primaryPassword.masterPassword;

  is(
    changeMPWindow.document.getElementById("changemp").getButton("accept")
      .disabled,
    true,
    "OK button should not be enabled if there is an old password."
  );

  await BrowserTestUtils.closeWindow(changeMPWindow);

  LoginTestUtils.primaryPassword.disable();
});

// Test that admin message shows
add_task(async function test_policy_admin() {
  await setupPolicyEngineWithJson({
    policies: {
      PrimaryPassword: true,
    },
  });

  let changeMPWindow = window.openDialog(
    "chrome://mozapps/content/preferences/changemp.xhtml",
    "",
    ""
  );
  await BrowserTestUtils.waitForEvent(changeMPWindow, "load");

  is(
    changeMPWindow.document.getElementById("admin").hidden,
    false,
    true,
    "Admin message should not be hidden because there is not a password."
  );

  await BrowserTestUtils.closeWindow(changeMPWindow);
});
