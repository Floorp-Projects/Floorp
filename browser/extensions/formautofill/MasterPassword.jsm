/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Helpers for the Master Password Dialog.
 * In the future the Master Password implementation may move here.
 */

"use strict";

var EXPORTED_SYMBOLS = [
  "MasterPassword",
];

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "cryptoSDR",
                                   "@mozilla.org/login-manager/crypto/SDR;1",
                                   Ci.nsILoginManagerCrypto);

var MasterPassword = {
  get _token() {
    let tokendb = Cc["@mozilla.org/security/pk11tokendb;1"].createInstance(Ci.nsIPK11TokenDB);
    return tokendb.getInternalKeyToken();
  },

  /**
   * @returns {boolean} True if a master password is set and false otherwise.
   */
  get isEnabled() {
    return this._token.hasPassword;
  },

  /**
   * @returns {boolean} True if master password is logged in and false if not.
   */
  get isLoggedIn() {
    return Services.logins.isLoggedIn;
  },

  /**
   * @returns {boolean} True if there is another master password login dialog
   *                    existing and false otherwise.
   */
  get isUIBusy() {
    return Services.logins.uiBusy;
  },

  /**
   * Ensure the master password is logged in. It will display the master password
   * login prompt or do nothing if it's logged in already. If an existing MP
   * prompt is already prompted, the result from it will be used instead.
   *
   * @param   {boolean} reauth Prompt the login dialog no matter it's logged in
   *                           or not if it's set to true.
   * @returns {Promise<boolean>} True if it's logged in or no password is set
   *                             and false if it's still not logged in (prompt
   *                             canceled or other error).
   */
  async ensureLoggedIn(reauth = false) {
    if (!this.isEnabled) {
      return true;
    }

    if (this.isLoggedIn && !reauth) {
      return true;
    }

    // If a prompt is already showing then wait for and focus it.
    if (this.isUIBusy) {
      return this.waitForExistingDialog();
    }

    let token = this._token;
    try {
      // 'true' means always prompt for token password. User will be prompted until
      // clicking 'Cancel' or entering the correct password.
      token.login(true);
    } catch (e) {
      // An exception will be thrown if the user cancels the login prompt dialog.
      // User is also logged out.
    }

    // If we triggered a master password prompt, notify observers.
    if (token.isLoggedIn()) {
      Services.obs.notifyObservers(null, "passwordmgr-crypto-login");
    } else {
      Services.obs.notifyObservers(null, "passwordmgr-crypto-loginCanceled");
    }

    return token.isLoggedIn();
  },

  /**
   * Decrypts cipherText.
   *
   * @param   {string} cipherText Encrypted string including the algorithm details.
   * @param   {boolean} reauth True if we want to force the prompt to show up
   *                    even if the user is already logged in.
   * @returns {Promise<string>} resolves to the decrypted string, or rejects otherwise.
   */
  async decrypt(cipherText, reauth = false) {
    if (!await this.ensureLoggedIn(reauth)) {
      throw Components.Exception("User canceled master password entry", Cr.NS_ERROR_ABORT);
    }
    return cryptoSDR.decrypt(cipherText);
  },

  /**
   * Decrypts cipherText synchronously. "ensureLoggedIn()" needs to be called
   * outside in case another dialog is showing.
   *
   * NOTE: This method will be removed soon once the FormAutofillStorage APIs are
   *       refactored to be async functions (bug 1399367). Please use async
   *       version instead.
   *
   * @deprecated
   * @param   {string} cipherText Encrypted string including the algorithm details.
   * @returns {string} The decrypted string.
   */
  decryptSync(cipherText) {
    if (this.isUIBusy) {
      throw Components.Exception("\"ensureLoggedIn()\" should be called first", Cr.NS_ERROR_UNEXPECTED);
    }
    return cryptoSDR.decrypt(cipherText);
  },

  /**
   * Encrypts a string and returns cipher text containing algorithm information used for decryption.
   *
   * @param   {string} plainText Original string without encryption.
   * @returns {Promise<string>} resolves to the encrypted string (with algorithm), otherwise rejects.
   */
  async encrypt(plainText) {
    if (!await this.ensureLoggedIn()) {
      throw Components.Exception("User canceled master password entry", Cr.NS_ERROR_ABORT);
    }

    return cryptoSDR.encrypt(plainText);
  },

  /**
   * Encrypts plainText synchronously. "ensureLoggedIn()" needs to be called
   * outside in case another dialog is showing.
   *
   * NOTE: This method will be removed soon once the FormAutofillStorage APIs are
   *       refactored to be async functions (bug 1399367). Please use async
   *       version instead.
   *
   * @deprecated
   * @param   {string} plainText A plain string to be encrypted.
   * @returns {string} The encrypted cipher string.
   */
  encryptSync(plainText) {
    if (this.isUIBusy) {
      throw Components.Exception("\"ensureLoggedIn()\" should be called first", Cr.NS_ERROR_UNEXPECTED);
    }
    return cryptoSDR.encrypt(plainText);
  },

  /**
   * Resolve when master password dialogs are closed, immediately if none are open.
   *
   * An existing MP dialog will be focused and will request attention.
   *
   * @returns {Promise<boolean>}
   *          Resolves with whether the user is logged in to MP.
   */
  async waitForExistingDialog() {
    if (!this.isUIBusy) {
      log.debug("waitForExistingDialog: Dialog isn't showing. isLoggedIn:", this.isLoggedIn);
      return this.isLoggedIn;
    }

    return new Promise((resolve) => {
      log.debug("waitForExistingDialog: Observing the open dialog");
      let observer = {
        QueryInterface: ChromeUtils.generateQI([
          Ci.nsIObserver,
          Ci.nsISupportsWeakReference,
        ]),

        observe(subject, topic, data) {
          log.debug("waitForExistingDialog: Got notification:", topic);
          // Only run observer once.
          Services.obs.removeObserver(this, "passwordmgr-crypto-login");
          Services.obs.removeObserver(this, "passwordmgr-crypto-loginCanceled");
          if (topic == "passwordmgr-crypto-loginCanceled") {
            resolve(false);
            return;
          }

          resolve(true);
        },
      };

      // Possible leak: it's possible that neither of these notifications
      // will fire, and if that happens, we'll leak the observer (and
      // never return). We should guarantee that at least one of these
      // will fire.
      // See bug XXX.
      Services.obs.addObserver(observer, "passwordmgr-crypto-login");
      Services.obs.addObserver(observer, "passwordmgr-crypto-loginCanceled");

      // Focus and draw attention to the existing master password dialog for the
      // occassions where it's not attached to the current window.
      let promptWin = Services.wm.getMostRecentWindow("prompt:promptPassword");
      promptWin.focus();
      promptWin.getAttention();
    });
  },
};

XPCOMUtils.defineLazyGetter(this, "log", () => {
  let ConsoleAPI = ChromeUtils.import("resource://gre/modules/Console.jsm", {}).ConsoleAPI;
  return new ConsoleAPI({
    maxLogLevelPref: "masterPassword.loglevel",
    prefix: "Master Password",
  });
});
