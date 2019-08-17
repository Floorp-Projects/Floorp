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
});

add_task(async function test_login_added() {
  let login = {
    guid: "70",
    username: "jared",
    password: "deraj",
    origin: "https://www.example.com",
  };
  let browser = gBrowser.selectedBrowser;
  browser.messageManager.sendAsyncMessage("AboutLogins:LoginAdded", login);

  await ContentTask.spawn(browser, login, async addedLogin => {
    let loginList = Cu.waiveXrays(content.document.querySelector("login-list"));
    let loginFound = await ContentTaskUtils.waitForCondition(() => {
      return (
        loginList._loginGuidsSortedOrder.length == 1 &&
        loginList._loginGuidsSortedOrder[0] == addedLogin.guid
      );
    }, "Waiting for login to be added");
    ok(loginFound, "Newly added logins should be added to the page");
  });
});

add_task(async function test_login_modified() {
  let login = {
    guid: "70",
    username: "jared@example.com",
    password: "deraj",
    origin: "https://www.example.com",
  };
  let browser = gBrowser.selectedBrowser;
  browser.messageManager.sendAsyncMessage("AboutLogins:LoginModified", login);

  await ContentTask.spawn(browser, login, async modifiedLogin => {
    let loginList = Cu.waiveXrays(content.document.querySelector("login-list"));
    let loginFound = await ContentTaskUtils.waitForCondition(() => {
      return (
        loginList._loginGuidsSortedOrder.length == 1 &&
        loginList._loginGuidsSortedOrder[0] == modifiedLogin.guid &&
        loginList._logins[loginList._loginGuidsSortedOrder[0]].login.username ==
          modifiedLogin.username
      );
    }, "Waiting for username to get updated");
    ok(loginFound, "The login should get updated on the page");
  });
});

add_task(async function test_login_removed() {
  let login = {
    guid: "70",
    username: "jared@example.com",
    password: "deraj",
    origin: "https://www.example.com",
  };
  let browser = gBrowser.selectedBrowser;
  browser.messageManager.sendAsyncMessage("AboutLogins:LoginRemoved", login);

  await ContentTask.spawn(browser, login, async removedLogin => {
    let loginList = Cu.waiveXrays(content.document.querySelector("login-list"));
    let loginRemoved = await ContentTaskUtils.waitForCondition(() => {
      return loginList._loginGuidsSortedOrder.length == 0;
    }, "Waiting for login to get removed");
    ok(loginRemoved, "The login should be removed from the page");
  });
});

add_task(async function test_all_logins_removed() {
  // Setup the test with 2 logins.
  let logins = [
    {
      guid: "70",
      username: "jared",
      password: "deraj",
      origin: "https://www.example.com",
    },
    {
      guid: "71",
      username: "ntim",
      password: "verysecurepassword",
      origin: "https://www.example.com",
    },
  ];

  let browser = gBrowser.selectedBrowser;
  browser.messageManager.sendAsyncMessage("AboutLogins:AllLogins", logins);

  await ContentTask.spawn(browser, logins, async addedLogins => {
    let loginList = Cu.waiveXrays(content.document.querySelector("login-list"));
    let loginFound = await ContentTaskUtils.waitForCondition(() => {
      return (
        loginList._loginGuidsSortedOrder.length == 2 &&
        loginList._loginGuidsSortedOrder[0] == addedLogins[0].guid &&
        loginList._loginGuidsSortedOrder[1] == addedLogins[1].guid
      );
    }, "Waiting for login to be added");
    ok(loginFound, "Newly added logins should be added to the page");
    ok(
      !content.document.documentElement.classList.contains("no-logins"),
      "Should not be in no logins view after adding logins"
    );
    ok(
      !loginList.classList.contains("no-logins"),
      "login-list should not be in no logins view after adding logins"
    );
  });

  Services.logins.removeAllLogins();

  await ContentTask.spawn(browser, null, async () => {
    let loginList = Cu.waiveXrays(content.document.querySelector("login-list"));
    let loginFound = await ContentTaskUtils.waitForCondition(() => {
      return loginList._loginGuidsSortedOrder.length == 0;
    }, "Waiting for logins to be cleared");
    ok(loginFound, "Logins should be cleared");
    ok(
      content.document.documentElement.classList.contains("no-logins"),
      "Should be in no logins view after clearing"
    );
    ok(
      loginList.classList.contains("no-logins"),
      "login-list should be in no logins view after clearing"
    );
  });
});
