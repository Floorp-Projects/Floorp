/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Class to handle encryption and decryption of logins stored in Chrome/Chromium
 * on Windows.
 */

var EXPORTED_SYMBOLS = ["ChromeWindowsLoginCrypto"];

Cu.importGlobalProperties(["atob", "crypto"]);

const { ChromeMigrationUtils } = ChromeUtils.import(
  "resource:///modules/ChromeMigrationUtils.jsm"
);
const { OSCrypto } = ChromeUtils.import("resource://gre/modules/OSCrypto.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

/**
 * These constants should match those from Chromium.
 * @see https://source.chromium.org/chromium/chromium/src/+/master:components/os_crypt/os_crypt_win.cc
 */
const AEAD_KEY_LENGTH = 256 / 8;
const ALGORITHM_NAME = "AES-GCM";
const DPAPI_KEY_PREFIX = "DPAPI";
const ENCRYPTION_VERSION_PREFIX = "v10";
const NONCE_LENGTH = 96 / 8;

const gTextDecoder = new TextDecoder();
const gTextEncoder = new TextEncoder();

/**
 * Instances of this class have a shape similar to OSCrypto so it can be dropped
 * into code which uses that. The algorithms here are
 * specific to what is needed for Chrome login storage on Windows.
 */
class ChromeWindowsLoginCrypto {
  /**
   * @param {string} userDataPathSuffix
   */
  constructor(userDataPathSuffix) {
    this.osCrypto = new OSCrypto();

    // Lazily decrypt the key from "Chrome"s local state using OSCrypto and save
    // it as the master key to decrypt or encrypt passwords.
    XPCOMUtils.defineLazyGetter(this, "_keyPromise", async () => {
      let keyData;
      try {
        // NB: For testing, allow directory service to be faked before getting.
        const localState = await ChromeMigrationUtils.getLocalState(
          userDataPathSuffix
        );
        const withHeader = atob(localState.os_crypt.encrypted_key);
        if (!withHeader.startsWith(DPAPI_KEY_PREFIX)) {
          throw new Error("Invalid key format");
        }
        const encryptedKey = withHeader.slice(DPAPI_KEY_PREFIX.length);
        keyData = this.osCrypto.decryptData(encryptedKey, null, "bytes");
      } catch (ex) {
        Cu.reportError(`${userDataPathSuffix} os_crypt key: ${ex}`);

        // Use a generic key that will fail for actually encrypted data, but for
        // testing it'll be consistent for both encrypting and decrypting.
        keyData = AEAD_KEY_LENGTH;
      }
      return crypto.subtle.importKey(
        "raw",
        new Uint8Array(keyData),
        ALGORITHM_NAME,
        false,
        ["decrypt", "encrypt"]
      );
    });
  }

  /**
   * Must be invoked once after last use of any of the provided helpers.
   */
  finalize() {
    this.osCrypto.finalize();
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
    const len = binary_string.length;
    const bytes = new Uint8Array(len);
    for (let i = 0; i < len; i++) {
      bytes[i] = binary_string.charCodeAt(i);
    }
    return bytes;
  }

  /**
   * @param {string} ciphertext ciphertext optionally prefixed by the encryption version
   *                            (see ENCRYPTION_VERSION_PREFIX).
   * @returns {string} plaintext password
   */
  async decryptData(ciphertext) {
    const ciphertextString = this.arrayToString(ciphertext);
    return ciphertextString.startsWith(ENCRYPTION_VERSION_PREFIX)
      ? this._decryptV10(ciphertext)
      : this._decryptUnversioned(ciphertextString);
  }

  async _decryptUnversioned(ciphertext) {
    return this.osCrypto.decryptData(ciphertext);
  }

  async _decryptV10(ciphertext) {
    const key = await this._keyPromise;
    if (!key) {
      throw new Error("Cannot decrypt without a key");
    }

    // Split the nonce/iv from the rest of the encrypted value and decrypt.
    const nonceIndex = ENCRYPTION_VERSION_PREFIX.length;
    const cipherIndex = nonceIndex + NONCE_LENGTH;
    const iv = new Uint8Array(ciphertext.slice(nonceIndex, cipherIndex));
    const algorithm = {
      name: ALGORITHM_NAME,
      iv,
    };
    const cipherArray = new Uint8Array(ciphertext.slice(cipherIndex));
    const plaintext = await crypto.subtle.decrypt(algorithm, key, cipherArray);
    return gTextDecoder.decode(new Uint8Array(plaintext));
  }

  /**
   * @param {USVString} plaintext to encrypt
   * @param {?string} version to encrypt default unversioned
   * @returns {string} encrypted string consisting of UTF-16 code units prefixed
   *                   by the ENCRYPTION_VERSION_PREFIX.
   */
  async encryptData(plaintext, version = undefined) {
    return version === ENCRYPTION_VERSION_PREFIX
      ? this._encryptV10(plaintext)
      : this._encryptUnversioned(plaintext);
  }

  async _encryptUnversioned(plaintext) {
    return this.osCrypto.encryptData(plaintext);
  }

  async _encryptV10(plaintext) {
    const key = await this._keyPromise;
    if (!key) {
      throw new Error("Cannot encrypt without a key");
    }

    // Encrypt and concatenate the prefix, nonce/iv and encrypted value.
    const iv = crypto.getRandomValues(new Uint8Array(NONCE_LENGTH));
    const algorithm = {
      name: ALGORITHM_NAME,
      iv,
    };
    const plainArray = gTextEncoder.encode(plaintext);
    const ciphertext = await crypto.subtle.encrypt(algorithm, key, plainArray);
    return (
      ENCRYPTION_VERSION_PREFIX +
      this.arrayToString(iv) +
      this.arrayToString(new Uint8Array(ciphertext))
    );
  }
}
