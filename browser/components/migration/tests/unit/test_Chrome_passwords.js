"use strict";

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

const PROFILE = {
  id: "Default",
  name: "Person 1",
};

const TEST_LOGINS = [
  {
    id: 1, // id of the row in the chrome login db
    username: "username",
    password: "password",
    origin: "https://c9.io",
    formActionOrigin: "https://c9.io",
    httpRealm: null,
    usernameField: "inputEmail",
    passwordField: "inputPassword",
    timeCreated: 1437418416037,
    timePasswordChanged: 1437418416037,
    timesUsed: 1,
  },
  {
    id: 2,
    username: "username@gmail.com",
    password: "password2",
    origin: "https://accounts.google.com",
    formActionOrigin: "https://accounts.google.com",
    httpRealm: null,
    usernameField: "Email",
    passwordField: "Passwd",
    timeCreated: 1437418446598,
    timePasswordChanged: 1437418446598,
    timesUsed: 6,
  },
  {
    id: 3,
    username: "username",
    password: "password3",
    origin: "https://www.facebook.com",
    formActionOrigin: "https://www.facebook.com",
    httpRealm: null,
    usernameField: "email",
    passwordField: "pass",
    timeCreated: 1437418478851,
    timePasswordChanged: 1437418478851,
    timesUsed: 1,
  },
  {
    id: 4,
    username: "user",
    password: "اقرأPÀßwörd",
    origin: "http://httpbin.org",
    formActionOrigin: null,
    httpRealm: "me@kennethreitz.com", // Digest auth.
    usernameField: "",
    passwordField: "",
    timeCreated: 1437787462368,
    timePasswordChanged: 1437787462368,
    timesUsed: 1,
  },
  {
    id: 5,
    username: "buser",
    password: "bpassword",
    origin: "http://httpbin.org",
    formActionOrigin: null,
    httpRealm: "Fake Realm", // Basic auth.
    usernameField: "",
    passwordField: "",
    timeCreated: 1437787539233,
    timePasswordChanged: 1437787539233,
    timesUsed: 1,
  },
  {
    id: 6,
    username: "username",
    password: "password6",
    origin: "https://www.example.com",
    formActionOrigin: "", // NULL `action_url`
    httpRealm: null,
    usernameField: "",
    passwordField: "pass",
    timeCreated: 1557291348878,
    timePasswordChanged: 1557291348878,
    timesUsed: 1,
  },
];

var loginCrypto;
var dbConn;

async function promiseSetPassword(login) {
  let encryptedString = await loginCrypto.encryptData(login.password);
  info(`promiseSetPassword: ${encryptedString}`);
  let passwordValue = new Uint8Array(
    loginCrypto.stringToArray(encryptedString)
  );
  return dbConn.execute(
    `UPDATE logins
                         SET password_value = :password_value
                         WHERE rowid = :rowid
                        `,
    { password_value: passwordValue, rowid: login.id }
  );
}

function checkLoginsAreEqual(passwordManagerLogin, chromeLogin, id) {
  passwordManagerLogin.QueryInterface(Ci.nsILoginMetaInfo);

  Assert.equal(
    passwordManagerLogin.username,
    chromeLogin.username,
    "The two logins ID " + id + " have the same username"
  );
  Assert.equal(
    passwordManagerLogin.password,
    chromeLogin.password,
    "The two logins ID " + id + " have the same password"
  );
  Assert.equal(
    passwordManagerLogin.origin,
    chromeLogin.origin,
    "The two logins ID " + id + " have the same origin"
  );
  Assert.equal(
    passwordManagerLogin.formActionOrigin,
    chromeLogin.formActionOrigin,
    "The two logins ID " + id + " have the same formActionOrigin"
  );
  Assert.equal(
    passwordManagerLogin.httpRealm,
    chromeLogin.httpRealm,
    "The two logins ID " + id + " have the same httpRealm"
  );
  Assert.equal(
    passwordManagerLogin.usernameField,
    chromeLogin.usernameField,
    "The two logins ID " + id + " have the same usernameElement"
  );
  Assert.equal(
    passwordManagerLogin.passwordField,
    chromeLogin.passwordField,
    "The two logins ID " + id + " have the same passwordElement"
  );
  Assert.equal(
    passwordManagerLogin.timeCreated,
    chromeLogin.timeCreated,
    "The two logins ID " + id + " have the same timeCreated"
  );
  Assert.equal(
    passwordManagerLogin.timePasswordChanged,
    chromeLogin.timePasswordChanged,
    "The two logins ID " + id + " have the same timePasswordChanged"
  );
  Assert.equal(
    passwordManagerLogin.timesUsed,
    chromeLogin.timesUsed,
    "The two logins ID " + id + " have the same timesUsed"
  );
}

function generateDifferentLogin(login) {
  let newLogin = Cc["@mozilla.org/login-manager/loginInfo;1"].createInstance(
    Ci.nsILoginInfo
  );

  newLogin.init(
    login.origin,
    login.formActionOrigin,
    null,
    login.username,
    login.password + 1,
    login.usernameField + 1,
    login.passwordField + 1
  );
  newLogin.QueryInterface(Ci.nsILoginMetaInfo);
  newLogin.timeCreated = login.timeCreated + 1;
  newLogin.timePasswordChanged = login.timePasswordChanged + 1;
  newLogin.timesUsed = login.timesUsed + 1;
  return newLogin;
}

