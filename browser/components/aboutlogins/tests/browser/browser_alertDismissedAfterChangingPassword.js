/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

ChromeUtils.import("resource://testing-common/OSKeyStoreTestUtils.jsm", this);

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

let VULNERABLE_TEST_LOGIN2 = new nsLoginInfo(
  "https://2.example.com",
  "https://2.example.com",
  null,
  "user2",
  "pass3",
  "username",
  "password"
);

add_task(async function setup() {
  TEST_LOGIN1 = await addLogin(TEST_LOGIN1);
  VULNERABLE_TEST_LOGIN2 = await addLogin(VULNERABLE_TEST_LOGIN2);
  TEST_LOGIN3 = await addLogin(TEST_LOGIN3);
  await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "about:logins",
  });
  registerCleanupFunction(() => {
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
    Services.logins.removeAllLogins();
  });
});

add_task(async function test_added_login_shows_breach_warning() {
  let browser = gBrowser.selectedBrowser;
  await SpecialPowers.spawn(
    browser,
    [[TEST_LOGIN1.guid, VULNERABLE_TEST_LOGIN2.guid, TEST_LOGIN3.guid]],
    async ([regularLoginGuid, vulnerableLoginGuid, breachedLoginGuid]) => {
      let loginList = Cu.waiveXrays(
        content.document.querySelector("login-list")
      );
      await ContentTaskUtils.waitForCondition(
        () => loginList.shadowRoot.querySelectorAll(".login-list-item").length,
        "Waiting for login-list to get populated"
      );
      let { listItem: regularListItem } = loginList._logins[regularLoginGuid];
      let { listItem: vulnerableListItem } = loginList._logins[
        vulnerableLoginGuid
      ];
      let { listItem: breachedListItem } = loginList._logins[breachedLoginGuid];
      await ContentTaskUtils.waitForCondition(() => {
        return (
          !regularListItem.matches(".breached.vulnerable") &&
          vulnerableListItem.matches(".vulnerable") &&
          breachedListItem.matches(".breached")
        );
      }, `waiting for the list items to get their classes updated: ${regularListItem.className} / ${vulnerableListItem.className} / ${breachedListItem.className}`);
      ok(
        !regularListItem.classList.contains("breached") &&
          !regularListItem.classList.contains("vulnerable"),
        "regular login should not be marked breached or vulnerable: " +
          regularLoginGuid.className
      );
      ok(
        !vulnerableListItem.classList.contains("breached") &&
          vulnerableListItem.classList.contains("vulnerable"),
        "vulnerable login should be marked vulnerable: " +
          vulnerableListItem.className
      );
      ok(
        breachedListItem.classList.contains("breached") &&
          !breachedListItem.classList.contains("vulnerable"),
        "breached login should be marked breached: " +
          breachedListItem.className
      );

      breachedListItem.click();
      let loginItem = Cu.waiveXrays(
        content.document.querySelector("login-item")
      );
      await ContentTaskUtils.waitForCondition(() => {
        return loginItem._login && loginItem._login.guid == breachedLoginGuid;
      }, "waiting for breached login to get selected");
      ok(
        !ContentTaskUtils.is_hidden(
          loginItem.shadowRoot.querySelector(".breach-alert")
        ),
        "the breach alert should be visible"
      );
    }
  );

  if (!OSKeyStoreTestUtils.canTestOSKeyStoreLogin()) {
    info(
      "leaving test early since the remaining part of the test requires 'edit' mode which requires 'oskeystore' login"
    );
    return;
  }

  let reauthObserved = forceAuthTimeoutAndWaitForOSKeyStoreLogin({
    loginResult: true,
  });
  // Change the password on the breached login and check that the
  // login is no longer marked as breached. The vulnerable login
  // should still be marked as vulnerable afterwards.
  await SpecialPowers.spawn(browser, [], () => {
    let loginItem = Cu.waiveXrays(content.document.querySelector("login-item"));
    loginItem.shadowRoot.querySelector(".edit-button").click();
  });
  await reauthObserved;
  await SpecialPowers.spawn(
    browser,
    [[TEST_LOGIN1.guid, VULNERABLE_TEST_LOGIN2.guid, TEST_LOGIN3.guid]],
    async ([regularLoginGuid, vulnerableLoginGuid, breachedLoginGuid]) => {
      let loginList = Cu.waiveXrays(
        content.document.querySelector("login-list")
      );
      let loginItem = Cu.waiveXrays(
        content.document.querySelector("login-item")
      );
      await ContentTaskUtils.waitForCondition(
        () => loginItem.dataset.editing == "true",
        "waiting for login-item to enter edit mode"
      );
      let passwordInput = loginItem.shadowRoot.querySelector(
        "input[type='password']"
      );
      const CHANGED_PASSWORD_VALUE = "changedPassword";
      passwordInput.value = CHANGED_PASSWORD_VALUE;
      let saveChangesButton = loginItem.shadowRoot.querySelector(
        ".save-changes-button"
      );
      saveChangesButton.click();

      await ContentTaskUtils.waitForCondition(() => {
        return (
          loginList._logins[breachedLoginGuid].login.password ==
          CHANGED_PASSWORD_VALUE
        );
      }, "waiting for stored login to get updated");

      ok(
        ContentTaskUtils.is_hidden(
          loginItem.shadowRoot.querySelector(".breach-alert")
        ),
        "the breach alert should be hidden now"
      );

      let { listItem: breachedListItem } = loginList._logins[breachedLoginGuid];
      let { listItem: vulnerableListItem } = loginList._logins[
        vulnerableLoginGuid
      ];
      ok(
        !breachedListItem.classList.contains("breached") &&
          !breachedListItem.classList.contains("vulnerable"),
        "the originally breached login should no longer be marked as breached"
      );
      ok(
        !vulnerableListItem.classList.contains("breached") &&
          vulnerableListItem.classList.contains("vulnerable"),
        "vulnerable login should still be marked vulnerable: " +
          vulnerableListItem.className
      );

      // Change the password on the vulnerable login and check that the
      // login is no longer marked as vulnerable.
      vulnerableListItem.click();
      await ContentTaskUtils.waitForCondition(() => {
        return loginItem._login && loginItem._login.guid == vulnerableLoginGuid;
      }, "waiting for vulnerable login to get selected");
      ok(
        !ContentTaskUtils.is_hidden(
          loginItem.shadowRoot.querySelector(".vulnerable-alert")
        ),
        "the vulnerable alert should be visible"
      );
      loginItem.shadowRoot.querySelector(".edit-button").click();
      await ContentTaskUtils.waitForCondition(
        () => loginItem.dataset.editing == "true",
        "waiting for login-item to enter edit mode"
      );
      passwordInput = loginItem.shadowRoot.querySelector(
        "input[type='password']"
      );
      passwordInput.value = CHANGED_PASSWORD_VALUE;
      saveChangesButton.click();

      await ContentTaskUtils.waitForCondition(() => {
        return (
          loginList._logins[vulnerableLoginGuid].login.password ==
          CHANGED_PASSWORD_VALUE
        );
      }, "waiting for stored login to get updated");

      ok(
        ContentTaskUtils.is_hidden(
          loginItem.shadowRoot.querySelector(".vulnerable-alert")
        ),
        "the vulnerable alert should be hidden now"
      );
      is(
        vulnerableListItem.querySelector(".alert-icon").src,
        "",
        ".alert-icon for the vulnerable list item should have its source removed"
      );
      vulnerableListItem = loginList._logins[vulnerableLoginGuid].listItem;
      ok(
        !vulnerableListItem.classList.contains("breached") &&
          !vulnerableListItem.classList.contains("vulnerable"),
        "vulnerable login should no longer be marked vulnerable: " +
          vulnerableListItem.className
      );
    }
  );
});
