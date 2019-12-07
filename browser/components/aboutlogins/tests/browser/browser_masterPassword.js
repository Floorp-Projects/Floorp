/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

ChromeUtils.import("resource://testing-common/LoginTestUtils.jsm", this);

/**
 * Waits for the master password prompt and performs an action.
 * @param {string} action Set to "authenticate" to log in or "cancel" to
 *        close the dialog without logging in.
 */
function waitForMPDialog(action) {
  let dialogShown = TestUtils.topicObserved("common-dialog-loaded");
  return dialogShown.then(function([subject]) {
    let dialog = subject.Dialog;
    is(
      dialog.args.title,
      "Password Required",
      "Dialog is the Master Password dialog"
    );
    if (action == "authenticate") {
      SpecialPowers.wrap(dialog.ui.password1Textbox).setUserInput(
        LoginTestUtils.masterPassword.masterPassword
      );
      dialog.ui.button0.click();
    } else if (action == "cancel") {
      dialog.ui.button1.click();
    }
    return BrowserTestUtils.waitForEvent(window, "DOMModalDialogClosed");
  });
}

function waitForLoginCountToReach(browser, loginCount) {
  return ContentTask.spawn(browser, loginCount, async expectedLoginCount => {
    let loginList = Cu.waiveXrays(content.document.querySelector("login-list"));
    await ContentTaskUtils.waitForCondition(() => {
      return loginList._loginGuidsSortedOrder.length == expectedLoginCount;
    });
    return loginList._loginGuidsSortedOrder.length;
  });
}

