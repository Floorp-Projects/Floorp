/* jshint moz: true, esnext: true */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

const Cu = Components.utils;

Cu.import('resource://gre/modules/Services.jsm');
Cu.import('resource://gre/modules/XPCOMUtils.jsm');

XPCOMUtils.defineLazyGetter(this, 'gDOMBundle', () =>
  Services.strings.createBundle('chrome://global/locale/dom/dom.properties'));

Cu.importGlobalProperties(['crypto']);

this.EXPORTED_SYMBOLS = ['PushCrypto', 'concatArray'];

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

/** Localized error property names. */

// `Encryption` header missing or malformed.
const BAD_ENCRYPTION_HEADER = 'PushMessageBadEncryptionHeader';
// `Crypto-Key` or legacy `Encryption-Key` header missing.
const BAD_CRYPTO_KEY_HEADER = 'PushMessageBadCryptoKeyHeader';
const BAD_ENCRYPTION_KEY_HEADER = 'PushMessageBadEncryptionKeyHeader';
// `Content-Encoding` header missing or contains unsupported encoding.
const BAD_ENCODING_HEADER = 'PushMessageBadEncodingHeader';
// `dh` parameter of `Crypto-Key` header missing or not base64url-encoded.
const BAD_DH_PARAM = 'PushMessageBadSenderKey';
// `salt` parameter of `Encryption` header missing or not base64url-encoded.
const BAD_SALT_PARAM = 'PushMessageBadSalt';
// `rs` parameter of `Encryption` header not a number or less than pad size.
const BAD_RS_PARAM = 'PushMessageBadRecordSize';
// Invalid or insufficient padding for encrypted chunk.
const BAD_PADDING = 'PushMessageBadPaddingError';
// Generic crypto error.
const BAD_CRYPTO = 'PushMessageBadCryptoError';

class CryptoError extends Error {
  /**
   * Creates an error object indicating an incoming push message could not be
   * decrypted.
   *
   * @param {String} message A human-readable error message. This is only for
   * internal module logging, and doesn't need to be localized.
   * @param {String} property The localized property name from `dom.properties`.
   * @param {String...} params Substitutions to insert into the localized
   *  string.
   */
  constructor(message, property, ...params) {
    super(message);
    this.isCryptoError = true;
    this.property = property;
    this.params = params;
  }

  /**
   * Formats a localized string for reporting decryption errors to the Web
   * Console.
   *
   * @param {String} scope The scope of the service worker receiving the
   *  message, prepended to any other substitutions in the string.
   * @returns {String} The localized string.
   */
  format(scope) {
    let params = [scope, ...this.params].map(String);
    return gDOMBundle.formatStringFromName(this.property, params,
                                           params.length);
  }
}

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
  if (!encryptField) {
    throw new CryptoError('Missing encryption header',
                          BAD_ENCRYPTION_HEADER);
  }
  var p = encryptField.split(',', 1)[0];
  if (!p) {
    throw new CryptoError('Encryption header missing params',
                          BAD_ENCRYPTION_HEADER);
  }
  return p.split(';').reduce(parseHeaderFieldParams, {});
}

function getCryptoParams(headers) {
  if (!headers) {
    return null;
  }

  var keymap;
  var padSize;
  if (!headers.encoding) {
    throw new CryptoError('Missing Content-Encoding header',
                          BAD_ENCODING_HEADER);
  }
  if (headers.encoding == AESGCM_ENCODING) {
    // aesgcm uses the Crypto-Key header, 2 bytes for the pad length, and an
    // authentication secret.
    // https://tools.ietf.org/html/draft-ietf-httpbis-encryption-encoding-01
    keymap = getEncryptionKeyParams(headers.crypto_key);
    if (!keymap) {
      throw new CryptoError('Missing Crypto-Key header',
                            BAD_CRYPTO_KEY_HEADER);
    }
    padSize = 2;
  } else if (headers.encoding == AESGCM128_ENCODING) {
    // aesgcm128 uses Encryption-Key, 1 byte for the pad length, and no secret.
    // https://tools.ietf.org/html/draft-thomson-http-encryption-02
    keymap = getEncryptionKeyParams(headers.encryption_key);
    if (!keymap) {
      throw new CryptoError('Missing Encryption-Key header',
                            BAD_ENCRYPTION_KEY_HEADER);
    }
    padSize = 1;
  } else {
    throw new CryptoError('Unsupported Content-Encoding: ' + headers.encoding,
                          BAD_ENCODING_HEADER);
  }

  var enc = getEncryptionParams(headers.encryption);
  var dh = keymap[enc.keyid || DEFAULT_KEYID];
  if (!dh) {
    throw new CryptoError('Missing dh parameter', BAD_DH_PARAM);
  }
  var salt = enc.salt;
  if (!salt) {
    throw new CryptoError('Missing salt parameter', BAD_SALT_PARAM);
  }
  var rs = enc.rs ? parseInt(enc.rs, 10) : 4096;
  if (isNaN(rs)) {
    throw new CryptoError('rs parameter must be a number', BAD_RS_PARAM);
  }
  if (rs <= padSize) {
    throw new CryptoError('rs parameter must be at least ' + padSize,
                          BAD_RS_PARAM, padSize);
  }
  return {dh, salt, rs, padSize};
}

