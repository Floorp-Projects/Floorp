Cu.import("resource://gre/modules/AppConstants.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "WindowsRegistry",
                                  "resource://gre/modules/WindowsRegistry.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OSCrypto",
                                  "resource://gre/modules/OSCrypto.jsm");

const CRYPT_PROTECT_UI_FORBIDDEN = 1;
const IE7_FORM_PASSWORDS_MIGRATOR_NAME = "IE7FormPasswords";
const LOGINS_KEY =  "Software\\Microsoft\\Internet Explorer\\IntelliForms\\Storage2";
const EXTENSION = "-backup";
const TESTED_WEBSITES = {
  twitter: {
    uri: makeURI("https://twitter.com"),
    hash: "A89D42BC6406E27265B1AD0782B6F376375764A301",
    data: [12, 0, 0, 0, 56, 0, 0, 0, 38, 0, 0, 0, 87, 73, 67, 75, 24, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 68, 36, 67, 124, 118, 212, 208, 1, 8, 0, 0, 0, 18, 0, 0, 0, 68, 36, 67, 124, 118, 212, 208, 1, 9, 0, 0, 0, 97, 0, 98, 0, 99, 0, 100, 0, 101, 0, 102, 0, 103, 0, 104, 0, 0, 0, 49, 0, 50, 0, 51, 0, 52, 0, 53, 0, 54, 0, 55, 0, 56, 0, 57, 0, 0, 0],
    logins: [
      {
        username: "abcdefgh",
        password: "123456789",
        hostname: "https://twitter.com",
        formSubmitURL: "",
        httpRealm: null,
        usernameField: "",
        passwordField: "",
        timeCreated: 1439325854000,
        timeLastUsed: 1439325854000,
        timePasswordChanged: 1439325854000,
        timesUsed: 1,
      },
    ],
  },
  facebook: {
    uri: makeURI("https://www.facebook.com/"),
    hash: "EF44D3E034009CB0FD1B1D81A1FF3F3335213BD796",
    data: [12, 0, 0, 0, 152, 0, 0, 0, 160, 0, 0, 0, 87, 73, 67, 75, 24, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 88, 182, 125, 18, 121, 212, 208, 1, 9, 0, 0, 0, 20, 0, 0, 0, 88, 182, 125, 18, 121, 212, 208, 1, 9, 0, 0, 0, 40, 0, 0, 0, 134, 65, 33, 37, 121, 212, 208, 1, 9, 0, 0, 0, 60, 0, 0, 0, 134, 65, 33, 37, 121, 212, 208, 1, 9, 0, 0, 0, 80, 0, 0, 0, 45, 242, 246, 62, 121, 212, 208, 1, 9, 0, 0, 0, 100, 0, 0, 0, 45, 242, 246, 62, 121, 212, 208, 1, 9, 0, 0, 0, 120, 0, 0, 0, 28, 10, 193, 80, 121, 212, 208, 1, 9, 0, 0, 0, 140, 0, 0, 0, 28, 10, 193, 80, 121, 212, 208, 1, 9, 0, 0, 0, 117, 0, 115, 0, 101, 0, 114, 0, 110, 0, 97, 0, 109, 0, 101, 0, 48, 0, 0, 0, 112, 0, 97, 0, 115, 0, 115, 0, 119, 0, 111, 0, 114, 0, 100, 0, 48, 0, 0, 0, 117, 0, 115, 0, 101, 0, 114, 0, 110, 0, 97, 0, 109, 0, 101, 0, 49, 0, 0, 0, 112, 0, 97, 0, 115, 0, 115, 0, 119, 0, 111, 0, 114, 0, 100, 0, 49, 0, 0, 0, 117, 0, 115, 0, 101, 0, 114, 0, 110, 0, 97, 0, 109, 0, 101, 0, 50, 0, 0, 0, 112, 0, 97, 0, 115, 0, 115, 0, 119, 0, 111, 0, 114, 0, 100, 0, 50, 0, 0, 0, 117, 0, 115, 0, 101, 0, 114, 0, 110, 0, 97, 0, 109, 0, 101, 0, 51, 0, 0, 0, 112, 0, 97, 0, 115, 0, 115, 0, 119, 0, 111, 0, 114, 0, 100, 0, 51, 0, 0, 0],
    logins: [
      {
        username: "username0",
        password: "password0",
        hostname: "https://www.facebook.com",
        formSubmitURL: "",
        httpRealm: null,
        usernameField: "",
        passwordField: "",
        timeCreated: 1439326966000,
        timeLastUsed: 1439326966000,
        timePasswordChanged: 1439326966000,
        timesUsed: 1,
     },
     {
        username: "username1",
        password: "password1",
        hostname: "https://www.facebook.com",
        formSubmitURL: "",
        httpRealm: null,
        usernameField: "",
        passwordField: "",
        timeCreated: 1439326997000,
        timeLastUsed: 1439326997000,
        timePasswordChanged: 1439326997000,
        timesUsed: 1,
      },
      {
        username: "username2",
        password: "password2",
        hostname: "https://www.facebook.com",
        formSubmitURL: "",
        httpRealm: null,
        usernameField: "",
        passwordField: "",
        timeCreated: 1439327040000,
        timeLastUsed: 1439327040000,
        timePasswordChanged: 1439327040000,
        timesUsed: 1,
      },
      {
        username: "username3",
        password: "password3",
        hostname: "https://www.facebook.com",
        formSubmitURL: "",
        httpRealm: null,
        usernameField: "",
        passwordField: "",
        timeCreated: 1439327070000,
        timeLastUsed: 1439327070000,
        timePasswordChanged: 1439327070000,
        timesUsed: 1,
      },
    ],
  },
  live: {
    uri: makeURI("https://login.live.com/"),
    hash: "7B506F2D6B81D939A8E0456F036EE8970856FF705E",
    data: [12, 0, 0, 0, 56, 0, 0, 0, 44, 0, 0, 0, 87, 73, 67, 75, 24, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 212, 17, 219, 140, 148, 212, 208, 1, 9, 0, 0, 0, 20, 0, 0, 0, 212, 17, 219, 140, 148, 212, 208, 1, 11, 0, 0, 0, 114, 0, 105, 0, 97, 0, 100, 0, 104, 0, 49, 6, 74, 6, 39, 6, 54, 6, 0, 0, 39, 6, 66, 6, 49, 6, 35, 6, 80, 0, 192, 0, 223, 0, 119, 0, 246, 0, 114, 0, 100, 0, 0, 0],
    logins: [
      {
        username: "riadhرياض",
        password: "اقرأPÀßwörd",
        hostname: "https://login.live.com",
        formSubmitURL: "",
        httpRealm: null,
        usernameField: "",
        passwordField: "",
        timeCreated: 1439338767000,
        timeLastUsed: 1439338767000,
        timePasswordChanged: 1439338767000,
        timesUsed: 1,
       },
    ],
  },
  reddit: {
    uri: makeURI("http://www.reddit.com/"),
    hash: "B644028D1C109A91EC2C4B9D1F145E55A1FAE42065",
    data: [12, 0, 0, 0, 152, 0, 0, 0, 212, 0, 0, 0, 87, 73, 67, 75, 24, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 32, 8, 234, 114, 153, 212, 208, 1, 1, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 0, 97, 93, 131, 116, 153, 212, 208, 1, 3, 0, 0, 0, 14, 0, 0, 0, 97, 93, 131, 116, 153, 212, 208, 1, 16, 0, 0, 0, 48, 0, 0, 0, 88, 150, 78, 174, 153, 212, 208, 1, 4, 0, 0, 0, 58, 0, 0, 0, 88, 150, 78, 174, 153, 212, 208, 1, 29, 0, 0, 0, 118, 0, 0, 0, 79, 102, 137, 34, 154, 212, 208, 1, 15, 0, 0, 0, 150, 0, 0, 0, 79, 102, 137, 34, 154, 212, 208, 1, 30, 0, 0, 0, 97, 0, 0, 0, 0, 0, 252, 140, 173, 138, 146, 48, 0, 0, 66, 0, 105, 0, 116, 0, 116, 0, 101, 0, 32, 0, 98, 0, 101, 0, 115, 0, 116, 0, 228, 0, 116, 0, 105, 0, 103, 0, 101, 0, 110, 0, 0, 0, 205, 145, 110, 127, 198, 91, 1, 120, 0, 0, 31, 4, 48, 4, 64, 4, 62, 4, 59, 4, 76, 4, 32, 0, 67, 4, 65, 4, 63, 4, 53, 4, 72, 4, 61, 4, 62, 4, 32, 0, 65, 4, 49, 4, 64, 4, 62, 4, 72, 4, 53, 4, 61, 4, 46, 0, 32, 0, 18, 4, 62, 4, 57, 4, 66, 4, 56, 4, 0, 0, 40, 6, 51, 6, 69, 6, 32, 0, 39, 6, 68, 6, 68, 6, 71, 6, 32, 0, 39, 6, 68, 6, 49, 6, 45, 6, 69, 6, 70, 6, 0, 0, 118, 0, 101, 0, 117, 0, 105, 0, 108, 0, 108, 0, 101, 0, 122, 0, 32, 0, 108, 0, 101, 0, 32, 0, 118, 0, 233, 0, 114, 0, 105, 0, 102, 0, 105, 0, 101, 0, 114, 0, 32, 0, 224, 0, 32, 0, 110, 0, 111, 0, 117, 0, 118, 0, 101, 0, 97, 0, 117, 0, 0, 0],
    logins: [
      {
        username: "購読を",
        password: "Bitte bestätigen",
        hostname: "http://www.reddit.com",
        formSubmitURL: "",
        httpRealm: null,
        usernameField: "",
        passwordField: "",
        timeCreated: 1439340874000,
        timeLastUsed: 1439340874000,
        timePasswordChanged: 1439340874000,
        timesUsed: 1,
      },
      {
        username: "重置密码",
        password: "Пароль успешно сброшен. Войти",
        hostname: "http://www.reddit.com",
        formSubmitURL: "",
        httpRealm: null,
        usernameField: "",
        passwordField: "",
        timeCreated: 1439340971000,
        timeLastUsed: 1439340971000,
        timePasswordChanged: 1439340971000,
        timesUsed: 1,
      },
      {
        username: "بسم الله الرحمن",
        password: "veuillez le vérifier à nouveau",
        hostname: "http://www.reddit.com",
        formSubmitURL: "",
        httpRealm: null,
        usernameField: "",
        passwordField: "",
        timeCreated: 1439341166000,
        timeLastUsed: 1439341166000,
        timePasswordChanged: 1439341166000,
        timesUsed: 1,
     },
    ],
  },
};

