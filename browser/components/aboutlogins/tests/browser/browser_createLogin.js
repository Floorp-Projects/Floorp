/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function setup() {
  let aboutLoginsTab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "about:logins",
  });
  registerCleanupFunction(() => {
    BrowserTestUtils.removeTab(aboutLoginsTab);
    Services.logins.removeAllLogins();
  });
});

add_task(async function test_create_login() {
  let browser = gBrowser.selectedBrowser;
  await ContentTask.spawn(browser, null, async () => {
    let loginList = Cu.waiveXrays(content.document.querySelector("login-list"));
    ok(!loginList._selectedGuid, "should not be a selected guid by default");
    ok(
      content.document.documentElement.classList.contains("no-logins"),
      "Should initially be in no logins view"
    );
    ok(
      loginList.classList.contains("no-logins"),
      "login-list should initially be in no logins view"
    );
  });

  let testCases = [
    ["ftp://ftp.example.com/", "ftp://ftp.example.com"],
    ["https://example.com/foo", "https://example.com"],
    ["http://example.com/", "http://example.com"],
    [
      "https://testuser1:testpass1@bugzilla.mozilla.org/show_bug.cgi?id=1556934",
      "https://bugzilla.mozilla.org",
    ],
    ["https://www.example.com/bar", "https://www.example.com"],
  ];

  for (let i = 0; i < testCases.length; i++) {
    let originTuple = testCases[i];
    info("Testcase " + i);
    let storageChangedPromised = TestUtils.topicObserved(
      "passwordmgr-storage-changed",
      (_, data) => data == "addLogin"
    );

    await ContentTask.spawn(browser, originTuple, async aOriginTuple => {
      let loginList = Cu.waiveXrays(
        content.document.querySelector("login-list")
      );
      let createButton = loginList._createLoginButton;
      is(
        content.getComputedStyle(loginList._blankLoginListItem).display,
        "none",
        "the blank login list item should be hidden initially"
      );
      ok(
        !createButton.disabled,
        "Create button should not be disabled initially"
      );

      createButton.click();

      isnot(
        content.getComputedStyle(loginList._blankLoginListItem).display,
        "none",
        "the blank login list item should be visible after clicking on the create button"
      );
      ok(
        createButton.disabled,
        "Create button should be disabled after being clicked"
      );

      let loginItem = Cu.waiveXrays(
        content.document.querySelector("login-item")
      );

      let originInput = loginItem.shadowRoot.querySelector(
        "input[name='origin']"
      );
      let usernameInput = loginItem.shadowRoot.querySelector(
        "input[name='username']"
      );
      let passwordInput = loginItem.shadowRoot.querySelector(
        "input[name='password']"
      );

      originInput.value = aOriginTuple[0];
      usernameInput.value = "testuser1";
      passwordInput.value = "testpass1";

      let saveChangesButton = loginItem.shadowRoot.querySelector(
        ".save-changes-button"
      );
      saveChangesButton.click();
    });

    info("waiting for login to get added to storage");
    await storageChangedPromised;
    info("login added to storage");

    storageChangedPromised = TestUtils.topicObserved(
      "passwordmgr-storage-changed",
      (_, data) => data == "modifyLogin"
    );
    let expectedCount = i + 1;
    await ContentTask.spawn(
      browser,
      { expectedCount, originTuple },
      async ({ expectedCount: aExpectedCount, originTuple: aOriginTuple }) => {
        ok(
          !content.document.documentElement.classList.contains("no-logins"),
          "Should no longer be in no logins view"
        );
        let loginList = Cu.waiveXrays(
          content.document.querySelector("login-list")
        );
        ok(
          !loginList.classList.contains("no-logins"),
          "login-list should no longer be in no logins view"
        );
        is(
          content.getComputedStyle(loginList._blankLoginListItem).display,
          "none",
          "the blank login list item should be hidden after adding new login"
        );
        ok(
          !loginList._createLoginButton.disabled,
          "Create button shouldn't be disabled after exiting create login view"
        );

        let loginGuid = await ContentTaskUtils.waitForCondition(() => {
          return loginList._loginGuidsSortedOrder.find(
            guid => loginList._logins[guid].login.origin == aOriginTuple[1]
          );
        }, "Waiting for login to be displayed");
        ok(loginGuid, "Expected login found in login-list");

        let loginItem = Cu.waiveXrays(
          content.document.querySelector("login-item")
        );
        is(loginItem._login.guid, loginGuid, "login-item should match");

        let { login, listItem } = loginList._logins[loginGuid];
        ok(
          listItem.classList.contains("selected"),
          "list item should be selected"
        );
        ok(
          !!listItem,
          `Stored login should only include the origin of the URL provided during creation (${
            aOriginTuple[1]
          })`
        );
        is(
          login.username,
          "testuser1",
          "Stored login should have username provided during creation"
        );
        is(
          login.password,
          "testpass1",
          "Stored login should have password provided during creation"
        );

        let editButton = loginItem.shadowRoot.querySelector(".edit-button");
        editButton.click();

        let usernameInput = loginItem.shadowRoot.querySelector(
          "input[name='username']"
        );
        let passwordInput = loginItem.shadowRoot.querySelector(
          "input[name='password']"
        );
        usernameInput.value = "testuser2";
        passwordInput.value = "testpass2";

        let saveChangesButton = loginItem.shadowRoot.querySelector(
          ".save-changes-button"
        );
        saveChangesButton.click();
      }
    );

    info("waiting for login to get modified in storage");
    await storageChangedPromised;
    info("login modified in storage");

    await ContentTask.spawn(browser, originTuple, async aOriginTuple => {
      let loginList = Cu.waiveXrays(
        content.document.querySelector("login-list")
      );
      let login = Object.values(loginList._logins).find(
        obj => obj.login.origin == aOriginTuple[1]
      ).login;
      ok(
        !!login,
        "Stored login should only include the origin of the URL provided during creation"
      );
      is(
        login.username,
        "testuser2",
        "Stored login should have modified username"
      );
      is(
        login.password,
        "testpass2",
        "Stored login should have modified password"
      );
    });
  }
});

add_task(async function test_cancel_create_login() {
  let browser = gBrowser.selectedBrowser;
  await ContentTask.spawn(browser, null, async () => {
    let loginList = Cu.waiveXrays(content.document.querySelector("login-list"));
    ok(
      loginList._selectedGuid,
      "there should be a selected guid before create mode"
    );
    is(
      content.getComputedStyle(loginList._blankLoginListItem).display,
      "none",
      "the blank login list item should be hidden before create mode"
    );

    let createButton = content.document
      .querySelector("login-list")
      .shadowRoot.querySelector(".create-login-button");
    createButton.click();

    ok(
      !loginList._selectedGuid,
      "there should be no selected guid when in create mode"
    );
    isnot(
      content.getComputedStyle(loginList._blankLoginListItem).display,
      "none",
      "the blank login list item should be visible in create mode"
    );

    let loginItem = Cu.waiveXrays(content.document.querySelector("login-item"));
    let cancelButton = loginItem.shadowRoot.querySelector(".cancel-button");
    cancelButton.click();

    ok(
      loginList._selectedGuid,
      "there should be a selected guid after canceling create mode"
    );
    is(
      content.getComputedStyle(loginList._blankLoginListItem).display,
      "none",
      "the blank login list item should be hidden after canceling create mode"
    );
  });
});
