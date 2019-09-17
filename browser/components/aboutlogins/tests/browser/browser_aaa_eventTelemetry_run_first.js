/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

requestLongerTimeout(2);

ChromeUtils.import("resource://testing-common/TelemetryTestUtils.jsm", this);

function waitForTelemetryEventCount(count) {
  info("waiting for telemetry event count of " + count);
  return TestUtils.waitForCondition(() => {
    let events = Services.telemetry.snapshotEvents(
      Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
      false
    ).content;
    info("got " + (events && events.length) + " events");
    return events && events.length == count;
  }, "waiting for telemetry event count of: " + count);
}

add_task(async function setup() {
  let storageChangedPromised = TestUtils.topicObserved(
    "passwordmgr-storage-changed",
    (_, data) => data == "addLogin"
  );
  TEST_LOGIN1 = Services.logins.addLogin(TEST_LOGIN1);
  await storageChangedPromised;
  storageChangedPromised = TestUtils.topicObserved(
    "passwordmgr-storage-changed",
    (_, data) => data == "addLogin"
  );
  TEST_LOGIN2 = Services.logins.addLogin(TEST_LOGIN2);
  await storageChangedPromised;
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

  await ContentTask.spawn(gBrowser.selectedBrowser, null, async function() {
    let loginList = content.document.querySelector("login-list");
    let loginListItem = loginList.shadowRoot.querySelector(
      ".login-list-item:nth-child(2)"
    );
    loginListItem.click();
  });
  await waitForTelemetryEventCount(1);

  await ContentTask.spawn(gBrowser.selectedBrowser, null, async function() {
    let loginItem = content.document.querySelector("login-item");
    let copyButton = loginItem.shadowRoot.querySelector(
      ".copy-username-button"
    );
    copyButton.click();
  });
  await waitForTelemetryEventCount(2);

  await ContentTask.spawn(gBrowser.selectedBrowser, null, async function() {
    let loginItem = content.document.querySelector("login-item");
    let copyButton = loginItem.shadowRoot.querySelector(
      ".copy-password-button"
    );
    copyButton.click();
  });
  await waitForTelemetryEventCount(3);

  let promiseNewTab = BrowserTestUtils.waitForNewTab(
    gBrowser,
    TEST_LOGIN2.origin + "/"
  );
  await ContentTask.spawn(gBrowser.selectedBrowser, null, async function() {
    let loginItem = content.document.querySelector("login-item");
    let originInput = loginItem.shadowRoot.querySelector(".origin-input");
    originInput.click();
  });
  let newTab = await promiseNewTab;
  ok(true, "New tab opened to " + TEST_LOGIN2.origin);
  BrowserTestUtils.removeTab(newTab);
  await waitForTelemetryEventCount(4);

  // Show the password
  await ContentTask.spawn(gBrowser.selectedBrowser, null, async function() {
    let loginItem = content.document.querySelector("login-item");
    let revealCheckbox = loginItem.shadowRoot.querySelector(
      ".reveal-password-checkbox"
    );
    revealCheckbox.click();
  });
  await waitForTelemetryEventCount(5);

  // Hide the password
  await ContentTask.spawn(gBrowser.selectedBrowser, null, async function() {
    let loginItem = content.document.querySelector("login-item");
    let revealCheckbox = loginItem.shadowRoot.querySelector(
      ".reveal-password-checkbox"
    );
    revealCheckbox.click();
  });
  await waitForTelemetryEventCount(6);

  await ContentTask.spawn(gBrowser.selectedBrowser, null, async function() {
    let loginItem = content.document.querySelector("login-item");
    let editButton = loginItem.shadowRoot.querySelector(".edit-button");
    editButton.click();
  });
  await waitForTelemetryEventCount(7);

  await ContentTask.spawn(gBrowser.selectedBrowser, null, async function() {
    let loginItem = content.document.querySelector("login-item");
    let usernameField = loginItem.shadowRoot.querySelector(
      'input[name="username"]'
    );
    usernameField.value = "user1-modified";

    let saveButton = loginItem.shadowRoot.querySelector(".save-changes-button");
    saveButton.click();
  });
  await waitForTelemetryEventCount(8);

  await ContentTask.spawn(gBrowser.selectedBrowser, null, async function() {
    let newLoginButton = content.document
      .querySelector("login-list")
      .shadowRoot.querySelector(".create-login-button");
    newLoginButton.click();
  });
  await waitForTelemetryEventCount(9);

  await ContentTask.spawn(gBrowser.selectedBrowser, null, async function() {
    let loginItem = content.document.querySelector("login-item");
    let cancelButton = loginItem.shadowRoot.querySelector(".cancel-button");
    cancelButton.click();
  });
  await waitForTelemetryEventCount(10);

  await ContentTask.spawn(gBrowser.selectedBrowser, null, async function() {
    let loginList = content.document.querySelector("login-list");
    let loginListItem = loginList.shadowRoot.querySelector(
      ".login-list-item[data-guid]"
    );
    loginListItem.click();
  });
  await waitForTelemetryEventCount(11);

  await ContentTask.spawn(gBrowser.selectedBrowser, null, async function() {
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
  await waitForTelemetryEventCount(12);

  await ContentTask.spawn(gBrowser.selectedBrowser, null, async function() {
    let loginFilter = content.document.querySelector("login-filter");
    let input = loginFilter.shadowRoot.querySelector("input");
    input.setUserInput("test");
  });
  await waitForTelemetryEventCount(13);

  let expectedEvents = [
    ["pwmgr", "select", "existing_login"],
    ["pwmgr", "copy", "username"],
    ["pwmgr", "copy", "password"],
    ["pwmgr", "open_site", "existing_login"],
    ["pwmgr", "show", "password"],
    ["pwmgr", "hide", "password"],
    ["pwmgr", "edit", "existing_login"],
    ["pwmgr", "save", "existing_login"],
    ["pwmgr", "new", "new_login"],
    ["pwmgr", "cancel", "new_login"],
    ["pwmgr", "select", "existing_login"],
    ["pwmgr", "delete", "existing_login"],
    ["pwmgr", "filter", "list"],
  ];

  TelemetryTestUtils.assertEvents(
    expectedEvents,
    { category: "pwmgr" },
    { process: "content" }
  );
});
