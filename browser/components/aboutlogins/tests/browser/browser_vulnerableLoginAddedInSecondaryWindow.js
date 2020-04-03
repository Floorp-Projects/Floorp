/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

EXPECTED_BREACH = {
  AddedDate: "2018-12-20T23:56:26Z",
  BreachDate: "2018-12-16",
  Domain: "breached.example.com",
  Name: "Breached",
  PwnCount: 1643100,
  DataClasses: ["Email addresses", "Usernames", "Passwords", "IP addresses"],
  _status: "synced",
  id: "047940fe-d2fd-4314-b636-b4a952ee0043",
  last_modified: "1541615610052",
  schema: "1541615609018",
};

let tabInSecondWindow;

add_task(async function setup() {
  TEST_LOGIN1 = await addLogin(TEST_LOGIN1);
  TEST_LOGIN2 = await addLogin(TEST_LOGIN2);
  TEST_LOGIN3 = await addLogin(TEST_LOGIN3);

  await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "about:logins",
  });

  let newWin = await BrowserTestUtils.openNewBrowserWindow();
  tabInSecondWindow = await BrowserTestUtils.openNewForegroundTab({
    gBrowser: newWin.gBrowser,
    url: "about:logins",
  });

  registerCleanupFunction(async () => {
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
    Services.logins.removeAllLogins();
    await BrowserTestUtils.closeWindow(newWin);
  });
});

add_task(async function test_new_login_marked_vulnerable_in_both_windows() {
  const ORIGIN_FOR_NEW_VULNERABLE_LOGIN = "https://vulnerable";

  await SpecialPowers.spawn(
    tabInSecondWindow.linkedBrowser,
    [[TEST_LOGIN3.password, ORIGIN_FOR_NEW_VULNERABLE_LOGIN]],
    async ([passwordOfBreachedAccount, originForNewVulnerableLogin]) => {
      let loginList = content.document.querySelector("login-list");

      await ContentTaskUtils.waitForCondition(
        () =>
          loginList.shadowRoot.querySelectorAll(".login-list-item[data-guid]")
            .length == 3,
        "waiting for all three initials logins to get added to login-list"
      );

      let createButton = loginList.shadowRoot.querySelector(
        ".create-login-button"
      );
      createButton.click();

      let loginItem = content.document.querySelector("login-item");
      await ContentTaskUtils.waitForCondition(
        () => loginItem.dataset.isNewLogin,
        "waiting for create login form to be visible"
      );

      let originInput = loginItem.shadowRoot.querySelector(
        "input[name='origin']"
      );
      originInput.value = originForNewVulnerableLogin;
      let passwordInput = loginItem.shadowRoot.querySelector(
        "input[name='password']"
      );
      passwordInput.value = passwordOfBreachedAccount;

      let saveButton = loginItem.shadowRoot.querySelector(
        ".save-changes-button"
      );
      saveButton.click();

      await ContentTaskUtils.waitForCondition(
        () =>
          loginList.shadowRoot.querySelectorAll(".login-list-item[data-guid]")
            .length == 4,
        "waiting for new login to get added to login-list"
      );

      let vulnerableLoginGuid = Cu.waiveXrays(loginItem)._login.guid;
      let vulnerableListItem = loginList.shadowRoot.querySelector(
        `.login-list-item[data-guid="${vulnerableLoginGuid}"]`
      );

      ok(
        vulnerableListItem.classList.contains("vulnerable"),
        "vulnerable login list item should be marked as such"
      );
      ok(
        !loginItem.shadowRoot.querySelector(".vulnerable-alert").hidden,
        "vulnerable alert on login-item should be visible"
      );

      is(
        Cu.waiveXrays(loginList)._sortSelect.value,
        "alerts",
        "The login list should be sorted by Alerts"
      );
      let loginListItems = loginList.shadowRoot.querySelectorAll(
        ".login-list-item[data-guid]"
      );
      for (let i = 1; i < loginListItems.length; i++) {
        if (loginListItems[i].matches(".vulnerable, .breached")) {
          ok(
            loginListItems[i - 1].matches(".vulnerable, .breached"),
            `The previous login-list-item must be vulnerable or breached if the current one is (second window, i=${i})`
          );
        }
      }
    }
  );

  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [ORIGIN_FOR_NEW_VULNERABLE_LOGIN],
    async originForNewVulnerableLogin => {
      let loginList = Cu.waiveXrays(
        content.document.querySelector("login-list")
      );
      let vulnerableListItem;
      await ContentTaskUtils.waitForCondition(() => {
        let entry = Object.entries(loginList._logins).find(
          ([guid, { login, listItem }]) =>
            login.origin == originForNewVulnerableLogin
        );
        vulnerableListItem = entry[1].listItem;
        return !!entry;
      }, "waiting for vulnerable list item to get added to login-list");
      ok(
        vulnerableListItem.classList.contains("vulnerable"),
        "vulnerable login list item should be marked as such"
      );

      let loginListItems = loginList.shadowRoot.querySelectorAll(
        ".login-list-item[data-guid]"
      );
      for (let i = 1; i < loginListItems.length; i++) {
        if (loginListItems[i].matches(".vulnerable, .breached")) {
          ok(
            loginListItems[i - 1].matches(".vulnerable, .breached"),
            `The previous login-list-item must be vulnerable or breached if the current one is (first window, i=${i})`
          );
        }
      }
    }
  );
});
