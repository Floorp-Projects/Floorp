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

add_task(async function test_launch_login_item() {
  let promiseNewTab = BrowserTestUtils.waitForNewTab(
    gBrowser,
    TEST_LOGIN1.origin + "/"
  );

  let browser = gBrowser.selectedBrowser;
  await ContentTask.spawn(browser, null, async () => {
    let loginItem = Cu.waiveXrays(content.document.querySelector("login-item"));
    loginItem._editButton.click();
    loginItem._usernameInput.value += "-changed";

    ok(
      content.document.querySelector("confirmation-dialog").hidden,
      "discard-changes confirmation-dialog should be hidden before opening the site"
    );

    let originInput = loginItem.shadowRoot.querySelector(".origin-input");
    originInput.click();
  });

  info("waiting for new tab to get opened");
  let newTab = await promiseNewTab;
  ok(true, "New tab opened to " + TEST_LOGIN1.origin);

  let modifiedLogin = TEST_LOGIN1.clone();
  modifiedLogin.timeLastUsed = 9000;
  let storageChangedPromised = TestUtils.topicObserved(
    "passwordmgr-storage-changed",
    (_, data) => data == "modifyLogin"
  );
  Services.logins.modifyLogin(TEST_LOGIN1, modifiedLogin);
  await storageChangedPromised;

  BrowserTestUtils.removeTab(newTab);

  await ContentTask.spawn(browser, null, async () => {
    ok(
      !content.document.querySelector("confirmation-dialog").hidden,
      "discard-changes confirmation-dialog should be visible after logging in to a site with a modified login present in the form"
    );
  });
});
