/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_URL = "https://example.com/";

let expectNotSupportedError = expectError("NotSupported");
let expectNotAllowedError = expectError("NotAllowed");
let expectSecurityError = expectError("Security");

let gAppId = "https://example.com/appId";
let gCrossOriginAppId = "https://example.org/appId";
let gAuthenticatorId = add_virtual_authenticator();

add_task(async function test_appid() {
  // Open a new tab.
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  // The FIDO AppId extension can't be used for MakeCredential.
  await promiseWebAuthnMakeCredential(tab, "none", "discouraged", {
    appid: gAppId,
  })
    .then(arrivingHereIsBad)
    .catch(expectNotSupportedError);

  // Side-load a credential with an RP ID matching the App ID.
  let credIdB64 = await addCredential(gAuthenticatorId, gAppId);
  let credId = base64ToBytesUrlSafe(credIdB64);

  // And another for a different origin
  let crossOriginCredIdB64 = await addCredential(
    gAuthenticatorId,
    gCrossOriginAppId
  );
  let crossOriginCredId = base64ToBytesUrlSafe(crossOriginCredIdB64);

  // The App ID extension is required
  await promiseWebAuthnGetAssertion(tab, credId)
    .then(arrivingHereIsBad)
    .catch(expectNotAllowedError);

  // The value in the App ID extension must match the origin.
  await promiseWebAuthnGetAssertion(tab, crossOriginCredId, {
    appid: gCrossOriginAppId,
  })
    .then(arrivingHereIsBad)
    .catch(expectSecurityError);

  // The value in the App ID extension must match the credential's RP ID.
  await promiseWebAuthnGetAssertion(tab, credId, { appid: gAppId + "2" })
    .then(arrivingHereIsBad)
    .catch(expectNotAllowedError);

  // Succeed with the right App ID.
  let rpIdHash = await promiseWebAuthnGetAssertion(tab, credId, {
    appid: gAppId,
  })
    .then(({ authenticatorData, extensions }) => {
      is(extensions.appid, true, "appid extension was acted upon");
      return authenticatorData.slice(0, 32);
    })
    .then(rpIdHash => {
      // Make sure the returned RP ID hash matches the hash of the App ID.
      checkRpIdHash(rpIdHash, gAppId);
    })
    .catch(arrivingHereIsBad);

  removeCredential(gAuthenticatorId, credIdB64);
  removeCredential(gAuthenticatorId, crossOriginCredIdB64);

  // Close tab.
  BrowserTestUtils.removeTab(tab);
});

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
    "" + (attestation.flags & flag_TUP),
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
