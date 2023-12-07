/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { CONCEALED_PASSWORD_TEXT } = ChromeUtils.importESModule(
  "chrome://browser/content/aboutlogins/aboutLoginsUtils.mjs"
);

add_setup(async function () {
  TEST_LOGIN1 = await addLogin(TEST_LOGIN1);
  await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "about:logins",
  });
  registerCleanupFunction(() => {
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
    Services.logins.removeAllUserFacingLogins();
  });
});

add_task(async function test_show_logins() {
  let browser = gBrowser.selectedBrowser;
  await SpecialPowers.spawn(browser, [TEST_LOGIN1.guid], async loginGuid => {
    let loginList = Cu.waiveXrays(content.document.querySelector("login-list"));
    let loginFound = await ContentTaskUtils.waitForCondition(() => {
      return (
        loginList._loginGuidsSortedOrder.length == 1 &&
        loginList._loginGuidsSortedOrder[0] == loginGuid
      );
    }, "Waiting for login to be displayed");
    Assert.ok(
      loginFound,
      "Stored logins should be displayed upon loading the page"
    );
  });
});

add_task(async function test_login_item() {
  if (!OSKeyStoreTestUtils.canTestOSKeyStoreLogin()) {
    return;
  }

  async function test_discard_dialog(
    login,
    exitPointSelector,
    concealedPasswordText
  ) {
    let loginItem = Cu.waiveXrays(content.document.querySelector("login-item"));
    let loginList = Cu.waiveXrays(content.document.querySelector("login-list"));
    await ContentTaskUtils.waitForCondition(
      () => loginItem.dataset.editing,
      "Entering edit mode"
    );
    await Promise.resolve();

    let usernameInput = loginItem.shadowRoot.querySelector(
      "input[name='username']"
    );
    let passwordInput = loginItem._passwordInput;
    usernameInput.value += "-undome";
    passwordInput.value += "-undome";

    let dialog = content.document.querySelector("confirmation-dialog");
    Assert.ok(dialog.hidden, "Confirm dialog should initially be hidden");

    let exitPoint =
      loginItem.shadowRoot.querySelector(exitPointSelector) ||
      loginList.shadowRoot.querySelector(exitPointSelector);
    exitPoint.click();

    Assert.ok(!dialog.hidden, "Confirm dialog should be visible");

    let confirmDiscardButton =
      dialog.shadowRoot.querySelector(".confirm-button");
    await content.document.l10n.translateElements([
      dialog.shadowRoot.querySelector(".title"),
      dialog.shadowRoot.querySelector(".message"),
      confirmDiscardButton,
    ]);

    confirmDiscardButton.click();

    Assert.ok(
      dialog.hidden,
      "Confirm dialog should be hidden after confirming"
    );

    await Promise.resolve();

    let loginListItem = Cu.waiveXrays(
      loginList.shadowRoot.querySelector("login-list-item[data-guid]")
    );

    loginListItem.click();

    await ContentTaskUtils.waitForCondition(
      () => usernameInput.value == login.username
    );

    Assert.equal(
      usernameInput.value,
      login.username,
      "Username change should be reverted"
    );
    Assert.equal(
      passwordInput.value,
      login.password,
      "Password change should be reverted"
    );
    let passwordDisplayInput = loginItem._passwordDisplayInput;
    Assert.equal(
      passwordDisplayInput.value,
      concealedPasswordText,
      "Password change should be reverted for display"
    );
    Assert.ok(
      !passwordInput.hasAttribute("value"),
      "Password shouldn't be exposed in @value"
    );
    Assert.equal(
      passwordInput.style.width,
      login.password.length + "ch",
      "Password field width shouldn't have changed"
    );
  }

  let browser = gBrowser.selectedBrowser;
  let reauthObserved = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(true);
  await SpecialPowers.spawn(
    browser,
    [LoginHelper.loginToVanillaObject(TEST_LOGIN1)],
    async login => {
      let loginList = Cu.waiveXrays(
        content.document.querySelector("login-list")
      );
      let loginListItem = Cu.waiveXrays(
        loginList.shadowRoot.querySelector("login-list-item[data-guid]")
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
      Assert.ok(loginItemPopulated, "The login item should get populated");

      let editButton = loginItem.shadowRoot
        .querySelector(".edit-button")
        .shadowRoot.querySelector("button");
      editButton.click();
    }
  );
  info("waiting for oskeystore auth #1");
  await reauthObserved;
  await SpecialPowers.spawn(
    browser,
    [
      LoginHelper.loginToVanillaObject(TEST_LOGIN1),
      ".create-login-button",
      CONCEALED_PASSWORD_TEXT,
    ],
    test_discard_dialog
  );
  reauthObserved = forceAuthTimeoutAndWaitForOSKeyStoreLogin({
    loginResult: true,
  });
  await SpecialPowers.spawn(browser, [], async () => {
    let loginItem = Cu.waiveXrays(content.document.querySelector("login-item"));
    let editButton = loginItem.shadowRoot
      .querySelector(".edit-button")
      .shadowRoot.querySelector("button");
    editButton.click();
  });
  info("waiting for oskeystore auth #2");
  await reauthObserved;
  await SpecialPowers.spawn(
    browser,
    [
      LoginHelper.loginToVanillaObject(TEST_LOGIN1),
      ".cancel-button",
      CONCEALED_PASSWORD_TEXT,
    ],
    test_discard_dialog
  );
  reauthObserved = forceAuthTimeoutAndWaitForOSKeyStoreLogin({
    loginResult: true,
  });
  await SpecialPowers.spawn(browser, [], async () => {
    let loginItem = Cu.waiveXrays(content.document.querySelector("login-item"));
    let editButton = loginItem.shadowRoot
      .querySelector(".edit-button")
      .shadowRoot.querySelector("button");
    editButton.click();
  });
  info("waiting for oskeystore auth #3");
  await reauthObserved;
  await SpecialPowers.spawn(
    browser,
    [LoginHelper.loginToVanillaObject(TEST_LOGIN1), CONCEALED_PASSWORD_TEXT],
    async (login, concealedPasswordText) => {
      let loginItem = Cu.waiveXrays(
        content.document.querySelector("login-item")
      );
      await ContentTaskUtils.waitForCondition(
        () => loginItem.dataset.editing,
        "Entering edit mode"
      );
      await Promise.resolve();
      let usernameInput = loginItem.shadowRoot.querySelector(
        "input[name='username']"
      );
      let passwordInput = loginItem._passwordInput;
      let passwordDisplayInput = loginItem._passwordDisplayInput;

      Assert.ok(
        loginItem.dataset.editing,
        "LoginItem should be in 'edit' mode"
      );
      Assert.equal(
        passwordInput.type,
        "password",
        "Password should still be hidden before revealed in edit mode"
      );

      passwordDisplayInput.focus();

      let revealCheckbox = loginItem.shadowRoot.querySelector(
        ".reveal-password-checkbox"
      );
      Assert.ok(
        revealCheckbox.checked,
        "reveal-checkbox should be checked when password input is focused"
      );

      Assert.equal(
        passwordInput.type,
        "text",
        "Password should be shown as text when focused"
      );
      let saveChangesButton = loginItem.shadowRoot.querySelector(
        ".save-changes-button"
      );

      saveChangesButton.click();

      await ContentTaskUtils.waitForCondition(() => {
        let editButton = loginItem.shadowRoot
          .querySelector(".edit-button")
          .shadowRoot.querySelector("button");
        return !editButton.disabled;
      }, "Waiting to exit edit mode");

      Assert.ok(
        !revealCheckbox.checked,
        "reveal-checkbox should be unchecked after saving changes"
      );
      Assert.ok(
        !loginItem.dataset.editing,
        "LoginItem should not be in 'edit' mode after saving"
      );
      Assert.equal(
        passwordInput.type,
        "password",
        "Password should be hidden after exiting edit mode"
      );
      Assert.equal(
        usernameInput.value,
        login.username,
        "Username change should be reverted"
      );
      Assert.equal(
        passwordInput.value,
        login.password,
        "Password change should be reverted"
      );
      Assert.equal(
        passwordDisplayInput.value,
        concealedPasswordText,
        "Password change should be reverted for display"
      );
      Assert.ok(
        !passwordInput.hasAttribute("value"),
        "Password shouldn't be exposed in @value"
      );
      Assert.equal(
        passwordInput.style.width,
        login.password.length + "ch",
        "Password field width shouldn't have changed"
      );
    }
  );
  reauthObserved = forceAuthTimeoutAndWaitForOSKeyStoreLogin({
    loginResult: true,
  });
  await SpecialPowers.spawn(browser, [], async () => {
    let loginItem = Cu.waiveXrays(content.document.querySelector("login-item"));
    let editButton = loginItem.shadowRoot
      .querySelector(".edit-button")
      .shadowRoot.querySelector("button");
    editButton.click();
  });
  info("waiting for oskeystore auth #4");
  await reauthObserved;
  await SpecialPowers.spawn(
    browser,
    [LoginHelper.loginToVanillaObject(TEST_LOGIN1)],
    async login => {
      let loginItem = Cu.waiveXrays(
        content.document.querySelector("login-item")
      );
      await ContentTaskUtils.waitForCondition(
        () => loginItem.dataset.editing,
        "Entering edit mode"
      );
      await Promise.resolve();

      let revealCheckbox = loginItem.shadowRoot.querySelector(
        ".reveal-password-checkbox"
      );
      revealCheckbox.click();
      Assert.ok(
        revealCheckbox.checked,
        "reveal-checkbox should be checked after clicking"
      );

      let usernameInput = loginItem.shadowRoot.querySelector(
        "input[name='username']"
      );
      let passwordInput = loginItem._passwordInput;

      usernameInput.value += "-saveme";
      passwordInput.value += "-saveme";

      Assert.ok(
        loginItem.dataset.editing,
        "LoginItem should be in 'edit' mode"
      );

      let saveChangesButton = loginItem.shadowRoot.querySelector(
        ".save-changes-button"
      );
      saveChangesButton.click();

      await ContentTaskUtils.waitForCondition(() => {
        let loginList = Cu.waiveXrays(
          content.document.querySelector("login-list")
        );
        let guid = loginList._loginGuidsSortedOrder[0];
        let updatedLogin = loginList._logins[guid].login;
        return (
          updatedLogin &&
          updatedLogin.username == usernameInput.value &&
          updatedLogin.password == passwordInput.value
        );
      }, "Waiting for corresponding login in login list to update");

      Assert.ok(
        !revealCheckbox.checked,
        "reveal-checkbox should be unchecked after saving changes"
      );
      Assert.ok(
        !loginItem.dataset.editing,
        "LoginItem should not be in 'edit' mode after saving"
      );
      Assert.equal(
        passwordInput.style.width,
        passwordInput.value.length + "ch",
        "Password field width should be correctly updated"
      );
    }
  );
  reauthObserved = forceAuthTimeoutAndWaitForOSKeyStoreLogin({
    loginResult: true,
  });
  await SpecialPowers.spawn(browser, [], async () => {
    let loginItem = Cu.waiveXrays(content.document.querySelector("login-item"));
    let editButton = loginItem.shadowRoot
      .querySelector(".edit-button")
      .shadowRoot.querySelector("button");
    editButton.click();
  });
  info("waiting for oskeystore auth #5");
  await reauthObserved;
  await SpecialPowers.spawn(
    browser,
    [LoginHelper.loginToVanillaObject(TEST_LOGIN1)],
    async login => {
      let loginItem = Cu.waiveXrays(
        content.document.querySelector("login-item")
      );
      await ContentTaskUtils.waitForCondition(
        () => loginItem.dataset.editing,
        "Entering edit mode"
      );
      await Promise.resolve();

      Assert.ok(
        loginItem.dataset.editing,
        "LoginItem should be in 'edit' mode"
      );
      let deleteButton = loginItem.shadowRoot
        .querySelector(".delete-button")
        .shadowRoot.querySelector("button");
      deleteButton.click();
      let confirmDeleteDialog = Cu.waiveXrays(
        content.document.querySelector("confirmation-dialog")
      );
      let confirmDeleteButton =
        confirmDeleteDialog.shadowRoot.querySelector(".confirm-button");
      confirmDeleteButton.click();

      let loginList = Cu.waiveXrays(
        content.document.querySelector("login-list")
      );
      let loginListItem = Cu.waiveXrays(
        loginList.shadowRoot.querySelector("login-list-item[data-guid]")
      );
      await ContentTaskUtils.waitForCondition(() => {
        loginListItem = loginList.shadowRoot.querySelector(
          "login-list-item[data-guid]"
        );
        return !loginListItem;
      }, "Waiting for login to be removed from list");

      Assert.ok(
        !loginItem.dataset.editing,
        "LoginItem should not be in 'edit' mode after deleting"
      );
    }
  );
});
