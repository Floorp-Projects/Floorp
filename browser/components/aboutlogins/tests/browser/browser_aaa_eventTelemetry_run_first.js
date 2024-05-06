/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

requestLongerTimeout(2);

const { TelemetryTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TelemetryTestUtils.sys.mjs"
);

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

let VULNERABLE_TEST_LOGIN2 = new nsLoginInfo(
  "https://2.example.com",
  "https://2.example.com",
  null,
  "user2",
  "pass3",
  "username",
  "password"
);

add_setup(async function () {
  TEST_LOGIN1 = await addLogin(TEST_LOGIN1);
  VULNERABLE_TEST_LOGIN2 = await addLogin(VULNERABLE_TEST_LOGIN2);
  TEST_LOGIN3 = await addLogin(TEST_LOGIN3);

  await TestUtils.waitForCondition(() => {
    Services.telemetry.clearEvents();
    let events = Services.telemetry.snapshotEvents(
      Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
      true
    ).content;
    return !events || !events.length;
  }, "Waiting for telemetry events to get cleared");

  await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "about:logins",
  });
  registerCleanupFunction(() => {
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
    Services.logins.removeAllUserFacingLogins();
  });
});

add_task(async function test_telemetry_events() {
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    let loginList = content.document.querySelector("login-list");
    let loginListItem = loginList.shadowRoot.querySelector(
      "login-list-item.breached"
    );
    loginListItem.click();
  });
  await LoginTestUtils.telemetry.waitForEventCount(2);

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    let loginItem = content.document.querySelector("login-item");
    let copyButton = loginItem.shadowRoot.querySelector("copy-username-button");
    copyButton.click();
  });
  await LoginTestUtils.telemetry.waitForEventCount(3);

  if (OSKeyStoreTestUtils.canTestOSKeyStoreLogin()) {
    let reauthObserved = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(true);
    await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
      let loginItem = content.document.querySelector("login-item");
      let copyButton = loginItem.shadowRoot.querySelector(
        "copy-password-button"
      );
      copyButton.click();
    });
    await reauthObserved;
    // When reauth is observed an extra telemetry event will be recorded
    // for the reauth, hence the event count increasing by 2 here, and later
    // in the test as well.
    await LoginTestUtils.telemetry.waitForEventCount(5);
  }
  let nextTelemetryEventCount = OSKeyStoreTestUtils.canTestOSKeyStoreLogin()
    ? 6
    : 4;

  let promiseNewTab = BrowserTestUtils.waitForNewTab(
    gBrowser,
    TEST_LOGIN3.origin + "/"
  );
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    let loginItem = content.document.querySelector("login-item");
    let originInput = loginItem.shadowRoot.querySelector(".origin-input");
    originInput.click();
  });
  let newTab = await promiseNewTab;
  Assert.ok(true, "New tab opened to " + TEST_LOGIN3.origin);
  BrowserTestUtils.removeTab(newTab);
  await LoginTestUtils.telemetry.waitForEventCount(nextTelemetryEventCount++);

  // Show the password
  if (OSKeyStoreTestUtils.canTestOSKeyStoreLogin()) {
    let reauthObserved = forceAuthTimeoutAndWaitForOSKeyStoreLogin({
      loginResult: true,
    });
    nextTelemetryEventCount++; // An extra event is observed for the reauth event.
    await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
      let loginItem = content.document.querySelector("login-item");
      let revealCheckbox = loginItem.shadowRoot.querySelector(
        ".reveal-password-checkbox"
      );
      revealCheckbox.click();
    });
    await reauthObserved;
    await LoginTestUtils.telemetry.waitForEventCount(nextTelemetryEventCount++);

    // Hide the password
    await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
      let loginItem = content.document.querySelector("login-item");
      let revealCheckbox = loginItem.shadowRoot.querySelector(
        ".reveal-password-checkbox"
      );
      revealCheckbox.click();
    });
    await LoginTestUtils.telemetry.waitForEventCount(nextTelemetryEventCount++);

    // Don't force the auth timeout here to check that `auth_skipped: true` is set as
    // in `extra`.
    nextTelemetryEventCount++; // An extra event is observed for the reauth event.
    await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
      let loginItem = content.document.querySelector("login-item");
      let editButton = loginItem.shadowRoot.querySelector("edit-button");
      editButton.click();
    });
    await LoginTestUtils.telemetry.waitForEventCount(nextTelemetryEventCount++);

    await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
      let loginItem = content.document.querySelector("login-item");
      let usernameField = loginItem.shadowRoot.querySelector(
        'input[name="username"]'
      );
      usernameField.value = "user1-modified";

      let saveButton = loginItem.shadowRoot.querySelector(
        ".save-changes-button"
      );
      saveButton.click();
    });
    await LoginTestUtils.telemetry.waitForEventCount(nextTelemetryEventCount++);
  }

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    let loginItem = content.document.querySelector("login-item");
    let deleteButton = loginItem.shadowRoot.querySelector("delete-button");
    deleteButton.click();
    let confirmDeleteDialog = content.document.querySelector(
      "confirmation-dialog"
    );
    let confirmDeleteButton =
      confirmDeleteDialog.shadowRoot.querySelector(".confirm-button");
    confirmDeleteButton.click();
  });
  await LoginTestUtils.telemetry.waitForEventCount(nextTelemetryEventCount++);

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    let newLoginButton = content.document
      .querySelector("login-list")
      .shadowRoot.querySelector("create-login-button");
    newLoginButton.click();
  });
  await LoginTestUtils.telemetry.waitForEventCount(nextTelemetryEventCount++);

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    let loginItem = content.document.querySelector("login-item");
    let cancelButton = loginItem.shadowRoot.querySelector(".cancel-button");
    cancelButton.click();
  });
  await LoginTestUtils.telemetry.waitForEventCount(nextTelemetryEventCount++);

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    let loginList = content.document.querySelector("login-list");
    let loginListItem = loginList.shadowRoot.querySelector(
      "login-list-item.vulnerable"
    );
    loginListItem.click();
  });
  await LoginTestUtils.telemetry.waitForEventCount(nextTelemetryEventCount++);

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    let loginItem = content.document.querySelector("login-item");
    let copyButton = loginItem.shadowRoot.querySelector("copy-username-button");
    copyButton.click();
  });
  await LoginTestUtils.telemetry.waitForEventCount(nextTelemetryEventCount++);

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    let loginItem = content.document.querySelector("login-item");
    let deleteButton = loginItem.shadowRoot.querySelector("delete-button");
    deleteButton.click();
    let confirmDeleteDialog = content.document.querySelector(
      "confirmation-dialog"
    );
    let confirmDeleteButton =
      confirmDeleteDialog.shadowRoot.querySelector(".confirm-button");
    confirmDeleteButton.click();
  });
  await LoginTestUtils.telemetry.waitForEventCount(nextTelemetryEventCount++);

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    let loginSort = content.document
      .querySelector("login-list")
      .shadowRoot.querySelector("#login-sort");
    loginSort.value = "last-used";
    loginSort.dispatchEvent(new content.Event("change", { bubbles: true }));
  });
  await LoginTestUtils.telemetry.waitForEventCount(nextTelemetryEventCount++);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("signon.management.page.sort");
  });

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    const loginList = content.document.querySelector("login-list");
    const loginFilter = loginList.shadowRoot.querySelector("login-filter");
    const input = loginFilter.shadowRoot.querySelector("input");
    input.setUserInput("test");
  });
  await LoginTestUtils.telemetry.waitForEventCount(nextTelemetryEventCount++);

  const testOSAuth = OSKeyStoreTestUtils.canTestOSKeyStoreLogin();
  let expectedEvents = [
    [true, "pwmgr", "open_management", "direct"],
    [true, "pwmgr", "select", "existing_login", null, { breached: "true" }],
    [true, "pwmgr", "copy", "username", null, { breached: "true" }],
    [testOSAuth, "pwmgr", "reauthenticate", "os_auth", "success"],
    [testOSAuth, "pwmgr", "copy", "password", null, { breached: "true" }],
    [true, "pwmgr", "open_site", "existing_login", null, { breached: "true" }],
    [testOSAuth, "pwmgr", "reauthenticate", "os_auth", "success"],
    [testOSAuth, "pwmgr", "show", "password", null, { breached: "true" }],
    [testOSAuth, "pwmgr", "hide", "password", null, { breached: "true" }],
    [testOSAuth, "pwmgr", "reauthenticate", "os_auth", "success_no_prompt"],
    [testOSAuth, "pwmgr", "edit", "existing_login", null, { breached: "true" }],
    [testOSAuth, "pwmgr", "save", "existing_login", null, { breached: "true" }],
    [true, "pwmgr", "delete", "existing_login", null, { breached: "true" }],
    [true, "pwmgr", "new", "new_login"],
    [true, "pwmgr", "cancel", "new_login"],
    [true, "pwmgr", "select", "existing_login", null, { vulnerable: "true" }],
    [true, "pwmgr", "copy", "username", null, { vulnerable: "true" }],
    [true, "pwmgr", "delete", "existing_login", null, { vulnerable: "true" }],
    [true, "pwmgr", "sort", "list"],
    [true, "pwmgr", "filter", "list"],
  ];
  expectedEvents = expectedEvents
    .filter(event => event[0])
    .map(event => event.slice(1));

  TelemetryTestUtils.assertEvents(
    expectedEvents,
    { category: "pwmgr" },
    { clear: true, process: "content" }
  );
});
