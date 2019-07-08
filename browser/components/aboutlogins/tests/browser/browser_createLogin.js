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

    let browser = gBrowser.selectedBrowser;
    await ContentTask.spawn(browser, originTuple, async aOriginTuple => {
      let createButton = content.document.querySelector("#create-login-button");
      createButton.click();
      await Promise.resolve();

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
        let loginList = Cu.waiveXrays(
          content.document.querySelector("login-list")
        );
        let loginFound = await ContentTaskUtils.waitForCondition(() => {
          return loginList._logins.length == aExpectedCount;
        }, "Waiting for login to be displayed");
        ok(loginFound, "Expected number of logins found in login-list");

        let loginListItem = [
          ...loginList.shadowRoot.querySelectorAll("login-list-item"),
        ].find(l => l._login.origin == aOriginTuple[1]);
        ok(
          !!loginListItem,
          `Stored login should only include the origin of the URL provided during creation (${
            aOriginTuple[1]
          })`
        );
        is(
          loginListItem._login.username,
          "testuser1",
          "Stored login should have username provided during creation"
        );
        is(
          loginListItem._login.password,
          "testpass1",
          "Stored login should have password provided during creation"
        );
        loginListItem.click();

        let loginItem = Cu.waiveXrays(
          content.document.querySelector("login-item")
        );
        is(
          loginItem._login.guid,
          loginListItem._login.guid,
          "Login should be selected"
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
      let login = loginList._logins.find(l => l.origin == aOriginTuple[1]);
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
