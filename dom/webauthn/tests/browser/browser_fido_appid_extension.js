/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_URL = "https://example.com/";

let expectNotSupportedError = expectError("NotSupported");
let expectInvalidStateError = expectError("InvalidState");
let expectSecurityError = expectError("Security");

add_task(async function test_appid_unused() {
  // Open a new tab.
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  let appid = "https://example.com/appId";

  let { attObj, rawId } = await promiseWebAuthnMakeCredential(tab);
  let { authDataObj } = await webAuthnDecodeCBORAttestation(attObj);

  // Make sure the RP ID hash matches what we calculate.
  await checkRpIdHash(authDataObj.rpIdHash, "example.com");

  // Get a new assertion.
  let { clientDataJSON, authenticatorData, signature, extensions } =
    await promiseWebAuthnGetAssertion(tab, rawId, { appid });

  ok(
    "appid" in extensions,
    `appid should be populated in the extensions data, but saw: ` +
      `${JSON.stringify(extensions)}`
  );
  is(extensions.appid, false, "appid extension should indicate it was unused");

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

  // Close tab.
  BrowserTestUtils.removeTab(tab);
});
