/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_URL = "https://example.com/browser/dom/u2f/tests/browser/tab_u2f_result.html";

async function assertStatus(tab, expected) {
  let actual = await ContentTask.spawn(tab.linkedBrowser, null, async function () {
    return content.document.getElementById("status").value;
  });
  is(actual, expected, "u2f request " + expected);
}

async function waitForStatus(tab, expected) {
  await ContentTask.spawn(tab.linkedBrowser, [expected], async function (expected) {
    return ContentTaskUtils.waitForCondition(() => {
      return content.document.getElementById("status").value == expected;
    });
  });

  await assertStatus(tab, expected);
}

function startMakeCredentialRequest(tab) {
  let challenge = crypto.getRandomValues(new Uint8Array(16));
  challenge = bytesToBase64UrlSafe(challenge);

  return ContentTask.spawn(tab.linkedBrowser, [challenge], async function ([challenge]) {
    let appId = content.location.origin;
    let request = {version: "U2F_V2", challenge};

    let status = content.document.getElementById("status");

    content.u2f.register(appId, [request], [], result => {
      status.value = result.errorCode ? "aborted" : completed;
    });

    status.value = "pending";
  });
}

function startGetAssertionRequest(tab) {
  let challenge = crypto.getRandomValues(new Uint8Array(16));
  challenge = bytesToBase64UrlSafe(challenge);

  let keyHandle = crypto.getRandomValues(new Uint8Array(16));
  keyHandle = bytesToBase64UrlSafe(keyHandle);

  return ContentTask.spawn(tab.linkedBrowser, [challenge, keyHandle], async function ([challenge, keyHandle]) {
    let appId = content.location.origin;
    let key = {version: "U2F_V2", keyHandle};

    let status = content.document.getElementById("status");

    content.u2f.sign(appId, challenge, [key], result => {
      status.value = result.errorCode ? "aborted" : completed;
    });

    status.value = "pending";
  });
}


// Test that MakeCredential() and GetAssertion() requests
// are aborted when the current tab loses its focus.
add_task(async function test_abort() {
  // Enable the USB token.
  Services.prefs.setBoolPref("security.webauth.u2f", true);
  Services.prefs.setBoolPref("security.webauth.webauthn_enable_softtoken", false);
  Services.prefs.setBoolPref("security.webauth.webauthn_enable_usbtoken", true);

  // Create a new tab for the MakeCredential() request.
  let tab_create = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  // Start the request.
  await startMakeCredentialRequest(tab_create);
  await assertStatus(tab_create, "pending");

  // Open another tab and switch to it. The first will lose focus.
  let tab_get = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);
  await waitForStatus(tab_create, "aborted");

  // Start a GetAssertion() request in the second tab.
  await startGetAssertionRequest(tab_get);
  await assertStatus(tab_get, "pending");

  // Switch back to the first tab, the get() request is aborted.
  await BrowserTestUtils.switchTab(gBrowser, tab_create);
  await waitForStatus(tab_get, "aborted");

  // Close tabs.
  await BrowserTestUtils.removeTab(tab_create);
  await BrowserTestUtils.removeTab(tab_get);

  // Cleanup.
  Services.prefs.clearUserPref("security.webauth.u2f");
  Services.prefs.clearUserPref("security.webauth.webauthn_enable_softtoken");
  Services.prefs.clearUserPref("security.webauth.webauthn_enable_usbtoken");
});
