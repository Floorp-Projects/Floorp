/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var util = {
  // Compare the contents of two ArrayBuffer(View)s
  memcmp: function util_memcmp(x, y) {
    if (!x || !y) { return false; }

    var xb = new Uint8Array(x);
    var yb = new Uint8Array(y);
    if (x.byteLength !== y.byteLength) { return false; }

    for (var i=0; i<xb.byteLength; ++i) {
      if (xb[i] !== yb[i]) {
        return false;
      }
    }
    return true;
  },

  // Convert an ArrayBufferView to a hex string
  abv2hex: function util_abv2hex(abv) {
    var b = new Uint8Array(abv);
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

  clone: function (obj) {
    return new Promise(resolve => {
      let {port1, port2} = new MessageChannel();

      // Wait for the cloned object to arrive.
      port1.onmessage = msg => resolve(msg.data);

      // Clone the object.
      port2.postMessage(obj);
    });
  },

  cloneExportCompareKeys: function (key) {
    return util.clone(key).then(clone => {
      var exports = [];

      if (key instanceof CryptoKey) {
        exports.push(crypto.subtle.exportKey("raw", key));
        exports.push(crypto.subtle.exportKey("raw", clone));
      } else {
        exports.push(crypto.subtle.exportKey("spki", key.publicKey));
        exports.push(crypto.subtle.exportKey("spki", clone.publicKey));
        exports.push(crypto.subtle.exportKey("pkcs8", key.privateKey));
        exports.push(crypto.subtle.exportKey("pkcs8", clone.privateKey));
      }

      return Promise.all(exports).then(pairs => {
        for (var i = 0; i < pairs.length; i += 2) {
          if (!util.memcmp(pairs[i], pairs[i + 1])) {
            throw new Error("keys don't match");
          }
        }

        return clone;
      });
    });
  }
};

function exists(x) {
  return (x !== undefined);
}

function hasFields(object, fields) {
  return fields
          .map(x => exists(object[x]))
          .reduce((x,y) => (x && y));
}

function hasKeyFields(x) {
  return hasFields(x, ["algorithm", "extractable", "type", "usages"]);
}

function hasBaseJwkFields(x) {
  return hasFields(x, ["kty", "alg", "ext", "key_ops"]);
}

function shallowArrayEquals(x, y) {
  if (x.length != y.length) {
    return false;
  }

  for (i in x) {
    if (x[i] != y[i]) {
      return false;
    }
  }

  return true;
}
