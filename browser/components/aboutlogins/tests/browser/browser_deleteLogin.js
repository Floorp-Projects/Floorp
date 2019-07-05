/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function setup() {
  await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "about:logins",
  });
  registerCleanupFunction(() => {
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  });
  TEST_LOGIN1 = await addLogin(TEST_LOGIN1);
  TEST_LOGIN2 = await addLogin(TEST_LOGIN2);
});

add_task(async function test_show_logins() {
  let browser = gBrowser.selectedBrowser;

  await ContentTask.spawn(browser, [TEST_LOGIN1, TEST_LOGIN2], async logins => {
    let loginList = Cu.waiveXrays(content.document.querySelector("login-list"));
    let loginFound = await ContentTaskUtils.waitForCondition(() => {
      return (
        loginList._logins.length == 2 &&
        loginList._logins[0].guid == logins[0].guid &&
        loginList._logins[1].guid == logins[1].guid
      );
    }, "Waiting for logins to be displayed");
    ok(loginFound, "Newly added logins should be added to the page");
  });
});

add_task(async function test_login_item() {
  let browser = gBrowser.selectedBrowser;
  let deleteLoginMessageReceived = false;
  browser.messageManager.addMessageListener(
    "AboutLogins:DeleteLogin",
    function onMsg() {
      deleteLoginMessageReceived = true;
      browser.messageManager.removeMessageListener(
        "AboutLogins:DeleteLogin",
        onMsg
      );
    }
  );
  await ContentTask.spawn(browser, [TEST_LOGIN1, TEST_LOGIN2], async logins => {
    let loginList = content.document.querySelector("login-list");
    let loginListItem = loginList.shadowRoot.querySelector(
      "login-list-item[data-guid]"
    );
    info("Clicking on the first login");
    loginListItem.click();

    let loginItem = Cu.waiveXrays(content.document.querySelector("login-item"));
    let loginItemPopulated = await ContentTaskUtils.waitForCondition(() => {
      return loginItem._login.guid == loginListItem.dataset.guid;
    }, "Waiting for login item to get populated");
    ok(loginItemPopulated, "The login item should get populated");

    let deleteButton = loginItem.shadowRoot.querySelector(".delete-button");
    deleteButton.click();

    let confirmDeleteDialog = Cu.waiveXrays(
      content.document.querySelector("confirm-delete-dialog")
    );
    let confirmButton = confirmDeleteDialog.shadowRoot.querySelector(
      ".confirm-button"
    );
    confirmButton.click();
  });
  ok(
    deleteLoginMessageReceived,
    "Clicking the delete button should send the AboutLogins:DeleteLogin messsage"
  );
});
