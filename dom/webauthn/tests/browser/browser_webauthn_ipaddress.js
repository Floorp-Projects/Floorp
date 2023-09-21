/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_virtual_authenticator();

let expectSecurityError = expectError("Security");

add_task(async function test_setup() {
  return SpecialPowers.pushPrefEnv({
    set: [["network.proxy.allow_hijacking_localhost", true]],
  });
});

add_task(async function test_appid() {
  // 127.0.0.1 triggers special cases in ssltunnel, so let's use .2!
  const TEST_URL = "https://127.0.0.2/";

  // Open a new tab.
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  await promiseWebAuthnMakeCredential(tab)
    .then(arrivingHereIsBad)
    .catch(expectSecurityError);

  // Close tab.
  BrowserTestUtils.removeTab(tab);
});
