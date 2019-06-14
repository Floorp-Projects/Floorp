/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function setup() {
  let aboutLoginsTab = await BrowserTestUtils.openNewForegroundTab({gBrowser, url: "about:logins"});
  registerCleanupFunction(() => {
    BrowserTestUtils.removeTab(aboutLoginsTab);
    Services.logins.removeAllLogins();
  });
});

add_task(async function test_create_login() {
  let storageChangedPromised = TestUtils.topicObserved("passwordmgr-storage-changed",
                                                       (_, data) => data == "addLogin");

  let browser = gBrowser.selectedBrowser;
  await ContentTask.spawn(browser, null, async () => {
    let createButton = content.document.querySelector("#create-login-button");
    createButton.click();
    await Promise.resolve();

    let loginItem = Cu.waiveXrays(content.document.querySelector("login-item"));

    let originInput = loginItem.shadowRoot.querySelector("input[name='origin']");
    let usernameInput = loginItem.shadowRoot.querySelector("modal-input[name='username']");
    let passwordInput = loginItem.shadowRoot.querySelector("modal-input[name='password']");

    originInput.value = "https://testuser1:testpass1@bugzilla.mozilla.org/show_bug.cgi?id=1556934";
    usernameInput.value = "testuser1";
    passwordInput.value = "testpass1";

    let saveChangesButton = loginItem.shadowRoot.querySelector(".save-changes-button");
    saveChangesButton.click();
  });

  await storageChangedPromised;

  await ContentTask.spawn(browser, null, async () => {
    let loginList = Cu.waiveXrays(content.document.querySelector("login-list"));
    let loginFound = await ContentTaskUtils.waitForCondition(() => {
      return loginList._logins.length == 1;
    }, "Waiting for login to be displayed");
    ok(loginFound, "Stored logins should be displayed upon loading the page");

    let loginListItem = Cu.waiveXrays(loginList.shadowRoot.querySelector("login-list-item[guid]"));
    is(loginListItem._login.origin, "https://bugzilla.mozilla.org",
      "Stored login should only include the origin of the URL provided during creation");
  });
});
