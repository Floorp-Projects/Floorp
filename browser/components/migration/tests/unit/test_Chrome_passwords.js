"use strict";

Cu.import("resource://gre/modules/OSCrypto.jsm");
Cu.import("resource://gre/modules/Services.jsm");

const PROFILE = {
  id: "Default",
  name: "Person 1",
};

const TEST_LOGINS = [
  {
    id: 1, // id of the row in the chrome login db
    username: "username",
    password: "password",
    hostname: "https://c9.io",
    formSubmitURL: "https://c9.io",
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
    hostname: "https://accounts.google.com",
    formSubmitURL: "https://accounts.google.com",
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
    hostname: "https://www.facebook.com",
    formSubmitURL: "https://www.facebook.com",
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
    password: "password",
    hostname: "http://httpbin.org",
    formSubmitURL: null,
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
    hostname: "http://httpbin.org",
    formSubmitURL: null,
    httpRealm: "Fake Realm", // Basic auth.
    usernameField: "",
    passwordField: "",
    timeCreated: 1437787539233,
    timePasswordChanged: 1437787539233,
    timesUsed: 1,
  },
];

var crypto = new OSCrypto();
var dbConn;

function promiseSetPassword(login) {
  return new Promise((resolve, reject) => {
    let stmt = dbConn.createAsyncStatement(`
      UPDATE logins
      SET password_value = :password_value
      WHERE rowid = :rowid
    `);
    let passwordValue = crypto.stringToArray(crypto.encryptData(login.password));
    stmt.bindBlobByName("password_value", passwordValue, passwordValue.length);
    stmt.params.rowid = login.id;

    stmt.executeAsync({
      handleError(aError) {
        reject("Error with the query: " + aError.message);
      },

      handleCompletion(aReason) {
        if (aReason === Ci.mozIStorageStatementCallback.REASON_FINISHED) {
          resolve();
        } else {
          reject("Query has failed: " + aReason);
        }
      },
    });
    stmt.finalize();
  });
}

function checkLoginsAreEqual(passwordManagerLogin, chromeLogin, id) {
  passwordManagerLogin.QueryInterface(Ci.nsILoginMetaInfo);

  Assert.equal(passwordManagerLogin.username, chromeLogin.username,
               "The two logins ID " + id + " have the same username");
  Assert.equal(passwordManagerLogin.password, chromeLogin.password,
               "The two logins ID " + id + " have the same password");
  Assert.equal(passwordManagerLogin.hostname, chromeLogin.hostname,
               "The two logins ID " + id + " have the same hostname");
  Assert.equal(passwordManagerLogin.formSubmitURL, chromeLogin.formSubmitURL,
               "The two logins ID " + id + " have the same formSubmitURL");
  Assert.equal(passwordManagerLogin.httpRealm, chromeLogin.httpRealm,
               "The two logins ID " + id + " have the same httpRealm");
  Assert.equal(passwordManagerLogin.usernameField, chromeLogin.usernameField,
               "The two logins ID " + id + " have the same usernameElement");
  Assert.equal(passwordManagerLogin.passwordField, chromeLogin.passwordField,
               "The two logins ID " + id + " have the same passwordElement");
  Assert.equal(passwordManagerLogin.timeCreated, chromeLogin.timeCreated,
               "The two logins ID " + id + " have the same timeCreated");
  Assert.equal(passwordManagerLogin.timePasswordChanged, chromeLogin.timePasswordChanged,
               "The two logins ID " + id + " have the same timePasswordChanged");
  Assert.equal(passwordManagerLogin.timesUsed, chromeLogin.timesUsed,
               "The two logins ID " + id + " have the same timesUsed");
}

function generateDifferentLogin(login) {
  let newLogin = Cc["@mozilla.org/login-manager/loginInfo;1"].createInstance(Ci.nsILoginInfo);

  newLogin.init(login.hostname, login.formSubmitURL, null,
                login.username, login.password + 1, login.usernameField + 1,
                login.passwordField + 1);
  newLogin.QueryInterface(Ci.nsILoginMetaInfo);
  newLogin.timeCreated = login.timeCreated + 1;
  newLogin.timePasswordChanged = login.timePasswordChanged + 1;
  newLogin.timesUsed = login.timesUsed + 1;
  return newLogin;
}

add_task(function* setup() {
  let loginDataFile = do_get_file("AppData/Local/Google/Chrome/User Data/Default/Login Data");
  dbConn = Services.storage.openUnsharedDatabase(loginDataFile);
  registerFakePath("LocalAppData", do_get_file("AppData/Local/"));

  do_register_cleanup(() => {
    Services.logins.removeAllLogins();
    dbConn.asyncClose();
    crypto.finalize();
  });
});

add_task(function* test_importIntoEmptyDB() {
  for (let login of TEST_LOGINS) {
    yield promiseSetPassword(login);
  }

  let migrator = MigrationUtils.getMigrator("chrome");
  Assert.ok(migrator.sourceExists, "Sanity check the source exists");

  let logins = Services.logins.getAllLogins({});
  Assert.equal(logins.length, 0, "There are no logins initially");

  // Migrate the logins.
  yield promiseMigration(migrator, MigrationUtils.resourceTypes.PASSWORDS, PROFILE);

  logins = Services.logins.getAllLogins({});
  Assert.equal(logins.length, TEST_LOGINS.length, "Check login count after importing the data");
  Assert.equal(logins.length, MigrationUtils._importQuantities.logins,
               "Check telemetry matches the actual import.");

  for (let i = 0; i < TEST_LOGINS.length; i++) {
    checkLoginsAreEqual(logins[i], TEST_LOGINS[i], i + 1);
  }
});

// Test that existing logins for the same primary key don't get overwritten
add_task(function* test_importExistingLogins() {
  let migrator = MigrationUtils.getMigrator("chrome");
  Assert.ok(migrator.sourceExists, "Sanity check the source exists");

  Services.logins.removeAllLogins();
  let logins = Services.logins.getAllLogins({});
  Assert.equal(logins.length, 0, "There are no logins after removing all of them");

  let newLogins = [];

  // Create 3 new logins that are different but where the key properties are still the same.
  for (let i = 0; i < 3; i++) {
    newLogins.push(generateDifferentLogin(TEST_LOGINS[i]));
    Services.logins.addLogin(newLogins[i]);
  }

  logins = Services.logins.getAllLogins({});
  Assert.equal(logins.length, newLogins.length, "Check login count after the insertion");

  for (let i = 0; i < newLogins.length; i++) {
    checkLoginsAreEqual(logins[i], newLogins[i], i + 1);
  }
  // Migrate the logins.
  yield promiseMigration(migrator, MigrationUtils.resourceTypes.PASSWORDS, PROFILE);

  logins = Services.logins.getAllLogins({});
  Assert.equal(logins.length, TEST_LOGINS.length,
               "Check there are still the same number of logins after re-importing the data");
  Assert.equal(logins.length, MigrationUtils._importQuantities.logins,
               "Check telemetry matches the actual import.");

  for (let i = 0; i < newLogins.length; i++) {
    checkLoginsAreEqual(logins[i], newLogins[i], i + 1);
  }
});
