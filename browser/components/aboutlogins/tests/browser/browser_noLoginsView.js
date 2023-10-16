/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_setup(async function () {
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
  let wizardPromise;

  // The import link is hidden on Linux, so we don't wait for the migration
  // wizard to open on that platform.
  if (AppConstants.platform != "linux") {
    wizardPromise = BrowserTestUtils.waitForMigrationWizard(window);
  }

  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [platform],
    async aPlatform => {
      let loginList = content.document.querySelector("login-list");

      Assert.ok(
        content.document.documentElement.classList.contains("no-logins"),
        "root should be in no logins view"
      );
      Assert.ok(
        loginList.classList.contains("no-logins"),
        "login-list should be in no logins view"
      );

      let loginIntro = Cu.waiveXrays(
        content.document.querySelector("login-intro")
      );
      let loginItem = content.document.querySelector("login-item");
      let loginListIntro = loginList.shadowRoot.querySelector(".intro");
      let loginListList = loginList.shadowRoot.querySelector("ol");

      Assert.ok(
        !ContentTaskUtils.is_hidden(loginIntro),
        "login-intro should be shown in no logins view"
      );
      Assert.ok(
        !ContentTaskUtils.is_hidden(loginListIntro),
        "login-list intro should be shown in no logins view"
      );

      Assert.ok(
        ContentTaskUtils.is_hidden(loginItem),
        "login-item should be hidden in no logins view"
      );
      Assert.ok(
        ContentTaskUtils.is_hidden(loginListList),
        "login-list logins list should be hidden in no logins view"
      );
      Assert.equal(
        content.document.l10n.getAttributes(
          loginIntro.shadowRoot.querySelector(".heading")
        ).id,
        "about-logins-login-intro-heading-logged-out2",
        "The default message should be the non-logged-in message"
      );
      Assert.ok(
        loginIntro.shadowRoot
          .querySelector("a.intro-help-link")
          .href.includes("password-manager-remember-delete-edit-logins"),
        "Check support href populated"
      );

      loginIntro.updateState(Cu.cloneInto({ loggedIn: true }, content));

      Assert.equal(
        content.document.l10n.getAttributes(
          loginIntro.shadowRoot.querySelector(".heading")
        ).id,
        "about-logins-login-intro-heading-logged-in",
        "When logged in the message should update"
      );

      let importClass = Services.prefs.getBoolPref(
        "signon.management.page.fileImport.enabled"
      )
        ? ".intro-import-text.file-import"
        : ".intro-import-text.no-file-import";
      Assert.equal(
        ContentTaskUtils.is_hidden(
          loginIntro.shadowRoot.querySelector(importClass)
        ),
        aPlatform == "linux",
        "the import link should be hidden on Linux builds"
      );
      if (aPlatform == "linux") {
        // End the test now for Linux since the link is hidden.
        return;
      }
      loginIntro.shadowRoot.querySelector(importClass + " > a").click();
      info("waiting for MigrationWizard to open");
    }
  );
  if (AppConstants.platform == "linux") {
    // End the test now for Linux since the link is hidden.
    return;
  }
  let wizardTab = await wizardPromise;
  Assert.ok(wizardTab, "Migrator wizard tab opened");
  await BrowserTestUtils.removeTab(wizardTab);
});

add_task(
  async function login_selected_when_login_added_and_in_no_logins_view() {
    await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async () => {
      let loginList = content.document.querySelector("login-list");
      let loginItem = content.document.querySelector("login-item");
      let loginIntro = content.document.querySelector("login-intro");
      Assert.ok(
        loginList.classList.contains("empty-search"),
        "login-list should be showing no logins view from a search with no results"
      );
      Assert.ok(
        loginList.classList.contains("no-logins"),
        "login-list should be showing no logins view since there are no saved logins"
      );
      Assert.ok(
        !loginList.classList.contains("create-login-selected"),
        "login-list should not be in create-login-selected mode"
      );
      Assert.ok(
        loginItem.classList.contains("no-logins"),
        "login-item should be marked as having no-logins"
      );
      Assert.ok(
        ContentTaskUtils.is_hidden(loginItem),
        "login-item should be hidden"
      );
      Assert.ok(
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
        await ContentTaskUtils.waitForCondition(() => {
          return !loginList.classList.contains("no-logins");
        }, "waiting for login-list to leave the no-logins view");
        Assert.ok(
          !loginList.classList.contains("empty-search"),
          "login-list should not be showing no logins view since one login exists"
        );
        Assert.ok(
          !loginList.classList.contains("no-logins"),
          "login-list should not be showing no logins view since one login exists"
        );
        Assert.ok(
          !loginList.classList.contains("create-login-selected"),
          "login-list should not be in create-login-selected mode"
        );
        Assert.equal(
          loginList.shadowRoot.querySelector(
            "login-list-item.selected[data-guid]"
          ).dataset.guid,
          testLogin1Guid,
          "the login that was just added should be selected"
        );
        Assert.ok(
          !loginItem.classList.contains("no-logins"),
          "login-item should not be marked as having no-logins"
        );
        Assert.equal(
          Cu.waiveXrays(loginItem)._login.guid,
          testLogin1Guid,
          "the login-item should have the newly added login selected"
        );
        Assert.ok(
          !ContentTaskUtils.is_hidden(loginItem),
          "login-item should be visible"
        );
        Assert.ok(
          ContentTaskUtils.is_hidden(loginIntro),
          "login-intro should be hidden"
        );
      }
    );
  }
);
