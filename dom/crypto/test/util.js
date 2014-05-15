/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var util = {
  // Compare the contents of two ArrayBufferViews
  memcmp: function util_memcmp(x, y) {
    if (!x || !y) { return false; }

    if (x.byteLength !== y.byteLength) { return false; }

    var xb = new Uint8Array(x.buffer, x.byteOffset, x.byteLength);
    var yb = new Uint8Array(y.buffer, y.byteOffset, y.byteLength);
    for (var i=0; i<xb.byteLength; ++i) {
      if (xb[i] !== yb[i]) {
        return false;
      }
    }
    return true;
  },

  // Convert an ArrayBufferView to a hex string
  abv2hex: function util_abv2hex(abv) {
    var b = new Uint8Array(abv.buffer, abv.byteOffset, abv.byteLength);
    var hex = "";
    for (var i=0; i <b.length; ++i) {
      var zeropad = (b[i] < 0x10) ? "0" : "";
      hex += zeropad + b[i].toString(16);
    }
    return hex;
  },

  // Convert a hex string to an ArrayBufferView
  hex2abv: function util_hex2abv(hex) {
    if (hex.length % 2 !== 0) {
      hex = "0" + hex;
    }

    var abv = new Uint8Array(hex.length / 2);
    for (var i=0; i<abv.length; ++i) {
      abv[i] = parseInt(hex.substr(2*i, 2), 16);
    }
    return abv;
  },
};