add_task(async function setup() {
  let dirSvcPath;
  let pathId;
  let profilePathSegments;

  // Use a mock service and account name to avoid a Keychain auth. prompt that
  // would block the test from finishing if Chrome has already created a matching
  // Keychain entry. This allows us to still exercise the keychain lookup code.
  // The mock encryption passphrase is used when the Keychain item isn't found.
  let mockMacOSKeychain = {
    passphrase: "bW96aWxsYWZpcmVmb3g=",
    serviceName: "TESTING Chrome Safe Storage",
    accountName: "TESTING Chrome",
  };
  if (AppConstants.platform == "macosx") {
    let { ChromeMacOSLoginCrypto } = ChromeUtils.import(
      "resource:///modules/ChromeMacOSLoginCrypto.jsm"
    );
    loginCrypto = new ChromeMacOSLoginCrypto(
      mockMacOSKeychain.serviceName,
      mockMacOSKeychain.accountName,
      mockMacOSKeychain.passphrase
    );
    dirSvcPath = "Library/";
    pathId = "ULibDir";
    profilePathSegments = [
      "Application Support",
      "Google",
      "Chrome",
      "Default",
      "Login Data",
    ];
  } else if (AppConstants.platform == "win") {
    const { OSCrypto } = ChromeUtils.import(
      "resource://gre/modules/OSCrypto.jsm"
    );
    loginCrypto = new OSCrypto();
    dirSvcPath = "AppData/Local/";
    pathId = "LocalAppData";
    profilePathSegments = [
      "Google",
      "Chrome",
      "User Data",
      "Default",
      "Login Data",
    ];
  } else {
    throw new Error("Not implemented");
  }
  let dirSvcFile = do_get_file(dirSvcPath);
  registerFakePath(pathId, dirSvcFile);

  // We don't import osfile.jsm until after registering the fake path, because
  // importing osfile will sometimes greedily fetch certain path identifiers
  // from the dir service, which means they get cached, which means we can't
  // register a fake path for them anymore.
  const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");
  info(OS.Path.join(dirSvcFile.path, ...profilePathSegments));
  let loginDataFilePath = OS.Path.join(dirSvcFile.path, ...profilePathSegments);
  dbConn = await Sqlite.openConnection({ path: loginDataFilePath });

  if (AppConstants.platform == "macosx") {
    let migrator = await MigrationUtils.getMigrator("chrome");
    Object.assign(migrator.wrappedJSObject, {
      _keychainServiceName: mockMacOSKeychain.serviceName,
      _keychainAccountName: mockMacOSKeychain.accountName,
      _keychainMockPassphrase: mockMacOSKeychain.passphrase,
    });
  }

  registerCleanupFunction(() => {
    Services.logins.removeAllLogins();
    if (loginCrypto.finalize) {
      loginCrypto.finalize();
    }
    return dbConn.close();
  });
});

add_task(async function test_importIntoEmptyDB() {
  for (let login of TEST_LOGINS) {
    await promiseSetPassword(login);
  }

  let migrator = await MigrationUtils.getMigrator("chrome");
  Assert.ok(
    await migrator.isSourceAvailable(),
    "Sanity check the source exists"
  );

  let logins = Services.logins.getAllLogins();
  Assert.equal(logins.length, 0, "There are no logins initially");

  // Migrate the logins.
  await promiseMigration(
    migrator,
    MigrationUtils.resourceTypes.PASSWORDS,
    PROFILE
  );

  logins = Services.logins.getAllLogins();
  Assert.equal(
    logins.length,
    TEST_LOGINS.length,
    "Check login count after importing the data"
  );
  Assert.equal(
    logins.length,
    MigrationUtils._importQuantities.logins,
    "Check telemetry matches the actual import."
  );

  for (let i = 0; i < TEST_LOGINS.length; i++) {
    checkLoginsAreEqual(logins[i], TEST_LOGINS[i], i + 1);
  }
});

// Test that existing logins for the same primary key don't get overwritten
add_task(async function test_importExistingLogins() {
  let migrator = await MigrationUtils.getMigrator("chrome");
  Assert.ok(
    await migrator.isSourceAvailable(),
    "Sanity check the source exists"
  );

  Services.logins.removeAllLogins();
  let logins = Services.logins.getAllLogins();
  Assert.equal(
    logins.length,
    0,
    "There are no logins after removing all of them"
  );

  let newLogins = [];

  // Create 3 new logins that are different but where the key properties are still the same.
  for (let i = 0; i < 3; i++) {
    newLogins.push(generateDifferentLogin(TEST_LOGINS[i]));
    Services.logins.addLogin(newLogins[i]);
  }

  logins = Services.logins.getAllLogins();
  Assert.equal(
    logins.length,
    newLogins.length,
    "Check login count after the insertion"
  );

  for (let i = 0; i < newLogins.length; i++) {
    checkLoginsAreEqual(logins[i], newLogins[i], i + 1);
  }
  // Migrate the logins.
  await promiseMigration(
    migrator,
    MigrationUtils.resourceTypes.PASSWORDS,
    PROFILE
  );

  logins = Services.logins.getAllLogins();
  Assert.equal(
    logins.length,
    TEST_LOGINS.length,
    "Check there are still the same number of logins after re-importing the data"
  );
  Assert.equal(
    logins.length,
    MigrationUtils._importQuantities.logins,
    "Check telemetry matches the actual import."
  );

  for (let i = 0; i < newLogins.length; i++) {
    checkLoginsAreEqual(logins[i], newLogins[i], i + 1);
  }
});
