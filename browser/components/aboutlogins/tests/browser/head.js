/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

let { LoginBreaches } = ChromeUtils.import(
  "resource:///modules/LoginBreaches.jsm"
);
let { RemoteSettings } = ChromeUtils.import(
  "resource://services-settings/remote-settings.js"
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
    if (!Services.logins.searchLogins(matchData).length) {
      return;
    }
    Services.logins.removeLogin(login);
  });
  return login;
}

let EXPECTED_BREACH = null;
let EXPECTED_ERROR_MESSAGE = null;
add_task(async function setup() {
  const collection = await RemoteSettings(
    LoginBreaches.REMOTE_SETTINGS_COLLECTION
  ).openCollection();
  if (EXPECTED_BREACH) {
    await collection.create(EXPECTED_BREACH, {
      useRecordId: true,
    });
  }
  await collection.db.saveLastModified(42);
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
    if (msg.errorMessage.includes(EXPECTED_ERROR_MESSAGE)) {
      return;
    }
    ok(false, msg.message || msg.errorMessage);
  });

  registerCleanupFunction(async () => {
    EXPECTED_ERROR_MESSAGE = null;
    await collection.clear();
    SpecialPowers.postConsoleSentinel();
  });
});
