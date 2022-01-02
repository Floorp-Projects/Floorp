/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "TelemetryTestUtils",
  "resource://testing-common/TelemetryTestUtils.jsm"
);

const TEST_URL = "https://example.com/";

function getTelemetryForScalar(aName) {
  let scalars = TelemetryTestUtils.getProcessScalars("parent", true);
  return scalars[aName] || 0;
}

function cleanupTelemetry() {
  Services.telemetry.clearScalars();
  Services.telemetry.clearEvents();
  Services.telemetry.getHistogramById("WEBAUTHN_CREATE_CREDENTIAL_MS").clear();
  Services.telemetry.getHistogramById("WEBAUTHN_GET_ASSERTION_MS").clear();
}

function validateHistogramEntryCount(aHistogramName, aExpectedCount) {
  let hist = Services.telemetry.getHistogramById(aHistogramName);
  let resultIndexes = hist.snapshot();

  let entriesSeen = Object.values(resultIndexes.values).reduce(
    (a, b) => a + b,
    0
  );

  is(
    entriesSeen,
    aExpectedCount,
    "Expecting " + aExpectedCount + " histogram entries in " + aHistogramName
  );
}

add_task(async function test_setup() {
  cleanupTelemetry();

  await SpecialPowers.pushPrefEnv({
    set: [
      ["security.webauth.webauthn", true],
      ["security.webauth.webauthn_enable_softtoken", true],
      ["security.webauth.webauthn_enable_usbtoken", false],
      ["security.webauth.webauthn_enable_android_fido2", false],
      ["security.webauth.webauthn_testing_allow_direct_attestation", true],
    ],
  });
});

add_task(async function test() {
  // These tests can't run simultaneously as the preference changes will race.
  // So let's run them sequentially here, but in an async function so we can
  // use await.

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  // Create a new credential.
  let { attObj, rawId } = await promiseWebAuthnMakeCredential(tab);
  let { authDataObj } = await webAuthnDecodeCBORAttestation(attObj);

  // Make sure the RP ID hash matches what we calculate.
  await checkRpIdHash(authDataObj.rpIdHash, "example.com");

  // Get a new assertion.
  let {
    clientDataJSON,
    authenticatorData,
    signature,
  } = await promiseWebAuthnGetAssertion(tab, rawId);

  // Check the we can parse clientDataJSON.
  JSON.parse(buffer2string(clientDataJSON));

  // Check auth data.
  let attestation = await webAuthnDecodeAuthDataArray(
    new Uint8Array(authenticatorData)
  );
  is(
    "" + attestation.flags,
    "" + flag_TUP,
    "Assertion's user presence byte set correctly"
  );

  // Verify the signature.
  let params = await deriveAppAndChallengeParam(
    "example.com",
    clientDataJSON,
    attestation
  );
  let signedData = await assembleSignedData(
    params.appParam,
    params.attestation.flags,
    params.attestation.counter,
    params.challengeParam
  );
  let valid = await verifySignature(
    authDataObj.publicKeyHandle,
    signedData,
    signature
  );
  ok(valid, "signature is valid");

  // Check telemetry data.
  let webauthn_used = getTelemetryForScalar("security.webauthn_used");
  ok(
    webauthn_used,
    "Scalar keys are set: " + Object.keys(webauthn_used).join(", ")
  );
  is(
    webauthn_used.U2FRegisterFinish,
    1,
    "webauthn_used U2FRegisterFinish scalar should be 1"
  );
  is(
    webauthn_used.U2FSignFinish,
    1,
    "webauthn_used U2FSignFinish scalar should be 1"
  );
  is(
    webauthn_used.U2FSignAbort,
    undefined,
    "webauthn_used U2FSignAbort scalar must be unset"
  );
  is(
    webauthn_used.U2FRegisterAbort,
    undefined,
    "webauthn_used U2FRegisterAbort scalar must be unset"
  );

  validateHistogramEntryCount("WEBAUTHN_CREATE_CREDENTIAL_MS", 1);
  validateHistogramEntryCount("WEBAUTHN_GET_ASSERTION_MS", 1);

  BrowserTestUtils.removeTab(tab);

  // There aren't tests for register succeeding and sign failing, as I don't see an easy way to prompt
  // the soft token to fail that way _and_ trigger the Abort telemetry.
});