const TESTED_URLS = [
  "http://a.foo.com",
  "http://b.foo.com",
  "http://c.foo.com",
  "http://www.test.net",
  "http://www.test.net/home",
  "http://www.test.net/index",
  "https://a.bar.com",
  "https://b.bar.com",
  "https://c.bar.com",
];

var nsIWindowsRegKey = Ci.nsIWindowsRegKey;
var Storage2Key;

/*
 * If the key value exists, it's going to be backed up and replaced, so the value could be restored.
 * Otherwise a new value is going to be created.
 */
function backupAndStore(key, name, value) {
  if (key.hasValue(name)) {
    // backup the the current value
    let type = key.getValueType(name);
    // create a new value using use the current value name followed by EXTENSION as its new name
    switch (type) {
      case nsIWindowsRegKey.TYPE_STRING:
        key.writeStringValue(name + EXTENSION, key.readStringValue(name));
        break;
      case nsIWindowsRegKey.TYPE_BINARY:
        key.writeBinaryValue(name + EXTENSION, key.readBinaryValue(name));
        break;
      case nsIWindowsRegKey.TYPE_INT:
        key.writeIntValue(name + EXTENSION, key.readIntValue(name));
        break;
      case nsIWindowsRegKey.TYPE_INT64:
        key.writeInt64Value(name + EXTENSION, key.readInt64Value(name));
        break;
    }
  }
  key.writeBinaryValue(name, value);
}