// Decodes an unpadded, base64url-encoded string.
function base64URLDecode(string) {
  try {
    return ChromeUtils.base64URLDecode(string, {
      // draft-ietf-httpbis-encryption-encoding-01 prohibits padding.
      padding: 'reject',
    });
  } catch (ex) {}
  return null;
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
        throw new CryptoError('HKDF length is too long', BAD_CRYPTO);
      }
      return h.slice(0, len);
    });
};

/* generate a 96-bit nonce for use in GCM, 48-bits of which are populated */
function generateNonce(base, index) {
  if (index >= Math.pow(2, 48)) {
    throw new CryptoError('Nonce index is too large', BAD_CRYPTO);
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

  /**
   * Decrypts a push message.
   *
   * @param {JsonWebKey} privateKey The ECDH private key of the subscription
   *  receiving the message, in JWK form.
   * @param {BufferSource} publicKey The ECDH public key of the subscription
   *  receiving the message, in raw form.
   * @param {BufferSource} authenticationSecret The 16-byte shared
   *  authentication secret of the subscription receiving the message.
   * @param {Object} headers The encryption headers passed to `getCryptoParams`.
   * @param {BufferSource} ciphertext The encrypted message data.
   * @returns {Promise} Resolves with a `Uint8Array` containing the decrypted
   *  message data. Rejects with a `CryptoError` if decryption fails.
   */
  decrypt(privateKey, publicKey, authenticationSecret, headers, ciphertext) {
    return Promise.resolve().then(_ => {
      let cryptoParams = getCryptoParams(headers);
      if (!cryptoParams) {
        return null;
      }
      return this._decodeMsg(ciphertext, privateKey, publicKey,
                             cryptoParams.dh, cryptoParams.salt,
                             cryptoParams.rs, authenticationSecret,
                             cryptoParams.padSize);
    }).catch(error => {
      if (error.isCryptoError) {
        throw error;
      }
      // Web Crypto returns an unhelpful "operation failed for an
      // operation-specific reason" error if decryption fails. We don't have
      // context about what went wrong, so we throw a generic error instead.
      throw new CryptoError('Bad encryption', BAD_CRYPTO);
    });
  },

  _decodeMsg(aData, aPrivateKey, aPublicKey, aSenderPublicKey, aSalt, aRs,
             aAuthenticationSecret, aPadSize) {

    if (aData.byteLength === 0) {
      // Zero length messages will be passed as null.
      return null;
    }

    // The last chunk of data must be less than aRs, if it is not return an
    // error.
    if (aData.byteLength % (aRs + 16) === 0) {
      throw new CryptoError('Encrypted data truncated', BAD_CRYPTO);
    }

    let senderKey = base64URLDecode(aSenderPublicKey);
    if (!senderKey) {
      throw new CryptoError('dh parameter is not base64url-encoded',
                            BAD_DH_PARAM);
    }

    let salt = base64URLDecode(aSalt);
    if (!salt) {
      throw new CryptoError('salt parameter is not base64url-encoded',
                            BAD_SALT_PARAM);
    }

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
                                         salt,
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
      throw new CryptoError('Unsupported pad size', BAD_CRYPTO);
    }
    if (decoded.length < padSize) {
      throw new CryptoError('Decoded array is too short!', BAD_PADDING);
    }
    var pad = decoded[0];
    if (padSize == 2) {
      pad = (pad << 8) | decoded[1];
    }
    if (pad > decoded.length) {
      throw new CryptoError('Padding is wrong!', BAD_PADDING);
    }
    // All padded bytes must be zero except the first one.
    for (var i = padSize; i <= pad; i++) {
      if (decoded[i] !== 0) {
        throw new CryptoError('Padding is wrong!', BAD_PADDING);
      }
    }
    return decoded.slice(pad + padSize);
  },
};
