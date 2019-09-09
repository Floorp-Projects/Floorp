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
  await ContentTask.spawn(
    browser,
    LoginHelper.loginToVanillaObject(TEST_LOGIN1),
    async login => {
      let loginItem = Cu.waiveXrays(
        content.document.querySelector("login-item")
      );
      loginItem.setLogin(login);
      let originInput = loginItem.shadowRoot.querySelector(".origin-input");
      originInput.click();
    }
  );

  info("waiting for new tab to get opened");
  let newTab = await promiseNewTab;
  ok(true, "New tab opened to " + TEST_LOGIN1.origin);

  BrowserTestUtils.removeTab(newTab);
});
