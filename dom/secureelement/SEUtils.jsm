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
};

this.EXPORTED_SYMBOLS = ["SEUtils"];