// Remove all values where their names are members of the names array from the key of registry
function removeAllValues(key, names) {
  for (let name of names) {
    key.removeValue(name);
  }
}

// Restore all the backed up values
function restore(key) {
  let count = key.valueCount;
  let names = []; // the names of the key values
  for (let i = 0; i < count; ++i) {
    names.push(key.getValueName(i));
  }

  for (let name of names) {
    // backed up values have EXTENSION at the end of their names
    if (name.lastIndexOf(EXTENSION) == name.length - EXTENSION.length) {
      let valueName = name.substr(0, name.length - EXTENSION.length);
      let type = key.getValueType(name);
      // create a new value using the name before the backup and removed the backed up one
      switch (type) {
        case nsIWindowsRegKey.TYPE_STRING:
          key.writeStringValue(valueName, key.readStringValue(name));
          key.removeValue(name);
          break;
        case nsIWindowsRegKey.TYPE_BINARY:
          key.writeBinaryValue(valueName, key.readBinaryValue(name));
          key.removeValue(name);
          break;
        case nsIWindowsRegKey.TYPE_INT:
          key.writeIntValue(valueName, key.readIntValue(name));
          key.removeValue(name);
          break;
        case nsIWindowsRegKey.TYPE_INT64:
          key.writeInt64Value(valueName, key.readInt64Value(name));
          key.removeValue(name);
          break;
      }
    }
  }
}

function checkLoginsAreEqual(passwordManagerLogin, IELogin, id) {
  passwordManagerLogin.QueryInterface(Ci.nsILoginMetaInfo);
  for (let attribute in IELogin) {
    Assert.equal(passwordManagerLogin[attribute], IELogin[attribute],
                 "The two logins ID " + id + " have the same " + attribute);
  }
}

function createRegistryPath(path) {
  let loginPath = path.split("\\");
  let parentKey = Cc["@mozilla.org/windows-registry-key;1"].
                  createInstance(nsIWindowsRegKey);
  let currentPath =[];
  for (let currentKey of loginPath) {
    parentKey.open(nsIWindowsRegKey.ROOT_KEY_CURRENT_USER, currentPath.join("\\"),
                   nsIWindowsRegKey.ACCESS_ALL);

    if (!parentKey.hasChild(currentKey)) {
      parentKey.createChild(currentKey, 0);
    }
    currentPath.push(currentKey);
    parentKey.close();
  }
}