add_task(async function test() {
  TEST_LOGIN1 = await addLogin(TEST_LOGIN1);
  LoginTestUtils.masterPassword.enable();

  let mpDialogShown = waitForMPDialog("cancel");
  await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "about:logins",
  });
  await mpDialogShown;

  registerCleanupFunction(function() {
    Services.logins.removeAllLogins();
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  });

  let browser = gBrowser.selectedBrowser;
  let logins = await waitForLoginCountToReach(browser, 0);
  is(
    logins,
    0,
    "No logins should be displayed when MP is set and unauthenticated"
  );

  let notification;
  await BrowserTestUtils.waitForCondition(
    () =>
      (notification = gBrowser
        .getNotificationBox()
        .getNotificationWithValue("master-password-login-required")),
    "waiting for master-password-login-required notification"
  );

  ok(
    notification,
    "master-password-login-required notification should be visible"
  );

  let buttons = notification.querySelectorAll(".notification-button");
  is(buttons.length, 1, "Should have one button.");

  let refreshPromise = BrowserTestUtils.browserLoaded(browser);
  // Sign in with the Master Password this time the dialog is shown
  mpDialogShown = waitForMPDialog("authenticate");
  // Click the button to reload the page.
  buttons[0].click();
  await refreshPromise;
  info("Page reloaded");

  await mpDialogShown;
  info("Master Password dialog shown and authenticated");

  logins = await waitForLoginCountToReach(browser, 1);
  is(logins, 1, "Logins should be displayed when MP is set and authenticated");

  // Show MP dialog when Copy Password button clicked
  mpDialogShown = waitForMPDialog("cancel");
  await ContentTask.spawn(gBrowser.selectedBrowser, null, async function() {
    let loginItem = content.document.querySelector("login-item");
    let copyButton = loginItem.shadowRoot.querySelector(
      ".copy-password-button"
    );
    copyButton.click();
  });
  await mpDialogShown;
  info("Master Password dialog shown and canceled");
  mpDialogShown = waitForMPDialog("authenticate");
  info("Clicking copy password button again");
  await ContentTask.spawn(gBrowser.selectedBrowser, null, async function() {
    let loginItem = content.document.querySelector("login-item");
    let copyButton = loginItem.shadowRoot.querySelector(
      ".copy-password-button"
    );
    copyButton.click();
  });
  await mpDialogShown;
  info("Master Password dialog shown and authenticated");
  await ContentTask.spawn(browser, null, async function() {
    let loginItem = content.document.querySelector("login-item");
    let copyButton = loginItem.shadowRoot.querySelector(
      ".copy-password-button"
    );
    await ContentTaskUtils.waitForCondition(() => {
      return copyButton.disabled;
    }, "Waiting for copy button to be disabled");
    info("Password was copied to clipboard");
  });

  // Show MP dialog when Reveal Password checkbox is checked if not on a new login
  mpDialogShown = waitForMPDialog("cancel");
  await ContentTask.spawn(gBrowser.selectedBrowser, null, async function() {
    let loginItem = content.document.querySelector("login-item");
    let revealCheckbox = loginItem.shadowRoot.querySelector(
      ".reveal-password-checkbox"
    );
    revealCheckbox.click();
  });
  await mpDialogShown;
  info("Master Password dialog shown and canceled");
  await ContentTask.spawn(gBrowser.selectedBrowser, null, async function() {
    let loginItem = content.document.querySelector("login-item");
    let revealCheckbox = loginItem.shadowRoot.querySelector(
      ".reveal-password-checkbox"
    );
    ok(
      !revealCheckbox.checked,
      "reveal checkbox should be unchecked if MP dialog canceled"
    );
  });
  mpDialogShown = waitForMPDialog("authenticate");
  await ContentTask.spawn(gBrowser.selectedBrowser, null, async function() {
    let loginItem = content.document.querySelector("login-item");
    let revealCheckbox = loginItem.shadowRoot.querySelector(
      ".reveal-password-checkbox"
    );
    revealCheckbox.click();
  });
  await mpDialogShown;
  info("Master Password dialog shown and authenticated");
  await ContentTask.spawn(gBrowser.selectedBrowser, null, async function() {
    let loginItem = content.document.querySelector("login-item");
    let revealCheckbox = loginItem.shadowRoot.querySelector(
      ".reveal-password-checkbox"
    );
    ok(
      revealCheckbox.checked,
      "reveal checkbox should be checked if MP dialog authenticated"
    );
  });

  info("Test toggling the password visibility on a new login");
  await ContentTask.spawn(browser, null, async function createNewToggle() {
    let createButton = content.document
      .querySelector("login-list")
      .shadowRoot.querySelector(".create-login-button");
    createButton.click();

    let loginItem = Cu.waiveXrays(content.document.querySelector("login-item"));
    let passwordField = loginItem.shadowRoot.querySelector(
      "input[name='password']"
    );
    let revealCheckbox = loginItem.shadowRoot.querySelector(
      ".reveal-password-checkbox"
    );
    ok(ContentTaskUtils.is_visible(revealCheckbox), "Toggle visible");
    ok(!revealCheckbox.checked, "Not revealed initially");
    is(passwordField.type, "password", "type is password");
    revealCheckbox.click();

    await ContentTaskUtils.waitForCondition(() => {
      return passwordField.type == "text";
    }, "Waiting for type='text'");
    ok(revealCheckbox.checked, "Not revealed after click");

    let cancelButton = loginItem.shadowRoot.querySelector(".cancel-button");
    cancelButton.click();
  });

  await ContentTask.spawn(gBrowser.selectedBrowser, null, async function() {
    let loginFilter = Cu.waiveXrays(
      content.document.querySelector("login-filter")
    );
    loginFilter.value = "pass1";
    let loginList = Cu.waiveXrays(content.document.querySelector("login-list"));
    is(
      loginList._list.querySelectorAll(
        ".login-list-item[data-guid]:not([hidden])"
      ).length,
      0,
      "login-list should not show any results since the filter won't search passwords when MP is enabled"
    );
    loginFilter.value = "";
    is(
      loginList._list.querySelectorAll(
        ".login-list-item[data-guid]:not([hidden])"
      ).length,
      1,
      "login-list should show all results since the filter is empty"
    );
  });
  LoginTestUtils.masterPassword.disable();
  await ContentTask.spawn(gBrowser.selectedBrowser, null, async function() {
    Cu.waiveXrays(content).AboutLoginsUtils.masterPasswordEnabled = false;
    let loginFilter = Cu.waiveXrays(
      content.document.querySelector("login-filter")
    );
    loginFilter.value = "pass1";
    let loginList = Cu.waiveXrays(content.document.querySelector("login-list"));
    is(
      loginList._list.querySelectorAll(
        ".login-list-item[data-guid]:not([hidden])"
      ).length,
      1,
      "login-list should show login with matching password since MP is disabled"
    );
  });
});
