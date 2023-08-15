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

add_setup(async function () {
  await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "about:logins",
  });
  registerCleanupFunction(() => {
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
    Services.logins.removeAllUserFacingLogins();
  });
});

add_task(async function test_added_login_shows_breach_warning() {
  let browser = gBrowser.selectedBrowser;
  await SpecialPowers.spawn(browser, [], async () => {
    let loginList = Cu.waiveXrays(content.document.querySelector("login-list"));
    Assert.equal(
      loginList._loginGuidsSortedOrder.length,
      0,
      "the login list should be empty"
    );
  });

  TEST_LOGIN3 = await addLogin(TEST_LOGIN3);
  await SpecialPowers.spawn(
    browser,
    [TEST_LOGIN3.guid],
    async aTestLogin3Guid => {
      let loginList = Cu.waiveXrays(
        content.document.querySelector("login-list")
      );
      await ContentTaskUtils.waitForCondition(
        () => loginList._loginGuidsSortedOrder.length == 1,
        "waiting for login list count to equal one. count=" +
          loginList._loginGuidsSortedOrder.length
      );
      Assert.equal(
        loginList._loginGuidsSortedOrder.length,
        1,
        "one login should be in the list"
      );
      let breachedLoginListItems;
      await ContentTaskUtils.waitForCondition(() => {
        breachedLoginListItems = loginList._list.querySelectorAll(
          "login-list-item[data-guid].breached"
        );
        return breachedLoginListItems.length == 1;
      }, "waiting for the login to get marked as breached");
      Assert.equal(
        breachedLoginListItems[0].dataset.guid,
        aTestLogin3Guid,
        "the breached login should be login3"
      );
    }
  );

  info("adding a login that uses the same password as the breached login");
  let vulnerableLogin = new nsLoginInfo(
    "https://2.example.com",
    "https://2.example.com",
    null,
    "user2",
    "pass3",
    "username",
    "password"
  );
  vulnerableLogin = await addLogin(vulnerableLogin);
  await SpecialPowers.spawn(
    browser,
    [[TEST_LOGIN3.guid, vulnerableLogin.guid]],
    async ([aTestLogin3Guid, aVulnerableLoginGuid]) => {
      let loginList = Cu.waiveXrays(
        content.document.querySelector("login-list")
      );
      await ContentTaskUtils.waitForCondition(
        () => loginList._loginGuidsSortedOrder.length == 2,
        "waiting for login list count to equal two. count=" +
          loginList._loginGuidsSortedOrder.length
      );
      Assert.equal(
        loginList._loginGuidsSortedOrder.length,
        2,
        "two logins should be in the list"
      );
      let breachedAndVulnerableLoginListItems;
      await ContentTaskUtils.waitForCondition(() => {
        breachedAndVulnerableLoginListItems = [
          ...loginList._list.querySelectorAll(".breached, .vulnerable"),
        ];
        return breachedAndVulnerableLoginListItems.length == 2;
      }, "waiting for the logins to get marked as breached and vulnerable");
      Assert.ok(
        !!breachedAndVulnerableLoginListItems.find(
          listItem => listItem.dataset.guid == aTestLogin3Guid
        ),
        "the list should include the breached login: " +
          breachedAndVulnerableLoginListItems.map(li => li.dataset.guid)
      );
      Assert.ok(
        !!breachedAndVulnerableLoginListItems.find(
          listItem => listItem.dataset.guid == aVulnerableLoginGuid
        ),
        "the list should include the vulnerable login: " +
          breachedAndVulnerableLoginListItems.map(li => li.dataset.guid)
      );
    }
  );
});
