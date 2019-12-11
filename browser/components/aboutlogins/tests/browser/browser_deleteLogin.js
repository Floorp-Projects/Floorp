/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function setup() {
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

  await ContentTask.spawn(
    browser,
    [TEST_LOGIN1.guid, TEST_LOGIN2.guid],
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
      ok(
        !content.document.documentElement.classList.contains("no-logins"),
        "Should no longer be in no logins view"
      );
      ok(
        !loginList.classList.contains("no-logins"),
        "login-list should no longer be in no logins view"
      );
      ok(loginFound, "Newly added logins should be added to the page");
    }
  );
});

add_task(async function test_login_item() {
  let browser = gBrowser.selectedBrowser;

  function waitForDelete() {
    return new Promise(resolve => {
      browser.messageManager.addMessageListener(
        "AboutLogins:DeleteLogin",
        function onMsg() {
          resolve(true);
          browser.messageManager.removeMessageListener(
            "AboutLogins:DeleteLogin",
            onMsg
          );
        }
      );
    });
  }

  function deleteFirstLoginAfterEdit() {
    return ContentTask.spawn(browser, null, async () => {
      let loginList = content.document.querySelector("login-list");
      let loginListItem = loginList.shadowRoot.querySelector(
        ".login-list-item[data-guid]:not([hidden])"
      );
      info("Clicking on the first login");
      loginListItem.click();

      let loginItem = Cu.waiveXrays(
        content.document.querySelector("login-item")
      );
      let loginItemPopulated = await ContentTaskUtils.waitForCondition(() => {
        return loginItem._login.guid == loginListItem.dataset.guid;
      }, "Waiting for login item to get populated");
      ok(loginItemPopulated, "The login item should get populated");

      let usernameInput = loginItem.shadowRoot.querySelector(
        "input[name='username']"
      );
      let passwordInput = loginItem.shadowRoot.querySelector(
        "input[name='password']"
      );

      let editButton = loginItem.shadowRoot.querySelector(".edit-button");
      editButton.click();

      usernameInput.value += "-undone";
      passwordInput.value += "-undone";

      let deleteButton = loginItem.shadowRoot.querySelector(".delete-button");
      deleteButton.click();

      let confirmDeleteDialog = Cu.waiveXrays(
        content.document.querySelector("confirmation-dialog")
      );
      let confirmButton = confirmDeleteDialog.shadowRoot.querySelector(
        ".confirm-button"
      );
      confirmButton.click();
    });
  }

  function deleteFirstLogin() {
    return ContentTask.spawn(browser, null, async () => {
      let loginList = content.document.querySelector("login-list");
      let loginListItem = loginList.shadowRoot.querySelector(
        ".login-list-item[data-guid]:not([hidden])"
      );
      info("Clicking on the first login");
      loginListItem.click();

      let loginItem = Cu.waiveXrays(
        content.document.querySelector("login-item")
      );
      let loginItemPopulated = await ContentTaskUtils.waitForCondition(() => {
        return loginItem._login.guid == loginListItem.dataset.guid;
      }, "Waiting for login item to get populated");
      ok(loginItemPopulated, "The login item should get populated");

      let deleteButton = loginItem.shadowRoot.querySelector(".delete-button");
      deleteButton.click();

      let confirmDeleteDialog = Cu.waiveXrays(
        content.document.querySelector("confirmation-dialog")
      );
      let confirmButton = confirmDeleteDialog.shadowRoot.querySelector(
        ".confirm-button"
      );
      confirmButton.click();
    });
  }

  let onDeletePromise = waitForDelete();

  await deleteFirstLoginAfterEdit();
  await onDeletePromise;

  onDeletePromise = waitForDelete();

  await ContentTask.spawn(browser, null, async () => {
    let loginList = content.document.querySelector("login-list");
    ok(
      !content.document.documentElement.classList.contains("no-logins"),
      "Should not be in no logins view as there is still one login"
    );
    ok(
      !loginList.classList.contains("no-logins"),
      "Should not be in no logins view as there is still one login"
    );

    let confirmDiscardDialog = Cu.waiveXrays(
      content.document.querySelector("confirmation-dialog")
    );
    ok(
      confirmDiscardDialog.hidden,
      "Discard confirm dialog should not show up after delete an edited login"
    );
  });

  await deleteFirstLogin();
  await onDeletePromise;

  await ContentTask.spawn(browser, null, async () => {
    let loginList = content.document.querySelector("login-list");
    ok(
      content.document.documentElement.classList.contains("no-logins"),
      "Should be in no logins view as all logins got deleted"
    );
    ok(
      loginList.classList.contains("no-logins"),
      "login-list should be in no logins view as all logins got deleted"
    );
  });
});
