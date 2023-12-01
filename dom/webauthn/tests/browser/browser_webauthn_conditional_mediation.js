/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_URL = "https://example.com";

let gAuthenticatorId = add_virtual_authenticator();
let gExpectNotAllowedError = expectError("NotAllowed");
let gPendingConditionalGetSubject = "webauthn:conditional-get-pending";
let gWebAuthnService = Cc["@mozilla.org/webauthn/service;1"].getService(
  Ci.nsIWebAuthnService
);

add_task(async function test_webauthn_resume_conditional_get() {
  // Open a new tab.
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  // TODO: Is there a better way to get the browsing context id?
  let browser = tab.linkedBrowser.browsingContext.embedderElement;
  let browsingContextId = browser.browsingContext.id;

  let transactionId = gWebAuthnService.hasPendingConditionalGet(
    browsingContextId,
    TEST_URL
  );
  ok(transactionId == 0, "should not have a pending conditional get");

  let requestStarted = TestUtils.topicObserved(gPendingConditionalGetSubject);

  let active = true;
  let promise = promiseWebAuthnGetAssertionDiscoverable(tab, "conditional")
    .then(arrivingHereIsBad)
    .catch(gExpectNotAllowedError)
    .then(() => (active = false));

  await requestStarted;

  transactionId = gWebAuthnService.hasPendingConditionalGet(0, TEST_URL);
  ok(
    transactionId == 0,
    "hasPendingConditionalGet should check the browsing context id"
  );

  transactionId = gWebAuthnService.hasPendingConditionalGet(
    browsingContextId,
    "https://example.org"
  );
  ok(transactionId == 0, "hasPendingConditionalGet should check the origin");

  transactionId = gWebAuthnService.hasPendingConditionalGet(
    browsingContextId,
    TEST_URL
  );
  ok(transactionId != 0, "should have a pending conditional get");

  ok(active, "request should still be active");

  gWebAuthnService.resumeConditionalGet(transactionId);
  await promise;

  ok(!active, "request should not be active");

  // Close tab.
  await BrowserTestUtils.removeTab(tab);
});

add_task(async function test_webauthn_select_autofill_entry() {
  // Open a new tab.
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  // Add credentials
  let cred1 = await addCredential(gAuthenticatorId, "example.com");
  let cred2 = await addCredential(gAuthenticatorId, "example.com");

  // TODO: Is there a better way to get the browsing context id?
  let browser = tab.linkedBrowser.browsingContext.embedderElement;
  let browsingContextId = browser.browsingContext.id;

  let transactionId = gWebAuthnService.hasPendingConditionalGet(
    browsingContextId,
    TEST_URL
  );
  ok(transactionId == 0, "should not have a pending conditional get");

  let requestStarted = TestUtils.topicObserved(gPendingConditionalGetSubject);

  let active = true;
  let promise = promiseWebAuthnGetAssertionDiscoverable(tab, "conditional")
    .catch(arrivingHereIsBad)
    .then(() => (active = false));

  await requestStarted;

  transactionId = gWebAuthnService.hasPendingConditionalGet(
    browsingContextId,
    TEST_URL
  );
  ok(transactionId != 0, "should have a pending conditional get");

  let autoFillEntries = gWebAuthnService.getAutoFillEntries(transactionId);
  ok(
    autoFillEntries.length == 2 &&
      autoFillEntries[0].rpId == "example.com" &&
      autoFillEntries[1].rpId == "example.com",
    "should have two autofill entries for example.com"
  );

  gWebAuthnService.selectAutoFillEntry(
    transactionId,
    autoFillEntries[0].credentialId
  );
  let result = await promise;

  ok(!active, "request should not be active");

  // Remove credentials
  gWebAuthnService.removeCredential(gAuthenticatorId, cred1);
  gWebAuthnService.removeCredential(gAuthenticatorId, cred2);

  // Close tab.
  await BrowserTestUtils.removeTab(tab);
});
