/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_URL = "https://example.com/";

let expectNotSupportedError = expectError("NotSupported");
let expectInvalidStateError = expectError("InvalidState");
let expectSecurityError = expectError("Security");

function promiseU2FRegister(tab, app_id_) {
  let challenge_ = crypto.getRandomValues(new Uint8Array(16));
  challenge_ = bytesToBase64UrlSafe(challenge_);

  return SpecialPowers.spawn(
    tab.linkedBrowser,
    [[app_id_, challenge_]],
    function([app_id, challenge]) {
      return new Promise(resolve => {
        content.u2f.register(
          app_id,
          [{ version: "U2F_V2", challenge }],
          [],
          resolve
        );
      });
    }
  ).then(res => {
    is(res.errorCode, 0, "u2f.register() succeeded");
    let data = base64ToBytesUrlSafe(res.registrationData);
    is(data[0], 0x05, "Reserved byte is correct");
    return data.slice(67, 67 + data[66]);
  });
}

add_task(function test_setup() {
  Services.prefs.setBoolPref("security.webauth.u2f", true);
  Services.prefs.setBoolPref("security.webauth.webauthn", true);
  Services.prefs.setBoolPref(
    "security.webauth.webauthn_enable_android_fido2",
    false
  );
  Services.prefs.setBoolPref(
    "security.webauth.webauthn_enable_softtoken",
    true
  );
  Services.prefs.setBoolPref(
    "security.webauth.webauthn_enable_usbtoken",
    false
  );
});

add_task(async function test_appid() {
  // Open a new tab.
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  // Get a keyHandle for a FIDO AppId.
  let appid = "https://example.com/appId";
  let keyHandle = await promiseU2FRegister(tab, appid);

  // The FIDO AppId extension can't be used for MakeCredential.
  await promiseWebAuthnMakeCredential(tab, "none", { appid })
    .then(arrivingHereIsBad)
    .catch(expectNotSupportedError);

  // Using the keyHandle shouldn't work without the FIDO AppId extension.
  // This will be an invalid state, because the softtoken will consent without
  // having the correct "RP ID" via the FIDO extension.
  await promiseWebAuthnGetAssertion(tab, keyHandle)
    .then(arrivingHereIsBad)
    .catch(expectInvalidStateError);

  // Invalid app IDs (for the current origin) must be rejected.
  await promiseWebAuthnGetAssertion(tab, keyHandle, {
    appid: "https://bogus.com/appId",
  })
    .then(arrivingHereIsBad)
    .catch(expectSecurityError);

  // Non-matching app IDs must be rejected. Even when the user/softtoken
  // consents, leading to an invalid state.
  await promiseWebAuthnGetAssertion(tab, keyHandle, { appid: appid + "2" })
    .then(arrivingHereIsBad)
    .catch(expectInvalidStateError);

  let rpId = new TextEncoder("utf-8").encode(appid);
  let rpIdHash = await crypto.subtle.digest("SHA-256", rpId);

  // Succeed with the right fallback rpId.
  await promiseWebAuthnGetAssertion(tab, keyHandle, { appid }).then(
    ({ authenticatorData, clientDataJSON, extensions }) => {
      is(extensions.appid, true, "appid extension was acted upon");

      // Check that the correct rpIdHash is returned.
      let rpIdHashSign = authenticatorData.slice(0, 32);
      ok(memcmp(rpIdHash, rpIdHashSign), "rpIdHash is correct");
    }
  );

  // Close tab.
  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_appid_unused() {
  // Open a new tab.
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  // Get a keyHandle for a FIDO AppId.
  let appid = "https://example.com/appId";

  let { attObj, rawId } = await promiseWebAuthnMakeCredential(tab);
  let { authDataObj } = await webAuthnDecodeCBORAttestation(attObj);

  // Make sure the RP ID hash matches what we calculate.
  await checkRpIdHash(authDataObj.rpIdHash, "example.com");

  // Get a new assertion.
  let {
    clientDataJSON,
    authenticatorData,
    signature,
    extensions,
  } = await promiseWebAuthnGetAssertion(tab, rawId, { appid });

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

add_task(function test_cleanup() {
  Services.prefs.clearUserPref("security.webauth.u2f");
  Services.prefs.clearUserPref("security.webauth.webauthn");
  Services.prefs.clearUserPref("security.webauth.webauthn_enable_softtoken");
  Services.prefs.clearUserPref("security.webauth.webauthn_enable_usbtoken");
});
