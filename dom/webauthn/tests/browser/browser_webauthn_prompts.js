/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_URL = "https://example.com/";

function promiseNotification(id) {
  return new Promise(resolve => {
    PopupNotifications.panel.addEventListener("popupshown", function shown() {
      let notification = PopupNotifications.getNotification(id);
      if (notification) {
        ok(true, `${id} prompt visible`);
        PopupNotifications.panel.removeEventListener("popupshown", shown);
        resolve();
      }
    });
  });
}

function triggerMainPopupCommand(popup) {
  info("triggering main command");
  let notifications = popup.childNodes;
  ok(notifications.length, "at least one notification displayed");
  let notification = notifications[0];
  info("triggering command: " + notification.getAttribute("buttonlabel"));

  return EventUtils.synthesizeMouseAtCenter(notification.button, {});
}

let expectAbortError = expectError("Abort");

function verifyAnonymizedCertificate(result) {
  let { attObj, rawId } = result;
  return webAuthnDecodeCBORAttestation(attObj).then(({ fmt, attStmt }) => {
    is("none", fmt, "Is a None Attestation");
    is("object", typeof attStmt, "attStmt is a map");
    is(0, Object.keys(attStmt).length, "attStmt is empty");
  });
}

function verifyDirectCertificate(result) {
  let { attObj, rawId } = result;
  return webAuthnDecodeCBORAttestation(attObj).then(({ fmt, attStmt }) => {
    is("fido-u2f", fmt, "Is a FIDO U2F Attestation");
    is("object", typeof attStmt, "attStmt is a map");
    ok(attStmt.hasOwnProperty("x5c"), "attStmt.x5c exists");
    ok(attStmt.hasOwnProperty("sig"), "attStmt.sig exists");
  });
}

add_task(async function test_setup_usbtoken() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["security.webauth.u2f", false],
      ["security.webauth.webauthn", true],
      ["security.webauth.webauthn_enable_softtoken", false],
      ["security.webauth.webauthn_enable_android_fido2", false],
      ["security.webauth.webauthn_enable_usbtoken", true],
    ],
  });
});

add_task(async function test_register() {
  // Open a new tab.
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  // Request a new credential and wait for the prompt.
  let active = true;
  let request = promiseWebAuthnMakeCredential(tab, "none", {})
    .then(arrivingHereIsBad)
    .catch(expectAbortError)
    .then(() => (active = false));
  await promiseNotification("webauthn-prompt-register");

  // Cancel the request.
  ok(active, "request should still be active");
  PopupNotifications.panel.firstElementChild.button.click();
  await request;

  // Close tab.
  await BrowserTestUtils.removeTab(tab);
});

add_task(async function test_sign() {
  // Open a new tab.
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  // Request a new assertion and wait for the prompt.
  let active = true;
  let request = promiseWebAuthnGetAssertion(tab)
    .then(arrivingHereIsBad)
    .catch(expectAbortError)
    .then(() => (active = false));
  await promiseNotification("webauthn-prompt-sign");

  // Cancel the request.
  ok(active, "request should still be active");
  PopupNotifications.panel.firstElementChild.button.click();
  await request;

  // Close tab.
  await BrowserTestUtils.removeTab(tab);
});

add_task(async function test_register_direct_cancel() {
  // Open a new tab.
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  // Request a new credential with direct attestation and wait for the prompt.
  let active = true;
  let promise = promiseWebAuthnMakeCredential(tab, "direct", {})
    .then(arrivingHereIsBad)
    .catch(expectAbortError)
    .then(() => (active = false));
  await promiseNotification("webauthn-prompt-register-direct");

  // Cancel the request.
  ok(active, "request should still be active");
  PopupNotifications.panel.firstElementChild.secondaryButton.click();
  await promise;

  // Close tab.
  await BrowserTestUtils.removeTab(tab);
});

