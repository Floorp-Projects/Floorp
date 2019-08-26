/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

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

add_task(async function test_showLoginItemErrors() {
  const browser = gBrowser.selectedBrowser;
  let LOGIN_TO_UPDATE = new nsLoginInfo(
    "https://example.com/",
    "https://example.com/",
    null,
    "user2",
    "pass2",
    "username",
    "password"
  );
  LOGIN_TO_UPDATE = Services.logins.addLogin(LOGIN_TO_UPDATE);

  await ContentTask.spawn(
    browser,
    LoginHelper.loginToVanillaObject(LOGIN_TO_UPDATE),
    async loginToUpdate => {
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

      const loginUpdates = {
        origin: "https://example.com/",
        password: "my1GoodPassword",
        username: "user1",
      };

      const event = Cu.cloneInto(
        {
          bubbles: true,
          detail: loginUpdates,
        },
        content
      );

      content.dispatchEvent(
        // adds first lgoin
        new content.CustomEvent("AboutLoginsCreateLogin", event)
      );

      ok(
        loginItemErrorMessage.hidden,
        "An error message should not be displayed after adding a new login."
      );

      content.dispatchEvent(
        // adds a duplicate of the first login
        new content.CustomEvent("AboutLoginsCreateLogin", event)
      );

      const loginItemErrorMessageVisible = await ContentTaskUtils.waitForCondition(
        () => {
          return !loginItemErrorMessage.hidden;
        },
        "Waiting for error message to be shown after attempting to create a duplicate login."
      );
      ok(
        loginItemErrorMessageVisible,
        "An error message should be shown after user attempts to add a login that already exists."
      );

      const loginItemErrorMessageText = loginItemErrorMessage.querySelector(
        "span"
      );
      ok(
        loginItemErrorMessageText.dataset.l10nId ===
          "about-logins-error-message-duplicate-login",
        "The correct error message is displayed."
      );

      let loginListItem = Cu.waiveXrays(
        loginList.shadowRoot.querySelector(
          `.login-list-item[data-guid='${loginToUpdate.guid}']`
        )
      );
      loginListItem.click();

      ok(
        loginItemErrorMessage.hidden,
        "The error message should no longer be visible."
      );

      const editButton = loginItem.shadowRoot.querySelector(".edit-button");
      editButton.click();

      content.dispatchEvent(
        // attempt to update LOGIN_TO_UPDATE to a username/origin combination that already exists.
        new content.CustomEvent("AboutLoginsUpdateLogin", event)
      );

      const loginAlreadyExistsErrorShownAfterUpdate = ContentTaskUtils.waitForCondition(
        () => {
          return !loginItemErrorMessage.hidden;
        },
        "Waiting for error message to show after updating login to existing login."
      );
      ok(
        loginAlreadyExistsErrorShownAfterUpdate,
        "An error message should be shown after updating a login to a username/origin combination that already exists."
      );
    }
  );
});
