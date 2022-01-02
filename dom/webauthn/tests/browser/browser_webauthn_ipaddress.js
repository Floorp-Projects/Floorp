/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let expectSecurityError = expectError("Security");

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
  Services.prefs.setBoolPref("network.proxy.allow_hijacking_localhost", true);
});

registerCleanupFunction(async function() {
  Services.prefs.clearUserPref("security.webauth.u2f");
  Services.prefs.clearUserPref("security.webauth.webauthn");
  Services.prefs.clearUserPref(
    "security.webauth.webauthn_enable_android_fido2"
  );
  Services.prefs.clearUserPref("security.webauth.webauthn_enable_softtoken");
  Services.prefs.clearUserPref("security.webauth.webauthn_enable_usbtoken");
  Services.prefs.clearUserPref("network.proxy.allow_hijacking_localhost");
});

add_task(async function test_appid() {
  // 127.0.0.1 triggers special cases in ssltunnel, so let's use .2!
  const TEST_URL = "https://127.0.0.2/";

  // Open a new tab.
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  await promiseWebAuthnMakeCredential(tab, "none", {})
    .then(arrivingHereIsBad)
    .catch(expectSecurityError);

  // Close tab.
  BrowserTestUtils.removeTab(tab);
});
