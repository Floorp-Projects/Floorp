/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_URL = "https://example.com/";

function arrivingHereIsBad(aResult) {
  ok(false, "Bad result! Received a: " + aResult);
}

function expectNotAllowedError(aResult) {
  let expected = "NotAllowedError";
  is(aResult.slice(0, expected.length), expected, `Expecting a ${expected}`);
}

function promiseMakeCredential(tab) {
  return ContentTask.spawn(tab.linkedBrowser, null, async function() {
    const cose_alg_ECDSA_w_SHA256 = -7;

    let publicKey = {
      rp: { id: content.document.domain, name: "none", icon: "none" },
      user: {
        id: new Uint8Array(),
        name: "none",
        icon: "none",
        displayName: "none",
      },
      challenge: content.crypto.getRandomValues(new Uint8Array(16)),
      timeout: 5000, // the minimum timeout is actually 15 seconds
      pubKeyCredParams: [{ type: "public-key", alg: cose_alg_ECDSA_w_SHA256 }],
    };

    return content.navigator.credentials.create({ publicKey });
  });
}

function promiseGetAssertion(tab) {
  return ContentTask.spawn(tab.linkedBrowser, null, async function() {
    let newCredential = {
      type: "public-key",
      id: content.crypto.getRandomValues(new Uint8Array(16)),
      transports: ["usb"],
    };

    let publicKey = {
      challenge: content.crypto.getRandomValues(new Uint8Array(16)),
      timeout: 5000, // the minimum timeout is actually 15 seconds
      rpId: content.document.domain,
      allowCredentials: [newCredential],
    };

    return content.navigator.credentials.get({ publicKey });
  });
}

add_task(async function test_setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["security.webauth.webauthn", true],
      ["security.webauth.webauthn_enable_softtoken", true],
      ["security.webauth.webauthn_enable_usbtoken", false],
    ],
  });
});

add_task(async function test_background_tab() {
  // Open two tabs, the last one will selected.
  let tab_bg = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);
  let tab_fg = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  // Requests from background tabs must fail.
  await promiseMakeCredential(tab_bg)
    .then(arrivingHereIsBad)
    .catch(expectNotAllowedError);

  // Requests from background tabs must fail.
  await promiseGetAssertion(tab_bg)
    .then(arrivingHereIsBad)
    .catch(expectNotAllowedError);

  // Close tabs.
  await BrowserTestUtils.removeTab(tab_bg);
  await BrowserTestUtils.removeTab(tab_fg);
});

add_task(async function test_background_window() {
  // Open a tab, then a new window.
  let tab_bg = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);
  let win = await BrowserTestUtils.openNewBrowserWindow();

  // Wait until the new window is really focused.
  await new Promise(resolve => SimpleTest.waitForFocus(resolve, win));

  // Requests from selected tabs not in the active window must fail.
  await promiseMakeCredential(tab_bg)
    .then(arrivingHereIsBad)
    .catch(expectNotAllowedError);

  // Requests from selected tabs not in the active window must fail.
  await promiseGetAssertion(tab_bg)
    .then(arrivingHereIsBad)
    .catch(expectNotAllowedError);

  // Close tab and window.
  await BrowserTestUtils.closeWindow(win);
  await BrowserTestUtils.removeTab(tab_bg);
});

add_task(async function test_minimized() {
  let env = Cc["@mozilla.org/process/environment;1"].getService(
    Ci.nsIEnvironment
  );
  // Minimizing windows doesn't supported in headless mode.
  if (env.get("MOZ_HEADLESS")) {
    return;
  }

  // Open a window with a tab.
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  // Minimize the window.
  window.minimize();
  await TestUtils.waitForCondition(() => !tab.linkedBrowser.docShellIsActive);

  // Requests from minimized windows must fail.
  await promiseMakeCredential(tab)
    .then(arrivingHereIsBad)
    .catch(expectNotAllowedError);

  // Requests from minimized windows must fail.
  await promiseGetAssertion(tab)
    .then(arrivingHereIsBad)
    .catch(expectNotAllowedError);

  // Restore the window.
  await new Promise(resolve => SimpleTest.waitForFocus(resolve, window));

  // Close tab.
  await BrowserTestUtils.removeTab(tab);
});
