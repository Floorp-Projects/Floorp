/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Google Safe Browsing.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Fritz Schneider <fritz@google.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


// This file implements our query param encryption. You hand it a set
// of query params, and it will hand you a set of (maybe) encrypted
// query params back. It takes the query params you give it, 
// encodes and encrypts them into a encrypted query param, and adds
// the extra query params the server will need to decrypt them
// (e.g., the version of encryption and the decryption key).
// 
// The key manager provides the keys we need; this class just focuses
// on encrypting query params. See the url crypto key manager for
// details of our protocol, but essentially encryption is
// RC4_key(input) with key == MD5(K_C || nonce) where nonce is a
// 32-bit integer appended big-endian and K_C is the client's key.
//
// If for some reason we don't have an encryption key, encrypting is the 
// identity function.

/**
 * This class knows how to encrypt query parameters that will be
 * understood by the lookupserver.
 * 
 * @constructor
 */
function PROT_UrlCrypto() {
  this.debugZone = "urlcrypto";
  this.hasher_ = new G_CryptoHasher(G_CryptoHasher.algorithms.MD5);
  this.base64_ = new G_Base64();
  this.rc4_ = new ARC4();
  // Note: the UrlCryptoKeyManager will set this.manager_ automatically

  // Convenience properties
  this.VERSION = PROT_UrlCrypto.VERSION;
  this.RC4_DISCARD_BYTES = PROT_UrlCrypto.RC4_DISCARD_BYTES;
  this.VERSION_QUERY_PARAM_NAME = PROT_UrlCrypto.QPS.VERSION_QUERY_PARAM_NAME;
  this.ENCRYPTED_PARAMS_PARAM_NAME = 
    PROT_UrlCrypto.QPS.ENCRYPTED_PARAMS_PARAM_NAME;
  this.COUNT_QUERY_PARAM_NAME = PROT_UrlCrypto.QPS.COUNT_QUERY_PARAM_NAME;
  this.WRAPPEDKEY_QUERY_PARAM_NAME = 
    PROT_UrlCrypto.QPS.WRAPPEDKEY_QUERY_PARAM_NAME;
}

// The version of encryption we implement
PROT_UrlCrypto.VERSION = "1";

PROT_UrlCrypto.RC4_DISCARD_BYTES = 1600;

// The query params are we going to send to let the server know what is
// encrypted, and how
PROT_UrlCrypto.QPS = {};
PROT_UrlCrypto.QPS.VERSION_QUERY_PARAM_NAME = "encver";
PROT_UrlCrypto.QPS.ENCRYPTED_PARAMS_PARAM_NAME = "encparams";
PROT_UrlCrypto.QPS.COUNT_QUERY_PARAM_NAME = "nonce";
PROT_UrlCrypto.QPS.WRAPPEDKEY_QUERY_PARAM_NAME = "wrkey";

/**
 * @returns Reference to the keymanager (if one exists), else undefined
 */
PROT_UrlCrypto.prototype.getManager = function() {
  return this.manager_;
}

/**
 * Helper method that takes a map of query params (param name ->
 * value) and turns them into a query string. Note that it encodes
 * the values as it writes the string.
 *
 * @param params Object (map) of query names to values. Values should
 *               not be uriencoded.
 *
 * @returns String of query params from the map. Values will be uri
 *          encoded
 */
PROT_UrlCrypto.prototype.appendParams_ = function(params) {
  var queryString = "";
  for (var param in params)
    queryString += "&" + param + "=" + encodeURIComponent(params[param]);
                   
  return queryString;
}

/**
 * Encrypt a set of query params if we can. If we can, we return a new
 * set of query params that should be added to a query string. The set
 * of query params WILL BE different than the input query params if we
 * can encrypt (e.g., there will be extra query params with meta-
 * information such as the version of encryption we're using). If we
 * can't encrypt, we just return the query params we're passed.
 *
 * @param params Object (map) of query param names to values. Values should
 *               not be uriencoded.
 *
 * @returns Object (map) of query param names to values. Values are NOT
 *          uriencoded; the caller should encode them as it writes them
 *          to a proper query string.
 */
