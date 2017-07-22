// Used by local_addTest() / local_completeTest()
var _countCompletions = 0;
var _expectedCompletions = 0;

function handleEventMessage(event) {
  if ("test" in event.data) {
    let summary = event.data.test + ": " + event.data.msg;
    log(event.data.status + ": " + summary);
    ok(event.data.status, summary);
  } else if ("done" in event.data) {
    SimpleTest.finish();
  } else {
    ok(false, "Unexpected message in the test harness: " + event.data)
  }
}

function log(msg) {
  console.log(msg)
  let logBox = document.getElementById("log");
  if (logBox) {
    logBox.textContent += "\n" + msg;
  }
}

function local_is(value, expected, message) {
  if (value === expected) {
    local_ok(true, message);
  } else {
    local_ok(false, message + " unexpectedly: " + value + " !== " + expected);
  }
}

function local_isnot(value, expected, message) {
  if (value !== expected) {
    local_ok(true, message);
  } else {
    local_ok(false, message + " unexpectedly: " + value + " === " + expected);
  }
}

function local_ok(expression, message) {
  let body = {"test": this.location.pathname, "status":expression, "msg": message}
  parent.postMessage(body, "http://mochi.test:8888");
}

function local_doesThrow(fn, name) {
  let gotException = false;
  try {
    fn();
  } catch (ex) { gotException = true; }
  local_ok(gotException, name);
};

function local_expectThisManyTests(count) {
  if (_expectedCompletions > 0) {
    local_ok(false, "Error: local_expectThisManyTests should only be called once.");
  }
  _expectedCompletions = count;
}

function local_completeTest() {
  _countCompletions += 1
  if (_countCompletions == _expectedCompletions) {
    log("All tests completed.")
    local_finished();
  }
  if (_countCompletions > _expectedCompletions) {
    local_ok(false, "Error: local_completeTest called more than local_addTest.");
  }
}

function local_finished() {
  parent.postMessage({"done":true}, "http://mochi.test:8888");
}

function string2buffer(str) {
  return (new Uint8Array(str.length)).map((x, i) => str.charCodeAt(i));
}

function buffer2string(buf) {
  let str = "";
  buf.map(x => str += String.fromCharCode(x));
  return str;
}

function bytesToBase64(u8a){
  let CHUNK_SZ = 0x8000;
  let c = [];
  for (let i = 0; i < u8a.length; i += CHUNK_SZ) {
    c.push(String.fromCharCode.apply(null, u8a.subarray(i, i + CHUNK_SZ)));
  }
  return window.btoa(c.join(""));
}

function base64ToBytes(b64encoded) {
  return new Uint8Array(window.atob(b64encoded).split("").map(function(c) {
    return c.charCodeAt(0);
  }));
}

