/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

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

add_task(async function test_showLoginItemErrors() {
  const browser = gBrowser.selectedBrowser;
  let LOGIN_TO_UPDATE = new nsLoginInfo(
    "https://example.com",
    "https://example.com",
    null,
    "user2",
    "pass2"
  );
  LOGIN_TO_UPDATE = await Services.logins.addLoginAsync(LOGIN_TO_UPDATE);
  EXPECTED_ERROR_MESSAGE = "This login already exists.";
  const LOGIN_UPDATES = {
    origin: "https://example.com",
    password: "my1GoodPassword",
    username: "user1",
  };

  await SpecialPowers.spawn(
    browser,
    [[LoginHelper.loginToVanillaObject(LOGIN_TO_UPDATE), LOGIN_UPDATES]],
    async ([loginToUpdate, loginUpdates]) => {
      const loginItem = Cu.waiveXrays(
        content.document.querySelector("login-item")
      );
      const loginItemErrorMessage = Cu.waiveXrays(
        loginItem.shadowRoot.querySelector(".error-message")
      );
      const loginList = Cu.waiveXrays(
        content.document.querySelector("login-list")
      );

      const createButton = loginList._createLoginButton;
      createButton.click();

      const event = Cu.cloneInto(
        {
          bubbles: true,
          detail: loginUpdates,
        },
        content
      );

      content.dispatchEvent(
        // adds first login
        new content.CustomEvent("AboutLoginsCreateLogin", event)
      );

      await ContentTaskUtils.waitForCondition(() => {
        return loginList.shadowRoot.querySelectorAll(".list-item").length === 3;
      }, "Waiting for login item to be created.");

      Assert.ok(
        loginItemErrorMessage.hidden,
        "An error message should not be displayed after adding a new login."
      );

      content.dispatchEvent(
        // adds a duplicate of the first login
        new content.CustomEvent("AboutLoginsCreateLogin", event)
      );

      const loginItemErrorMessageVisible =
        await ContentTaskUtils.waitForCondition(() => {
          return !loginItemErrorMessage.hidden;
        }, "Waiting for error message to be shown after attempting to create a duplicate login.");
      Assert.ok(
        loginItemErrorMessageVisible,
        "An error message should be shown after user attempts to add a login that already exists."
      );

      const loginItemErrorMessageText =
        loginItemErrorMessage.querySelector("span:not([hidden])");
      Assert.equal(
        loginItemErrorMessageText.dataset.l10nId,
        "about-logins-error-message-duplicate-login-with-link",
        "The correct error message is displayed."
      );

      let loginListItem = Cu.waiveXrays(
        loginList.shadowRoot.querySelector(
          `login-list-item[data-guid='${loginToUpdate.guid}']`
        )
      );
      loginListItem.click();

      Assert.ok(
        loginItemErrorMessage.hidden,
        "The error message should no longer be visible."
      );
    }
  );
  if (!OSKeyStoreTestUtils.canTestOSKeyStoreLogin()) {
    // The rest of the test uses Edit mode which causes an OS prompt in official builds.
    return;
  }
  let reauthObserved = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(true);
  await SpecialPowers.spawn(
    browser,
    [[LoginHelper.loginToVanillaObject(LOGIN_TO_UPDATE), LOGIN_UPDATES]],
    async ([loginToUpdate, loginUpdates]) => {
      const loginItem = Cu.waiveXrays(
        content.document.querySelector("login-item")
      );
      const editButton = loginItem.shadowRoot
        .querySelector(".edit-button")
        .shadowRoot.querySelector("button");
      editButton.click();

      const updateEvent = Cu.cloneInto(
        {
          bubbles: true,
          detail: Object.assign({ guid: loginToUpdate.guid }, loginUpdates),
        },
        content
      );

      content.dispatchEvent(
        // attempt to update LOGIN_TO_UPDATE to a username/origin combination that already exists.
        new content.CustomEvent("AboutLoginsUpdateLogin", updateEvent)
      );

      const loginItemErrorMessage = Cu.waiveXrays(
        loginItem.shadowRoot.querySelector(".error-message")
      );
      const loginAlreadyExistsErrorShownAfterUpdate =
        await ContentTaskUtils.waitForCondition(() => {
          return !loginItemErrorMessage.hidden;
        }, "Waiting for error message to show after updating login to existing login.");
      Assert.ok(
        loginAlreadyExistsErrorShownAfterUpdate,
        "An error message should be shown after updating a login to a username/origin combination that already exists."
      );
    }
  );
  info("making sure os auth dialog is shown");
  await reauthObserved;
  info("saw os auth dialog");
  EXPECTED_ERROR_MESSAGE = null;
});