// Add two tabs, open WebAuthn in the first, switch, assert the prompt is
// not visible, switch back, assert the prompt is there and cancel it.
add_task(async function test_tab_switching() {
  // Open a new tab.
  let tab_one = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  // Request a new credential and wait for the prompt.
  let active = true;
  let request = promiseWebAuthnMakeCredential(tab_one, "none", {})
    .then(arrivingHereIsBad)
    .catch(expectAbortError)
    .then(() => (active = false));
  await promiseNotification("webauthn-prompt-register");
  is(PopupNotifications.panel.state, "open", "Doorhanger is visible");

  // Open and switch to a second tab.
  let tab_two = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.org/"
  );

  await TestUtils.waitForCondition(
    () => PopupNotifications.panel.state == "closed"
  );
  is(PopupNotifications.panel.state, "closed", "Doorhanger is hidden");

  // Go back to the first tab
  await BrowserTestUtils.removeTab(tab_two);

  await promiseNotification("webauthn-prompt-register");

  await TestUtils.waitForCondition(
    () => PopupNotifications.panel.state == "open"
  );
  is(PopupNotifications.panel.state, "open", "Doorhanger is visible");

  // Cancel the request.
  ok(active, "request should still be active");
  await triggerMainPopupCommand(PopupNotifications.panel);
  await request;
  ok(!active, "request should be stopped");

  // Close tab.
  await BrowserTestUtils.removeTab(tab_one);
});

// Add two tabs, open WebAuthn in the first, switch, assert the prompt is
// not visible, switch back, assert the prompt is there and cancel it.
add_task(async function test_window_switching() {
  // Open a new tab.
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  // Request a new credential and wait for the prompt.
  let active = true;
  let request = promiseWebAuthnMakeCredential(tab, "none", {})
    .then(arrivingHereIsBad)
    .catch(expectAbortError)
    .then(() => (active = false));
  await promiseNotification("webauthn-prompt-register");

  await TestUtils.waitForCondition(
    () => PopupNotifications.panel.state == "open"
  );
  is(PopupNotifications.panel.state, "open", "Doorhanger is visible");

  // Open and switch to a second window
  let new_window = await BrowserTestUtils.openNewBrowserWindow();
  await SimpleTest.promiseFocus(new_window);

  await TestUtils.waitForCondition(
    () => new_window.PopupNotifications.panel.state == "closed"
  );
  is(
    new_window.PopupNotifications.panel.state,
    "closed",
    "Doorhanger is hidden"
  );

  // Go back to the first tab
  await BrowserTestUtils.closeWindow(new_window);
  await SimpleTest.promiseFocus(window);

  await TestUtils.waitForCondition(
    () => PopupNotifications.panel.state == "open"
  );
  is(PopupNotifications.panel.state, "open", "Doorhanger is still visible");

  // Cancel the request.
  ok(active, "request should still be active");
  await triggerMainPopupCommand(PopupNotifications.panel);
  await request;
  ok(!active, "request should be stopped");

  // Close tab.
  await BrowserTestUtils.removeTab(tab);
});

add_task(async function test_setup_softtoken() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["security.webauth.u2f", false],
      ["security.webauth.webauthn", true],
      ["security.webauth.webauthn_enable_softtoken", true],
      ["security.webauth.webauthn_enable_usbtoken", false],
    ],
  });
});

add_task(async function test_register_direct_proceed() {
  // Open a new tab.
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  // Request a new credential with direct attestation and wait for the prompt.
  let request = promiseWebAuthnMakeCredential(tab, "direct", {});
  await promiseNotification("webauthn-prompt-register-direct");

  // Proceed.
  PopupNotifications.panel.firstElementChild.button.click();

  // Ensure we got "direct" attestation.
  await request.then(verifyDirectCertificate);

  // Close tab.
  await BrowserTestUtils.removeTab(tab);
});

add_task(async function test_register_direct_proceed_anon() {
  // Open a new tab.
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  // Request a new credential with direct attestation and wait for the prompt.
  let request = promiseWebAuthnMakeCredential(tab, "direct", {});
  await promiseNotification("webauthn-prompt-register-direct");

  // Check "anonymize anyway" and proceed.
  PopupNotifications.panel.firstElementChild.checkbox.checked = true;
  PopupNotifications.panel.firstElementChild.button.click();

  // Ensure we got "none" attestation.
  await request.then(verifyAnonymizedCertificate);

  // Close tab.
  await BrowserTestUtils.removeTab(tab);
});
