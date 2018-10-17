/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Helpers for using OS Key Store.
 */

"use strict";

var EXPORTED_SYMBOLS = [
  "OSKeyStore",
];

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

ChromeUtils.defineModuleGetter(this, "AppConstants", "resource://gre/modules/AppConstants.jsm");
XPCOMUtils.defineLazyServiceGetter(this, "nativeOSKeyStore",
                                   "@mozilla.org/security/oskeystore;1", Ci.nsIOSKeyStore);

// Skip reauth during tests, only works in non-official builds.
const TEST_ONLY_REAUTH = "extensions.formautofill.osKeyStore.unofficialBuildOnlyLogin";

var OSKeyStore = {
  /**
   * On macOS this becomes part of the name label visible on Keychain Acesss as
   * "org.mozilla.nss.keystore.firefox" (where "firefox" is the MOZ_APP_NAME).
   */
  STORE_LABEL: AppConstants.MOZ_APP_NAME,

  /**
   * Consider the module is initialized as locked. OS might unlock without a
   * prompt.
   * @type {Boolean}
   */
  _isLocked: true,

  _pendingUnlockPromise: null,

  /**
   * @returns {boolean} Always considered enabled because OS store is always
   *                    protected via OS user login password.
   *                    TODO: Figure out if the affacted behaviors
   *                    (e.g. like bug 1486954 or confirming payment transaction)
   *                    is correct or not.
   */
  get isEnabled() {
    return true;
  },

  /**
   * @returns {boolean} True if logged in (i.e. decrypt(reauth = false) will
   *                    not retrigger a dialog) and false if not.
   *                    User might log out elsewhere in the OS, so even if this
   *                    is true a prompt might still pop up.
   */
  get isLoggedIn() {
    return !this._isLocked;
  },

  /**
   * @returns {boolean} True if there is another login dialog existing and false
   *                    otherwise.
   */
  get isUIBusy() {
    return !!this._pendingUnlockPromise;
  },

  /**
   * If the test pref exist and applicable,
   * this method will dispatch a observer message and return
   * to simulate successful reauth, or throw to simulate
   * failed reauth.
   *
   * @returns {boolean} True when reauth should NOT be skipped,
   *                    false when reauth has been skipped.
   * @throws            If it needs to simulate reauth login failure.
   */
  _maybeSkipReauthForTest() {
    // Don't take test reauth pref in the following configurations.
    if (nativeOSKeyStore.isNSSKeyStore ||
        AppConstants.MOZILLA_OFFICIAL ||
        !this._testReauth) {
      return true;
    }

    // Skip this reauth because there is no way to mock the
    // native dialog in the testing environment, for now.
    log.debug("_ensureReauth: _testReauth: ", this._testReauth);
    switch (this._testReauth) {
      case "pass":
        Services.obs.notifyObservers(null, "oskeystore-testonly-reauth", "pass");
        return false;
      case "cancel":
        Services.obs.notifyObservers(null, "oskeystore-testonly-reauth", "cancel");
        throw new Components.Exception("Simulating user cancelling login dialog", Cr.NS_ERROR_FAILURE);
      default:
        throw new Components.Exception("Unknown test pref value", Cr.NS_ERROR_FAILURE);
    }
  },

  /**
   * Ensure the store in use is logged in. It will display the OS login
   * login prompt or do nothing if it's logged in already. If an existing login
   * prompt is already prompted, the result from it will be used instead.
   *
   * Note: This method must set _pendingUnlockPromise before returning the
   * promise (i.e. the first |await|), otherwise we'll risk re-entry.
   * This is why there aren't an |await| in the method. The method is marked as
   * |async| to communicate that it's async.
   *
   * @param   {boolean} reauth Prompt the login dialog no matter it's logged in
   *                           or not if it's set to true.
   * @returns {Promise<boolean>} True if it's logged in or no password is set
   *                             and false if it's still not logged in (prompt
   *                             canceled or other error).
   */
  async ensureLoggedIn(reauth = false) {
    if (this._pendingUnlockPromise) {
      log.debug("ensureLoggedIn: Has a pending unlock operation");
      return this._pendingUnlockPromise;
    }
    log.debug("ensureLoggedIn: Creating new pending unlock promise. reauth: ", reauth);

    // TODO: Implementing re-auth by passing this value to the native implementation
    // in some way. Set this to false for now to ignore the reauth request (bug 1429265).
    reauth = false;

    let unlockPromise = Promise.resolve().then(async () => {
      if (reauth) {
        reauth = this._maybeSkipReauthForTest();
      }

      if (!await nativeOSKeyStore.asyncSecretAvailable(this.STORE_LABEL)) {
        log.debug("ensureLoggedIn: Secret unavailable, attempt to generate new secret.");
        let recoveryPhrase = await nativeOSKeyStore.asyncGenerateSecret(this.STORE_LABEL);
        // TODO We should somehow have a dialog to ask the user to write this down,
        // and another dialog somewhere for the user to restore the secret with it.
        // (Intentionally not printing it out in the console)
        log.debug("ensureLoggedIn: Secret generated. Recovery phrase length: " + recoveryPhrase.length);
      }
    });

    if (nativeOSKeyStore.isNSSKeyStore) {
      // Workaround bug 1492305: NSS-implemented methods don't reject when user cancels.
      unlockPromise = unlockPromise.then(() => {
        log.debug("ensureLoggedIn: isNSSKeyStore: ", reauth, Services.logins.isLoggedIn);
        // User has hit the cancel button on the master password prompt.
        // We must reject the promise chain here.
        if (!Services.logins.isLoggedIn) {
          throw Components.Exception("User canceled OS unlock entry (Workaround)", Cr.NS_ERROR_FAILURE);
        }
      });
    }

    unlockPromise = unlockPromise.then(() => {
      log.debug("ensureLoggedIn: Logged in");
      this._pendingUnlockPromise = null;
      this._isLocked = false;

      return true;
    }, (err) => {
      log.debug("ensureLoggedIn: Not logged in", err);
      this._pendingUnlockPromise = null;
      this._isLocked = true;

      return false;
    });

    this._pendingUnlockPromise = unlockPromise;

    return this._pendingUnlockPromise;
  },

  /**
   * Decrypts cipherText.
   *
   * Note: In the event of an rejection, check the result property of the Exception
   *       object. Handles NS_ERROR_ABORT as user has cancelled the action (e.g.,
   *       don't show that dialog), apart from other errors (e.g., gracefully
   *       recover from that and still shows the dialog.)
   *
   * @param   {string} cipherText Encrypted string including the algorithm details.
   * @param   {boolean} reauth True if we want to force the prompt to show up
   *                    even if the user is already logged in.
   * @returns {Promise<string>} resolves to the decrypted string, or rejects otherwise.
   */
  async decrypt(cipherText, reauth = false) {
    if (!await this.ensureLoggedIn(reauth)) {
      throw Components.Exception("User canceled OS unlock entry", Cr.NS_ERROR_ABORT);
    }
    let bytes = await nativeOSKeyStore.asyncDecryptBytes(this.STORE_LABEL, cipherText);
    return String.fromCharCode.apply(String, bytes);
  },

  /**
   * Encrypts a string and returns cipher text containing algorithm information used for decryption.
   *
   * @param   {string} plainText Original string without encryption.
   * @returns {Promise<string>} resolves to the encrypted string (with algorithm), otherwise rejects.
   */
  async encrypt(plainText) {
    if (!await this.ensureLoggedIn()) {
      throw Components.Exception("User canceled OS unlock entry", Cr.NS_ERROR_ABORT);
    }

    // Convert plain text into a UTF-8 binary string
    plainText = unescape(encodeURIComponent(plainText));

    // Convert it to an array
    let textArr = [];
    for (let char of plainText) {
      textArr.push(char.charCodeAt(0));
    }

    let rawEncryptedText = await nativeOSKeyStore.asyncEncryptBytes(this.STORE_LABEL, textArr.length, textArr);

    // Mark the output with a version number.
    return rawEncryptedText;
  },

  /**
   * Resolve when the login dialogs are closed, immediately if none are open.
   *
   * An existing MP dialog will be focused and will request attention.
   *
   * @returns {Promise<boolean>}
   *          Resolves with whether the user is logged in to MP.
   */
  async waitForExistingDialog() {
    if (this.isUIBusy) {
      return this._pendingUnlockPromise;
    }
    return this.isLoggedIn;
  },

  /**
   * Remove the store. For tests.
   */
  async cleanup() {
    return nativeOSKeyStore.asyncDeleteSecret(this.STORE_LABEL);
  },

  /**
   * Check if the implementation is using the NSS key store.
   * If so, tests will be able to handle the reauth dialog.
   */
  get isNSSKeyStore() {
    return nativeOSKeyStore.isNSSKeyStore;
  },
};

XPCOMUtils.defineLazyGetter(this, "log", () => {
  let ConsoleAPI = ChromeUtils.import("resource://gre/modules/Console.jsm", {}).ConsoleAPI;
  return new ConsoleAPI({
    maxLogLevelPref: "extensions.formautofill.loglevel",
    prefix: "OSKeyStore",
  });
});

XPCOMUtils.defineLazyPreferenceGetter(OSKeyStore, "_testReauth", TEST_ONLY_REAUTH, "");