PROT_UrlCrypto.prototype.maybeCryptParams = function(params) {
  if (!this.manager_)
    throw new Error("Need a key manager for UrlCrypto");
  if (typeof params != "object")
    throw new Error("params is an associative array of name/value params");

  var clientKeyArray = this.manager_.getClientKeyArray();
  var wrappedKey = this.manager_.getWrappedKey();

  // No keys? Can't encrypt. Damn.
  if (!clientKeyArray || !wrappedKey) {
    G_Debug(this, "No key; can't encrypt query params");
    return params;
  }

  // Serialize query params to a query string that we will then
  // encrypt and place in a special query param the front-end knows is
  // encrypted.
  var queryString = this.appendParams_(params);
  
  // Nonce, really. We want 32 bits; make it so.
  var counter = this.getCount_();
  counter = counter & 0xFFFFFFFF;
  
  var encrypted = this.encryptV1(clientKeyArray, 
                                 this.VERSION,
                                 counter,
                                 this.base64_.arrayifyString(queryString));

  params = {};
  params[this.VERSION_QUERY_PARAM_NAME] = this.VERSION;
  params[this.COUNT_QUERY_PARAM_NAME] = counter;
  params[this.WRAPPEDKEY_QUERY_PARAM_NAME] = wrappedKey;
  params[this.ENCRYPTED_PARAMS_PARAM_NAME] = encrypted;

  return params;
}

/**
 * Encrypt something IN PLACE. Did you hear that? It works IN PLACE.
 * That is, it replaces the plaintext with ciphertext. It also returns
 * the websafe base64-encoded ciphertext. The client key is untouched.
 *
 * This method runs in about ~5ms on a 2Ghz P4. (Turn debugging off if
 * you see it much slower).
 *
 * @param clientKeyArray Array of bytes (numbers in [0,255]) composing K_C
 *
 * @param version String indicating the version of encryption we should use.
 *
 * @param counter Number that acts as a nonce for this encryption
 *
 * @param inOutArray Array of plaintext bytes that will be replaced
 *                   with the array of ciphertext bytes
 *
 * @returns String containing the websafe base64-encoded ciphertext
 */
PROT_UrlCrypto.prototype.encryptV1 = function(clientKeyArray,
                                              version, 
                                              counter,
                                              inOutArray) {

  // We're a version1 encrypter, after all
  if (version != "1") 
    throw new Error("Unknown encryption version");

  var key = this.deriveEncryptionKey(clientKeyArray, counter);

  this.rc4_.setKey(key, key.length);

  if (this.RC4_DISCARD_BYTES > 0)
    this.rc4_.discard(this.RC4_DISCARD_BYTES);
  
  // The crypt() method works in-place
  this.rc4_.crypt(inOutArray, inOutArray.length);
  
  return this.base64_.encodeByteArray(inOutArray, true /* websafe */);
}
  
/**
 * Create an encryption key from K_C and a nonce
 *
 * @param clientKeyArray Array of bytes comprising K_C
 *
 * @param count Number that acts as a nonce for this key
 *
 * @returns Array of bytes containing the encryption key
 */
PROT_UrlCrypto.prototype.deriveEncryptionKey = function(clientKeyArray, 
                                                        count) {
  G_Assert(this, clientKeyArray instanceof Array,
           "Client key should be an array of bytes");
  G_Assert(this, typeof count == "number", "Count should be a number");
  
  // Don't clobber the client key by appending the nonce; use another array
  var paddingArray = [];
  paddingArray.push(count >> 24);
  paddingArray.push((count >> 16) & 0xFF);
  paddingArray.push((count >> 8) & 0xFF);
  paddingArray.push(count & 0xFF);

  this.hasher_.init(G_CryptoHasher.algorithms.MD5);
  this.hasher_.updateFromArray(clientKeyArray);
  this.hasher_.updateFromArray(paddingArray);

  return this.base64_.arrayifyString(this.hasher_.digestRaw());
}

/**
 * Return a new nonce for us to use. Rather than keeping a counter and
 * the headaches that entails, just use the low ms since the epoch.
 *
 * @returns 32-bit number that is the nonce to use for this encryption
 */
PROT_UrlCrypto.prototype.getCount_ = function() {
  return ((new Date).getTime() & 0xFFFFFFFF);
}


/**
 * Cheeseball unittest
 */ 
