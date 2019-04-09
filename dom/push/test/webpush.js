/*
 * Browser-based Web Push client for the application server piece.
 *
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 *
 * Uses the WebCrypto API.
 *
 * Note that this test file uses the old, deprecated aesgcm128 encryption
 * scheme. PushCrypto.encrypt() exists and uses the later aes128gcm, but
 * there's no good reason to upgrade this at this time (and having mochitests
 * use PushCrypto directly is easier said than done.)
 */

(function(g) {
  "use strict";

  var P256DH = {
    name: "ECDH",
    namedCurve: "P-256",
  };
  var webCrypto = g.crypto.subtle;
  var ENCRYPT_INFO = new TextEncoder("utf-8").encode("Content-Encoding: aesgcm128");
  var NONCE_INFO = new TextEncoder("utf-8").encode("Content-Encoding: nonce");

  function chunkArray(array, size) {
    var start = array.byteOffset || 0;
    array = array.buffer || array;
    var index = 0;
    var result = [];
    while (index + size <= array.byteLength) {
      result.push(new Uint8Array(array, start + index, size));
      index += size;
    }
    if (index < array.byteLength) {
      result.push(new Uint8Array(array, start + index));
    }
    return result;
  }

  /* I can't believe that this is needed here, in this day and age ...
   * Note: these are not efficient, merely expedient.
   */
  var base64url = {
    _strmap: "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_",
    encode(data) {
      data = new Uint8Array(data);
      var len = Math.ceil(data.length * 4 / 3);
      return chunkArray(data, 3).map(chunk => [
        chunk[0] >>> 2,
        ((chunk[0] & 0x3) << 4) | (chunk[1] >>> 4),
        ((chunk[1] & 0xf) << 2) | (chunk[2] >>> 6),
        chunk[2] & 0x3f,
      ].map(v => base64url._strmap[v]).join("")).join("").slice(0, len);
    },
    _lookup(s, i) {
      return base64url._strmap.indexOf(s.charAt(i));
    },
    decode(str) {
      var v = new Uint8Array(Math.floor(str.length * 3 / 4));
      var vi = 0;
      for (var si = 0; si < str.length;) {
        var w = base64url._lookup(str, si++);
        var x = base64url._lookup(str, si++);
        var y = base64url._lookup(str, si++);
        var z = base64url._lookup(str, si++);
        v[vi++] = w << 2 | x >>> 4;
        v[vi++] = x << 4 | y >>> 2;
        v[vi++] = y << 6 | z;
      }
      return v;
    },
  };

  g.base64url = base64url;

  /* Coerces data into a Uint8Array */
  function ensureView(data) {
    if (typeof data === "string") {
      return new TextEncoder("utf-8").encode(data);
    }
    if (data instanceof ArrayBuffer) {
      return new Uint8Array(data);
    }
    if (ArrayBuffer.isView(data)) {
      return new Uint8Array(data.buffer);
    }
    throw new Error("webpush() needs a string or BufferSource");
  }

  function bsConcat(arrays) {
    var size = arrays.reduce((total, a) => total + a.byteLength, 0);
    var index = 0;
    return arrays.reduce((result, a) => {
      result.set(new Uint8Array(a), index);
      index += a.byteLength;
      return result;
    }, new Uint8Array(size));
  }

  function hmac(key) {
    this.keyPromise = webCrypto.importKey("raw", key, { name: "HMAC", hash: "SHA-256" },
                                          false, ["sign"]);
  }
  hmac.prototype.hash = function(input) {
    return this.keyPromise.then(k => webCrypto.sign("HMAC", k, input));
  };

  function hkdf(salt, ikm) {
    this.prkhPromise = new hmac(salt).hash(ikm)
      .then(prk => new hmac(prk));
  }

  hkdf.prototype.generate = function(info, len) {
    var input = bsConcat([info, new Uint8Array([1])]);
    return this.prkhPromise
      .then(prkh => prkh.hash(input))
      .then(h => {
        if (h.byteLength < len) {
          throw new Error("Length is too long");
        }
        return h.slice(0, len);
      });
  };

  /* generate a 96-bit IV for use in GCM, 48-bits of which are populated */
  function generateNonce(base, index) {
    var nonce = base.slice(0, 12);
    for (var i = 0; i < 6; ++i) {
      nonce[nonce.length - 1 - i] ^= (index / Math.pow(256, i)) & 0xff;
    }
    return nonce;
  }

  function encrypt(localKey, remoteShare, salt, data) {
    return webCrypto.importKey("raw", remoteShare, P256DH, false, ["deriveBits"])
      .then(remoteKey =>
            webCrypto.deriveBits({ name: P256DH.name, public: remoteKey },
                                 localKey, 256))
      .then(rawKey => {
        var kdf = new hkdf(salt, rawKey);
        return Promise.all([
          kdf.generate(ENCRYPT_INFO, 16)
            .then(gcmBits =>
                  webCrypto.importKey("raw", gcmBits, "AES-GCM", false, ["encrypt"])),
          kdf.generate(NONCE_INFO, 12),
        ]);
      })
      .then(([key, nonce]) => {
        if (data.byteLength === 0) {
          // Send an authentication tag for empty messages.
          return webCrypto.encrypt({
            name: "AES-GCM",
            iv: generateNonce(nonce, 0),
          }, key, new Uint8Array([0])).then(value => [value]);
        }
        // 4096 is the default size, though we burn 1 for padding
        return Promise.all(chunkArray(data, 4095).map((slice, index) => {
          var padded = bsConcat([new Uint8Array([0]), slice]);
          return webCrypto.encrypt({
            name: "AES-GCM",
            iv: generateNonce(nonce, index),
          }, key, padded);
        }));
      }).then(bsConcat);
  }

  function webPushEncrypt(subscription, data) {
    data = ensureView(data);

    var salt = g.crypto.getRandomValues(new Uint8Array(16));
    return webCrypto.generateKey(P256DH, false, ["deriveBits"])
      .then(localKey => {
        return Promise.all([
          encrypt(localKey.privateKey, subscription.getKey("p256dh"), salt, data),
          // 1337 p-256 specific haxx to get the raw value out of the spki value
          webCrypto.exportKey("raw", localKey.publicKey),
        ]);
      }).then(([payload, pubkey]) => {
        return {
          data: base64url.encode(payload),
          encryption: "keyid=p256dh;salt=" + base64url.encode(salt),
          encryption_key: "keyid=p256dh;dh=" + base64url.encode(pubkey),
          encoding: "aesgcm128",
        };
      });
  }

  g.webPushEncrypt = webPushEncrypt;
}(this));
