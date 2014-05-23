/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function exists(x) {
  return (x !== undefined);
}

function hasKeyFields(x) {
  return exists(x.algorithm) &&
         exists(x.extractable) &&
         exists(x.type) &&
         exists(x.usages);
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
    var alg = { name: "RSASSA-PKCS1-v1_5", hash: "SHA1" };

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
    var alg = { name: "RSASSA-PKCS1-v1_5", hash: "SHA1" };

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

