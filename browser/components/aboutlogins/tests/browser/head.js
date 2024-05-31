/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

let { LoginBreaches } = ChromeUtils.importESModule(
  "resource:///modules/LoginBreaches.sys.mjs"
);
let { RemoteSettings } = ChromeUtils.importESModule(
  "resource://services-settings/remote-settings.sys.mjs"
);
let { _AboutLogins } = ChromeUtils.importESModule(
  "resource:///actors/AboutLoginsParent.sys.mjs"
);
let { OSKeyStoreTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/OSKeyStoreTestUtils.sys.mjs"
);

const { OSKeyStore } = ChromeUtils.importESModule(
  "resource://gre/modules/OSKeyStore.sys.mjs"
);

let { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

// Always pretend OS Auth is enabled in this dir.
if (OSKeyStoreTestUtils.canTestOSKeyStoreLogin() && OSKeyStore.canReauth()) {
  // Enable OS reauth so we can test it.
  sinon.stub(LoginHelper, "getOSAuthEnabled").returns(true);
  registerCleanupFunction(() => {
    sinon.restore();
  });
}

var { LoginTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/LoginTestUtils.sys.mjs"
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

const PASSWORDS_OS_REAUTH_PREF = "signon.management.page.os-auth.optout";
const CryptoErrors = {
  USER_CANCELED_PASSWORD: "User canceled primary password entry",
  ENCRYPTION_FAILURE: "Couldn't encrypt string",
  INVALID_ARG_ENCRYPT: "Need at least one plaintext to encrypt",
  INVALID_ARG_DECRYPT: "Need at least one ciphertext to decrypt",
  DECRYPTION_FAILURE: "Couldn't decrypt string",
};

async function addLogin(login) {
  const result = await Services.logins.addLoginAsync(login);
  registerCleanupFunction(() => {
    let matchData = Cc["@mozilla.org/hash-property-bag;1"].createInstance(
      Ci.nsIWritablePropertyBag2
    );
    matchData.setPropertyAsAUTF8String("guid", result.guid);

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
  return result;
}

let EXPECTED_BREACH = null;
let EXPECTED_ERROR_MESSAGE = null;
add_setup(async function setup_head() {
  const db = RemoteSettings(LoginBreaches.REMOTE_SETTINGS_COLLECTION).db;
  if (EXPECTED_BREACH) {
    await db.create(EXPECTED_BREACH, {
      useRecordId: true,
    });
  }
  await db.importChanges({}, Date.now());
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

    if (msg.errorMessage.includes('Unknown event: ["jsonfile", "load"')) {
      // Ignore telemetry errors from JSONFile.sys.mjs.
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
      // Ignore error messages for no profile found in old XULStore.sys.mjs
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
    if (msg.errorMessage == "FILE_FORMAT_ERROR") {
      // Ignore errors handled by the error message dialog.
      return;
    }
    if (
      msg.errorMessage ==
      "NotFoundError: No such JSWindowActor 'MarionetteEvents'"
    ) {
      // Ignore MarionetteEvents error (Bug 1730837, Bug 1710079).
      return;
    }
    if (msg.errorMessage.includes(CryptoErrors.DECRYPTION_FAILURE)) {
      // Ignore decyption errors, we want to test if decryption failed
      // But we cannot use try / catch in the test to catch this for some reason
      // Bug 1403081 and Bug 1877720
      return;
    }
    Assert.ok(false, msg.message || msg.errorMessage);
  });

  registerCleanupFunction(async () => {
    EXPECTED_ERROR_MESSAGE = null;
    await db.clear();
    Services.telemetry.clearEvents();
    SpecialPowers.postConsoleSentinel();
  });
});

/**
 * Waits for the primary password prompt and performs an action.
 * @param {string} action Set to "authenticate" to log in or "cancel" to
 *        close the dialog without logging in.
 */
function waitForMPDialog(action, aWindow = window) {
  const BRAND_BUNDLE = Services.strings.createBundle(
    "chrome://branding/locale/brand.properties"
  );
  const BRAND_FULL_NAME = BRAND_BUNDLE.GetStringFromName("brandFullName");
  let dialogShown = TestUtils.topicObserved("common-dialog-loaded");
  return dialogShown.then(function ([subject]) {
    let dialog = subject.Dialog;
    let expected = "Password Required - " + BRAND_FULL_NAME;
    Assert.equal(
      dialog.args.title,
      expected,
      "Dialog is the Primary Password dialog"
    );
    if (action == "authenticate") {
      SpecialPowers.wrap(dialog.ui.password1Textbox).setUserInput(
        LoginTestUtils.primaryPassword.primaryPassword
      );
      dialog.ui.button0.click();
    } else if (action == "cancel") {
      dialog.ui.button1.click();
    }
    return BrowserTestUtils.waitForEvent(aWindow, "DOMModalDialogClosed");
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
function forceAuthTimeoutAndWaitForMPDialog(action, aWindow = window) {
  const AUTH_TIMEOUT_MS = 5 * 60 * 1000; // 5 minutes (duplicated from AboutLoginsParent.sys.mjs)
  _AboutLogins._authExpirationTime -= AUTH_TIMEOUT_MS + 1;
  return waitForMPDialog(action, aWindow);
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
  const AUTH_TIMEOUT_MS = 5 * 60 * 1000; // 5 minutes (duplicated from AboutLoginsParent.sys.mjs)
  _AboutLogins._authExpirationTime -= AUTH_TIMEOUT_MS + 1;
  return OSKeyStoreTestUtils.waitForOSKeyStoreLogin(loginResult);
}