function getFirstResourceOfType(type) {
  let migrator = Cc["@mozilla.org/profile/migrator;1?app=browser&type=ie"]
                 .createInstance(Ci.nsISupports)
                 .wrappedJSObject;
  let migrators = migrator.getResources();
  for (let m of migrators) {
    if (m.name == IE7_FORM_PASSWORDS_MIGRATOR_NAME && m.type == type) {
      return m;
    }
  }
  throw new Error("failed to find the " + type + " migrator");
}

function makeURI(aURL) {
  return Services.io.newURI(aURL, null, null);
}

add_task(function* setup() {
  if (AppConstants.isPlatformAndVersionAtLeast("win", "6.2")) {
    Assert.throws(() => getFirstResourceOfType(MigrationUtils.resourceTypes.PASSWORDS),
                  "The migrator doesn't exist for win8+");
    return;
  }
  // create the path to Storage2 in the registry if it doest exist.
  createRegistryPath(LOGINS_KEY);
  Storage2Key = Cc["@mozilla.org/windows-registry-key;1"].
                createInstance(nsIWindowsRegKey);
  Storage2Key.open(nsIWindowsRegKey.ROOT_KEY_CURRENT_USER, LOGINS_KEY,
                   nsIWindowsRegKey.ACCESS_ALL);

  // create a dummy value otherwise the migrator doesn't exist
  if (!Storage2Key.hasValue("dummy")) {
    Storage2Key.writeBinaryValue("dummy", "dummy");
  }
});

add_task(function* test_passwordsNotAvailable() {
  if (AppConstants.isPlatformAndVersionAtLeast("win", "6.2")) {
    return;
  }

  let migrator = getFirstResourceOfType(MigrationUtils.resourceTypes.PASSWORDS);
  Assert.ok(migrator.exists, "The migrator has to exist");
  let logins = Services.logins.getAllLogins({});
  Assert.equal(logins.length, 0, "There are no logins at the beginning of the test");

  let uris = []; // the uris of the migrated logins
  for (let url of TESTED_URLS) {
    uris.push(makeURI(url));
     // in this test, there is no IE login data in the registry, so after the migration, the number
     // of logins in the store should be 0
    migrator._migrateURIs(uris);
    logins = Services.logins.getAllLogins({});
    Assert.equal(logins.length, 0,
                 "There are no logins after doing the migration without adding values to the registry");
  }
});

add_task(function* test_passwordsAvailable() {
  if (AppConstants.isPlatformAndVersionAtLeast("win", "6.2")) {
    return;
  }

  let crypto = new OSCrypto();
  let hashes = []; // the hashes of all migrator websites, this is going to be used for the clean up

  do_register_cleanup(() => {
    Services.logins.removeAllLogins();
    logins = Services.logins.getAllLogins({});
    Assert.equal(logins.length, 0, "There are no logins after the cleanup");
    //remove all the values created in this test from the registry
    removeAllValues(Storage2Key, hashes);
    // restore all backed up values
    restore(Storage2Key);

    // clean the dummy value
    if (Storage2Key.hasValue("dummy")) {
      Storage2Key.removeValue("dummy");
    }
    Storage2Key.close();
    crypto.finalize();
  });

  let migrator = getFirstResourceOfType(MigrationUtils.resourceTypes.PASSWORDS);
  Assert.ok(migrator.exists, "The migrator has to exist");
  let logins = Services.logins.getAllLogins({});
  Assert.equal(logins.length, 0, "There are no logins at the beginning of the test");

  let uris = []; // the uris of the migrated logins

  let loginCount = 0;
  for (let current in TESTED_WEBSITES) {
    let website = TESTED_WEBSITES[current];
    // backup the current the registry value if it exists and replace the existing value/create a
    // new value with the encrypted data
    backupAndStore(Storage2Key, website.hash,
                   crypto.encryptData(crypto.arrayToString(website.data),
                                      website.uri.spec, true));
    Assert.ok(migrator.exists, "The migrator has to exist");
    uris.push(website.uri);
    hashes.push(website.hash);

    migrator._migrateURIs(uris);
    logins = Services.logins.getAllLogins({});
    // check that the number of logins in the password manager has increased as expected which means
    // that all the values for the current website were imported
    loginCount += website.logins.length;
    Assert.equal(logins.length, loginCount,
                 "The number of logins has increased after the migration");
    let startIndex = loginCount - website.logins.length;
    // compares the imported password manager logins with their expected logins
    for (let i = 0; i < website.logins.length; i++) {
      checkLoginsAreEqual(logins[startIndex + i], website.logins[i],
                          " " + current + " - " + i + " ");
    }
  }
});
