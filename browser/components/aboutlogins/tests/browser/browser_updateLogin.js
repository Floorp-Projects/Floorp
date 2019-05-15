/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

let nsLoginInfo = new Components.Constructor("@mozilla.org/login-manager/loginInfo;1",
                                             Ci.nsILoginInfo, "init");
const LOGIN_URL = "https://www.example.com";
let TEST_LOGIN1 = new nsLoginInfo(LOGIN_URL, LOGIN_URL, null, "user1", "pass1", "username", "password");

add_task(async function setup() {
  let storageChangedPromised = TestUtils.topicObserved("passwordmgr-storage-changed",
                                                       (_, data) => data == "addLogin");
  TEST_LOGIN1 = Services.logins.addLogin(TEST_LOGIN1);
  await storageChangedPromised;
  await BrowserTestUtils.openNewForegroundTab({gBrowser, url: "about:logins"});
  registerCleanupFunction(() => {
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
    Services.logins.removeAllLogins();
  });
});

add_task(async function test_show_logins() {
  let browser = gBrowser.selectedBrowser;
  await ContentTask.spawn(browser, TEST_LOGIN1.guid, async (loginGuid) => {
    let loginList = Cu.waiveXrays(content.document.querySelector("login-list"));
    let loginFound = await ContentTaskUtils.waitForCondition(() => {
      return loginList._logins.length == 1 &&
             loginList._logins[0].guid == loginGuid;
    }, "Waiting for login to be displayed");
    ok(loginFound, "Stored logins should be displayed upon loading the page");
  });
});

add_task(async function test_login_item() {
  let browser = gBrowser.selectedBrowser;
  await ContentTask.spawn(browser, LoginHelper.loginToVanillaObject(TEST_LOGIN1), async (login) => {
    let loginList = content.document.querySelector("login-list");
    let loginListItem = Cu.waiveXrays(loginList.shadowRoot.querySelector("login-list-item"));
    loginListItem.click();

    let loginItem = Cu.waiveXrays(content.document.querySelector("login-item"));
    let loginItemPopulated = await ContentTaskUtils.waitForCondition(() => {
      return loginItem._login.guid == loginListItem.getAttribute("guid") &&
             loginItem._login.guid == login.guid;
    }, "Waiting for login item to get populated");
    ok(loginItemPopulated, "The login item should get populated");

    let usernameInput = loginItem.shadowRoot.querySelector("modal-input[name='username']");
    let passwordInput = loginItem.shadowRoot.querySelector("modal-input[name='password']");

    let editButton = loginItem.shadowRoot.querySelector(".edit-button");
    editButton.click();
    await Promise.resolve();

    usernameInput.value += "-undome";
    passwordInput.value += "-undome";

    let cancelButton = loginItem.shadowRoot.querySelector(".cancel-button");
    cancelButton.click();
    await Promise.resolve();
    is(usernameInput.value, login.username, "Username change should be reverted");
    is(passwordInput.value, login.password, "Password change should be reverted");

    editButton.click();
    await Promise.resolve();

    usernameInput.value += "-saveme";
    passwordInput.value += "-saveme";

    let saveChangesButton = loginItem.shadowRoot.querySelector(".save-changes-button");
    saveChangesButton.click();

    await ContentTaskUtils.waitForCondition(() => {
      return loginListItem._login.username == usernameInput.value &&
             loginListItem._login.password == passwordInput.value;
    }, "Waiting for corresponding login in login list to update");
  });
});
