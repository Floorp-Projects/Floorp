/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Class to handle encryption and decryption of logins stored in Chrome/Chromium
 * on macOS.
 */

var EXPORTED_SYMBOLS = ["ChromeMacOSLoginCrypto"];

Cu.importGlobalProperties(["crypto"]);

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gKeychainUtils",
  "@mozilla.org/profile/migrator/keychainmigrationutils;1",
  "nsIKeychainMigrationUtils"
);

const gTextEncoder = new TextEncoder();
const gTextDecoder = new TextDecoder();

/**
 * From macOS' CommonCrypto/CommonCryptor.h
 */
const kCCBlockSizeAES128 = 16;

/* Chromium constants */

/**
 * kSalt from Chromium.
 * @see https://cs.chromium.org/chromium/src/components/os_crypt/os_crypt_mac.mm?l=43&rcl=1771751f87e3e99bb6cd67b5d0e159ae487f8db0
 */
const SALT = "saltysalt";

/**
 * kDerivedKeySizeInBits from Chromium.
 * @see https://cs.chromium.org/chromium/src/components/os_crypt/os_crypt_mac.mm?l=46&rcl=1771751f87e3e99bb6cd67b5d0e159ae487f8db0
 */
const DERIVED_KEY_SIZE_BITS = 128;

/**
 * kEncryptionIterations from Chromium.
 * @see https://cs.chromium.org/chromium/src/components/os_crypt/os_crypt_mac.mm?l=49&rcl=1771751f87e3e99bb6cd67b5d0e159ae487f8db0
 */
const ITERATIONS = 1003;

/**
 * kEncryptionVersionPrefix from Chromium.
 * @see https://cs.chromium.org/chromium/src/components/os_crypt/os_crypt_mac.mm?l=61&rcl=1771751f87e3e99bb6cd67b5d0e159ae487f8db0
 */
const ENCRYPTION_VERSION_PREFIX = "v10";

/**
 * The initialization vector is 16 space characters (character code 32 in decimal).
 * @see https://cs.chromium.org/chromium/src/components/os_crypt/os_crypt_mac.mm?l=220&rcl=1771751f87e3e99bb6cd67b5d0e159ae487f8db0
 */
const IV = new Uint8Array(kCCBlockSizeAES128).fill(32);

/**
 * Instances of this class have a shape similar to OSCrypto so it can be dropped
 * into code which uses that. This isn't implemented as OSCrypto_mac.js since
 * it isn't calling into encryption functions provided by macOS but instead
 * relies on OS encryption key storage in Keychain. The algorithms here are
 * specific to what is needed for Chrome login storage on macOS.
 */
class ChromeMacOSLoginCrypto {
  /**
   * @param {string} serviceName of the Keychain Item to use to derive a key.
   * @param {string} accountName of the Keychain Item to use to derive a key.
   * @param {string?} [testingPassphrase = null] A string to use as the passphrase
   *                  to derive a key for testing purposes rather than retrieving
   *                  it from the macOS Keychain since we don't yet have a way to
   *                  mock the Keychain auth dialog.
   */
  constructor(serviceName, accountName, testingPassphrase = null) {
    // We still exercise the keychain migration utils code when using a
    // `testingPassphrase` in order to get some test coverage for that
    // component, even though it's expected to throw since a login item with the
    // service name and account name usually won't be found.
    let encKey = testingPassphrase;
    try {
      encKey = lazy.gKeychainUtils.getGenericPassword(serviceName, accountName);
    } catch (ex) {
      if (!testingPassphrase) {
        throw ex;
      }
    }

    this.ALGORITHM = "AES-CBC";

    this._keyPromise = crypto.subtle
      .importKey("raw", gTextEncoder.encode(encKey), "PBKDF2", false, [
        "deriveKey",
      ])
      .then(key => {
        return crypto.subtle.deriveKey(
          {
            name: "PBKDF2",
            salt: gTextEncoder.encode(SALT),
            iterations: ITERATIONS,
            hash: "SHA-1",
          },
          key,
          { name: this.ALGORITHM, length: DERIVED_KEY_SIZE_BITS },
          false,
          ["decrypt", "encrypt"]
        );
      })
      .catch(Cu.reportError);
  }

  /**
   * Convert an array containing only two bytes unsigned numbers to a string.
   * @param {number[]} arr - the array that needs to be converted.
   * @returns {string} the string representation of the array.
   */
  arrayToString(arr) {
    let str = "";
    for (let i = 0; i < arr.length; i++) {
      str += String.fromCharCode(arr[i]);
    }
    return str;
  }

  stringToArray(binary_string) {
    let len = binary_string.length;
    let bytes = new Uint8Array(len);
    for (var i = 0; i < len; i++) {
      bytes[i] = binary_string.charCodeAt(i);
    }
    return bytes;
  }

  /**
   * @param {array} ciphertextArray ciphertext prefixed by the encryption version
   *                            (see ENCRYPTION_VERSION_PREFIX).
   * @returns {string} plaintext password
   */
  async decryptData(ciphertextArray) {
    let ciphertext = this.arrayToString(ciphertextArray);
    if (!ciphertext.startsWith(ENCRYPTION_VERSION_PREFIX)) {
      throw new Error("Unknown encryption version");
    }
    let key = await this._keyPromise;
    if (!key) {
      throw new Error("Cannot decrypt without a key");
    }
    let plaintext = await crypto.subtle.decrypt(
      { name: this.ALGORITHM, iv: IV },
      key,
      this.stringToArray(ciphertext.substring(ENCRYPTION_VERSION_PREFIX.length))
    );
    return gTextDecoder.decode(plaintext);
  }

  /**
   * @param {USVString} plaintext to encrypt
   * @returns {string} encrypted string consisting of UTF-16 code units prefixed
   *                   by the ENCRYPTION_VERSION_PREFIX.
   */
  async encryptData(plaintext) {
    let key = await this._keyPromise;
    if (!key) {
      throw new Error("Cannot encrypt without a key");
    }

    let ciphertext = await crypto.subtle.encrypt(
      { name: this.ALGORITHM, iv: IV },
      key,
      gTextEncoder.encode(plaintext)
    );
    return (
      ENCRYPTION_VERSION_PREFIX +
      String.fromCharCode(...new Uint8Array(ciphertext))
    );
  }
}
