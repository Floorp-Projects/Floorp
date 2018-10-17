/**
 * Tests of OSKeyStore.jsm
 */

"use strict";

ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
ChromeUtils.import("resource://testing-common/MockRegistrar.jsm");

let OSKeyStore;
add_task(async function setup() {
  ({OSKeyStore} = ChromeUtils.import("resource://formautofill/OSKeyStore.jsm", {}));
});

// Ensure that the appropriate initialization has happened.
do_get_profile();

// For NSS key store, mocking out the dialog and control it from here.
let gMockPrompter = {
  passwordToTry: "hunter2",
  resolve: null,
  login: undefined,

  // This intentionally does not use arrow function syntax to avoid an issue
  // where in the context of the arrow function, |this != gMockPrompter| due to
  // how objects get wrapped when going across xpcom boundaries.
  promptPassword(dialogTitle, text, password, checkMsg, checkValue) {
    equal(text,
          "Please enter your master password.",
          "password prompt text should be as expected");
    equal(checkMsg, null, "checkMsg should be null");
    if (this.login) {
      password.value = this.passwordToTry;
    }
    this.resolve();
    this.resolve = null;

    return this.login;
  },

  QueryInterface: ChromeUtils.generateQI([Ci.nsIPrompt]),
};

// Mock nsIWindowWatcher. PSM calls getNewPrompter on this to get an nsIPrompt
// to call promptPassword. We return the mock one, above.
let gWindowWatcher = {
  getNewPrompter: () => gMockPrompter,
  QueryInterface: ChromeUtils.generateQI([Ci.nsIWindowWatcher]),
};

let nssToken;

const TEST_ONLY_REAUTH = "extensions.formautofill.osKeyStore.unofficialBuildOnlyLogin";

async function waitForReauth(login = false) {
  if (OSKeyStore.isNSSKeyStore) {
    gMockPrompter.login = login;
    await new Promise(resolve => { gMockPrompter.resolve = resolve; });

    return;
  }

  let value = login ? "pass" : "cancel";
  Services.prefs.setStringPref(TEST_ONLY_REAUTH, value);
  await TestUtils.topicObserved("oskeystore-testonly-reauth",
    (subject, data) => data == value);
}

const testText = "test string";
let cipherText;

add_task(async function test_encrypt_decrypt() {
  Assert.equal(await OSKeyStore.ensureLoggedIn(), true, "Started logged in.");

  cipherText = await OSKeyStore.encrypt(testText);
  Assert.notEqual(testText, cipherText);

  let plainText = await OSKeyStore.decrypt(cipherText);
  Assert.equal(testText, plainText);
});

// TODO: skipped because re-auth is not implemented (bug 1429265).
add_task(async function test_reauth() {
  let canTest = OSKeyStore.isNSSKeyStore || !AppConstants.MOZILLA_OFFICIAL;
  if (!canTest) {
    todo_check_false(canTest,
      "test_reauth: Cannot test OS key store login on official builds.");
    return;
  }

  if (OSKeyStore.isNSSKeyStore) {
    let windowWatcherCID;
    windowWatcherCID =
      MockRegistrar.register("@mozilla.org/embedcomp/window-watcher;1",
                             gWindowWatcher);
    registerCleanupFunction(() => {
      MockRegistrar.unregister(windowWatcherCID);
    });

    // If we use the NSS key store implementation test that everything works
    // when a master password is set.
    // Set an initial password.
    let tokenDB = Cc["@mozilla.org/security/pk11tokendb;1"]
                    .getService(Ci.nsIPK11TokenDB);
    nssToken = tokenDB.getInternalKeyToken();
    nssToken.initPassword("hunter2");
  }

  let reauthObserved = waitForReauth(false);
  await new Promise(resolve => TestUtils.executeSoon(resolve));
  try {
    await OSKeyStore.decrypt(cipherText, true);
    throw new Error("Not receiving canceled OS unlock error");
  } catch (ex) {
    Assert.equal(ex.message, "User canceled OS unlock entry");
    Assert.equal(ex.result, Cr.NS_ERROR_ABORT);
  }
  await reauthObserved;

  reauthObserved = waitForReauth(false);
  await new Promise(resolve => TestUtils.executeSoon(resolve));
  Assert.equal(await OSKeyStore.ensureLoggedIn(true), false, "Reauth cancelled.");
  await reauthObserved;

  reauthObserved = waitForReauth(true);
  await new Promise(resolve => TestUtils.executeSoon(resolve));
  let plainText2 = await OSKeyStore.decrypt(cipherText, true);
  await reauthObserved;
  Assert.equal(testText, plainText2);

  reauthObserved = waitForReauth(true);
  await new Promise(resolve => TestUtils.executeSoon(resolve));
  Assert.equal(await OSKeyStore.ensureLoggedIn(true), true, "Reauth logged in.");
  await reauthObserved;
}).skip();

add_task(async function test_decryption_failure() {
  try {
    await OSKeyStore.decrypt("Malformed cipher text");
    throw new Error("Not receiving decryption error");
  } catch (ex) {
    Assert.notEqual(ex.result, Cr.NS_ERROR_ABORT);
  }
});
