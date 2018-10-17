/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var EXPORTED_SYMBOLS = [
  "OSKeyStoreTestUtils",
];

ChromeUtils.import("resource://formautofill/OSKeyStore.jsm", this);
// TODO: Consider AppConstants.MOZILLA_OFFICIAL to decide if we could test re-auth (bug 1429265).
/*
ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
*/
ChromeUtils.import("resource://testing-common/LoginTestUtils.jsm", this);
ChromeUtils.import("resource://testing-common/TestUtils.jsm");

var OSKeyStoreTestUtils = {
  /*
  TEST_ONLY_REAUTH: "extensions.formautofill.osKeyStore.unofficialBuildOnlyLogin",
  */

  setup() {
    // TODO: run tests with master password enabled to ensure NSS-implemented
    // key store prompts on re-auth (bug 1429265)
    /*
    LoginTestUtils.masterPassword.enable();
    */

    this.ORIGINAL_STORE_LABEL = OSKeyStore.STORE_LABEL;
    OSKeyStore.STORE_LABEL = "test-" + Math.random().toString(36).substr(2);
  },

  async cleanup() {
    // TODO: run tests with master password enabled to ensure NSS-implemented
    // key store prompts on re-auth (bug 1429265)
    /*
    LoginTestUtils.masterPassword.disable();
    */

    await OSKeyStore.cleanup();
    OSKeyStore.STORE_LABEL = this.ORIGINAL_STORE_LABEL;
  },

  canTestOSKeyStoreLogin() {
    // TODO: return true based on whether or not we could test the prompt on
    // the platform (bug 1429265).
    /*
    return OSKeyStore.isNSSKeyStore || !AppConstants.MOZILLA_OFFICIAL;
    */
    return true;
  },

  // Wait for the master password dialog to popup and enter the password to log in
  // if "login" is "true" or dismiss it directly if otherwise.
  async waitForOSKeyStoreLogin(login = false) {
    // TODO: Always resolves for now, because we are skipping re-auth on all
    // platforms (bug 1429265).
    /*
    if (OSKeyStore.isNSSKeyStore) {
      await this.waitForMasterPasswordDialog(login);
      return;
    }

    const str = login ? "pass" : "cancel";

    Services.prefs.setStringPref(this.TEST_ONLY_REAUTH, str);

    await TestUtils.topicObserved("oskeystore-testonly-reauth",
      (subject, data) => data == str);

    Services.prefs.setStringPref(this.TEST_ONLY_REAUTH, "");
    */
  },

  async waitForMasterPasswordDialog(login = false) {
    let [subject] = await TestUtils.topicObserved("common-dialog-loaded");

    let dialog = subject.Dialog;
    if (dialog.args.title !== "Password Required") {
      throw new Error("Incorrect master password dialog title");
    }

    if (login) {
      dialog.ui.password1Textbox.value = LoginTestUtils.masterPassword.masterPassword;
      dialog.ui.button0.click();
    } else {
      dialog.ui.button1.click();
    }
    await TestUtils.waitForTick();
  },
};
