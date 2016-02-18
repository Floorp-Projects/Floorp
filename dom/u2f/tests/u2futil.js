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
  var gotException = false;
  try {
    fn();
  } catch (ex) { gotException = true; }
  local_ok(gotException, name);
};

function local_finished() {
  parent.postMessage({"done":true}, "http://mochi.test:8888");
}

function string2buffer(str) {
  return (new Uint8Array(str.length)).map((x, i) => str.charCodeAt(i));
}

function buffer2string(buf) {
  var str = "";
  buf.map(x => str += String.fromCharCode(x));
  return str;
}

function bytesToBase64(u8a){
  var CHUNK_SZ = 0x8000;
  var c = [];
  for (var i = 0; i < u8a.length; i += CHUNK_SZ) {
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

function assembleSignedData(appId, presenceAndCounter, clientData) {
  var appIdBuf = string2buffer(appId);
  return Promise.all([
    crypto.subtle.digest("SHA-256", appIdBuf),
    crypto.subtle.digest("SHA-256", clientData)
  ])
  .then(function(digests) {
    var appParam = new Uint8Array(digests[0]);
    var clientParam = new Uint8Array(digests[1]);

    var signedData = new Uint8Array(32 + 1 + 4 + 32);
    appParam.map((x, i) => signedData[0 + i] = x);
    presenceAndCounter.map((x, i) => signedData[32 + i] = x);
    clientParam.map((x, i) => signedData[37 + i] = x);
    return signedData;
  });
}

function verifySignature(key, data, derSig) {
  if (derSig.byteLength < 70) {
    console.log("bad sig: " + hexEncode(derSig))
    throw "Invalid signature length: " + derSig.byteLength;
  }

  // Poor man's ASN.1 decode
  // R and S are always 32 bytes.  If ether has a DER
  // length > 32, it's just zeros we can chop off.
  var lenR = derSig[3];
  var lenS = derSig[3 + lenR + 2];
  var padR = lenR - 32;
  var padS = lenS - 32;
  var sig = new Uint8Array(64);
  derSig.slice(4 + padR, 4 + lenR).map((x, i) => sig[i] = x);
  derSig.slice(4 + lenR + 2 + padS, 4 + lenR + 2 + lenS).map(
    (x, i) => sig[32 + i] = x
  );

  console.log("data: " + hexEncode(data));
  console.log("der:  " + hexEncode(derSig));
  console.log("raw:  " + hexEncode(sig));

  var alg = {name: "ECDSA", hash: "SHA-256"};
  return crypto.subtle.verify(alg, key, sig, data);
}