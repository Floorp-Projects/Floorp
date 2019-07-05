/**
 * Tests of OSKeyStore.jsm
 */

"use strict";

let OSKeyStore;
add_task(async function setup() {
  Services.prefs.setBoolPref("extensions.formautofill.reauth.enabled", true);

  ({ OSKeyStore } = ChromeUtils.import(
    "resource://formautofill/OSKeyStore.jsm"
  ));
});

// Ensure that the appropriate initialization has happened.
do_get_profile();

const testText = "test string";
let cipherText;

add_task(async function test_encrypt_decrypt() {
  Assert.equal(await OSKeyStore.ensureLoggedIn(), true, "Started logged in.");

  cipherText = await OSKeyStore.encrypt(testText);
  Assert.notEqual(testText, cipherText);

  let plainText = await OSKeyStore.decrypt(cipherText);
  Assert.equal(testText, plainText);
});

add_task(async function test_reauth() {
  let canTest = OSKeyStoreTestUtils.canTestOSKeyStoreLogin();
  if (!canTest) {
    todo_check_true(
      canTest,
      "test_reauth: Cannot test OS key store login on official builds."
    );
    return;
  }

  let reauthObserved = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(false);
  await new Promise(resolve => TestUtils.executeSoon(resolve));
  try {
    await OSKeyStore.decrypt(cipherText, true);
    throw new Error("Not receiving canceled OS unlock error");
  } catch (ex) {
    Assert.equal(ex.message, "User canceled OS unlock entry");
    Assert.equal(ex.result, Cr.NS_ERROR_ABORT);
  }
  await reauthObserved;

  reauthObserved = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(false);
  await new Promise(resolve => TestUtils.executeSoon(resolve));
  Assert.equal(
    await OSKeyStore.ensureLoggedIn(true),
    false,
    "Reauth cancelled."
  );
  await reauthObserved;

  reauthObserved = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(true);
  await new Promise(resolve => TestUtils.executeSoon(resolve));
  let plainText2 = await OSKeyStore.decrypt(cipherText, true);
  await reauthObserved;
  Assert.equal(testText, plainText2);

  reauthObserved = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(true);
  await new Promise(resolve => TestUtils.executeSoon(resolve));
  Assert.equal(
    await OSKeyStore.ensureLoggedIn(true),
    true,
    "Reauth logged in."
  );
  await reauthObserved;

  Services.prefs.setBoolPref("extensions.formautofill.reauth.enabled", false);
  Assert.equal(
    await OSKeyStore.ensureLoggedIn(true),
    true,
    "Reauth disabled so logged in without prompt"
  );
  Services.prefs.setBoolPref("extensions.formautofill.reauth.enabled", true);
});

add_task(async function test_decryption_failure() {
  try {
    await OSKeyStore.decrypt("Malformed cipher text");
    throw new Error("Not receiving decryption error");
  } catch (ex) {
    Assert.notEqual(ex.result, Cr.NS_ERROR_ABORT);
  }
});
