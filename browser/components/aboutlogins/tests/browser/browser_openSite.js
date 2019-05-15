/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

let nsLoginInfo = new Components.Constructor("@mozilla.org/login-manager/loginInfo;1",
                                             Ci.nsILoginInfo, "init");
const LOGIN_URL = "https://example.com/";
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

add_task(async function test_launch_login_item() {
  let promiseNewTab = BrowserTestUtils.waitForNewTab(gBrowser, LOGIN_URL);

  let browser = gBrowser.selectedBrowser;
  await ContentTask.spawn(browser, LoginHelper.loginToVanillaObject(TEST_LOGIN1), async (login) => {
    let loginItem = Cu.waiveXrays(content.document.querySelector("login-item"));
    loginItem.setLogin(login);
    let openSiteButton = loginItem.shadowRoot.querySelector(".open-site-button");
    openSiteButton.click();
  });

  info("waiting for new tab to get opened");
  let newTab = await promiseNewTab;
  ok(true, "New tab opened to " + LOGIN_URL);

  BrowserTestUtils.removeTab(newTab);
});
