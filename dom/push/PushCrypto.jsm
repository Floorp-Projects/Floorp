/* jshint moz: true, esnext: true */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

const Cu = Components.utils;

Cu.importGlobalProperties(['crypto']);

this.EXPORTED_SYMBOLS = ['PushCrypto', 'concatArray',
                         'getCryptoParams',
                         'base64UrlDecode'];

var UTF8 = new TextEncoder('utf-8');
var ENCRYPT_INFO = UTF8.encode('Content-Encoding: aesgcm128');
var NONCE_INFO = UTF8.encode('Content-Encoding: nonce');
var AUTH_INFO = UTF8.encode('Content-Encoding: auth\0'); // note nul-terminus
var P256DH_INFO = UTF8.encode('P-256\0');
var ECDH_KEY = { name: 'ECDH', namedCurve: 'P-256' };
// A default keyid with a name that won't conflict with a real keyid.
var DEFAULT_KEYID = '';

function getEncryptionKeyParams(encryptKeyField) {
  if (!encryptKeyField) {
    return null;
  }
  var params = encryptKeyField.split(',');
  return params.reduce((m, p) => {
    var pmap = p.split(';').reduce(parseHeaderFieldParams, {});
    if (pmap.keyid && pmap.dh) {
      m[pmap.keyid] = pmap.dh;
    }
    if (!m[DEFAULT_KEYID] && pmap.dh) {
      m[DEFAULT_KEYID] = pmap.dh;
    }
    return m;
  }, {});
}

function getEncryptionParams(encryptField) {
  var p = encryptField.split(',', 1)[0];
  if (!p) {
    return null;
  }
  return p.split(';').reduce(parseHeaderFieldParams, {});
}

this.getCryptoParams = function(headers) {
  if (!headers) {
    return null;
  }

  var requiresAuthenticationSecret = true;
  var keymap = getEncryptionKeyParams(headers.crypto_key);
  if (!keymap) {
    requiresAuthenticationSecret = false;
    keymap = getEncryptionKeyParams(headers.encryption_key);
    if (!keymap) {
      return null;
    }
  }
  var enc = getEncryptionParams(headers.encryption);
  if (!enc) {
    return null;
  }
  var dh = keymap[enc.keyid || DEFAULT_KEYID];
  var salt = enc.salt;
  var rs = (enc.rs)? parseInt(enc.rs, 10) : 4096;

  if (!dh || !salt || isNaN(rs) || (rs <= 1)) {
    return null;
  }
  return {dh, salt, rs, auth: requiresAuthenticationSecret};
}

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

hkdf.prototype.extract = function(info, len) {
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

/* generate a 96-bit nonce for use in GCM, 48-bits of which are populated */
function generateNonce(base, index) {
  if (index >= Math.pow(2, 48)) {
    throw new Error('Error generating nonce - index is too large.');
  }
  var nonce = base.slice(0, 12);
  nonce = new Uint8Array(nonce);
  for (var i = 0; i < 6; ++i) {
    nonce[nonce.byteLength - 1 - i] ^= (index / Math.pow(256, i)) & 0xff;
  }
  return nonce;
}

this.PushCrypto = {

  generateAuthenticationSecret() {
    return crypto.getRandomValues(new Uint8Array(12));
  },

  generateKeys() {
    return crypto.subtle.generateKey(ECDH_KEY, true, ['deriveBits'])
      .then(cryptoKey =>
         Promise.all([
           crypto.subtle.exportKey('raw', cryptoKey.publicKey),
           crypto.subtle.exportKey('jwk', cryptoKey.privateKey)
         ]));
  },

  decodeMsg(aData, aPrivateKey, aPublicKey, aSenderPublicKey,
            aSalt, aRs, aAuthenticationSecret) {

    if (aData.byteLength === 0) {
      // Zero length messages will be passed as null.
      return Promise.resolve(null);
    }

    // The last chunk of data must be less than aRs, if it is not return an
    // error.
    if (aData.byteLength % (aRs + 16) === 0) {
      return Promise.reject(new Error('Data truncated'));
    }

    let senderKey = base64UrlDecode(aSenderPublicKey)
    return Promise.all([
      crypto.subtle.importKey('raw', senderKey, ECDH_KEY,
                              false, ['deriveBits']),
      crypto.subtle.importKey('jwk', aPrivateKey, ECDH_KEY,
                              false, ['deriveBits'])
    ])
    .then(([appServerKey, subscriptionPrivateKey]) =>
          crypto.subtle.deriveBits({ name: 'ECDH', public: appServerKey },
                                   subscriptionPrivateKey, 256))
    .then(ikm => this._deriveKeyAndNonce(new Uint8Array(ikm),
                                         base64UrlDecode(aSalt),
                                         aPublicKey,
                                         senderKey,
                                         aAuthenticationSecret))
    .then(r =>
      // AEAD_AES_128_GCM expands ciphertext to be 16 octets longer.
      Promise.all(chunkArray(aData, aRs + 16).map((slice, index) =>
        this._decodeChunk(slice, index, r[1], r[0]))))
    .then(r => concatArray(r));
  },

  _deriveKeyAndNonce(ikm, salt, receiverKey, senderKey, authenticationSecret) {
    var kdfPromise;
    var context;
    // The authenticationSecret, when present, is mixed with the ikm using HKDF.
    // This is its primary purpose.  However, since the authentication secret
    // was added at the same time that the info string was changed, we also use
    // its presence to change how the final info string is calculated:
    //
    // 1. When there is no authenticationSecret, the context string is simply
    // "Content-Encoding: <blah>". This corresponds to old, deprecated versions
    // of the content encoding.  This should eventually be removed: bug 1230038.
    //
    // 2. When there is an authenticationSecret, the context string is:
    // "Content-Encoding: <blah>\0P-256\0" then the length and value of both the
    // receiver key and sender key.
    if (authenticationSecret) {
      // Since we are using an authentication secret, we need to run an extra
      // round of HKDF with the authentication secret as salt.
      var authKdf = new hkdf(authenticationSecret, ikm);
      kdfPromise = authKdf.extract(AUTH_INFO, 32)
        .then(ikm2 => new hkdf(salt, ikm2));

      // We also use the presence of the authentication secret to indicate that
      // we have extra context to add to the info parameter.
      context = concatArray([
        new Uint8Array([0]), P256DH_INFO,
        this._encodeLength(receiverKey), receiverKey,
        this._encodeLength(senderKey), senderKey
      ]);
    } else {
      kdfPromise = Promise.resolve(new hkdf(salt, ikm));
      context = new Uint8Array(0);
    }
    return kdfPromise.then(kdf => Promise.all([
      kdf.extract(concatArray([ENCRYPT_INFO, context]), 16)
        .then(gcmBits => crypto.subtle.importKey('raw', gcmBits, 'AES-GCM', false,
                                                 ['decrypt'])),
      kdf.extract(concatArray([NONCE_INFO, context]), 12)
    ]));
  },

  _encodeLength(buffer) {
    return new Uint8Array([0, buffer.byteLength]);
  },

  _decodeChunk(aSlice, aIndex, aNonce, aKey) {
    let params = {
      name: 'AES-GCM',
      iv: generateNonce(aNonce, aIndex)
    };
    return crypto.subtle.decrypt(params, aKey, aSlice)
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
