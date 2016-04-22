/* jshint moz: true, esnext: true */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

const Cu = Components.utils;

Cu.importGlobalProperties(['crypto']);

this.EXPORTED_SYMBOLS = ['PushCrypto', 'concatArray',
                         'getCryptoParams'];

var UTF8 = new TextEncoder('utf-8');

// Legacy encryption scheme (draft-thomson-http-encryption-02).
var AESGCM128_ENCODING = 'aesgcm128';
var AESGCM128_ENCRYPT_INFO = UTF8.encode('Content-Encoding: aesgcm128');

// New encryption scheme (draft-ietf-httpbis-encryption-encoding-01).
var AESGCM_ENCODING = 'aesgcm';
var AESGCM_ENCRYPT_INFO = UTF8.encode('Content-Encoding: aesgcm');

var NONCE_INFO = UTF8.encode('Content-Encoding: nonce');
var AUTH_INFO = UTF8.encode('Content-Encoding: auth\0'); // note nul-terminus
var P256DH_INFO = UTF8.encode('P-256\0');
var ECDH_KEY = { name: 'ECDH', namedCurve: 'P-256' };
var ECDSA_KEY =  { name: 'ECDSA', namedCurve: 'P-256' };
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

  var keymap;
  var padSize;
  if (headers.encoding == AESGCM_ENCODING) {
    // aesgcm uses the Crypto-Key header, 2 bytes for the pad length, and an
    // authentication secret.
    // https://tools.ietf.org/html/draft-ietf-httpbis-encryption-encoding-01
    keymap = getEncryptionKeyParams(headers.crypto_key);
    padSize = 2;
  } else if (headers.encoding == AESGCM128_ENCODING) {
    // aesgcm128 uses Encryption-Key, 1 byte for the pad length, and no secret.
    // https://tools.ietf.org/html/draft-thomson-http-encryption-02
    keymap = getEncryptionKeyParams(headers.encryption_key);
    padSize = 1;
  }
  if (!keymap) {
    return null;
  }

  var enc = getEncryptionParams(headers.encryption);
  if (!enc) {
    return null;
  }
  var dh = keymap[enc.keyid || DEFAULT_KEYID];
  var salt = enc.salt;
  var rs = (enc.rs)? parseInt(enc.rs, 10) : 4096;

  if (!dh || !salt || isNaN(rs) || (rs <= padSize)) {
    return null;
  }
  return {dh, salt, rs, padSize};
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
    return crypto.getRandomValues(new Uint8Array(16));
  },

  validateAppServerKey(key) {
    return crypto.subtle.importKey('raw', key, ECDSA_KEY,
                                   true, ['verify'])
      .then(_ => key);
  },

  generateKeys() {
    return crypto.subtle.generateKey(ECDH_KEY, true, ['deriveBits'])
      .then(cryptoKey =>
         Promise.all([
           crypto.subtle.exportKey('raw', cryptoKey.publicKey),
           crypto.subtle.exportKey('jwk', cryptoKey.privateKey)
         ]));
  },

  decodeMsg(aData, aPrivateKey, aPublicKey, aSenderPublicKey, aSalt, aRs,
            aAuthenticationSecret, aPadSize) {

    if (aData.byteLength === 0) {
      // Zero length messages will be passed as null.
      return Promise.resolve(null);
    }

    // The last chunk of data must be less than aRs, if it is not return an
    // error.
    if (aData.byteLength % (aRs + 16) === 0) {
      return Promise.reject(new Error('Data truncated'));
    }

    let senderKey = ChromeUtils.base64URLDecode(aSenderPublicKey, {
      // draft-ietf-httpbis-encryption-encoding-01 prohibits padding.
      padding: "reject",
    });

    return Promise.all([
      crypto.subtle.importKey('raw', senderKey, ECDH_KEY,
                              false, ['deriveBits']),
      crypto.subtle.importKey('jwk', aPrivateKey, ECDH_KEY,
                              false, ['deriveBits'])
    ])
    .then(([appServerKey, subscriptionPrivateKey]) =>
          crypto.subtle.deriveBits({ name: 'ECDH', public: appServerKey },
                                   subscriptionPrivateKey, 256))
    .then(ikm => this._deriveKeyAndNonce(aPadSize,
                                         new Uint8Array(ikm),
                                         ChromeUtils.base64URLDecode(aSalt,
                                                    { padding: "reject" }),
                                         aPublicKey,
                                         senderKey,
                                         aAuthenticationSecret))
    .then(r =>
      // AEAD_AES_128_GCM expands ciphertext to be 16 octets longer.
      Promise.all(chunkArray(aData, aRs + 16).map((slice, index) =>
        this._decodeChunk(aPadSize, slice, index, r[1], r[0]))))
    .then(r => concatArray(r));
  },

  _deriveKeyAndNonce(padSize, ikm, salt, receiverKey, senderKey,
                     authenticationSecret) {
    var kdfPromise;
    var context;
    var encryptInfo;
    // The size of the padding determines which key derivation we use.
    //
    // 1. If the pad size is 1, we assume "aesgcm128". This scheme ignores the
    // authenticationSecret, and uses "Content-Encoding: <blah>" for the
    // context string. It should eventually be removed: bug 1230038.
    //
    // 2. If the pad size is 2, we assume "aesgcm", and mix the
    // authenticationSecret with the ikm using HKDF. The context string is:
    // "Content-Encoding: <blah>\0P-256\0" then the length and value of both the
    // receiver key and sender key.
    if (padSize == 2) {
      // Since we are using an authentication secret, we need to run an extra
      // round of HKDF with the authentication secret as salt.
      var authKdf = new hkdf(authenticationSecret, ikm);
      kdfPromise = authKdf.extract(AUTH_INFO, 32)
        .then(ikm2 => new hkdf(salt, ikm2));

      // aesgcm requires extra context for the info parameter.
      context = concatArray([
        new Uint8Array([0]), P256DH_INFO,
        this._encodeLength(receiverKey), receiverKey,
        this._encodeLength(senderKey), senderKey
      ]);
      encryptInfo = AESGCM_ENCRYPT_INFO;
    } else {
      kdfPromise = Promise.resolve(new hkdf(salt, ikm));
      context = new Uint8Array(0);
      encryptInfo = AESGCM128_ENCRYPT_INFO;
    }
    return kdfPromise.then(kdf => Promise.all([
      kdf.extract(concatArray([encryptInfo, context]), 16)
        .then(gcmBits => crypto.subtle.importKey('raw', gcmBits, 'AES-GCM', false,
                                                 ['decrypt'])),
      kdf.extract(concatArray([NONCE_INFO, context]), 12)
    ]));
  },

  _encodeLength(buffer) {
    return new Uint8Array([0, buffer.byteLength]);
  },

  _decodeChunk(aPadSize, aSlice, aIndex, aNonce, aKey) {
    let params = {
      name: 'AES-GCM',
      iv: generateNonce(aNonce, aIndex)
    };
    return crypto.subtle.decrypt(params, aKey, aSlice)
      .then(decoded => this._unpadChunk(aPadSize, new Uint8Array(decoded)));
  },

  /**
   * Removes padding from a decrypted chunk.
   *
   * @param {Number} padSize The size of the padding length prepended to each
   *  chunk. For aesgcm, the padding length is expressed as a 16-bit unsigned
   *  big endian integer. For aesgcm128, the padding is an 8-bit integer.
   * @param {Uint8Array} decoded The decrypted, padded chunk.
   * @returns {Uint8Array} The chunk with padding removed.
   */
  _unpadChunk(padSize, decoded) {
    if (padSize < 1 || padSize > 2) {
      throw new Error('Unsupported pad size');
    }
    if (decoded.length < padSize) {
      throw new Error('Decoded array is too short!');
    }
    var pad = decoded[0];
    if (padSize == 2) {
      pad = (pad << 8) | decoded[1];
    }
    if (pad > decoded.length) {
      throw new Error ('Padding is wrong!');
    }
    // All padded bytes must be zero except the first one.
    for (var i = padSize; i <= pad; i++) {
      if (decoded[i] !== 0) {
        throw new Error('Padding is wrong!');
      }
    }
    return decoded.slice(pad + padSize);
  },
};
