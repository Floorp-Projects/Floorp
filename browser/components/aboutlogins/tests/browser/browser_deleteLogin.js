/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_setup(async function () {
  TEST_LOGIN1 = await addLogin(TEST_LOGIN1);
  TEST_LOGIN2 = await addLogin(TEST_LOGIN2);
  await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "about:logins",
  });
  registerCleanupFunction(() => {
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  });
});

add_task(async function test_show_logins() {
  let browser = gBrowser.selectedBrowser;

  await SpecialPowers.spawn(
    browser,
    [[TEST_LOGIN1.guid, TEST_LOGIN2.guid]],
    async loginGuids => {
      let loginList = Cu.waiveXrays(
        content.document.querySelector("login-list")
      );
      let loginFound = await ContentTaskUtils.waitForCondition(() => {
        return (
          loginList._loginGuidsSortedOrder.length == 2 &&
          loginList._loginGuidsSortedOrder.includes(loginGuids[0]) &&
          loginList._loginGuidsSortedOrder.includes(loginGuids[1])
        );
      }, "Waiting for logins to be displayed");
      Assert.ok(
        !content.document.documentElement.classList.contains("no-logins"),
        "Should no longer be in no logins view"
      );
      Assert.ok(
        !loginList.classList.contains("no-logins"),
        "login-list should no longer be in no logins view"
      );
      Assert.ok(loginFound, "Newly added logins should be added to the page");
    }
  );
});

add_task(async function test_login_item() {
  let browser = gBrowser.selectedBrowser;

  function waitForDelete() {
    let numLogins = Services.logins.countLogins("", "", "");
    return TestUtils.waitForCondition(
      () => Services.logins.countLogins("", "", "") < numLogins,
      "Error waiting for login deletion"
    );
  }

  async function deleteFirstLoginAfterEdit() {
    await SpecialPowers.spawn(browser, [], async () => {
      let loginList = content.document.querySelector("login-list");
      let loginListItem = loginList.shadowRoot.querySelector(
        "login-list-item[data-guid]:not([hidden])"
      );
      info("Clicking on the first login");
      loginListItem.click();

      let loginItem = Cu.waiveXrays(
        content.document.querySelector("login-item")
      );
      let loginItemPopulated = await ContentTaskUtils.waitForCondition(() => {
        return loginItem._login.guid == loginListItem.dataset.guid;
      }, "Waiting for login item to get populated");
      Assert.ok(loginItemPopulated, "The login item should get populated");
    });
    let reauthObserved = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(true);
    await SpecialPowers.spawn(browser, [], async () => {
      let loginItem = Cu.waiveXrays(
        content.document.querySelector("login-item")
      );
      let editButton = loginItem.shadowRoot
        .querySelector(".edit-button")
        .shadowRoot.querySelector("button");
      editButton.click();
    });
    await reauthObserved;
    return SpecialPowers.spawn(browser, [], async () => {
      let loginItem = Cu.waiveXrays(
        content.document.querySelector("login-item")
      );
      let usernameInput = loginItem.shadowRoot.querySelector(
        "input[name='username']"
      );
      let passwordInput = loginItem._passwordInput;
      usernameInput.value += "-undone";
      passwordInput.value += "-undone";

      let deleteButton = loginItem.shadowRoot
        .querySelector(".delete-button")
        .shadowRoot.querySelector("button");
      deleteButton.click();

      let confirmDeleteDialog = Cu.waiveXrays(
        content.document.querySelector("confirmation-dialog")
      );
      let confirmButton =
        confirmDeleteDialog.shadowRoot.querySelector(".confirm-button");
      confirmButton.click();
    });
  }

  function deleteFirstLogin() {
    return SpecialPowers.spawn(browser, [], async () => {
      let loginList = content.document.querySelector("login-list");
      let loginListItem = loginList.shadowRoot.querySelector(
        "login-list-item[data-guid]:not([hidden])"
      );
      info("Clicking on the first login");
      loginListItem.click();

      let loginItem = Cu.waiveXrays(
        content.document.querySelector("login-item")
      );
      let loginItemPopulated = await ContentTaskUtils.waitForCondition(() => {
        return loginItem._login.guid == loginListItem.dataset.guid;
      }, "Waiting for login item to get populated");
      Assert.ok(loginItemPopulated, "The login item should get populated");

      let deleteButton = loginItem.shadowRoot
        .querySelector(".delete-button")
        .shadowRoot.querySelector("button");
      deleteButton.click();

      let confirmDeleteDialog = Cu.waiveXrays(
        content.document.querySelector("confirmation-dialog")
      );
      let confirmButton =
        confirmDeleteDialog.shadowRoot.querySelector(".confirm-button");
      confirmButton.click();
    });
  }

  let onDeletePromise;
  if (OSKeyStoreTestUtils.canTestOSKeyStoreLogin()) {
    // Can only test Edit mode in official builds
    onDeletePromise = waitForDelete();
    await deleteFirstLoginAfterEdit();
    await onDeletePromise;

    await SpecialPowers.spawn(browser, [], async () => {
      let loginList = content.document.querySelector("login-list");
      Assert.ok(
        !content.document.documentElement.classList.contains("no-logins"),
        "Should not be in no logins view as there is still one login"
      );
      Assert.ok(
        !loginList.classList.contains("no-logins"),
        "Should not be in no logins view as there is still one login"
      );

      let confirmDiscardDialog = Cu.waiveXrays(
        content.document.querySelector("confirmation-dialog")
      );
      Assert.ok(
        confirmDiscardDialog.hidden,
        "Discard confirm dialog should not show up after delete an edited login"
      );
    });
  } else {
    onDeletePromise = waitForDelete();
    await deleteFirstLogin();
    await onDeletePromise;
  }

  onDeletePromise = waitForDelete();
  await deleteFirstLogin();
  await onDeletePromise;

  await SpecialPowers.spawn(browser, [], async () => {
    let loginList = content.document.querySelector("login-list");
    Assert.ok(
      content.document.documentElement.classList.contains("no-logins"),
      "Should be in no logins view as all logins got deleted"
    );
    Assert.ok(
      loginList.classList.contains("no-logins"),
      "login-list should be in no logins view as all logins got deleted"
    );
  });
});
