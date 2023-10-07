/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let exports = this;

const scripts = [
  "pkijs/common.js",
  "pkijs/asn1.js",
  "pkijs/x509_schema.js",
  "pkijs/x509_simpl.js",
  "browser/cbor.js",
  "browser/u2futil.js",
];

for (let script of scripts) {
  Services.scriptloader.loadSubScript(
    `chrome://mochitests/content/browser/dom/webauthn/tests/${script}`,
    this
  );
}

function add_virtual_authenticator(autoremove = true) {
  let webauthnService = Cc["@mozilla.org/webauthn/service;1"].getService(
    Ci.nsIWebAuthnService
  );
  let id = webauthnService.addVirtualAuthenticator(
    "ctap2_1",
    "internal",
    true,
    true,
    true,
    true
  );
  if (autoremove) {
    registerCleanupFunction(() => {
      webauthnService.removeVirtualAuthenticator(id);
    });
  }
  return id;
}

async function addCredential(authenticatorId, rpId) {
  let keyPair = await crypto.subtle.generateKey(
    {
      name: "ECDSA",
      namedCurve: "P-256",
    },
    true,
    ["sign"]
  );

  let credId = new Uint8Array(32);
  crypto.getRandomValues(credId);
  credId = bytesToBase64UrlSafe(credId);

  let privateKey = await crypto.subtle
    .exportKey("pkcs8", keyPair.privateKey)
    .then(privateKey => bytesToBase64UrlSafe(privateKey));

  let webauthnService = Cc["@mozilla.org/webauthn/service;1"].getService(
    Ci.nsIWebAuthnService
  );

  webauthnService.addCredential(
    authenticatorId,
    credId,
    true, // resident key
    rpId,
    privateKey,
    "VGVzdCBVc2Vy", // "Test User"
    0 // sign count
  );

  return credId;
}

async function removeCredential(authenticatorId, credId) {
  let webauthnService = Cc["@mozilla.org/webauthn/service;1"].getService(
    Ci.nsIWebAuthnService
  );

  webauthnService.removeCredential(authenticatorId, credId);
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

function arrivingHereIsBad(aResult) {
  ok(false, "Bad result! Received a: " + aResult);
}

function expectError(aType) {
  let expected = `${aType}Error`;
  return function (aResult) {
    is(
      aResult.slice(0, expected.length),
      expected,
      `Expecting a ${aType}Error`
    );
  };
}

/* eslint-disable no-shadow */
function promiseWebAuthnMakeCredential(
  tab,
  attestation = "none",
  residentKey = "discouraged",
  extensions = {}
) {
  return ContentTask.spawn(
    tab.linkedBrowser,
    [attestation, residentKey, extensions],
    ([attestation, residentKey, extensions]) => {
      const cose_alg_ECDSA_w_SHA256 = -7;

      let challenge = content.crypto.getRandomValues(new Uint8Array(16));

      let pubKeyCredParams = [
        {
          type: "public-key",
          alg: cose_alg_ECDSA_w_SHA256,
        },
      ];

      let publicKey = {
        rp: { id: content.document.domain, name: "none" },
        user: {
          id: new Uint8Array(),
          name: "none",
          displayName: "none",
        },
        pubKeyCredParams,
        authenticatorSelection: {
          authenticatorAttachment: "cross-platform",
          residentKey,
        },
        extensions,
        attestation,
        challenge,
      };

      return content.navigator.credentials
        .create({ publicKey })
        .then(credential => {
          return {
            clientDataJSON: credential.response.clientDataJSON,
            attObj: credential.response.attestationObject,
            rawId: credential.rawId,
          };
        });
    }
  );
}

function promiseWebAuthnGetAssertion(tab, key_handle = null, extensions = {}) {
  return ContentTask.spawn(
    tab.linkedBrowser,
    [key_handle, extensions],
    ([key_handle, extensions]) => {
      let challenge = content.crypto.getRandomValues(new Uint8Array(16));
      if (key_handle == null) {
        key_handle = content.crypto.getRandomValues(new Uint8Array(16));
      }

      let credential = {
        id: key_handle,
        type: "public-key",
        transports: ["usb"],
      };

      let publicKey = {
        challenge,
        extensions,
        rpId: content.document.domain,
        allowCredentials: [credential],
      };

      return content.navigator.credentials
        .get({ publicKey })
        .then(assertion => {
          return {
            authenticatorData: assertion.response.authenticatorData,
            clientDataJSON: assertion.response.clientDataJSON,
            extensions: assertion.getClientExtensionResults(),
            signature: assertion.response.signature,
          };
        });
    }
  );
}

function promiseWebAuthnGetAssertionDiscoverable(tab, extensions = {}) {
  return ContentTask.spawn(tab.linkedBrowser, [extensions], ([extensions]) => {
    let challenge = content.crypto.getRandomValues(new Uint8Array(16));

    let publicKey = {
      challenge,
      extensions,
      rpId: content.document.domain,
      allowCredentials: [],
    };

    return content.navigator.credentials.get({ publicKey });
  });
}

function checkRpIdHash(rpIdHash, hostname) {
  return crypto.subtle
    .digest("SHA-256", string2buffer(hostname))
    .then(calculatedRpIdHash => {
      let calcHashStr = bytesToBase64UrlSafe(
        new Uint8Array(calculatedRpIdHash)
      );
      let providedHashStr = bytesToBase64UrlSafe(new Uint8Array(rpIdHash));

      if (calcHashStr != providedHashStr) {
        throw new Error("Calculated RP ID hash doesn't match.");
      }
    });
}
/* eslint-enable no-shadow */
