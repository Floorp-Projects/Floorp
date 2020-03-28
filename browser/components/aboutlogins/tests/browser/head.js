/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

let { LoginBreaches } = ChromeUtils.import(
  "resource:///modules/LoginBreaches.jsm"
);
let { RemoteSettings } = ChromeUtils.import(
  "resource://services-settings/remote-settings.js"
);
let { _AboutLogins } = ChromeUtils.import(
  "resource:///actors/AboutLoginsParent.jsm"
);
let { OSKeyStoreTestUtils } = ChromeUtils.import(
  "resource://testing-common/OSKeyStoreTestUtils.jsm"
);
let { LoginTestUtils } = ChromeUtils.import(
  "resource://testing-common/LoginTestUtils.jsm"
);

let nsLoginInfo = new Components.Constructor(
  "@mozilla.org/login-manager/loginInfo;1",
  Ci.nsILoginInfo,
  "init"
);

let TEST_LOGIN1 = new nsLoginInfo(
  "https://example.com",
  "https://example.com",
  null,
  "user1",
  "pass1",
  "username",
  "password"
);
let TEST_LOGIN2 = new nsLoginInfo(
  "https://2.example.com",
  "https://2.example.com",
  null,
  "user2",
  "pass2",
  "username",
  "password"
);

let TEST_LOGIN3 = new nsLoginInfo(
  "https://breached.example.com",
  "https://breached.example.com",
  null,
  "breachedLogin1",
  "pass3",
  "breachedLogin",
  "password"
);
TEST_LOGIN3.QueryInterface(Ci.nsILoginMetaInfo).timePasswordChanged = 123456;

async function addLogin(login) {
  let storageChangedPromised = TestUtils.topicObserved(
    "passwordmgr-storage-changed",
    (_, data) => data == "addLogin"
  );
  login = Services.logins.addLogin(login);
  await storageChangedPromised;
  registerCleanupFunction(() => {
    let matchData = Cc["@mozilla.org/hash-property-bag;1"].createInstance(
      Ci.nsIWritablePropertyBag2
    );
    matchData.setPropertyAsAUTF8String("guid", login.guid);

    let logins = Services.logins.searchLogins(matchData);
    if (!logins.length) {
      return;
    }
    // Use the login that was returned from searchLogins
    // in case the initial login object was changed by the test code,
    // since removeLogin makes sure that the login argument exactly
    // matches the login that it will be removing.
    Services.logins.removeLogin(logins[0]);
  });
  return login;
}

let EXPECTED_BREACH = null;
let EXPECTED_ERROR_MESSAGE = null;
add_task(async function setup() {
  const db = await RemoteSettings(LoginBreaches.REMOTE_SETTINGS_COLLECTION).db;
  if (EXPECTED_BREACH) {
    await db.create(EXPECTED_BREACH, {
      useRecordId: true,
    });
  }
  await db.saveLastModified(42);
  if (EXPECTED_BREACH) {
    await RemoteSettings(LoginBreaches.REMOTE_SETTINGS_COLLECTION).emit(
      "sync",
      { data: { current: [EXPECTED_BREACH] } }
    );
  }

  SpecialPowers.registerConsoleListener(function onConsoleMessage(msg) {
    if (msg.isWarning || !msg.errorMessage) {
      // Ignore warnings and non-errors.
      return;
    }
    if (
      msg.errorMessage == "Refreshing device list failed." ||
      msg.errorMessage == "Skipping device list refresh; not signed in"
    ) {
      // Ignore errors from browser-sync.js.
      return;
    }
    if (
      msg.errorMessage.includes(
        "ReferenceError: MigrationWizard is not defined"
      )
    ) {
      // todo(Bug 1587237): Ignore error when loading the Migration Wizard in automation.
      return;
    }
    if (
      msg.errorMessage.includes("Error detecting Chrome profiles") ||
      msg.errorMessage.includes(
        "Library/Application Support/Chromium/Local State (No such file or directory)"
      ) ||
      msg.errorMessage.includes(
        "Library/Application Support/Google/Chrome/Local State (No such file or directory)"
      )
    ) {
      // Ignore errors that can occur when the migrator is looking for a
      // Chrome/Chromium profile
      return;
    }
    if (msg.errorMessage.includes("Can't find profile directory.")) {
      // Ignore error messages for no profile found in old XULStore.jsm
      return;
    }
    if (msg.errorMessage.includes("Error reading typed URL history")) {
      // The Migrator when opened can log this exception if there is no Edge
      // history on the machine.
      return;
    }
    if (msg.errorMessage.includes(EXPECTED_ERROR_MESSAGE)) {
      return;
    }
    ok(false, msg.message || msg.errorMessage);
  });

  registerCleanupFunction(async () => {
    EXPECTED_ERROR_MESSAGE = null;
    await db.clear();
    SpecialPowers.postConsoleSentinel();
  });
});

/**
 * Waits for the master password prompt and performs an action.
 * @param {string} action Set to "authenticate" to log in or "cancel" to
 *        close the dialog without logging in.
 */
function waitForMPDialog(action) {
  const BRAND_BUNDLE = Services.strings.createBundle(
    "chrome://branding/locale/brand.properties"
  );
  const BRAND_FULL_NAME = BRAND_BUNDLE.GetStringFromName("brandFullName");
  let dialogShown = TestUtils.topicObserved("common-dialog-loaded");
  return dialogShown.then(function([subject]) {
    let dialog = subject.Dialog;
    let expected = "Password Required - " + BRAND_FULL_NAME;
    is(dialog.args.title, expected, "Dialog is the Master Password dialog");
    if (action == "authenticate") {
      SpecialPowers.wrap(dialog.ui.password1Textbox).setUserInput(
        LoginTestUtils.masterPassword.masterPassword
      );
      dialog.ui.button0.click();
    } else if (action == "cancel") {
      dialog.ui.button1.click();
    }
    return BrowserTestUtils.waitForEvent(window, "DOMModalDialogClosed");
  });
}

/**
 * Allows for tests to reset the MP auth expiration and
 * return a promise that will resolve after the MP dialog has
 * been presented.
 *
 * @param {string} action Set to "authenticate" to log in or "cancel" to
 *        close the dialog without logging in.
 * @returns {Promise} Resolves after the MP dialog has been presented and actioned upon
 */
function forceAuthTimeoutAndWaitForMPDialog(action) {
  const AUTH_TIMEOUT_MS = 5 * 60 * 1000; // 5 minutes (duplicated from AboutLoginsParent.jsm)
  _AboutLogins._authExpirationTime -= AUTH_TIMEOUT_MS + 1;
  return waitForMPDialog(action);
}

/**
 * Allows for tests to reset the OS auth expiration and
 * return a promise that will resolve after the OS auth dialog has
 * been presented.
 *
 * @param {bool} loginResult True if the auth prompt should pass, otherwise false will fail
 * @returns {Promise} Resolves after the OS auth dialog has been presented
 */
function forceAuthTimeoutAndWaitForOSKeyStoreLogin({ loginResult }) {
  const AUTH_TIMEOUT_MS = 5 * 60 * 1000; // 5 minutes (duplicated from AboutLoginsParent.jsm)
  _AboutLogins._authExpirationTime -= AUTH_TIMEOUT_MS + 1;
  return OSKeyStoreTestUtils.waitForOSKeyStoreLogin(loginResult);
}
