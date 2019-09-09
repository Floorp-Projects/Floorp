/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function setup() {
  TEST_LOGIN1 = await addLogin(TEST_LOGIN1);
  await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "about:logins",
  });
  registerCleanupFunction(() => {
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
    Services.logins.removeAllLogins();
  });
});

add_task(async function test_show_logins() {
  let browser = gBrowser.selectedBrowser;
  await ContentTask.spawn(browser, TEST_LOGIN1.guid, async loginGuid => {
    let loginList = Cu.waiveXrays(content.document.querySelector("login-list"));
    let loginFound = await ContentTaskUtils.waitForCondition(() => {
      return (
        loginList._loginGuidsSortedOrder.length == 1 &&
        loginList._loginGuidsSortedOrder[0] == loginGuid
      );
    }, "Waiting for login to be displayed");
    ok(loginFound, "Stored logins should be displayed upon loading the page");
  });
});

add_task(async function test_login_item() {
  let browser = gBrowser.selectedBrowser;
  await ContentTask.spawn(
    browser,
    LoginHelper.loginToVanillaObject(TEST_LOGIN1),
    async login => {
      let loginList = Cu.waiveXrays(
        content.document.querySelector("login-list")
      );
      let loginListItem = Cu.waiveXrays(
        loginList.shadowRoot.querySelector(".login-list-item[data-guid]")
      );
      loginListItem.click();

      let loginItem = Cu.waiveXrays(
        content.document.querySelector("login-item")
      );
      let loginItemPopulated = await ContentTaskUtils.waitForCondition(() => {
        return (
          loginItem._login.guid == loginListItem.dataset.guid &&
          loginItem._login.guid == login.guid
        );
      }, "Waiting for login item to get populated");
      ok(loginItemPopulated, "The login item should get populated");

      let usernameInput = loginItem.shadowRoot.querySelector(
        "input[name='username']"
      );
      let passwordInput = loginItem.shadowRoot.querySelector(
        "input[name='password']"
      );

      let editButton = loginItem.shadowRoot.querySelector(".edit-button");

      async function test_discard_dialog(exitPoint) {
        editButton.click();
        await ContentTaskUtils.waitForCondition(
          () => loginItem.dataset.editing,
          "Entering edit mode"
        );
        await Promise.resolve();

        usernameInput.value += "-undome";
        passwordInput.value += "-undome";

        let dialog = content.document.querySelector("confirmation-dialog");
        ok(dialog.hidden, "Confirm dialog should initially be hidden");

        exitPoint.click();

        ok(!dialog.hidden, "Confirm dialog should be visible");

        let confirmDiscardButton = dialog.shadowRoot.querySelector(
          ".confirm-button"
        );
        await content.document.l10n.translateElements([
          dialog.shadowRoot.querySelector(".title"),
          dialog.shadowRoot.querySelector(".message"),
          confirmDiscardButton,
        ]);

        confirmDiscardButton.click();

        ok(dialog.hidden, "Confirm dialog should be hidden after confirming");

        await Promise.resolve();
        loginListItem.click();

        await ContentTaskUtils.waitForCondition(
          () => usernameInput.value == login.username
        );

        is(
          usernameInput.value,
          login.username,
          "Username change should be reverted"
        );
        is(
          passwordInput.value,
          " ".repeat(login.password.length),
          "Password change should be reverted"
        );
        is(
          passwordInput.style.width,
          login.password.length + "ch",
          "Password field width shouldn't have changed"
        );
      }

      await test_discard_dialog(loginList._createLoginButton);

      let cancelButton = loginItem.shadowRoot.querySelector(".cancel-button");
      await test_discard_dialog(cancelButton);

      editButton.click();
      await ContentTaskUtils.waitForCondition(
        () => loginItem.dataset.editing,
        "Entering edit mode"
      );
      await Promise.resolve();

      let revealCheckbox = loginItem.shadowRoot.querySelector(
        ".reveal-password-checkbox"
      );
      revealCheckbox.click();
      ok(
        revealCheckbox.checked,
        "reveal-checkbox should be checked after clicking"
      );

      usernameInput.value += "-saveme";
      passwordInput.value += "-saveme";

      // Cache the value since it will change upon leaving edit mode.
      let passwordInputValue = passwordInput.value;
      ok(loginItem.dataset.editing, "LoginItem should be in 'edit' mode");

      let saveChangesButton = loginItem.shadowRoot.querySelector(
        ".save-changes-button"
      );
      saveChangesButton.click();

      await ContentTaskUtils.waitForCondition(() => {
        let guid = loginList._loginGuidsSortedOrder[0];
        let updatedLogin = loginList._logins[guid].login;
        return (
          updatedLogin &&
          updatedLogin.username == usernameInput.value &&
          updatedLogin.password == passwordInputValue
        );
      }, "Waiting for corresponding login in login list to update");

      ok(
        !revealCheckbox.checked,
        "reveal-checkbox should be unchecked after saving changes"
      );
      ok(
        !loginItem.dataset.editing,
        "LoginItem should not be in 'edit' mode after saving"
      );
      is(
        passwordInput.style.width,
        passwordInput.value.length + "ch",
        "Password field width should be correctly updated"
      );

      editButton.click();
      await ContentTaskUtils.waitForCondition(
        () => loginItem.dataset.editing,
        "Entering edit mode"
      );
      await Promise.resolve();

      ok(loginItem.dataset.editing, "LoginItem should be in 'edit' mode");
      let deleteButton = loginItem.shadowRoot.querySelector(".delete-button");
      deleteButton.click();
      let confirmDeleteDialog = Cu.waiveXrays(
        content.document.querySelector("confirmation-dialog")
      );
      let confirmDeleteButton = confirmDeleteDialog.shadowRoot.querySelector(
        ".confirm-button"
      );
      confirmDeleteButton.click();

      await ContentTaskUtils.waitForCondition(() => {
        loginListItem = loginList.shadowRoot.querySelector(
          ".login-list-item[data-guid]"
        );
        return !loginListItem;
      }, "Waiting for login to be removed from list");

      ok(
        !loginItem.dataset.editing,
        "LoginItem should not be in 'edit' mode after deleting"
      );
    }
  );
});
