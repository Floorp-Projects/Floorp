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
  Services.prefs.setCharPref("signon.management.page.sort", "last-changed");
  TEST_LOGIN3 = await addLogin(TEST_LOGIN3);
  TEST_LOGIN1 = await addLogin(TEST_LOGIN1);
  await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "about:logins",
  });
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("signon.management.page.sort");
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
    Services.logins.removeAllLogins();
  });
});

add_task(async function test_show_login() {
  let browser = gBrowser.selectedBrowser;
  await SpecialPowers.spawn(browser, [], async () => {
    let loginItem = Cu.waiveXrays(content.document.querySelector("login-item"));
    let breachAlert = loginItem.shadowRoot.querySelector(".breach-alert");
    let breachAlertVisible = await ContentTaskUtils.waitForCondition(() => {
      return !breachAlert.hidden;
    }, "Waiting for breach alert to be visible");
    ok(
      breachAlertVisible,
      "Breach alert should be visible for a breached login."
    );

    let breachAlertDismissalButton = breachAlert.querySelector(
      ".dismiss-breach-alert"
    );
    breachAlertDismissalButton.click();

    let breachAlertDismissed = await ContentTaskUtils.waitForCondition(() => {
      return breachAlert.hidden;
    }, "Waiting for breach alert to be dismissed");
    ok(
      breachAlertDismissed,
      "Breach alert should not be visible after alert dismissal."
    );
  });
});
