/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

function error(test) {
  return function(x) {
    console.log("ERROR :: " + x);
    test.complete(false);
    throw x;
  }
}

function complete(test, valid) {
  return function(x) {
    console.log("COMPLETE")
    console.log(x);
    if (valid) {
      test.complete(valid(x));
    } else {
      test.complete(true);
    }
  }
}

function memcmp_complete(test, value) {
  return function(x) {
    console.log("COMPLETE")
    console.log(x);
    test.memcmp_complete(value, x);
  }
}


// -----------------------------------------------------------------------------
TestArray.addTest(
  "Test for presence of WebCrypto API methods",
  function() {
    var that = this;
    this.complete(
      exists(window.crypto.subtle) &&
      exists(window.crypto.subtle.encrypt) &&
      exists(window.crypto.subtle.decrypt) &&
      exists(window.crypto.subtle.sign) &&
      exists(window.crypto.subtle.verify) &&
      exists(window.crypto.subtle.digest) &&
      exists(window.crypto.subtle.importKey) &&
      exists(window.crypto.subtle.exportKey) &&
      exists(window.crypto.subtle.generateKey) &&
      exists(window.crypto.subtle.deriveKey) &&
      exists(window.crypto.subtle.deriveBits)
    );
  }
);

// -----------------------------------------------------------------------------

TestArray.addTest(
  "Clean failure on a mal-formed algorithm",
  function() {
    var that = this;
    var alg = {
      get name() {
        throw "Oh no, no name!";
      }
    };

    crypto.subtle.importKey("raw", tv.raw, alg, true, ["encrypt"])
      .then(
        error(that),
        complete(that, function(x) { return true; })
      );
  }
)

