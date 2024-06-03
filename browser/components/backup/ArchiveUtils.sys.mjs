/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This module expects to be able to load in both main-thread module contexts,
// as well as ChromeWorker contexts. Do not ChromeUtils.importESModule
// anything there at the top-level that's not compatible with both contexts.

export const ArchiveUtils = {
  /**
   * Convert an array containing only two bytes unsigned numbers to a base64
   * encoded string.
   *
   * @param {number[]} anArray
   *   The array that needs to be converted.
   * @returns {string}
   *   The string representation of the array.
   */
  arrayToBase64(anArray) {
    let result = "";
    let bytes = new Uint8Array(anArray);
    for (let i = 0; i < bytes.length; i++) {
      result += String.fromCharCode(bytes[i]);
    }
    return btoa(result);
  },

  /**
   * Convert a base64 encoded string to an Uint8Array.
   *
   * @param {string} base64Str
   *   The base64 encoded string that needs to be converted.
   * @returns {Uint8Array[]}
   *   The array representation of the string.
   */
  stringToArray(base64Str) {
    let binaryStr = atob(base64Str);
    let len = binaryStr.length;
    let bytes = new Uint8Array(len);
    for (let i = 0; i < len; i++) {
      bytes[i] = binaryStr.charCodeAt(i);
    }
    return bytes;
  },
};
