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

add_task(async function setup() {
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
  await ContentTask.spawn(browser, null, async () => {
    let loginList = Cu.waiveXrays(content.document.querySelector("login-list"));
    is(
      loginList._loginGuidsSortedOrder.length,
      0,
      "the login list should be empty"
    );
  });

  TEST_LOGIN3 = await addLogin(TEST_LOGIN3);
  await ContentTask.spawn(browser, TEST_LOGIN3.guid, async aTestLogin3Guid => {
    let loginList = Cu.waiveXrays(content.document.querySelector("login-list"));
    is(
      loginList._loginGuidsSortedOrder.length,
      1,
      "one login should be in the list"
    );
    let breachedLoginListItems;
    await ContentTaskUtils.waitForCondition(() => {
      breachedLoginListItems = loginList._list.querySelectorAll(
        ".login-list-item[data-guid].breached"
      );
      return breachedLoginListItems.length == 1;
    }, "waiting for the login to get marked as breached");
    is(
      breachedLoginListItems[0].dataset.guid,
      aTestLogin3Guid,
      "the breached login should be login3"
    );
  });
});