function TEST_PROT_UrlCrypto() {
  if (G_GDEBUG) {
    var z = "urlcrypto UNITTEST";
    G_debugService.enableZone(z);

    G_Debug(z, "Starting");

    // We set keys on the keymanager to ensure data flows properly, so 
    // make sure we can clean up after it

    var kf = "test.txt";
    function removeTestFile(f) {
      var appDir = new PROT_ApplicationDirectory();
      var file = appDir.getAppDirFileInterface();
      file.append(f);
      if (file.exists())
        file.remove(false /* do not recurse */ );
    };
    removeTestFile(kf);

    var km = new PROT_UrlCryptoKeyManager(kf, true /* testing */);

    // Test operation when it's not intialized

    var c = new PROT_UrlCrypto();

    var fakeManager = {
      getClientKeyArray: function() { return null; },
      getWrappedKey: function() { return null; },
    };
    c.manager_ = fakeManager;

    var params = {
      foo: "bar",
      baz: "bomb",
    };
    G_Assert(z, c.maybeCryptParams(params)["foo"] === "bar",
             "How can we encrypt if we don't have a key?");
    c.manager_ = km;
    G_Assert(z, c.maybeCryptParams(params)["foo"] === "bar",
             "Again, how can we encrypt if we don't have a key?");

    // Now we have a key! See if we can get a crypted url
    var realResponse = "clientkey:24:dtmbEN1kgN/LmuEoYifaFw==\n" +
                       "wrappedkey:24:MTpPH3pnLDKihecOci+0W5dk";
    km.onGetKeyResponse(realResponse);
    var crypted = c.maybeCryptParams(params);
    G_Assert(z, crypted["foo"] === undefined, "We have a key but can't crypt");
    G_Assert(z, crypted["bomb"] === undefined, "We have a key but can't crypt");

    // Now check to make sure all the expected query params are there
    for (var p in PROT_UrlCrypto.QPS)
      G_Assert(z, crypted[PROT_UrlCrypto.QPS[p]] != undefined, 
               "Output query params doesn't have: " + PROT_UrlCrypto.QPS[p]);
    
    // Now test that encryption is determinisitic
    var b64 = new G_Base64();
    
    // Some helper functions
    function arrayEquals(a1, a2) {
      if (a1.length != a2.length)
        return false;
      
      for (var i = 0; i < a1.length; i++)
        if (typeof a1[i] != typeof a2[i] || a1[i] != a2[i])
          return false;
      return true;
    };

    function arrayAsString(a) {
      var s = "[";
      for (var i = 0; i < a.length; i++)
        s += a[i] + ",";
      return s + "]";
    };

    function printArray(a) {
      var s = arrayAsString(a);
      G_Debug(z, s);
    };

    var keySizeBytes = km.clientKeyArray_.length;

    var startCrypt = (new Date).getTime();
    var numCrypts = 0;

    // Set this to true for extended testing
    var doLongTest = false;
    if (doLongTest) {
      
      // Now run it through its paces. For a variety of keys of a
      // variety of lengths, and a variety of coutns, encrypt
      // plaintexts of different lengths

      // For a variety of key lengths...
      for (var i = 0; i < 2 * keySizeBytes; i++) {
        var clientKeyArray = [];
        
        // For a variety of keys...
        for (var j = 0; j < keySizeBytes; j++)
          clientKeyArray[j] = i + j;
        
        // For a variety of counts...
        for (var count = 0; count < 40; count++) {
        
          var payload = "";

          // For a variety of plaintexts of different lengths
          for (var payloadPadding = 0; payloadPadding < count; payloadPadding++)
            payload += "a";
          
          var plaintext1 = b64.arrayifyString(payload);
          var plaintext2 = b64.arrayifyString(payload);
          var plaintext3 = b64.arrayifyString(payload);
          
          // Verify that encryption is deterministic given set parameters
          numCrypts++;
          var ciphertext1 = c.encryptV1(clientKeyArray, 
                                      "1", 
                                        count,
                                        plaintext1);
          
          numCrypts++;        
          var ciphertext2 = c.encryptV1(clientKeyArray, 
                                        "1", 
                                        count,
                                        plaintext2);
          
          G_Assert(z, ciphertext1 === ciphertext2,
                   "Two plaintexts having different ciphertexts:" +
                   ciphertext1 + " " + ciphertext2);
          
          numCrypts++;

          // Now verify that it is symmetrical
          var ciphertext3 = c.encryptV1(clientKeyArray, 
                                        "1", 
                                        count,
                                      b64.decodeString(ciphertext2), 
                                        true /* websafe */);
          
          G_Assert(z, arrayEquals(plaintext3, b64.decodeString(ciphertext3, 
                                                              true/*websafe*/)),
                   "Encryption and decryption not symmetrical");
        }
      }
      
      // Output some interesting info
      var endCrypt = (new Date).getTime();
      var totalMS = endCrypt - startCrypt;
      G_Debug(z, "Info: Did " + numCrypts + " encryptions in " +
              totalMS + "ms, for an average of " + 
              (totalMS / numCrypts) + "ms per crypt()");
    }
      
    // Now check for compatability with C++

    var ciphertexts = {}; 
    // Generated below, and tested in C++ as well. Ciphertexts is a map
    // from substring lengths to encrypted values.

    ciphertexts[0]="";
    ciphertexts[1]="dA==";
    ciphertexts[2]="akY=";
    ciphertexts[3]="u5mV";
    ciphertexts[4]="bhtioQ==";
    ciphertexts[5]="m2wSZnQ=";
    ciphertexts[6]="zd6gWyDO";
    ciphertexts[7]="bBN0WVrlCg==";
    ciphertexts[8]="Z6U_6bMelFM=";
    ciphertexts[9]="UVoiytL-gHzp";
    ciphertexts[10]="3Xr_ZMmdmvg7zw==";
    ciphertexts[11]="PIIyif7NFRS57mY=";
    ciphertexts[12]="QEKXrRWdZ3poJVSp";
    ciphertexts[13]="T3zsAsooHuAnflNsNQ==";
    ciphertexts[14]="qgYtOJjZSIByo0KtOG0=";
    ciphertexts[15]="NsEGHGK6Ju6FjD59Byai";
    ciphertexts[16]="1RVIsC0HYoUEycoA_0UL2w==";
    ciphertexts[17]="0xXe6Lsb1tZ79T96AJTT-ps=";
    ciphertexts[18]="cVXQCYoA4RV8t1CODXuCS88y";
    ciphertexts[19]="hVf4pd4WP4wPwSyqEXRRkQZSQA==";
    ciphertexts[20]="F6Y9MHwhd1e-bDHhqNSonZbR2Sg=";
    ciphertexts[21]="TiMClYbLUdyYweW8IDytU_HD2wTM";
    ciphertexts[22]="tYQtNqz83KXE4eqn6GhAu6ZZ23SqYw==";
    ciphertexts[23]="qjL-dMpiQ2LYgkYT5IfmE1FlN36wHek=";
    ciphertexts[24]="cL7HHiOZ9PbkvZ9yrJLiv4HXcw4Nf7y7";
    ciphertexts[25]="k4I-fdR6CyzxOpR_QEG5rnvPB8IbmRnpFg==";
    ciphertexts[26]="7LjCfA1dCMjAVT_O8DpiTQ0G7igwQ1HTUMU=";
    ciphertexts[27]="CAtijc6nB-REwAkqimToMn8RC_eZAaJy9Gn4";
    ciphertexts[28]="z8sEB1lDI32wsOkgYbVZ5pxIbpCrha9BmcqxFQ==";
    ciphertexts[29]="2eysfzsfGav0vPRsSnFl8H8fg9dQCT_bSiZwno0=";
    ciphertexts[30]="2BBNlF_mtV9TB2jZHHqCAtzkJQFdVKFn7N8YxsI9";
    ciphertexts[31]="9h4-nldHAr77Boks7lPzsi8TwVCIQzSkiJp2xatbGg==";
    ciphertexts[32]="DHTB8bDTXpUIrZ2ZlAujXLi-501NoWUVIEQJLaKCpqQ=";
    ciphertexts[33]="E9Av2GgnZg_q5r-JLSzM_ShCu1yPF2VeCaQfPPXSSE4I";
    ciphertexts[34]="UJzEucVBnGEfRNBQ6tvbaro0_I_-mQeJMpU2zQnfFdBuFg==";
    ciphertexts[35]="_p0OYras-Vn2rQ9X-J0dFRnhCfytuTEjheUTU7Ueaf1rIA4=";
    ciphertexts[36]="Q0nZXFPJbpx1WZPP-lLPuSGR-pD08B4CAW-6Uf0eEkS05-oM";
    ciphertexts[37]="XeKfieZGc9bPh7nRtCgujF8OY14zbIZSK20Lwg1HTpHi9HfXVQ==";
    
    var clientKeyArray = b64.decodeString("dtmbEN1kgN/LmuEoYifaFw==");
    // wrappedKey was "MTpPH3pnLDKihecOci+0W5dk"
    var count = 0xFEDCBA09;
    var plaintext = "http://www.foobar.com/this?is&some=url";
    
    // For every substring of the plaintext, change the count and verify
    // that we get what we expect when we encrypt

    for (var i = 0; i < plaintext.length; i++) {
      var plaintextArray = b64.arrayifyString(plaintext.substring(0, i));
      var crypted = c.encryptV1(clientKeyArray,
                                "1",
                                count + i,
                                plaintextArray);
      G_Assert(z, crypted === ciphertexts[i], 
               "Generated unexpected ciphertext");

      // Uncomment to generate
      // dump("\nciphertexts[" + i + "]=\"" + crypted + "\";");
    }

    removeTestFile(kf);

    G_Debug(z, "PASS");
  }
}
