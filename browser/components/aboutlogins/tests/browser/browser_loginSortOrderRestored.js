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

const SORT_PREF_NAME = "signon.management.page.sort";

add_setup(async function () {
  TEST_LOGIN3.QueryInterface(Ci.nsILoginMetaInfo).timePasswordChanged = 1;
  TEST_LOGIN1 = await addLogin(TEST_LOGIN1);
  info(`TEST_LOGIN1 added with guid=${TEST_LOGIN1.guid}`);
  TEST_LOGIN3 = await addLogin(TEST_LOGIN3);
  info(`TEST_LOGIN3 added with guid=${TEST_LOGIN3.guid}`);
  registerCleanupFunction(() => {
    Services.logins.removeAllUserFacingLogins();
    Services.prefs.clearUserPref(SORT_PREF_NAME);
  });
});

add_task(async function test_sort_order_persisted() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:logins",
    },
    async function (browser) {
      await ContentTask.spawn(
        browser,
        [TEST_LOGIN1.guid, TEST_LOGIN3.guid],
        async function ([testLogin1Guid, testLogin3Guid]) {
          let loginList = Cu.waiveXrays(
            content.document.querySelector("login-list")
          );
          await ContentTaskUtils.waitForCondition(
            () => loginList._sortSelect.value == "alerts",
            "Waiting for login-list sort to get changed to 'alerts'. Current value is: " +
              loginList._sortSelect.value
          );
          Assert.equal(
            loginList._sortSelect.value,
            "alerts",
            "selected sort should be 'alerts' since there is a breached login"
          );
          Assert.equal(
            loginList._list.querySelector(
              "login-list-item[data-guid]:not([hidden])"
            ).dataset.guid,
            testLogin3Guid,
            "the first login should be TEST_LOGIN3 since they are sorted by alerts"
          );

          loginList._sortSelect.value = "last-changed";
          loginList._sortSelect.dispatchEvent(
            new content.Event("change", { bubbles: true })
          );
          Assert.equal(
            loginList._list.querySelector(
              "login-list-item[data-guid]:not([hidden])"
            ).dataset.guid,
            testLogin1Guid,
            "the first login should be TEST_LOGIN1 since it has the most recent timePasswordChanged value"
          );
        }
      );
    }
  );

  Assert.equal(
    Services.prefs.getCharPref(SORT_PREF_NAME),
    "last-changed",
    "'last-changed' should be stored in the pref"
  );

  // Set the pref to the value used in Fx70-76 to confirm our
  // backwards-compat support that "breached" is changed to "alerts"
  Services.prefs.setCharPref(SORT_PREF_NAME, "breached");
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:logins",
    },
    async function (browser) {
      await ContentTask.spawn(
        browser,
        TEST_LOGIN3.guid,
        async function (testLogin3Guid) {
          let loginList = Cu.waiveXrays(
            content.document.querySelector("login-list")
          );
          await ContentTaskUtils.waitForCondition(
            () => loginList._sortSelect.value == "alerts",
            "Waiting for login-list sort to get changed to 'alerts'. Current value is: " +
              loginList._sortSelect.value
          );
          Assert.equal(
            loginList._sortSelect.value,
            "alerts",
            "selected sort should be restored to 'alerts' since 'breached' was in prefs"
          );
          Assert.equal(
            loginList._list.querySelector(
              "login-list-item[data-guid]:not([hidden])"
            ).dataset.guid,
            testLogin3Guid,
            "the first login should be TEST_LOGIN3 since they are sorted by alerts"
          );
        }
      );
    }
  );

  let storageChangedPromised = TestUtils.topicObserved(
    "passwordmgr-storage-changed",
    (_, data) => data == "removeLogin"
  );
  Services.logins.removeLogin(TEST_LOGIN3);
  await storageChangedPromised;
  TEST_LOGIN2 = await addLogin(TEST_LOGIN2);

  Assert.equal(
    Services.prefs.getCharPref(SORT_PREF_NAME),
    "breached",
    "confirm that the stored sort is still 'breached' and as such shouldn't apply when the page loads"
  );
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:logins",
    },
    async function (browser) {
      await ContentTask.spawn(
        browser,
        TEST_LOGIN2.guid,
        async function (testLogin2Guid) {
          let loginList = Cu.waiveXrays(
            content.document.querySelector("login-list")
          );
          await ContentTaskUtils.waitForCondition(
            () =>
              loginList._list.querySelector(
                "login-list-item[data-guid]:not([hidden])"
              ),
            "wait for a visible loging to get populated"
          );
          Assert.equal(
            loginList._sortSelect.value,
            "name",
            "selected sort should be name since 'alerts' no longer applies with no breached or vulnerable logins"
          );
          Assert.equal(
            loginList._list.querySelector(
              "login-list-item[data-guid]:not([hidden])"
            ).dataset.guid,
            testLogin2Guid,
            "the first login should be TEST_LOGIN2 since it is sorted first by 'name'"
          );
        }
      );
    }
  );
});
