/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let { LoginTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/LoginTestUtils.sys.mjs"
);

// Test that create in about:logins asks for primary password
add_task(async function test_policy_admin() {
  await setupPolicyEngineWithJson({
    policies: {
      PrimaryPassword: true,
    },
  });

  let aboutLoginsTab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "about:logins",
  });

  let browser = gBrowser.selectedBrowser;

  // Fake the subdialog
  let dialogURL = "";
  let originalOpenDialog = window.openDialog;
  window.openDialog = function (aDialogURL, unused, unused2, aCallback) {
    dialogURL = aDialogURL;
    if (aCallback) {
      aCallback();
    }
  };

  await SpecialPowers.spawn(browser, [], async () => {
    let loginList = Cu.waiveXrays(content.document.querySelector("login-list"));
    let createButton = loginList._createLoginButton;
    ok(
      !createButton.disabled,
      "Create button should not be disabled initially"
    );
    let loginItem = Cu.waiveXrays(content.document.querySelector("login-item"));

    createButton.click();

    let usernameInput = loginItem.shadowRoot.querySelector(
      "input[name='username']"
    );
    let originInput = loginItem.shadowRoot.querySelector(
      "input[name='origin']"
    );
    let passwordInput = loginItem.shadowRoot.querySelector(
      "input[name='password']"
    );

    originInput.value = "https://www.example.org";
    usernameInput.value = "testuser1";
    passwordInput.value = "testpass1";

    let saveChangesButton = loginItem.shadowRoot.querySelector(
      ".save-changes-button"
    );
    saveChangesButton.click();
  });
  await TestUtils.waitForCondition(
    () => dialogURL,
    "wait for open to get called asynchronously"
  );
  is(
    dialogURL,
    "chrome://mozapps/content/preferences/changemp.xhtml",
    "clicking on the save-changes-button should open the masterpassword dialog"
  );
  window.openDialog = originalOpenDialog;
  BrowserTestUtils.removeTab(aboutLoginsTab);
});
