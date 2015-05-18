/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Copyright © 2015, Deutsche Telekom, Inc. */

"use strict";

this.SEUtils = {
  byteArrayToHexString: function byteArrayToHexString(array) {
    let hexStr = "";

    let len = array ? array.length : 0;
    for (let i = 0; i < len; i++) {
      let hex = (array[i] & 0xff).toString(16);
      hex = (hex.length === 1) ? "0" + hex : hex;
      hexStr += hex;
    }

    return hexStr.toUpperCase();
  },

  hexStringToByteArray: function hexStringToByteArray(hexStr) {
    if (typeof hexStr !== "string" || hexStr.length % 2 !== 0) {
      return [];
    }

    let array = [];
    for (let i = 0, len = hexStr.length; i < len; i += 2) {
      array.push(parseInt(hexStr.substr(i, 2), 16));
    }

    return array;
  },

  arraysEqual: function arraysEqual(a1, a2) {
    if (!a1 || !a2) {
      return false;
    }

    if (a1.length !== a2.length) {
      return false;
    }

    for (let i = 0, len = a1.length; i < len; i++) {
      if (a1[i] !== a2[i]) {
        return false;
      }
    }

    return true;
  },

  ensureIsArray: function ensureIsArray(obj) {
    return Array.isArray(obj) ? obj : [obj];
  },

  /**
   * parseTLV is intended primarily to be used to parse Global Platform Device
   * Technology secure element access control data.
   *
   * The parsed result value is an internal format only.
   *
   * All tags will be treated as simple Tag Length Values (TLV), (i.e. with a
   * plain value, not subject to further unpacking), unless those tags are
   * listed in the containerTags array.
   *
   * @param bytes - byte array
   * @param containerTags - byte array of tags
   */
  parseTLV: function parseTLV(bytes, containerTags) {
    let result = {};

    if (typeof bytes === "string") {
      bytes = this.hexStringToByteArray(bytes);
    }

    if (!Array.isArray(bytes)) {
      debug("Passed value is not an array nor a string.");
      return null;
    }

    for (let pos = 0; pos < bytes.length; ) {
      let tag = bytes[pos],
          length = bytes[pos + 1],
          value = bytes.slice(pos + 2, pos + 2 + length),
          parsed = null;

      // Support for 0xFF padded files (GPD 7.1.2)
      if (tag === 0xFF) {
        break;
      }

      if (containerTags.indexOf(tag) >= 0) {
        parsed = this.parseTLV(value, containerTags);
      } else {
        parsed = value;
      }

      // Internal parsed format.
      if (!result[tag]) {
        result[tag] = parsed;
      } else if (Array.isArray(result[tag])) {
        result[tag].push(parsed);
      } else {
        result[tag] = [result[tag], parsed];
      }

      pos = pos + 2 + length;
    }

    return result;
  }
};

this.EXPORTED_SYMBOLS = ["SEUtils"];
