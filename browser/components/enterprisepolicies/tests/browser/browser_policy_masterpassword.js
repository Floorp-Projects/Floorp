/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let { LoginTestUtils } = ChromeUtils.import(
  "resource://testing-common/LoginTestUtils.jsm"
);

// Test that once a password is set, you can't unset it
add_task(async function test_policy_masterpassword_set() {
  await setupPolicyEngineWithJson({
    policies: {
      PrimaryPassword: true,
    },
  });

  LoginTestUtils.masterPassword.enable();

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

  LoginTestUtils.masterPassword.disable();
});

// Test that password can't be removed in changemp.xhtml
add_task(async function test_policy_nochangemp() {
  await setupPolicyEngineWithJson({
    policies: {
      PrimaryPassword: true,
    },
  });

  LoginTestUtils.masterPassword.enable();

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
    LoginTestUtils.masterPassword.masterPassword;

  is(
    changeMPWindow.document.getElementById("changemp").getButton("accept")
      .disabled,
    true,
    "OK button should not be enabled if there is an old password."
  );

  await BrowserTestUtils.closeWindow(changeMPWindow);

  LoginTestUtils.masterPassword.disable();
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
