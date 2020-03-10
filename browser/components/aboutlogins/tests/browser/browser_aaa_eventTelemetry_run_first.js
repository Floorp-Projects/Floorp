/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

requestLongerTimeout(2);

ChromeUtils.import("resource://testing-common/TelemetryTestUtils.jsm", this);
ChromeUtils.import("resource://testing-common/LoginTestUtils.jsm", this);
ChromeUtils.import("resource://testing-common/OSKeyStoreTestUtils.jsm", this);

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
  TEST_LOGIN1 = await addLogin(TEST_LOGIN1);
  TEST_LOGIN2 = await addLogin(TEST_LOGIN2);
  TEST_LOGIN3 = await addLogin(TEST_LOGIN3);
  await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "about:logins",
  });
  registerCleanupFunction(() => {
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
    Services.logins.removeAllLogins();
  });
});

add_task(async function test_telemetry_events() {
  await TestUtils.waitForCondition(() => {
    Services.telemetry.clearEvents();
    let events = Services.telemetry.snapshotEvents(
      Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
      true
    ).content;
    return !events || !events.length;
  }, "Waiting for telemetry events to get cleared");

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function() {
    let loginList = content.document.querySelector("login-list");
    let loginListItem = loginList.shadowRoot.querySelector(
      ".login-list-item:nth-child(2)"
    );
    loginListItem.click();
  });
  await LoginTestUtils.telemetry.waitForEventCount(2);

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function() {
    let loginItem = content.document.querySelector("login-item");
    let copyButton = loginItem.shadowRoot.querySelector(
      ".copy-username-button"
    );
    copyButton.click();
  });
  await LoginTestUtils.telemetry.waitForEventCount(3);

  if (OSKeyStoreTestUtils.canTestOSKeyStoreLogin()) {
    let reauthObserved = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(true);
    await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function() {
      let loginItem = content.document.querySelector("login-item");
      let copyButton = loginItem.shadowRoot.querySelector(
        ".copy-password-button"
      );
      copyButton.click();
    });
    await reauthObserved;
    await LoginTestUtils.telemetry.waitForEventCount(4);
  }
  let nextTelemetryEventCount = OSKeyStoreTestUtils.canTestOSKeyStoreLogin()
    ? 5
    : 4;

  let promiseNewTab = BrowserTestUtils.waitForNewTab(
    gBrowser,
    TEST_LOGIN3.origin + "/"
  );
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function() {
    let loginItem = content.document.querySelector("login-item");
    let originInput = loginItem.shadowRoot.querySelector(".origin-input");
    originInput.click();
  });
  let newTab = await promiseNewTab;
  ok(true, "New tab opened to " + TEST_LOGIN3.origin);
  BrowserTestUtils.removeTab(newTab);
  await LoginTestUtils.telemetry.waitForEventCount(nextTelemetryEventCount++);

  // Show the password
  if (OSKeyStoreTestUtils.canTestOSKeyStoreLogin()) {
    let reauthObserved = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(true);
    await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function() {
      let loginItem = content.document.querySelector("login-item");
      let revealCheckbox = loginItem.shadowRoot.querySelector(
        ".reveal-password-checkbox"
      );
      revealCheckbox.click();
    });
    await reauthObserved;
    await LoginTestUtils.telemetry.waitForEventCount(nextTelemetryEventCount++);

    // Hide the password
    await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function() {
      let loginItem = content.document.querySelector("login-item");
      let revealCheckbox = loginItem.shadowRoot.querySelector(
        ".reveal-password-checkbox"
      );
      revealCheckbox.click();
    });
    await LoginTestUtils.telemetry.waitForEventCount(nextTelemetryEventCount++);

    reauthObserved = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(true);
    await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function() {
      let loginItem = content.document.querySelector("login-item");
      let editButton = loginItem.shadowRoot.querySelector(".edit-button");
      editButton.click();
    });
    await reauthObserved;
    await LoginTestUtils.telemetry.waitForEventCount(nextTelemetryEventCount++);

    await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function() {
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

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function() {
    let newLoginButton = content.document
      .querySelector("login-list")
      .shadowRoot.querySelector(".create-login-button");
    newLoginButton.click();
  });
  await LoginTestUtils.telemetry.waitForEventCount(nextTelemetryEventCount++);

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function() {
    let loginItem = content.document.querySelector("login-item");
    let cancelButton = loginItem.shadowRoot.querySelector(".cancel-button");
    cancelButton.click();
  });
  await LoginTestUtils.telemetry.waitForEventCount(nextTelemetryEventCount++);

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function() {
    let loginList = content.document.querySelector("login-list");
    let loginListItem = loginList.shadowRoot.querySelector(
      ".login-list-item[data-guid]"
    );
    loginListItem.click();
  });
  await LoginTestUtils.telemetry.waitForEventCount(nextTelemetryEventCount++);

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function() {
    let loginItem = content.document.querySelector("login-item");
    let deleteButton = loginItem.shadowRoot.querySelector(".delete-button");
    deleteButton.click();
    let confirmDeleteDialog = content.document.querySelector(
      "confirmation-dialog"
    );
    let confirmDeleteButton = confirmDeleteDialog.shadowRoot.querySelector(
      ".confirm-button"
    );
    confirmDeleteButton.click();
  });
  await LoginTestUtils.telemetry.waitForEventCount(nextTelemetryEventCount++);

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function() {
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

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function() {
    let loginFilter = content.document.querySelector("login-filter");
    let input = loginFilter.shadowRoot.querySelector("input");
    input.setUserInput("test");
  });
  await LoginTestUtils.telemetry.waitForEventCount(nextTelemetryEventCount++);

  const canTestOSKeyStoreLogin = OSKeyStoreTestUtils.canTestOSKeyStoreLogin();
  let expectedEvents = [
    [true, "pwmgr", "open_management", "direct"],
    [true, "pwmgr", "select", "existing_login"],
    [true, "pwmgr", "copy", "username"],
    [canTestOSKeyStoreLogin, "pwmgr", "copy", "password"],
    [true, "pwmgr", "open_site", "existing_login"],
    [canTestOSKeyStoreLogin, "pwmgr", "show", "password"],
    [canTestOSKeyStoreLogin, "pwmgr", "hide", "password"],
    [canTestOSKeyStoreLogin, "pwmgr", "edit", "existing_login"],
    [canTestOSKeyStoreLogin, "pwmgr", "save", "existing_login"],
    [true, "pwmgr", "new", "new_login"],
    [true, "pwmgr", "cancel", "new_login"],
    [true, "pwmgr", "select", "existing_login"],
    [true, "pwmgr", "delete", "existing_login"],
    [true, "pwmgr", "sort", "list"],
    [true, "pwmgr", "filter", "list"],
  ];
  let actualExpectedEvents = expectedEvents
    .filter(event => event[0])
    .map(event => event.slice(1));

  TelemetryTestUtils.assertEvents(
    actualExpectedEvents,
    { category: "pwmgr" },
    { clear: true, process: "content" }
  );
});
