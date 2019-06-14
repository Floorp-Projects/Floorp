/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_URL = "https://localhost/";

function promiseU2FRegister(tab, app_id) {
  let challenge = crypto.getRandomValues(new Uint8Array(16));
  challenge = bytesToBase64UrlSafe(challenge);

  /* eslint-disable no-shadow */
  return ContentTask.spawn(tab.linkedBrowser, [app_id, challenge], async function ([app_id, challenge]) {
    return new Promise(resolve => {
      let version = "U2F_V2";
      content.u2f.register(app_id, [{version, challenge}], [], resolve);
    });
  });
  /* eslint-enable no-shadow */
}

add_task(async function () {
  // By default, proxies don't apply to localhost. We need them to for this test, though:
  await SpecialPowers.pushPrefEnv({set: [["network.proxy.allow_hijacking_localhost", true],]});
  // Enable the soft token.
  Services.prefs.setBoolPref("security.webauth.u2f", true);
  Services.prefs.setBoolPref("security.webauth.webauthn_enable_softtoken", true);
  Services.prefs.setBoolPref("security.webauth.webauthn_enable_usbtoken", false);
  Services.prefs.setBoolPref("security.webauth.webauthn_enable_android_fido2", false);

  // Open a new tab.
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  // Check that we have the right origin, and U2F is available.
  let ready = await ContentTask.spawn(tab.linkedBrowser, null, async () => {
    return content.location.origin == "https://localhost" && content.u2f;
  });
  ok(ready, "Origin is https://localhost. U2F is available.");

  // Test: Null AppID
  await promiseU2FRegister(tab, null).then(res => {
    is(res.errorCode, 0, "Null AppID should work.");
  });

  // Test: Empty AppID
  await promiseU2FRegister(tab, "").then(res => {
    is(res.errorCode, 0, "Empty AppID should work.");
  });

  // Test: Correct TLD, incorrect scheme
  await promiseU2FRegister(tab, "http://localhost/appId").then(res => {
    isnot(res.errorCode, 0, "Incorrect scheme.");
  });

  // Test: Incorrect TLD
  await promiseU2FRegister(tab, "https://localhost.ssl/appId").then(res => {
    isnot(res.errorCode, 0, "Incorrect TLD.");
  });

  // Test: Incorrect TLD
  await promiseU2FRegister(tab, "https://sub.localhost/appId").then(res => {
    isnot(res.errorCode, 0, "Incorrect TLD.");
  });

  // Test: Correct TLD
  await promiseU2FRegister(tab, "https://localhost/appId").then(res => {
    is(res.errorCode, 0, "https://localhost/appId should work.");
  });

  // Test: Correct TLD
  await promiseU2FRegister(tab, "https://localhost:443/appId").then(res => {
    is(res.errorCode, 0, "https://localhost:443/appId should work.");
  });

  // Close tab.
  BrowserTestUtils.removeTab(tab);

  // Cleanup.
  Services.prefs.clearUserPref("security.webauth.u2f");
  Services.prefs.clearUserPref("security.webauth.webauthn_enable_softtoken");
  Services.prefs.clearUserPref("security.webauth.webauthn_enable_usbtoken");
});
