/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global tv */

const WEBCRYPTO_ALG_PROBE = "WEBCRYPTO_ALG";

ChromeUtils.defineModuleGetter(this, "TelemetryTestUtils",
  "resource://testing-common/TelemetryTestUtils.jsm");

add_task(async function ecdh_key() {
  let hist = TelemetryTestUtils.getAndClearHistogram(WEBCRYPTO_ALG_PROBE);

  let alg = { name: "ECDH", namedCurve: "P-256" };

  let x = await crypto.subtle.generateKey(alg, false, ["deriveKey", "deriveBits"]);
  let data = await crypto.subtle.deriveBits({ name: "ECDH", public: x.publicKey }, x.privateKey, 128);
  is(data.byteLength, 128 / 8, "Should be 16 bytes derived");

  TelemetryTestUtils.assertHistogram(hist, 20, 1);
});


add_task(async function dh_key() {
  let hist = TelemetryTestUtils.getAndClearHistogram(WEBCRYPTO_ALG_PROBE);

  let alg = {
    name: "DH",
    prime: tv.dh.prime,
    generator: new Uint8Array([0x02]),
  };

  let x = await crypto.subtle.generateKey(alg, false, ["deriveKey", "deriveBits"]);
  let data = await crypto.subtle.deriveBits({ name: "DH", public: x.publicKey }, x.privateKey, 128);
  is(data.byteLength, 128 / 8, "Should be 16 bytes derived");

  TelemetryTestUtils.assertHistogram(hist, 24, 1);
});
