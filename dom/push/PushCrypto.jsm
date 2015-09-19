/* jshint moz: true, esnext: true */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

const Cu = Components.utils;

Cu.importGlobalProperties(['crypto']);

this.EXPORTED_SYMBOLS = ['PushCrypto', 'concatArray',
                         'getEncryptionKeyParams', 'getEncryptionParams',
                         'base64UrlDecode'];

var ENCRYPT_INFO = new TextEncoder('utf-8').encode('Content-Encoding: aesgcm128');
var NONCE_INFO = new TextEncoder('utf-8').encode('Content-Encoding: nonce');

this.getEncryptionKeyParams = function(encryptKeyField) {
  var params = encryptKeyField.split(',');
  return params.reduce((m, p) => {
    var pmap = p.split(';').reduce(parseHeaderFieldParams, {});
    if (pmap.keyid && pmap.dh) {
      m[pmap.keyid] = pmap.dh;
    }
    return m;
  }, {});
};

this.getEncryptionParams = function(encryptField) {
  var p = encryptField.split(',', 1)[0];
  if (!p) {
    return null;
  }
  return p.split(';').reduce(parseHeaderFieldParams, {});
};

var parseHeaderFieldParams = (m, v) => {
  var i = v.indexOf('=');
  if (i >= 0) {
    // A quoted string with internal quotes is invalid for all the possible
    // values of this header field.
    m[v.substring(0, i).trim()] = v.substring(i + 1).trim()
                                    .replace(/^"(.*)"$/, '$1');
  }
  return m;
};

function chunkArray(array, size) {
  var start = array.byteOffset || 0;
  array = array.buffer || array;
  var index = 0;
  var result = [];
  while(index + size <= array.byteLength) {
    result.push(new Uint8Array(array, start + index, size));
    index += size;
  }
  if (index < array.byteLength) {
    result.push(new Uint8Array(array, start + index));
  }
  return result;
}

this.base64UrlDecode = function(s) {
  s = s.replace(/-/g, '+').replace(/_/g, '/');

  // Replace padding if it was stripped by the sender.
  // See http://tools.ietf.org/html/rfc4648#section-4
  switch (s.length % 4) {
    case 0:
      break; // No pad chars in this case
    case 2:
      s += '==';
      break; // Two pad chars
    case 3:
      s += '=';
      break; // One pad char
    default:
      throw new Error('Illegal base64url string!');
  }

  // With correct padding restored, apply the standard base64 decoder
  var decoded = atob(s);

  var array = new Uint8Array(new ArrayBuffer(decoded.length));
  for (var i = 0; i < decoded.length; i++) {
    array[i] = decoded.charCodeAt(i);
  }
  return array;
};

this.concatArray = function(arrays) {
  var size = arrays.reduce((total, a) => total + a.byteLength, 0);
  var index = 0;
  return arrays.reduce((result, a) => {
    result.set(new Uint8Array(a), index);
    index += a.byteLength;
    return result;
  }, new Uint8Array(size));
};

var HMAC_SHA256 = { name: 'HMAC', hash: 'SHA-256' };

function hmac(key) {
  this.keyPromise = crypto.subtle.importKey('raw', key, HMAC_SHA256,
                                            false, ['sign']);
}

hmac.prototype.hash = function(input) {
  return this.keyPromise.then(k => crypto.subtle.sign('HMAC', k, input));
};

function hkdf(salt, ikm) {
  this.prkhPromise = new hmac(salt).hash(ikm)
    .then(prk => new hmac(prk));
}

hkdf.prototype.generate = function(info, len) {
  var input = concatArray([info, new Uint8Array([1])]);
  return this.prkhPromise
    .then(prkh => prkh.hash(input))
    .then(h => {
      if (h.byteLength < len) {
        throw new Error('Length is too long');
      }
      return h.slice(0, len);
    });
};

/* generate a 96-bit IV for use in GCM, 48-bits of which are populated */
function generateNonce(base, index) {
  if (index >= Math.pow(2, 48)) {
    throw new Error('Error generating IV - index is too large.');
  }
  var nonce = base.slice(0, 12);
  nonce = new Uint8Array(nonce);
  for (var i = 0; i < 6; ++i) {
    nonce[nonce.byteLength - 1 - i] ^= (index / Math.pow(256, i)) & 0xff;
  }
  return nonce;
}

this.PushCrypto = {

  generateKeys: function() {
    return crypto.subtle.generateKey({ name: 'ECDH', namedCurve: 'P-256'},
                                     true,
                                     ['deriveBits'])
      .then(cryptoKey =>
         Promise.all([
           crypto.subtle.exportKey('raw', cryptoKey.publicKey),
           // TODO: change this when bug 1048931 lands.
           crypto.subtle.exportKey('jwk', cryptoKey.privateKey)
         ]));
  },

  decodeMsg: function(aData, aPrivateKey, aRemotePublicKey, aSalt, aRs) {

    if (aData.byteLength === 0) {
      // Zero length messages will be passed as null.
      return Promise.resolve(null);
    }

    // The last chunk of data must be less than aRs, if it is not return an
    // error.
    if (aData.byteLength % (aRs + 16) === 0) {
      return Promise.reject(new Error('Data truncated'));
    }

    return Promise.all([
      crypto.subtle.importKey('raw', base64UrlDecode(aRemotePublicKey),
                              { name: 'ECDH', namedCurve: 'P-256' },
                              false,
                              ['deriveBits']),
      crypto.subtle.importKey('jwk', aPrivateKey,
                              { name: 'ECDH', namedCurve: 'P-256' },
                              false,
                              ['deriveBits'])
    ])
    .then(keys =>
      crypto.subtle.deriveBits({ name: 'ECDH', public: keys[0] }, keys[1], 256))
    .then(rawKey => {
      var kdf = new hkdf(base64UrlDecode(aSalt), new Uint8Array(rawKey));
      return Promise.all([
        kdf.generate(ENCRYPT_INFO, 16)
          .then(gcmBits =>
                crypto.subtle.importKey('raw', gcmBits, 'AES-GCM', false,
                                        ['decrypt'])),
        kdf.generate(NONCE_INFO, 12)
      ])
    })
    .then(r =>
      // AEAD_AES_128_GCM expands ciphertext to be 16 octets longer.
      Promise.all(chunkArray(aData, aRs + 16).map((slice, index) =>
        this._decodeChunk(slice, index, r[1], r[0]))))
    .then(r => concatArray(r));
  },

  _decodeChunk: function(aSlice, aIndex, aNonce, aKey) {
    return crypto.subtle.decrypt({name: 'AES-GCM',
                                  iv: generateNonce(aNonce, aIndex)
                                 },
                                 aKey, aSlice)
      .then(decoded => {
        decoded = new Uint8Array(decoded);
        if (decoded.length == 0) {
          return Promise.reject(new Error('Decoded array is too short!'));
        } else if (decoded[0] > decoded.length) {
          return Promise.reject(new Error ('Padding is wrong!'));
        } else {
          // All padded bytes must be zero except the first one.
          for (var i = 1; i <= decoded[0]; i++) {
            if (decoded[i] != 0) {
              return Promise.reject(new Error('Padding is wrong!'));
            }
          }
          return decoded.slice(decoded[0] + 1);
        }
      });
  }
};
