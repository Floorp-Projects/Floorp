function promiseU2FRegister(aAppId, aChallenges, aExcludedKeys, aFunc) {
  return new Promise(function(resolve, reject) {
    u2f.register(aAppId, aChallenges, aExcludedKeys, function(res) {
      aFunc(res);
      resolve(res);
    });
  });
}

function promiseU2FSign(aAppId, aChallenge, aAllowedKeys, aFunc) {
  return new Promise(function(resolve, reject) {
    u2f.sign(aAppId, aChallenge, aAllowedKeys, function(res) {
      aFunc(res);
      resolve(res);
    });
  });
}

function log(msg) {
  console.log(msg);
  let logBox = document.getElementById("log");
  if (logBox) {
    logBox.textContent += "\n" + msg;
  }
}

function string2buffer(str) {
  return new Uint8Array(str.length).map((x, i) => str.charCodeAt(i));
}

function buffer2string(buf) {
  let str = "";
  buf.map(x => (str += String.fromCharCode(x)));
  return str;
}

function bytesToBase64(u8a) {
  let CHUNK_SZ = 0x8000;
  let c = [];
  for (let i = 0; i < u8a.length; i += CHUNK_SZ) {
    c.push(String.fromCharCode.apply(null, u8a.subarray(i, i + CHUNK_SZ)));
  }
  return window.btoa(c.join(""));
}

function base64ToBytes(b64encoded) {
  return new Uint8Array(
    window
      .atob(b64encoded)
      .split("")
      .map(function(c) {
        return c.charCodeAt(0);
      })
  );
}

function bytesToBase64UrlSafe(buf) {
  return bytesToBase64(buf)
    .replace(/\+/g, "-")
    .replace(/\//g, "_")
    .replace(/=/g, "");
}

function base64ToBytesUrlSafe(str) {
  if (!str || str.length % 4 == 1) {
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
    .map(x => ("0" + x.toString(16)).substr(-2))
    .join("");
}

function hexDecode(str) {
  return new Uint8Array(str.match(/../g).map(x => parseInt(x, 16)));
}

function decodeU2FRegistration(aRegData) {
  if (aRegData[0] != 0x05) {
    return Promise.reject("Sentinal byte != 0x05");
  }

  let keyHandleLength = aRegData[66];
  let u2fRegObj = {
    publicKeyBytes: aRegData.slice(1, 66),
    keyHandleBytes: aRegData.slice(67, 67 + keyHandleLength),
    attestationBytes: aRegData.slice(67 + keyHandleLength),
  };

  u2fRegObj.keyHandle = bytesToBase64UrlSafe(u2fRegObj.keyHandleBytes);

  return importPublicKey(u2fRegObj.publicKeyBytes).then(function(keyObj) {
    u2fRegObj.publicKey = keyObj;
    return u2fRegObj;
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
    y: bytesToBase64UrlSafe(keyBytes.slice(33)),
  };
  return crypto.subtle.importKey(
    "jwk",
    jwk,
    { name: "ECDSA", namedCurve: "P-256" },
    true,
    ["verify"]
  );
}

function deriveAppAndChallengeParam(appId, clientData) {
  var appIdBuf = string2buffer(appId);
  return Promise.all([
    crypto.subtle.digest("SHA-256", appIdBuf),
    crypto.subtle.digest("SHA-256", clientData),
  ]).then(function(digests) {
    return {
      appParam: new Uint8Array(digests[0]),
      challengeParam: new Uint8Array(digests[1]),
    };
  });
}

function assembleSignedData(appParam, presenceAndCounter, challengeParam) {
  let signedData = new Uint8Array(32 + 1 + 4 + 32);
  appParam.map((x, i) => (signedData[0 + i] = x));
  presenceAndCounter.map((x, i) => (signedData[32 + i] = x));
  challengeParam.map((x, i) => (signedData[37 + i] = x));
  return signedData;
}

function assembleRegistrationSignedData(
  appParam,
  challengeParam,
  keyHandle,
  pubKey
) {
  let signedData = new Uint8Array(1 + 32 + 32 + keyHandle.length + 65);
  signedData[0] = 0x00;
  appParam.map((x, i) => (signedData[1 + i] = x));
  challengeParam.map((x, i) => (signedData[33 + i] = x));
  keyHandle.map((x, i) => (signedData[65 + i] = x));
  pubKey.map((x, i) => (signedData[65 + keyHandle.length + i] = x));
  return signedData;
}

function sanitizeSigArray(arr) {
  // ECDSA signature fields into WebCrypto must be exactly 32 bytes long, so
  // this method strips leading padding bytes, if added, and also appends
  // padding zeros, if needed.
  if (arr.length > 32) {
    arr = arr.slice(arr.length - 32);
  }
  let ret = new Uint8Array(32);
  ret.set(arr, ret.length - arr.length);
  return ret;
}

function verifySignature(key, data, derSig) {
  let sigAsn1 = org.pkijs.fromBER(derSig.buffer);
  let sigR = new Uint8Array(
    sigAsn1.result.value_block.value[0].value_block.value_hex
  );
  let sigS = new Uint8Array(
    sigAsn1.result.value_block.value[1].value_block.value_hex
  );

  // The resulting R and S values from the ASN.1 Sequence must be fit into 32
  // bytes. Sometimes they have leading zeros, sometimes they're too short, it
  // all depends on what lib generated the signature.
  let R = sanitizeSigArray(sigR);
  let S = sanitizeSigArray(sigS);

  console.log("Verifying these bytes: " + bytesToBase64UrlSafe(data));

  let sigData = new Uint8Array(R.length + S.length);
  sigData.set(R);
  sigData.set(S, R.length);

  let alg = { name: "ECDSA", hash: "SHA-256" };
  return crypto.subtle.verify(alg, key, sigData, data);
}