// -----------------------------------------------------------------------------
TestArray.addTest(
  "Import / export round-trip with 'raw'",
  function() {
    var that = this;
    var alg = "AES-GCM";

    function doExport(x) {
      if (!hasKeyFields(x)) {
        window.result = x;
        throw "Invalid key; missing field(s)";
      } else if ((x.algorithm.name != alg) ||
        (x.algorithm.length != 8 * tv.raw.length) ||
        (x.type != "secret") ||
        (!x.extractable) ||
        (x.usages.length != 1) ||
        (x.usages[0] != 'encrypt')){
        throw "Invalid key: incorrect key data";
      }
      return crypto.subtle.exportKey("raw", x);
    }

    crypto.subtle.importKey("raw", tv.raw, alg, true, ["encrypt"])
      .then(doExport, error(that))
      .then(
        memcmp_complete(that, tv.raw),
        error(that)
      );
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "Import failure with format 'raw'",
  function() {
    var that = this;
    var alg = "AES-GCM";

    crypto.subtle.importKey("raw", tv.negative_raw, alg, true, ["encrypt"])
      .then(error(that), complete(that));
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "Proper handling of an ABV representing part of a buffer",
  function() {
    var that = this;
    var alg = "AES-GCM";

    var u8 = new Uint8Array([0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                             0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
                             0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
                             0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f]);
    var u32 = new Uint32Array(u8.buffer, 8, 4);
    var out = u8.subarray(8, 24)

    function doExport(x) {
      return crypto.subtle.exportKey("raw", x);
    }

    crypto.subtle.importKey("raw", u32, alg, true, ["encrypt"])
      .then(doExport, error(that))
      .then(memcmp_complete(that, out), error(that));
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "Import / export round-trip with 'pkcs8'",
  function() {
    var that = this;
    var alg = { name: "RSASSA-PKCS1-v1_5", hash: "SHA-1" };

    function doExport(x) {
      if (!hasKeyFields(x)) {
        throw "Invalid key; missing field(s)";
      } else if ((x.algorithm.name != alg.name) ||
        (x.algorithm.hash.name != alg.hash) ||
        (x.algorithm.modulusLength != 512) ||
        (x.algorithm.publicExponent.byteLength != 3) ||
        (x.type != "private") ||
        (!x.extractable) ||
        (x.usages.length != 1) ||
        (x.usages[0] != 'sign')){
        throw "Invalid key: incorrect key data";
      }
      return crypto.subtle.exportKey("pkcs8", x);
    }

    crypto.subtle.importKey("pkcs8", tv.pkcs8, alg, true, ["sign"])
      .then(doExport, error(that))
      .then(
        memcmp_complete(that, tv.pkcs8),
        error(that)
      );
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "Import failure with format 'pkcs8'",
  function() {
    var that = this;
    var alg = { name: "RSASSA-PKCS1-v1_5", hash: "SHA-1" };

    crypto.subtle.importKey("pkcs8", tv.negative_pkcs8, alg, true, ["encrypt"])
      .then(error(that), complete(that));
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "Import / export round-trip with 'spki'",
  function() {
    var that = this;
    var alg = "RSAES-PKCS1-v1_5";

    function doExport(x) {
      if (!hasKeyFields(x)) {
        throw "Invalid key; missing field(s)";
      } else if ((x.algorithm.name != alg) ||
        (x.algorithm.modulusLength != 1024) ||
        (x.algorithm.publicExponent.byteLength != 3) ||
        (x.type != "public") ||
        (!x.extractable) ||
        (x.usages.length != 1) ||
        (x.usages[0] != 'encrypt')){
        throw "Invalid key: incorrect key data";
      }
      return crypto.subtle.exportKey("spki", x);
    }

    crypto.subtle.importKey("spki", tv.spki, alg, true, ["encrypt"])
      .then(doExport, error(that))
      .then(
        memcmp_complete(that, tv.spki),
        error(that)
      );
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "Import failure with format 'spki'",
  function() {
    var that = this;
    var alg = "RSAES-PKCS1-v1_5";

    crypto.subtle.importKey("spki", tv.negative_spki, alg, true, ["encrypt"])
      .then(error(that), complete(that));
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "Refuse to export non-extractable key",
  function() {
    var that = this;
    var alg = "AES-GCM";

    function doExport(x) {
      return crypto.subtle.exportKey("raw", x);
    }

    crypto.subtle.importKey("raw", tv.raw, alg, false, ["encrypt"])
      .then(doExport, error(that))
      .then(
        error(that),
        complete(that)
      );
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "IndexedDB store / retrieve round-trip",
  function() {
    var that = this;
    var alg = "AES-GCM";
    var importedKey;
    var dbname = "keyDB";
    var dbstore = "keystore";
    var dbversion = 1;
    var dbkey = 0;

    function doIndexedDB(x) {
      importedKey = x;
      var req = indexedDB.deleteDatabase(dbname);
      req.onerror = error(that);
      req.onsuccess = doCreateDB;
    }

    function doCreateDB() {
      var req = indexedDB.open(dbname, dbversion);
      req.onerror = error(that);
      req.onupgradeneeded = function(e) {
        db = e.target.result;
        db.createObjectStore(dbstore, {keyPath: "id"});
      }

      req.onsuccess = doPut;
    }

    function doPut() {
      req = db.transaction([dbstore], "readwrite")
              .objectStore(dbstore)
              .add({id: dbkey, val: importedKey});
      req.onerror = error(that);
      req.onsuccess = doGet;
    }

    function doGet() {
      req = db.transaction([dbstore], "readwrite")
              .objectStore(dbstore)
              .get(dbkey);
      req.onerror = error(that);
      req.onsuccess = complete(that, function(e) {
        return hasKeyFields(e.target.result.val);
      });
    }

    crypto.subtle.importKey("raw", tv.raw, alg, false, ['encrypt'])
      .then(doIndexedDB, error(that));
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "Generate a 256-bit HMAC-SHA-256 key",
  function() {
    var that = this;
    var alg = { name: "HMAC", length: 256, hash: {name: "SHA-256"} };
    crypto.subtle.generateKey(alg, true, ["sign", "verify"]).then(
      complete(that, function(x) {
        return hasKeyFields(x) && x.algorithm.length == 256;
      }),
      error(that)
    );
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "Generate a 256-bit HMAC-SHA-256 key without specifying a key length",
  function() {
    var that = this;
    var alg = { name: "HMAC", hash: {name: "SHA-256"} };
    crypto.subtle.generateKey(alg, true, ["sign", "verify"]).then(
      complete(that, function(x) {
        return hasKeyFields(x) && x.algorithm.length == 512;
      }),
      error(that)
    );
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "Generate a 256-bit HMAC-SHA-512 key without specifying a key length",
  function() {
    var that = this;
    var alg = { name: "HMAC", hash: {name: "SHA-512"} };
    crypto.subtle.generateKey(alg, true, ["sign", "verify"]).then(
      complete(that, function(x) {
        return hasKeyFields(x) && x.algorithm.length == 1024;
      }),
      error(that)
    );
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "Fail generating an HMAC key when specifying an invalid hash algorithm",
  function() {
    var that = this;
    var alg = { name: "HMAC", hash: {name: "SHA-123"} };
    crypto.subtle.generateKey(alg, true, ["sign", "verify"]).then(
      error(that),
      complete(that, function() { return true; })
    );
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "Fail generating an HMAC key when specifying a zero length",
  function() {
    var that = this;
    var alg = { name: "HMAC", hash: {name: "SHA-256"}, length: 0 };
    crypto.subtle.generateKey(alg, true, ["sign", "verify"]).then(
      error(that),
      complete(that, function() { return true; })
    );
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "Generate a 192-bit AES key",
  function() {
    var that = this;
    var alg = { name: "AES-GCM", length: 192 };
    crypto.subtle.generateKey(alg, true, ["encrypt"]).then(
      complete(that, function(x) {
        return hasKeyFields(x);
      }),
      error(that)
    );
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "Generate a 1024-bit RSA key",
  function() {
    var that = this;
    var alg = {
      name: "RSAES-PKCS1-v1_5",
      modulusLength: 1024,
      publicExponent: new Uint8Array([0x01, 0x00, 0x01])
    };
    crypto.subtle.generateKey(alg, false, ["encrypt", "decrypt"]).then(
      complete(that, function(x) {
        return exists(x.publicKey) &&
               (x.publicKey.algorithm.name == alg.name) &&
               (x.publicKey.algorithm.modulusLength == alg.modulusLength) &&
               (x.publicKey.type == "public") &&
               x.publicKey.extractable &&
               (x.publicKey.usages.length == 1) &&
               (x.publicKey.usages[0] == "encrypt") &&
               exists(x.privateKey) &&
               (x.privateKey.algorithm.name == alg.name) &&
               (x.privateKey.algorithm.modulusLength == alg.modulusLength) &&
               (x.privateKey.type == "private") &&
               !x.privateKey.extractable &&
               (x.privateKey.usages.length == 1) &&
               (x.privateKey.usages[0] == "decrypt");
      }),
      error(that)
    );
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "Fail cleanly when NSS refuses to generate a key pair",
  function() {
    var that = this;
    var alg = {
      name: "RSAES-PKCS1-v1_5",
      modulusLength: 2299, // NSS does not like this key length
      publicExponent: new Uint8Array([0x01, 0x00, 0x01])
    };

    crypto.subtle.generateKey(alg, false, ["encrypt"])
      .then( error(that), complete(that) );
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "SHA-256 digest",
  function() {
    var that = this;
    crypto.subtle.digest("SHA-256", tv.sha256.data).then(
      memcmp_complete(that, tv.sha256.result),
      error(that)
    );
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "Fail cleanly on unknown hash algorithm",
  function() {
    var that = this;
    crypto.subtle.digest("GOST-34_311-95", tv.sha256.data).then(
      error(that),
      complete(that, function() { return true; })
    );
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "AES-CBC encrypt",
  function () {
    var that = this;

    function doEncrypt(x) {
      console.log(x);
      return crypto.subtle.encrypt(
        { name: "AES-CBC", iv: tv.aes_cbc_enc.iv },
        x, tv.aes_cbc_enc.data);
    }

    crypto.subtle.importKey("raw", tv.aes_cbc_enc.key, "AES-CBC", false, ['encrypt'])
      .then(doEncrypt)
      .then(
        memcmp_complete(that, tv.aes_cbc_enc.result),
        error(that)
      );
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "AES-CBC encrypt with wrong IV size",
  function () {
    var that = this;

    function encrypt(x, iv) {
      console.log(x);
      return crypto.subtle.encrypt(
        { name: "AES-CBC", iv: iv },
        x, tv.aes_cbc_enc.data);
    }

    function doEncrypt(x) {
      return encrypt(x, new Uint8Array(15))
        .then(
          null,
          function () { return encrypt(new Uint8Array(17)); }
        );
    }

    crypto.subtle.importKey("raw", tv.aes_cbc_enc.key, "AES-CBC", false, ['encrypt'])
      .then(doEncrypt)
      .then(
        error(that),
        complete(that)
      );
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "AES-CBC decrypt",
  function () {
    var that = this;

    function doDecrypt(x) {
      return crypto.subtle.decrypt(
        { name: "AES-CBC", iv: tv.aes_cbc_dec.iv },
        x, tv.aes_cbc_dec.data);
    }

    crypto.subtle.importKey("raw", tv.aes_cbc_dec.key, "AES-CBC", false, ['decrypt'])
      .then(doDecrypt)
      .then(
        memcmp_complete(that, tv.aes_cbc_dec.result),
        error(that)
      );
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "AES-CBC decrypt with wrong IV size",
  function () {
    var that = this;

    function decrypt(x, iv) {
      return crypto.subtle.decrypt(
        { name: "AES-CBC", iv: iv },
        x, tv.aes_cbc_dec.data);
    }

    function doDecrypt(x) {
      return decrypt(x, new Uint8Array(15))
        .then(
          null,
          function () { return decrypt(x, new Uint8Array(17)); }
        );
    }

    crypto.subtle.importKey("raw", tv.aes_cbc_dec.key, "AES-CBC", false, ['decrypt'])
      .then(doDecrypt)
      .then(
        error(that),
        complete(that)
      );
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "AES-CTR encryption",
  function () {
    var that = this;

    function doEncrypt(x) {
      return crypto.subtle.encrypt(
        { name: "AES-CTR", counter: tv.aes_ctr_enc.iv, length: 32 },
        x, tv.aes_ctr_enc.data);
    }

    crypto.subtle.importKey("raw", tv.aes_ctr_enc.key, "AES-CTR", false, ['encrypt'])
      .then(doEncrypt)
      .then(
        memcmp_complete(that, tv.aes_ctr_enc.result),
        error(that)
      );
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "AES-CTR encryption with wrong IV size",
  function () {
    var that = this;

    function encrypt(x, iv) {
      return crypto.subtle.encrypt(
        { name: "AES-CTR", counter: iv, length: 32 },
        x, tv.aes_ctr_enc.data);
    }

    function doEncrypt(x) {
      return encrypt(x, new Uint8Array(15))
        .then(
          null,
          function () { return encrypt(x, new Uint8Array(17)); }
        );
    }

    crypto.subtle.importKey("raw", tv.aes_ctr_enc.key, "AES-CTR", false, ['encrypt'])
      .then(doEncrypt)
      .then(
        error(that),
        complete(that)
      );
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "AES-CTR decryption",
  function () {
    var that = this;

    function doDecrypt(x) {
      return crypto.subtle.decrypt(
        { name: "AES-CTR", counter: tv.aes_ctr_dec.iv, length: 32 },
        x, tv.aes_ctr_dec.data);
    }

    crypto.subtle.importKey("raw", tv.aes_ctr_dec.key, "AES-CTR", false, ['decrypt'])
      .then(doDecrypt)
      .then(
        memcmp_complete(that, tv.aes_ctr_dec.result),
        error(that)
      );
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "AES-CTR decryption with wrong IV size",
  function () {
    var that = this;

    function doDecrypt(x, iv) {
      return crypto.subtle.decrypt(
        { name: "AES-CTR", counter: iv, length: 32 },
        x, tv.aes_ctr_dec.data);
    }

    function decrypt(x) {
      return decrypt(x, new Uint8Array(15))
        .then(
          null,
          function () { return decrypt(x, new Uint8Array(17)); }
        );
    }

    crypto.subtle.importKey("raw", tv.aes_ctr_dec.key, "AES-CTR", false, ['decrypt'])
      .then(doDecrypt)
      .then(
        error(that),
        complete(that)
      );
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "AES-GCM encryption",
  function () {
    var that = this;

    function doEncrypt(x) {
      return crypto.subtle.encrypt(
        {
          name: "AES-GCM",
          iv: tv.aes_gcm_enc.iv,
          additionalData: tv.aes_gcm_enc.adata,
          tagLength: 128
        },
        x, tv.aes_gcm_enc.data);
    }

    crypto.subtle.importKey("raw", tv.aes_gcm_enc.key, "AES-GCM", false, ['encrypt'])
      .then(doEncrypt)
      .then(
        memcmp_complete(that, tv.aes_gcm_enc.result),
        error(that)
      );
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "AES-GCM decryption",
  function () {
    var that = this;

    function doDecrypt(x) {
      return crypto.subtle.decrypt(
        {
          name: "AES-GCM",
          iv: tv.aes_gcm_dec.iv,
          additionalData: tv.aes_gcm_dec.adata,
          tagLength: 128
        },
        x, tv.aes_gcm_dec.data);
    }

    crypto.subtle.importKey("raw", tv.aes_gcm_dec.key, "AES-GCM", false, ['decrypt'])
      .then(doDecrypt)
      .then(
        memcmp_complete(that, tv.aes_gcm_dec.result),
        error(that)
      );
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "AES-GCM decryption, failing authentication check",
  function () {
    var that = this;

    function doDecrypt(x) {
      return crypto.subtle.decrypt(
        {
          name: "AES-GCM",
          iv: tv.aes_gcm_dec_fail.iv,
          additionalData: tv.aes_gcm_dec_fail.adata,
          tagLength: 128
        },
        x, tv.aes_gcm_dec_fail.data);
    }

    crypto.subtle.importKey("raw", tv.aes_gcm_dec_fail.key, "AES-GCM", false, ['decrypt'])
      .then(doDecrypt)
      .then(
        error(that),
        complete(that)
      );
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "HMAC SHA-256 sign",
  function() {
    var that = this;
    var alg = {
      name: "HMAC",
      hash: "SHA-256"
    }

    function doSign(x) {
      return crypto.subtle.sign("HMAC", x, tv.hmac_sign.data);
    }

    crypto.subtle.importKey("raw", tv.hmac_sign.key, alg, false, ['sign'])
      .then(doSign)
      .then(
        memcmp_complete(that, tv.hmac_sign.result),
        error(that)
      );
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "HMAC SHA-256 verify",
  function() {
    var that = this;
    var alg = {
      name: "HMAC",
      hash: "SHA-256"
    }

    function doVerify(x) {
      return crypto.subtle.verify("HMAC", x, tv.hmac_verify.sig, tv.hmac_verify.data);
    }

    crypto.subtle.importKey("raw", tv.hmac_verify.key, alg, false, ['verify'])
      .then(doVerify)
      .then(
        complete(that, function(x) { return !!x; }),
        error(that)
      );
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "HMAC SHA-256, failing verification due to bad signature",
  function() {
    var that = this;
    var alg = {
      name: "HMAC",
      hash: "SHA-256"
    }

    function doVerify(x) {
      return crypto.subtle.verify("HMAC", x, tv.hmac_verify.sig_fail,
                                             tv.hmac_verify.data);
    }

    crypto.subtle.importKey("raw", tv.hmac_verify.key, alg, false, ['verify'])
      .then(doVerify)
      .then(
        complete(that, function(x) { return !x; }),
        error(that)
      );
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "HMAC SHA-256, failing verification due to key usage restriction",
  function() {
    var that = this;
    var alg = {
      name: "HMAC",
      hash: "SHA-256"
    }

    function doVerify(x) {
      return crypto.subtle.verify("HMAC", x, tv.hmac_verify.sig,
                                             tv.hmac_verify.data);
    }

    crypto.subtle.importKey("raw", tv.hmac_verify.key, alg, false, ['encrypt'])
      .then(doVerify)
      .then(
        error(that),
        complete(that, function(x) { return true; })
      );
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "RSAES-PKCS#1 encrypt/decrypt round-trip",
  function () {
    var that = this;
    var privKey, pubKey;
    var alg = {name:"RSAES-PKCS1-v1_5"};

    var privKey, pubKey, data, ct, pt;
    function setPriv(x) { privKey = x; }
    function setPub(x) { pubKey = x; }
    function doEncrypt() {
      return crypto.subtle.encrypt(alg.name, pubKey, tv.rsaes.data);
    }
    function doDecrypt(x) {
      return crypto.subtle.decrypt(alg.name, privKey, x);
    }

    function fail() { error(that); }

    Promise.all([
      crypto.subtle.importKey("pkcs8", tv.rsaes.pkcs8, alg, false, ['decrypt'])
          .then(setPriv, error(that)),
      crypto.subtle.importKey("spki", tv.rsaes.spki, alg, false, ['encrypt'])
          .then(setPub, error(that))
    ]).then(doEncrypt, error(that))
      .then(doDecrypt, error(that))
      .then(
        memcmp_complete(that, tv.rsaes.data),
        error(that)
      );
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "RSAES-PKCS#1 decryption known answer",
  function () {
    var that = this;
    var alg = {name:"RSAES-PKCS1-v1_5"};

    function doDecrypt(x) {
      return crypto.subtle.decrypt(alg.name, x, tv.rsaes.result);
    }
    function fail() { error(that); }

    crypto.subtle.importKey("pkcs8", tv.rsaes.pkcs8, alg, false, ['decrypt'])
      .then( doDecrypt, fail )
      .then( memcmp_complete(that, tv.rsaes.data), fail );
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "RSASSA/SHA-1 signature",
  function () {
    var that = this;
    var alg = { name: "RSASSA-PKCS1-v1_5", hash: "SHA-1" };

    function doSign(x) {
      console.log("sign");
      console.log(x);
      return crypto.subtle.sign(alg.name, x, tv.rsassa.data);
    }
    function fail() { console.log("fail"); error(that); }

    crypto.subtle.importKey("pkcs8", tv.rsassa.pkcs8, alg, false, ['sign'])
      .then( doSign, fail )
      .then( memcmp_complete(that, tv.rsassa.sig1), fail );
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "RSASSA verification (SHA-1)",
  function () {
    var that = this;
    var alg = { name: "RSASSA-PKCS1-v1_5", hash: "SHA-1" };

    function doVerify(x) {
      return crypto.subtle.verify(alg.name, x, tv.rsassa.sig1, tv.rsassa.data);
    }
    function fail(x) { error(that); }

    crypto.subtle.importKey("spki", tv.rsassa.spki, alg, false, ['verify'])
      .then( doVerify, fail )
      .then(
        complete(that, function(x) { return x; }),
        fail
      );
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "RSASSA verification (SHA-1), failing verification",
  function () {
    var that = this;
    var alg = { name: "RSASSA-PKCS1-v1_5", hash: "SHA-1" };

    function doVerify(x) {
      return crypto.subtle.verify(alg.name, x, tv.rsassa.sig_fail, tv.rsassa.data);
    }
    function fail(x) { error(that); }

    crypto.subtle.importKey("spki", tv.rsassa.spki, alg, false, ['verify'])
      .then( doVerify, fail )
      .then(
        complete(that, function(x) { return !x; }),
        fail
      );
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "RSASSA/SHA-256 signature",
  function () {
    var that = this;
    var alg = { name: "RSASSA-PKCS1-v1_5", hash: "SHA-256" };

    function doSign(x) {
      return crypto.subtle.sign(alg.name, x, tv.rsassa.data);
    }
    function fail(x) { console.log(x); error(that); }

    crypto.subtle.importKey("pkcs8", tv.rsassa.pkcs8, alg, false, ['sign'])
      .then( doSign, fail )
      .then( memcmp_complete(that, tv.rsassa.sig256), fail );
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "RSASSA verification (SHA-256)",
  function () {
    var that = this;
    var alg = { name: "RSASSA-PKCS1-v1_5", hash: "SHA-256" };

    function doVerify(x) {
      return crypto.subtle.verify(alg.name, x, tv.rsassa.sig256, tv.rsassa.data);
    }
    function fail(x) { error(that); }

    crypto.subtle.importKey("spki", tv.rsassa.spki, alg, false, ['verify'])
      .then( doVerify, fail )
      .then(
        complete(that, function(x) { return x; }),
        fail
      );
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "RSASSA verification (SHA-256), failing verification",
  function () {
    var that = this;
    var alg = { name: "RSASSA-PKCS1-v1_5", hash: "SHA-256" };
    var use = ['sign', 'verify'];

    function doVerify(x) {
      console.log("verifying")
      return crypto.subtle.verify(alg.name, x, tv.rsassa.sig_fail, tv.rsassa.data);
    }
    function fail(x) { console.log("failing"); error(that)(x); }

    console.log("running")
    crypto.subtle.importKey("spki", tv.rsassa.spki, alg, false, ['verify'])
      .then( doVerify, fail )
      .then(
        complete(that, function(x) { return !x; }),
        fail
      );
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "Import raw PBKDF2 key",
  function() {
    var that = this;
    var alg = "PBKDF2";
    var key = new TextEncoder("utf-8").encode("password");

    crypto.subtle.importKey("raw", key, alg, false, ["deriveKey"]).then(
      complete(that, hasKeyFields),
      error(that)
    );
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "Import raw PBKDF2 key and derive bits using HMAC-SHA-1",
  function() {
    var that = this;
    var alg = "PBKDF2";
    var key = tv.pbkdf2_sha1.password;

    function doDerive(x) {
      console.log("deriving");
      if (!hasKeyFields(x)) {
        throw "Invalid key; missing field(s)";
      }

      var alg = {
        name: "PBKDF2",
        hash: "SHA-1",
        salt: tv.pbkdf2_sha1.salt,
        iterations: tv.pbkdf2_sha1.iterations
      };
      return crypto.subtle.deriveBits(alg, x, tv.pbkdf2_sha1.length);
    }
    function fail(x) { console.log("failing"); error(that)(x); }

    crypto.subtle.importKey("raw", key, alg, false, ["deriveKey"])
      .then( doDerive, fail )
      .then( memcmp_complete(that, tv.pbkdf2_sha1.derived), fail );
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "Import raw PBKDF2 key and derive a new key using HMAC-SHA-1",
  function() {
    var that = this;
    var alg = "PBKDF2";
    var key = tv.pbkdf2_sha1.password;

    function doDerive(x) {
      console.log("deriving");
      if (!hasKeyFields(x)) {
        throw "Invalid key; missing field(s)";
      }

      var alg = {
        name: "PBKDF2",
        hash: "SHA-1",
        salt: tv.pbkdf2_sha1.salt,
        iterations: tv.pbkdf2_sha1.iterations
      };

      var algDerived = {
        name: "HMAC",
        hash: {name: "SHA-1"}
      };

      return crypto.subtle.deriveKey(alg, x, algDerived, false, ["sign", "verify"])
        .then(function (x) {
          if (!hasKeyFields(x)) {
            throw "Invalid key; missing field(s)";
          }

          if (x.algorithm.length != 512) {
            throw "Invalid key; incorrect length";
          }

          return x;
        });
    }

    function doSignAndVerify(x) {
      var data = new Uint8Array(1024);

      return crypto.subtle.sign("HMAC", x, data)
        .then(function (sig) {
          return crypto.subtle.verify("HMAC", x, sig, data);
        });
    }

    function fail(x) { console.log("failing"); error(that)(x); }

    crypto.subtle.importKey("raw", key, alg, false, ["deriveKey"])
      .then( doDerive, fail )
      .then( doSignAndVerify, fail )
      .then( complete(that), fail );
  }
);

// -----------------------------------------------------------------------------
/*TestArray.addTest(
  "Import raw PBKDF2 key and derive bits using HMAC-SHA-256",
  function() {
    var that = this;
    var alg = "PBKDF2";
    var key = tv.pbkdf2_sha256.password;

    function doDerive(x) {
      console.log("deriving");
      if (!hasKeyFields(x)) {
        throw "Invalid key; missing field(s)";
      }

      var alg = {
        name: "PBKDF2",
        hash: "SHA-256",
        salt: tv.pbkdf2_sha256.salt,
        iterations: tv.pbkdf2_sha256.iterations
      };
      return crypto.subtle.deriveBits(alg, x, tv.pbkdf2_sha256.length);
    }
    function fail(x) { console.log("failing"); error(that)(x); }

    crypto.subtle.importKey("raw", key, alg, false, ["deriveKey"])
      .then( doDerive, fail )
      .then( memcmp_complete(that, tv.pbkdf2_sha256.derived), fail );
  }
);*/

// -----------------------------------------------------------------------------
TestArray.addTest(
  "RSA-OAEP encrypt/decrypt round-trip",
  function () {
    var that = this;
    var privKey, pubKey;
    var alg = {name: "RSA-OAEP", hash: "SHA-1"};

    var privKey, pubKey;
    function setPriv(x) { privKey = x; }
    function setPub(x) { pubKey = x; }
    function doEncrypt() {
      return crypto.subtle.encrypt(alg, pubKey, tv.rsaoaep.data);
    }
    function doDecrypt(x) {
      return crypto.subtle.decrypt(alg, privKey, x);
    }

    Promise.all([
      crypto.subtle.importKey("pkcs8", tv.rsaoaep.pkcs8, alg, false, ['decrypt'])
          .then(setPriv, error(that)),
      crypto.subtle.importKey("spki", tv.rsaoaep.spki, alg, false, ['encrypt'])
          .then(setPub, error(that))
    ]).then(doEncrypt, error(that))
      .then(doDecrypt, error(that))
      .then(
        memcmp_complete(that, tv.rsaoaep.data),
        error(that)
      );
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "RSA-OAEP key generation and encrypt/decrypt round-trip (SHA-256)",
  function () {
    var that = this;
    var alg = {
      name: "RSA-OAEP",
      hash: "SHA-256",
      modulusLength: 2048,
      publicExponent: new Uint8Array([0x01, 0x00, 0x01])
    };

    var privKey, pubKey, data = crypto.getRandomValues(new Uint8Array(128));
    function setKey(x) { pubKey = x.publicKey; privKey = x.privateKey; }
    function doEncrypt() {
      return crypto.subtle.encrypt(alg, pubKey, data);
    }
    function doDecrypt(x) {
      return crypto.subtle.decrypt(alg, privKey, x);
    }

    crypto.subtle.generateKey(alg, false, ['encrypt', 'decrypt'])
      .then(setKey, error(that))
      .then(doEncrypt, error(that))
      .then(doDecrypt, error(that))
      .then(
        memcmp_complete(that, data),
        error(that)
      );
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "RSA-OAEP decryption known answer",
  function () {
    var that = this;
    var alg = {name: "RSA-OAEP", hash: "SHA-1"};

    function doDecrypt(x) {
      return crypto.subtle.decrypt(alg, x, tv.rsaoaep.result);
    }
    function fail() { error(that); }

    crypto.subtle.importKey("pkcs8", tv.rsaoaep.pkcs8, alg, false, ['decrypt'])
      .then( doDecrypt, fail )
      .then( memcmp_complete(that, tv.rsaoaep.data), fail );
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "RSA-OAEP input data length checks (2048-bit key)",
  function () {
    var that = this;
    var privKey, pubKey;
    var alg = {
      name: "RSA-OAEP",
      hash: "SHA-1",
      modulusLength: 2048,
      publicExponent: new Uint8Array([0x01, 0x00, 0x01])
    };

    var privKey, pubKey;
    function setKey(x) { pubKey = x.publicKey; privKey = x.privateKey; }
    function doEncrypt(n) {
      return function () {
        return crypto.subtle.encrypt(alg, pubKey, new Uint8Array(n));
      }
    }

    crypto.subtle.generateKey(alg, false, ['encrypt'])
      .then(setKey, error(that))
      .then(doEncrypt(214), error(that))
      .then(doEncrypt(215), error(that))
      .then(error(that), complete(that));
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "RSA-OAEP key import with invalid hash",
  function () {
    var that = this;
    var alg = {name: "RSA-OAEP", hash: "SHA-123"};

    crypto.subtle.importKey("pkcs8", tv.rsaoaep.pkcs8, alg, false, ['decrypt'])
      .then(error(that), complete(that));
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "Key wrap known answer, using AES-GCM",
  function () {
    var that = this;
    var alg = {
      name: "AES-GCM",
      iv: tv.key_wrap_known_answer.wrapping_iv,
      tagLength: 128
    };
    var key, wrappingKey;

    function doImport(k) {
      wrappingKey = k;
      return crypto.subtle.importKey("raw", tv.key_wrap_known_answer.key,
                                     alg, true, ['encrypt', 'decrypt']);
    }
    function doWrap(k) {
      key = k;
      return crypto.subtle.wrapKey("raw", key, wrappingKey, alg);
    }

    crypto.subtle.importKey("raw", tv.key_wrap_known_answer.wrapping_key,
                            alg, false, ['wrapKey'])
      .then(doImport, error(that))
      .then(doWrap, error(that))
      .then(
        memcmp_complete(that, tv.key_wrap_known_answer.wrapped_key),
        error(that)
      );
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "Key wrap failing on non-extractable key",
  function () {
    var that = this;
    var alg = {
      name: "AES-GCM",
      iv: tv.key_wrap_known_answer.wrapping_iv,
      tagLength: 128
    };
    var key, wrappingKey;

    function doImport(k) {
      wrappingKey = k;
      return crypto.subtle.importKey("raw", tv.key_wrap_known_answer.key,
                                     alg, false, ['encrypt', 'decrypt']);
    }
    function doWrap(k) {
      key = k;
      return crypto.subtle.wrapKey("raw", key, wrappingKey, alg);
    }

    crypto.subtle.importKey("raw", tv.key_wrap_known_answer.wrapping_key,
                            alg, false, ['wrapKey'])
      .then(doImport, error(that))
      .then(doWrap, error(that))
      .then(
        error(that),
        complete(that)
      );
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "Key unwrap known answer, using AES-GCM",
  function () {
    var that = this;
    var alg = {
      name: "AES-GCM",
      iv: tv.key_wrap_known_answer.wrapping_iv,
      tagLength: 128
    };
    var key, wrappingKey;

    function doUnwrap(k) {
      wrappingKey = k;
      return crypto.subtle.unwrapKey(
                "raw", tv.key_wrap_known_answer.wrapped_key,
                wrappingKey, alg,
                "AES-GCM", true, ['encrypt', 'decrypt']
             );
    }
    function doExport(k) {
      return crypto.subtle.exportKey("raw", k);
    }

    crypto.subtle.importKey("raw", tv.key_wrap_known_answer.wrapping_key,
                            alg, false, ['unwrapKey'])
      .then(doUnwrap, error(that))
      .then(doExport, error(that))
      .then(
        memcmp_complete(that, tv.key_wrap_known_answer.key),
        error(that)
      );
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "Key wrap/unwrap round-trip, using RSA-OAEP",
  function () {
    var that = this;
    var oaep = {
      name: "RSA-OAEP",
      hash: "SHA-256"
    };
    var gcm = {
      name: "AES-GCM",
      iv: tv.aes_gcm_enc.iv,
      additionalData: tv.aes_gcm_enc.adata,
      tagLength: 128
    };
    var unwrapKey;

    function doWrap(keys) {
      var originalKey = keys[0];
      var wrapKey = keys[1];
      unwrapKey = keys[2];
      return crypto.subtle.wrapKey("raw", originalKey, wrapKey, oaep);
    }
    function doUnwrap(wrappedKey) {
      return crypto.subtle.unwrapKey("raw", wrappedKey, unwrapKey, oaep,
                                     gcm, false, ['encrypt']);
    }
    function doEncrypt(aesKey) {
      return crypto.subtle.encrypt(gcm, aesKey, tv.aes_gcm_enc.data);
    }

    // 1.Import:
    //  -> HMAC key
    //  -> OAEP wrap key (public)
    //  -> OAEP unwrap key (private)
    // 2. Wrap the HMAC key
    // 3. Unwrap it
    // 4. Compute HMAC
    // 5. Check HMAC value
    Promise.all([
      crypto.subtle.importKey("raw", tv.aes_gcm_enc.key, gcm, true, ['encrypt']),
      crypto.subtle.importKey("spki", tv.rsaoaep.spki, oaep, true, ['wrapKey']),
      crypto.subtle.importKey("pkcs8", tv.rsaoaep.pkcs8, oaep, false, ['unwrapKey'])
    ])
      .then(doWrap, error(that))
      .then(doUnwrap, error(that))
      .then(doEncrypt, error(that))
      .then(
        memcmp_complete(that, tv.aes_gcm_enc.result),
        error(that)
      );
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "JWK import and use of an AES-GCM key",
  function () {
    var that = this;

    function doEncrypt(x) {
      return crypto.subtle.encrypt(
        {
          name: "AES-GCM",
          iv: tv.aes_gcm_enc.iv,
          additionalData: tv.aes_gcm_enc.adata,
          tagLength: 128
        },
        x, tv.aes_gcm_enc.data);
    }

    crypto.subtle.importKey("jwk", tv.aes_gcm_enc.key_jwk, "AES-GCM", false, ['encrypt'])
      .then(doEncrypt)
      .then(
        memcmp_complete(that, tv.aes_gcm_enc.result),
        error(that)
      );
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "JWK import and use of an RSASSA-PKCS1-v1_5 private key",
  function () {
    var that = this;
    var alg = { name: "RSASSA-PKCS1-v1_5", hash: "SHA-256" };

    function doSign(x) {
      return crypto.subtle.sign(alg.name, x, tv.rsassa.data);
    }
    function fail(x) { console.log(x); error(that); }

    crypto.subtle.importKey("jwk", tv.rsassa.jwk_priv, alg, false, ['sign'])
      .then( doSign, fail )
      .then( memcmp_complete(that, tv.rsassa.sig256), fail );
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "JWK import and use of an RSASSA-PKCS1-v1_5 public key",
  function () {
    var that = this;
    var alg = { name: "RSASSA-PKCS1-v1_5", hash: "SHA-256" };

    function doVerify(x) {
      return crypto.subtle.verify(alg.name, x, tv.rsassa.sig256, tv.rsassa.data);
    }
    function fail(x) { error(that); }

    crypto.subtle.importKey("jwk", tv.rsassa.jwk_pub, alg, false, ['verify'])
      .then( doVerify, fail )
      .then(
        complete(that, function(x) { return x; }),
        fail
      );
  });

// -----------------------------------------------------------------------------
TestArray.addTest(
  "JWK import failure on incomplete RSA private key (missing 'qi')",
  function () {
    var that = this;
    var alg = { name: "RSA-OAEP", hash: "SHA-256" };
    var jwk = {
      kty: "RSA",
      n: tv.rsassa.jwk_priv.n,
      e: tv.rsassa.jwk_priv.e,
      d: tv.rsassa.jwk_priv.d,
      p: tv.rsassa.jwk_priv.p,
      q: tv.rsassa.jwk_priv.q,
      dp: tv.rsassa.jwk_priv.dp,
      dq: tv.rsassa.jwk_priv.dq,
    };

    crypto.subtle.importKey("jwk", jwk, alg, true, ['encrypt', 'decrypt'])
      .then( error(that), complete(that) );
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "JWK import failure on algorithm mismatch",
  function () {
    var that = this;
    var alg = "AES-GCM";
    var jwk = { k: "c2l4dGVlbiBieXRlIGtleQ", alg: "A256GCM" };

    crypto.subtle.importKey("jwk", jwk, alg, true, ['encrypt', 'decrypt'])
      .then( error(that), complete(that) );
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "JWK import failure on usages mismatch",
  function () {
    var that = this;
    var alg = "AES-GCM";
    var jwk = { k: "c2l4dGVlbiBieXRlIGtleQ", key_ops: ['encrypt'] };

    crypto.subtle.importKey("jwk", jwk, alg, true, ['encrypt', 'decrypt'])
      .then( error(that), complete(that) );
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "JWK import failure on extractable mismatch",
  function () {
    var that = this;
    var alg = "AES-GCM";
    var jwk = { k: "c2l4dGVlbiBieXRlIGtleQ", ext: false };

    crypto.subtle.importKey("jwk", jwk, alg, true, ['encrypt'])
      .then( error(that), complete(that) );
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "JWK export of a symmetric key",
  function () {
    var that = this;
    var alg = "AES-GCM";
    var jwk = { k: "c2l4dGVlbiBieXRlIGtleQ" };

    function doExport(k) {
      return crypto.subtle.exportKey("jwk", k);
    }

    crypto.subtle.importKey("jwk", jwk, alg, true, ['encrypt', 'decrypt'])
      .then(doExport)
      .then(
        complete(that, function(x) {
          return hasBaseJwkFields(x) &&
                 hasFields(x, ['k']) &&
                 x.kty == 'oct' &&
                 x.alg == 'A128GCM' &&
                 x.ext &&
                 shallowArrayEquals(x.key_ops, ['encrypt','decrypt']) &&
                 x.k == jwk.k
        }),
        error(that)
      );
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "JWK export of an RSA private key",
  function () {
    var that = this;
    var alg = { name: "RSASSA-PKCS1-v1_5", hash: "SHA-256" };
    var jwk = tv.rsassa.jwk_priv;

    function doExport(k) {
      return crypto.subtle.exportKey("jwk", k);
    }

    crypto.subtle.importKey("jwk", jwk, alg, true, ['sign'])
      .then(doExport)
      .then(
        complete(that, function(x) {
          window.jwk_priv = x;
          console.log(JSON.stringify(x));
          return hasBaseJwkFields(x) &&
                 hasFields(x, ['n', 'e', 'd', 'p', 'q', 'dp', 'dq', 'qi']) &&
                 x.kty == 'RSA' &&
                 x.alg == 'RS256' &&
                 x.ext &&
                 shallowArrayEquals(x.key_ops, ['sign']) &&
                 x.n  == jwk.n  &&
                 x.e  == jwk.e  &&
                 x.d  == jwk.d  &&
                 x.p  == jwk.p  &&
                 x.q  == jwk.q  &&
                 x.dp == jwk.dp &&
                 x.dq == jwk.dq &&
                 x.qi == jwk.qi;
          }),
        error(that)
      );
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "JWK export of an RSA public key",
  function () {
    var that = this;
    var alg = { name: "RSASSA-PKCS1-v1_5", hash: "SHA-256" };
    var jwk = tv.rsassa.jwk_pub;

    function doExport(k) {
      return crypto.subtle.exportKey("jwk", k);
    }

    crypto.subtle.importKey("jwk", jwk, alg, true, ['verify'])
      .then(doExport)
      .then(
        complete(that, function(x) {
          window.jwk_pub = x;
          return hasBaseJwkFields(x) &&
                 hasFields(x, ['n', 'e']) &&
                 x.kty == 'RSA' &&
                 x.alg == 'RS256' &&
                 x.ext &&
                 shallowArrayEquals(x.key_ops, ['verify']) &&
                 x.n  == jwk.n  &&
                 x.e  == jwk.e;
          }),
        error(that)
      );
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "JWK wrap/unwrap round-trip, with AES-GCM",
  function () {
    var that = this;
    var genAlg = { name: "HMAC", hash: "SHA-384", length: 512 };
    var wrapAlg = { name: "AES-GCM", iv: tv.aes_gcm_enc.iv };
    var wrapKey, originalKey, originalKeyJwk;

    function doExport(k) {
      return crypto.subtle.exportKey("jwk", k);
    }
    function doWrap() {
      return crypto.subtle.wrapKey("jwk", originalKey, wrapKey, wrapAlg);
    }
    function doUnwrap(wrappedKey) {
      return crypto.subtle.unwrapKey("jwk", wrappedKey, wrapKey, wrapAlg,
                                     { name: "HMAC", hash: "SHA-384"},
                                     true, ['sign', 'verify']);
    }

    function temperr(x) { return function(y) { console.log("error in "+x); console.log(y); } }

    Promise.all([
      crypto.subtle.importKey("jwk", tv.aes_gcm_enc.key_jwk,
                              "AES-GCM", false, ['wrapKey','unwrapKey'])
        .then(function(x) { console.log("wrapKey"); wrapKey = x; }),
      crypto.subtle.generateKey(genAlg, true, ['sign', 'verify'])
        .then(function(x) { console.log("originalKey"); originalKey = x; return x; })
        .then(doExport)
        .then(function(x) { originalKeyJwk = x; })
    ])
      .then(doWrap, temperr("initial phase"))
      .then(doUnwrap, temperr("wrap"))
      .then(doExport, temperr("unwrap"))
      .then(
        complete(that, function(x) {
          return exists(x.k) && x.k == originalKeyJwk.k;
        }),
        error(that)
      );
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "AES-KW known answer",
  function () {
    var that = this;

    function doWrap(keys) {
      wrapKey = keys[0];
      originalKey = keys[1];
      return crypto.subtle.wrapKey("raw", originalKey, wrapKey, "AES-KW");
    }

    Promise.all([
      crypto.subtle.importKey("jwk", tv.aes_kw.wrapping_key,
                              "AES-KW", false, ['wrapKey']),
      crypto.subtle.importKey("jwk", tv.aes_kw.key,
                              "AES-GCM", true, ['encrypt'])
    ])
      .then(doWrap)
      .then(
        memcmp_complete(that, tv.aes_kw.wrapped_key),
        error(that)
      );

  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "AES-KW unwrap failure on tampered key data",
  function () {
    var that = this;
    var tamperedWrappedKey = new Uint8Array(tv.aes_kw.wrapped_key);
    tamperedWrappedKey[5] ^= 0xFF;

    function doUnwrap(wrapKey) {
      return crypto.subtle.unwrapKey("raw", tamperedWrappedKey, wrapKey,
                                     "AES-KW", "AES-GCM",
                                     true, ['encrypt', 'decrypt']);
    }

    crypto.subtle.importKey("jwk", tv.aes_kw.wrapping_key,
                              "AES-KW", false, ['unwrapKey'])
      .then(doUnwrap)
      .then(error(that), complete(that));
  }
);

// -----------------------------------------------------------------------------
TestArray.addTest(
  "AES-KW wrap/unwrap round-trip",
  function () {
    var that = this;
    var genAlg = { name: "HMAC", hash: "SHA-384", length: 512 };
    var wrapKey, originalKey, originalKeyJwk;

    function doExport(k) {
      return crypto.subtle.exportKey("jwk", k);
    }
    function doWrap() {
      return crypto.subtle.wrapKey("raw", originalKey, wrapKey, "AES-KW");
    }
    function doUnwrap(wrappedKey) {
      return crypto.subtle.unwrapKey("raw", wrappedKey, wrapKey,
                                     "AES-KW", { name: "HMAC", hash: "SHA-384"},
                                     true, ['sign', 'verify']);
    }

    Promise.all([
      crypto.subtle.importKey("jwk", tv.aes_kw.wrapping_key,
                              "AES-KW", false, ['wrapKey','unwrapKey'])
        .then(function(x) { console.log("wrapKey"); wrapKey = x; }),
      crypto.subtle.generateKey(genAlg, true, ['sign'])
        .then(function(x) { console.log("originalKey"); originalKey = x; return x; })
        .then(doExport)
        .then(function(x) { originalKeyJwk = x; })
    ])
      .then(doWrap)
      .then(doUnwrap)
      .then(doExport)
      .then(
        complete(that, function(x) {
          return exists(x.k) && x.k == originalKeyJwk.k;
        }),
        error(that)
      );
  }
);