function bytesToBase64UrlSafe(buf) {
  return bytesToBase64(buf)
                 .replace(/\+/g, "-")
                 .replace(/\//g, "_")
                 .replace(/=/g, "");
}

function base64ToBytesUrlSafe(str) {
  if (str.length % 4 == 1) {
    throw "Improper b64 string";
  }

  var b64 = str.replace(/\-/g, "+").replace(/\_/g, "/");
  while (b64.length % 4 != 0) {
    b64 += "=";
  }
  return base64ToBytes(b64);
}

function hexEncode(buf) {
  return Array.from(buf)
              .map(x => ("0"+x.toString(16)).substr(-2))
              .join("");
}

function hexDecode(str) {
  return new Uint8Array(str.match(/../g).map(x => parseInt(x, 16)));
}

function webAuthnDecodeAttestation(aAttestationBuf) {
  let attObj = CBOR.decode(aAttestationBuf);
  console.log("Attestation CBOR Object:", attObj);
  if (!("authData" in attObj && "fmt" in attObj && "attStmt" in attObj)) {
    throw "Invalid CBOR Attestation Object";
  }
  if (!("sig" in attObj.attStmt && "x5c" in attObj.attStmt)) {
    throw "Invalid CBOR Attestation Statement";
  }

  let rpIdHash = attObj.authData.slice(0, 32);
  let flags = attObj.authData.slice(32, 33);
  let counter = attObj.authData.slice(33, 37);
  let attData = {};
  attData.aaguid = attObj.authData.slice(37, 53);
  attData.credIdLen = (attObj.authData[53] << 8) + attObj.authData[54];
  attData.credId = attObj.authData.slice(55, 55 + attData.credIdLen);

  console.log(":: CBOR Attestation Object Data ::");
  console.log("RP ID Hash: " + hexEncode(rpIdHash));
  console.log("Counter: " + hexEncode(counter) + " Flags: " + flags);
  console.log("AAGUID: " + hexEncode(attData.aaguid));

  cborPubKey = attObj.authData.slice(55 + attData.credIdLen);
  var pubkeyObj = CBOR.decode(cborPubKey.buffer);
  if (!("alg" in pubkeyObj && "x" in pubkeyObj && "y" in pubkeyObj)) {
    throw "Invalid CBOR Public Key Object";
  }
  if (pubkeyObj.alg != "ES256") {
    throw "Unexpected public key algorithm";
  }

  let pubKeyBytes = assemblePublicKeyBytesData(pubkeyObj.x, pubkeyObj.y);
  console.log(":: CBOR Public Key Object Data ::");
  console.log("Algorithm: " + pubkeyObj.alg);
  console.log("X: " + pubkeyObj.x);
  console.log("Y: " + pubkeyObj.y);
  console.log("Uncompressed (hex): " + hexEncode(pubKeyBytes));

  return importPublicKey(pubKeyBytes)
  .then(function(aKeyHandle) {
    return {
      attestationObject: attObj,
      attestationAuthData: attData,
      publicKeyBytes: pubKeyBytes,
      publicKeyHandle: aKeyHandle,
    };
  });
}

function importPublicKey(keyBytes) {
  if (keyBytes[0] != 0x04 || keyBytes.byteLength != 65) {
    throw "Bad public key octet string";
  }
  var jwk = {
    kty: "EC",
    crv: "P-256",
    x: bytesToBase64UrlSafe(keyBytes.slice(1, 33)),
    y: bytesToBase64UrlSafe(keyBytes.slice(33))
  };
  return crypto.subtle.importKey("jwk", jwk, {name: "ECDSA", namedCurve: "P-256"}, true, ["verify"])
}

function deriveAppAndChallengeParam(appId, clientData) {
  var appIdBuf = string2buffer(appId);
  return Promise.all([
    crypto.subtle.digest("SHA-256", appIdBuf),
    crypto.subtle.digest("SHA-256", clientData)
  ])
  .then(function(digests) {
    return {
      appParam: new Uint8Array(digests[0]),
      challengeParam: new Uint8Array(digests[1]),
    };
  });
}

function assemblePublicKeyBytesData(xCoord, yCoord) {
  // Produce an uncompressed EC key point. These start with 0x04, and then
  // two 32-byte numbers denoting X and Y.
  if (xCoord.length != 32 || yCoord.length != 32) {
    throw ("Coordinates must be 32 bytes long");
  }
  let keyBytes = new Uint8Array(65);
  keyBytes[0] = 0x04;
  xCoord.map((x, i) => keyBytes[1 + i] = x);
  yCoord.map((x, i) => keyBytes[33 + i] = x);
  return keyBytes;
}

function assembleSignedData(appParam, presenceAndCounter, challengeParam) {
  let signedData = new Uint8Array(32 + 1 + 4 + 32);
  appParam.map((x, i) => signedData[0 + i] = x);
  presenceAndCounter.map((x, i) => signedData[32 + i] = x);
  challengeParam.map((x, i) => signedData[37 + i] = x);
  return signedData;
}

function assembleRegistrationSignedData(appParam, challengeParam, keyHandle, pubKey) {
  let signedData = new Uint8Array(1 + 32 + 32 + keyHandle.length + 65);
  signedData[0] = 0x00;
  appParam.map((x, i) => signedData[1 + i] = x);
  challengeParam.map((x, i) => signedData[33 + i] = x);
  keyHandle.map((x, i) => signedData[65 + i] = x);
  pubKey.map((x, i) => signedData[65 + keyHandle.length + i] = x);
  return signedData;
}

function sanitizeSigArray(arr) {
  // ECDSA signature fields into WebCrypto must be exactly 32 bytes long, so
  // this method strips leading padding bytes, if added, and also appends
  // padding zeros, if needed.
  if (arr.length > 32) {
    arr = arr.slice(arr.length - 32)
  }
  let ret = new Uint8Array(32);
  ret.set(arr, ret.length - arr.length);
  return ret;
}

function verifySignature(key, data, derSig) {
  let sigAsn1 = org.pkijs.fromBER(derSig.buffer);
  let sigR = new Uint8Array(sigAsn1.result.value_block.value[0].value_block.value_hex);
  let sigS = new Uint8Array(sigAsn1.result.value_block.value[1].value_block.value_hex);

  // The resulting R and S values from the ASN.1 Sequence must be fit into 32
  // bytes. Sometimes they have leading zeros, sometimes they're too short, it
  // all depends on what lib generated the signature.
  let R = sanitizeSigArray(sigR);
  let S = sanitizeSigArray(sigS);

  console.log("Verifying these bytes: " + bytesToBase64UrlSafe(data));

  let sigData = new Uint8Array(R.length + S.length);
  sigData.set(R);
  sigData.set(S, R.length);

  let alg = {name: "ECDSA", hash: "SHA-256"};
  return crypto.subtle.verify(alg, key, sigData, data);
}
