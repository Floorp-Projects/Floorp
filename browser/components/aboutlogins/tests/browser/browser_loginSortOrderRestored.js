/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

ChromeUtils.import("resource://testing-common/TelemetryTestUtils.jsm", this);
ChromeUtils.import("resource://testing-common/LoginTestUtils.jsm", this);

add_task(async function setup() {
  TEST_LOGIN2.QueryInterface(Ci.nsILoginMetaInfo).timePasswordChanged = 1;
  TEST_LOGIN1 = await addLogin(TEST_LOGIN1);
  info(`TEST_LOGIN1 added with guid=${TEST_LOGIN1.guid}`);
  TEST_LOGIN2 = await addLogin(TEST_LOGIN2);
  info(`TEST_LOGIN2 added with guid=${TEST_LOGIN2.guid}`);
  registerCleanupFunction(() => {
    Services.logins.removeAllLogins();
    Services.prefs.clearUserPref("signon.management.page.sort");
  });
});

add_task(async function test_sort_order_persisted() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:logins",
    },
    async function(browser) {
      await ContentTask.spawn(
        browser,
        [TEST_LOGIN1.guid, TEST_LOGIN2.guid],
        async function([testLogin1Guid, testLogin2Guid]) {
          let loginList = Cu.waiveXrays(
            content.document.querySelector("login-list")
          );
          is(
            loginList._sortSelect.value,
            "name",
            "default selected sort should be 'name'"
          );
          is(
            loginList._list.querySelector(
              ".login-list-item[data-guid]:not([hidden])"
            ).dataset.guid,
            testLogin2Guid,
            "the first login should be TEST_LOGIN2 since they are sorted by origin"
          );

          loginList._sortSelect.value = "last-changed";
          loginList._sortSelect.dispatchEvent(
            new content.Event("change", { bubbles: true })
          );
          is(
            loginList._list.querySelector(
              ".login-list-item[data-guid]:not([hidden])"
            ).dataset.guid,
            testLogin1Guid,
            "the first login should be TEST_LOGIN1 since it has the most recent timePasswordChanged value"
          );
        }
      );
    }
  );

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:logins",
    },
    async function(browser) {
      await ContentTask.spawn(browser, TEST_LOGIN1.guid, async function(
        testLogin1Guid
      ) {
        let loginList = Cu.waiveXrays(
          content.document.querySelector("login-list")
        );
        is(
          loginList._sortSelect.value,
          "last-changed",
          "selected sort should be restored to 'last-changed'"
        );
        is(
          loginList._list.querySelector(
            ".login-list-item[data-guid]:not([hidden])"
          ).dataset.guid,
          testLogin1Guid,
          "the first login should still be TEST_LOGIN1 since it has the most recent timePasswordChanged value"
        );
      });
    }
  );
});
