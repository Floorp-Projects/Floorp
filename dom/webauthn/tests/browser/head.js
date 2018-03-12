/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/dom/webauthn/tests/browser/cbor.js",
  this);
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/dom/webauthn/tests/browser/u2futil.js",
  this);

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
