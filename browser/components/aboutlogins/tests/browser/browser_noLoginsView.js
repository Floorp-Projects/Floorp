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

add_task(async function test_no_logins_class() {
  let { platform } = AppConstants;
  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [platform],
    async aPlatform => {
      let loginList = content.document.querySelector("login-list");

      ok(
        content.document.documentElement.classList.contains("no-logins"),
        "root should be in no logins view"
      );
      ok(
        loginList.classList.contains("no-logins"),
        "login-list should be in no logins view"
      );

      let loginIntro = Cu.waiveXrays(
        content.document.querySelector("login-intro")
      );
      let loginItem = content.document.querySelector("login-item");
      let loginListIntro = loginList.shadowRoot.querySelector(".intro");
      let loginListList = loginList.shadowRoot.querySelector("ol");

      ok(
        !ContentTaskUtils.is_hidden(loginIntro),
        "login-intro should be shown in no logins view"
      );
      ok(
        !ContentTaskUtils.is_hidden(loginListIntro),
        "login-list intro should be shown in no logins view"
      );

      ok(
        ContentTaskUtils.is_hidden(loginItem),
        "login-item should be hidden in no logins view"
      );
      ok(
        ContentTaskUtils.is_hidden(loginListList),
        "login-list logins list should be hidden in no logins view"
      );
      is(
        content.document.l10n.getAttributes(
          loginIntro.shadowRoot.querySelector(".heading")
        ).id,
        "login-intro-heading",
        "The default message should be the non-logged-in message"
      );

      loginIntro.updateState(Cu.cloneInto({ loggedIn: true }, content));

      is(
        content.document.l10n.getAttributes(
          loginIntro.shadowRoot.querySelector(".heading")
        ).id,
        "about-logins-login-intro-heading-logged-in",
        "When logged in the message should update"
      );

      is(
        ContentTaskUtils.is_hidden(
          loginIntro.shadowRoot.querySelector(".intro-import-text")
        ),
        aPlatform == "linux",
        "the import link should be hidden on Linux builds"
      );
      if (aPlatform == "linux") {
        // End the test now for Linux since the link is hidden.
        return;
      }
      loginIntro.shadowRoot.querySelector(".intro-import-text > a").click();
      info("waiting for MigrationWizard to open");
    }
  );
  if (AppConstants.platform == "linux") {
    // End the test now for Linux since the link is hidden.
    return;
  }
  await TestUtils.waitForCondition(() => {
    let win = Services.wm.getMostRecentWindow("Browser:MigrationWizard");
    return win && win.document && win.document.readyState == "complete";
  }, "Migrator window loaded");
  let migratorWindow = Services.wm.getMostRecentWindow(
    "Browser:MigrationWizard"
  );
  ok(migratorWindow, "Migrator window opened");
  await BrowserTestUtils.closeWindow(migratorWindow);
});

add_task(
  async function login_selected_when_login_added_and_in_no_logins_view() {
    await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async () => {
      let loginList = content.document.querySelector("login-list");
      let loginItem = content.document.querySelector("login-item");
      let loginIntro = content.document.querySelector("login-intro");
      ok(
        loginList.classList.contains("empty-search"),
        "login-list should be showing no logins view from a search with no results"
      );
      ok(
        loginList.classList.contains("no-logins"),
        "login-list should be showing no logins view since there are no saved logins"
      );
      ok(
        !loginList.classList.contains("create-login-selected"),
        "login-list should not be in create-login-selected mode"
      );
      ok(
        loginItem.classList.contains("no-logins"),
        "login-item should be marked as having no-logins"
      );
      ok(ContentTaskUtils.is_hidden(loginItem), "login-item should be hidden");
      ok(
        !ContentTaskUtils.is_hidden(loginIntro),
        "login-intro should be visible"
      );
    });

    TEST_LOGIN1 = await addLogin(TEST_LOGIN1);

    await SpecialPowers.spawn(
      gBrowser.selectedBrowser,
      [TEST_LOGIN1.guid],
      async testLogin1Guid => {
        let loginList = content.document.querySelector("login-list");
        let loginItem = content.document.querySelector("login-item");
        let loginIntro = content.document.querySelector("login-intro");
        ok(
          !loginList.classList.contains("empty-search"),
          "login-list should not be showing no logins view since one login exists"
        );
        ok(
          !loginList.classList.contains("no-logins"),
          "login-list should not be showing no logins view since one login exists"
        );
        ok(
          !loginList.classList.contains("create-login-selected"),
          "login-list should not be in create-login-selected mode"
        );
        is(
          loginList.shadowRoot.querySelector(
            ".login-list-item.selected[data-guid]"
          ).dataset.guid,
          testLogin1Guid,
          "the login that was just added should be selected"
        );
        ok(
          !loginItem.classList.contains("no-logins"),
          "login-item should not be marked as having no-logins"
        );
        is(
          Cu.waiveXrays(loginItem)._login.guid,
          testLogin1Guid,
          "the login-item should have the newly added login selected"
        );
        ok(
          !ContentTaskUtils.is_hidden(loginItem),
          "login-item should be visible"
        );
        ok(
          ContentTaskUtils.is_hidden(loginIntro),
          "login-intro should be hidden"
        );
      }
    );
  }
);
