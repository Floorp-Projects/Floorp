/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function bytesToBase64(u8a){
  let CHUNK_SZ = 0x8000;
  let c = [];
  for (let i = 0; i < u8a.length; i += CHUNK_SZ) {
    c.push(String.fromCharCode.apply(null, u8a.subarray(i, i + CHUNK_SZ)));
  }
  return window.btoa(c.join(""));
}

function bytesToBase64UrlSafe(buf) {
  return bytesToBase64(buf)
                 .replace(/\+/g, "-")
                 .replace(/\//g, "_")
                 .replace(/=/g, "");
}

function base64ToBytes(b64encoded) {
  return new Uint8Array(window.atob(b64encoded).split("").map(function(c) {
    return c.charCodeAt(0);
  }));
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

function buffer2string(buf) {
  let str = "";
  if (!(buf.constructor === Uint8Array)) {
    buf = new Uint8Array(buf);
  }
  buf.map(function(x){ return str += String.fromCharCode(x) });
  return str;
}

function memcmp(x, y) {
  let xb = new Uint8Array(x);
  let yb = new Uint8Array(y);

  if (x.byteLength != y.byteLength) {
    return false;
  }

  for (let i = 0; i < xb.byteLength; ++i) {
    if (xb[i] != yb[i]) {
      return false;
    }
  }

  return true;
}
