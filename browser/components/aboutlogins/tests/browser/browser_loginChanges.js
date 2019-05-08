/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function setup() {
  await BrowserTestUtils.openNewForegroundTab({gBrowser, url: "about:logins"});
  registerCleanupFunction(() => {
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  });
});

add_task(async function test_login_added() {
  let login = {
    guid: "70",
    username: "jared",
    password: "deraj",
    hostname: "https://www.example.com",
  };
  let browser = gBrowser.selectedBrowser;
  browser.messageManager.sendAsyncMessage("AboutLogins:LoginAdded", login);

  await ContentTask.spawn(browser, login, async (addedLogin) => {
    let loginList = Cu.waiveXrays(content.document.querySelector("login-list"));
    let loginFound = await ContentTaskUtils.waitForCondition(() => {
      return loginList._logins.length == 1 &&
             loginList._logins[0].guid == addedLogin.guid;
    }, "Waiting for login to be added");
    ok(loginFound, "Newly added logins should be added to the page");
  });
});

add_task(async function test_login_modified() {
  let login = {
    guid: "70",
    username: "jared@example.com",
    password: "deraj",
    hostname: "https://www.example.com",
  };
  let browser = gBrowser.selectedBrowser;
  browser.messageManager.sendAsyncMessage("AboutLogins:LoginModified", login);

  await ContentTask.spawn(browser, login, async (modifiedLogin) => {
    let loginList = Cu.waiveXrays(content.document.querySelector("login-list"));
    let loginFound = await ContentTaskUtils.waitForCondition(() => {
      return loginList._logins.length == 1 &&
             loginList._logins[0].guid == modifiedLogin.guid &&
             loginList._logins[0].username == modifiedLogin.username;
    }, "Waiting for username to get updated");
    ok(loginFound, "The login should get updated on the page");
  });
});

add_task(async function test_login_removed() {
  let login = {
    guid: "70",
    username: "jared@example.com",
    password: "deraj",
    hostname: "https://www.example.com",
  };
  let browser = gBrowser.selectedBrowser;
  browser.messageManager.sendAsyncMessage("AboutLogins:LoginRemoved", login);

  await ContentTask.spawn(browser, login, async (removedLogin) => {
    let loginList = Cu.waiveXrays(content.document.querySelector("login-list"));
    let loginRemoved = await ContentTaskUtils.waitForCondition(() => {
      return loginList._logins.length == 0;
    }, "Waiting for login to get removed");
    ok(loginRemoved, "The login should be removed from the page");
  });
});
