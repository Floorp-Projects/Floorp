/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_URL = "https://example.com/";

function arrivingHereIsBad(aResult) {
  ok(false, "Bad result! Received a: " + aResult);
}

function expectError(aType) {
  let expected = `${aType}Error`;
  return function (aResult) {
    is(aResult.slice(0, expected.length), expected, `Expecting a ${aType}Error`);
  };
}

let expectNotSupportedError = expectError("NotSupported");
let expectInvalidStateError = expectError("InvalidState");
let expectSecurityError = expectError("Security");

function promiseU2FRegister(tab, app_id) {
  let challenge = crypto.getRandomValues(new Uint8Array(16));
  challenge = bytesToBase64UrlSafe(challenge);

  return ContentTask.spawn(tab.linkedBrowser, [app_id, challenge], function ([app_id, challenge]) {
    return new Promise(resolve => {
      content.u2f.register(app_id, [{version: "U2F_V2", challenge}], [], resolve);
    });
  }).then(res => {
    is(res.errorCode, 0, "u2f.register() succeeded");
    let data = base64ToBytesUrlSafe(res.registrationData);
    is(data[0], 0x05, "Reserved byte is correct");
    return data.slice(67, 67 + data[66]);
  });
}

function promiseWebAuthnRegister(tab, appid) {
  return ContentTask.spawn(tab.linkedBrowser, [appid], ([appid]) => {
    const cose_alg_ECDSA_w_SHA256 = -7;

    let challenge = content.crypto.getRandomValues(new Uint8Array(16));

    let pubKeyCredParams = [{
      type: "public-key",
      alg: cose_alg_ECDSA_w_SHA256
    }];

    let publicKey = {
      rp: {id: content.document.domain, name: "none", icon: "none"},
      user: {id: new Uint8Array(), name: "none", icon: "none", displayName: "none"},
      pubKeyCredParams,
      extensions: {appid},
      challenge
    };

    return content.navigator.credentials.create({publicKey})
      .then(res => res.rawId);
  });
}

function promiseWebAuthnSign(tab, key_handle, extensions = {}) {
  return ContentTask.spawn(tab.linkedBrowser, [key_handle, extensions],
    ([key_handle, extensions]) => {
      let challenge = content.crypto.getRandomValues(new Uint8Array(16));

      let credential = {
        id: key_handle,
        type: "public-key",
        transports: ["usb"]
      };

      let publicKey = {
        challenge,
        extensions,
        rpId: content.document.domain,
        allowCredentials: [credential],
      };

      return content.navigator.credentials.get({publicKey})
        .then(credential => {
          return {
            authenticatorData: credential.response.authenticatorData,
            clientDataJSON: credential.response.clientDataJSON,
            extensions: credential.getClientExtensionResults()
          };
        })
    });
}

add_task(function test_setup() {
  Services.prefs.setBoolPref("security.webauth.u2f", true);
  Services.prefs.setBoolPref("security.webauth.webauthn", true);
  Services.prefs.setBoolPref("security.webauth.webauthn_enable_softtoken", true);
  Services.prefs.setBoolPref("security.webauth.webauthn_enable_usbtoken", false);
});

add_task(async function test_appid() {
  // Open a new tab.
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  // Get a keyHandle for a FIDO AppId.
  let appid = "https://example.com/appId";
  let keyHandle = await promiseU2FRegister(tab, appid);

  // The FIDO AppId extension can't be used for MakeCredential.
  await promiseWebAuthnRegister(tab, appid)
    .then(arrivingHereIsBad)
    .catch(expectNotSupportedError);

  // Using the keyHandle shouldn't work without the FIDO AppId extension.
  // This will be an invalid state, because the softtoken will consent without
  // having the correct "RP ID" via the FIDO extension.
  await promiseWebAuthnSign(tab, keyHandle)
    .then(arrivingHereIsBad)
    .catch(expectInvalidStateError);

  // Invalid app IDs (for the current origin) must be rejected.
  await promiseWebAuthnSign(tab, keyHandle, {appid: "https://bogus.com/appId"})
    .then(arrivingHereIsBad)
    .catch(expectSecurityError);

  // Non-matching app IDs must be rejected. Even when the user/softtoken
  // consents, leading to an invalid state.
  await promiseWebAuthnSign(tab, keyHandle, {appid: appid + "2"})
    .then(arrivingHereIsBad)
    .catch(expectInvalidStateError);

  let rpId = new TextEncoder("utf-8").encode(appid);
  let rpIdHash = await crypto.subtle.digest("SHA-256", rpId);

  // Succeed with the right fallback rpId.
  await promiseWebAuthnSign(tab, keyHandle, {appid})
    .then(({authenticatorData, clientDataJSON, extensions}) => {
      is(extensions.appid, true, "appid extension was acted upon");

      // Check that the correct rpIdHash is returned.
      let rpIdHashSign = authenticatorData.slice(0, 32);
      ok(memcmp(rpIdHash, rpIdHashSign), "rpIdHash is correct");

      let clientData = JSON.parse(buffer2string(clientDataJSON));
      is(clientData.clientExtensions.appid, appid, "appid extension sent");
    });

  // Close tab.
  BrowserTestUtils.removeTab(tab);
});

add_task(function test_cleanup() {
  Services.prefs.clearUserPref("security.webauth.u2f");
  Services.prefs.clearUserPref("security.webauth.webauthn");
  Services.prefs.clearUserPref("security.webauth.webauthn_enable_softtoken");
  Services.prefs.clearUserPref("security.webauth.webauthn_enable_usbtoken");
});
