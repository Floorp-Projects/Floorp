/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global tv */

const WEBCRYPTO_ALG_PROBE = "WEBCRYPTO_ALG";

ChromeUtils.defineModuleGetter(this, "TelemetryTestUtils",
  "resource://testing-common/TelemetryTestUtils.jsm");

function validateHistogramEntryCount(aHistogramName, aExpectedTotalCount, aBucketCounts) {
  let hist = Services.telemetry.getHistogramById(aHistogramName);
  let resultIndexes = hist.snapshot();

  let entriesSeen = Object.values(resultIndexes.values).reduce((a, b) => a + b, 0);

  is(entriesSeen, aExpectedTotalCount, `Expecting ${aExpectedTotalCount} histogram entries in ` +
     aHistogramName);

  for (let bucket in aBucketCounts) {
    is(resultIndexes.values[bucket], aBucketCounts[bucket], `Expecting bucket ${bucket} to be ` +
      aBucketCounts[bucket]);
  }
}

function cleanupTelemetry() {
  Services.telemetry.clearScalars();
  Services.telemetry.clearEvents();
  Services.telemetry.getHistogramById(WEBCRYPTO_ALG_PROBE).clear();
}

add_task(async function ecdh_key() {
  cleanupTelemetry();

  var alg = { name: "ECDH", namedCurve: "P-256" };

  let x = await crypto.subtle.generateKey(alg, false, ["deriveKey", "deriveBits"]);
  await crypto.subtle.deriveBits({ name: "ECDH", public: x.publicKey }, x.privateKey, 128);

  validateHistogramEntryCount(WEBCRYPTO_ALG_PROBE, 1, {20: 1});
});


add_task(async function dh_key() {
  cleanupTelemetry();

  var alg = {
    name: "DH",
    prime: tv.dh.prime,
    generator: new Uint8Array([0x02]),
  };

  let x = await crypto.subtle.generateKey(alg, false, ["deriveKey", "deriveBits"]);
  await crypto.subtle.deriveBits({ name: "DH", public: x.publicKey }, x.privateKey, 128);

  validateHistogramEntryCount(WEBCRYPTO_ALG_PROBE, 1, {24: 1});
});
