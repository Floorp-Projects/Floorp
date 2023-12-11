/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

XPCOMUtils.defineLazyScriptGetter(
  this,
  ["FullScreen"],
  "chrome://browser/content/browser-fullScreenAndPointerLock.js"
);

const TEST_URL = "https://example.com/";
var gAuthenticatorId;

/**
 * Waits for the PopupNotifications button enable delay to expire so the
 * Notification can be interacted with using the buttons.
 */
async function waitForPopupNotificationSecurityDelay() {
  let notification = PopupNotifications.panel.firstChild.notification;
  let notificationEnableDelayMS = Services.prefs.getIntPref(
    "security.notification_enable_delay"
  );
  await TestUtils.waitForCondition(
    () => {
      let timeSinceShown = performance.now() - notification.timeShown;
      return timeSinceShown > notificationEnableDelayMS;
    },
    "Wait for security delay to expire",
    500,
    50
  );
}

add_task(async function test_setup_usbtoken() {
  return SpecialPowers.pushPrefEnv({
    set: [
      ["security.webauth.webauthn_enable_softtoken", false],
      ["security.webauth.webauthn_enable_usbtoken", true],
    ],
  });
});
add_task(test_register);
add_task(test_register_escape);
add_task(test_register_direct_cancel);
add_task(test_register_direct_presence);
add_task(test_sign);
add_task(test_sign_escape);
add_task(test_tab_switching);
add_task(test_window_switching);
add_task(async function test_setup_fullscreen() {
  return SpecialPowers.pushPrefEnv({
    set: [
      ["browser.fullscreen.autohide", true],
      ["full-screen-api.enabled", true],
      ["full-screen-api.allow-trusted-requests-only", false],
    ],
  });
});
add_task(test_fullscreen_show_nav_toolbar);
add_task(test_no_fullscreen_dom);
add_task(async function test_setup_softtoken() {
  gAuthenticatorId = add_virtual_authenticator();
  return SpecialPowers.pushPrefEnv({
    set: [
      ["security.webauth.webauthn_enable_softtoken", true],
      ["security.webauth.webauthn_enable_usbtoken", false],
    ],
  });
});
add_task(test_register_direct_proceed);
add_task(test_register_direct_proceed_anon);
add_task(test_select_sign_result);

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

function promiseNavToolboxStatus(aExpectedStatus) {
  let navToolboxStatus;
  return TestUtils.topicObserved("fullscreen-nav-toolbox", (subject, data) => {
    navToolboxStatus = data;
    return data == aExpectedStatus;
  }).then(() =>
    ok(navToolboxStatus == aExpectedStatus, "nav toolbox is " + aExpectedStatus)
  );
}

function promiseFullScreenPaint(aExpectedStatus) {
  return TestUtils.topicObserved("fullscreen-painted");
}

function triggerMainPopupCommand(popup) {
  info("triggering main command");
  let notifications = popup.childNodes;
  ok(notifications.length, "at least one notification displayed");
  let notification = notifications[0];
  info("triggering command: " + notification.getAttribute("buttonlabel"));

  return EventUtils.synthesizeMouseAtCenter(notification.button, {});
}

let expectNotAllowedError = expectError("NotAllowed");

function verifyAnonymizedCertificate(aResult) {
  return webAuthnDecodeCBORAttestation(aResult.attObj).then(
    ({ fmt, attStmt }) => {
      is(fmt, "none", "Is a None Attestation");
      is(typeof attStmt, "object", "attStmt is a map");
      is(Object.keys(attStmt).length, 0, "attStmt is empty");
    }
  );
}

async function verifyDirectCertificate(aResult) {
  let clientDataHash = await crypto.subtle
    .digest("SHA-256", aResult.clientDataJSON)
    .then(digest => new Uint8Array(digest));
  let { fmt, attStmt, authData, authDataObj } =
    await webAuthnDecodeCBORAttestation(aResult.attObj);
  is(fmt, "packed", "Is a Packed Attestation");
  let signedData = new Uint8Array(authData.length + clientDataHash.length);
  signedData.set(authData);
  signedData.set(clientDataHash, authData.length);
  let valid = await verifySignature(
    authDataObj.publicKeyHandle,
    signedData,
    new Uint8Array(attStmt.sig)
  );
  ok(valid, "Signature is valid.");
}

async function test_register() {
  // Open a new tab.
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  // Request a new credential and wait for the prompt.
  let active = true;
  let request = promiseWebAuthnMakeCredential(tab)
    .then(arrivingHereIsBad)
    .catch(expectNotAllowedError)
    .then(() => (active = false));
  await promiseNotification("webauthn-prompt-presence");

  // Cancel the request with the button.
  ok(active, "request should still be active");
  PopupNotifications.panel.firstElementChild.button.click();
  await request;

  // Close tab.
  await BrowserTestUtils.removeTab(tab);
}

async function test_register_escape() {
  // Open a new tab.
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  // Request a new credential and wait for the prompt.
  let active = true;
  let request = promiseWebAuthnMakeCredential(tab)
    .then(arrivingHereIsBad)
    .catch(expectNotAllowedError)
    .then(() => (active = false));
  await promiseNotification("webauthn-prompt-presence");

  // Cancel the request by hitting escape.
  ok(active, "request should still be active");
  EventUtils.synthesizeKey("KEY_Escape");
  await request;

  // Close tab.
  await BrowserTestUtils.removeTab(tab);
}

async function test_sign() {
  // Open a new tab.
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  // Request a new assertion and wait for the prompt.
  let active = true;
  let request = promiseWebAuthnGetAssertion(tab)
    .then(arrivingHereIsBad)
    .catch(expectNotAllowedError)
    .then(() => (active = false));
  await promiseNotification("webauthn-prompt-presence");

  // Cancel the request with the button.
  ok(active, "request should still be active");
  PopupNotifications.panel.firstElementChild.button.click();
  await request;

  // Close tab.
  await BrowserTestUtils.removeTab(tab);
}

async function test_sign_escape() {
  // Open a new tab.
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  // Request a new assertion and wait for the prompt.
  let active = true;
  let request = promiseWebAuthnGetAssertion(tab)
    .then(arrivingHereIsBad)
    .catch(expectNotAllowedError)
    .then(() => (active = false));
  await promiseNotification("webauthn-prompt-presence");

  // Cancel the request by hitting escape.
  ok(active, "request should still be active");
  EventUtils.synthesizeKey("KEY_Escape");
  await request;

  // Close tab.
  await BrowserTestUtils.removeTab(tab);
}

async function test_register_direct_cancel() {
  // Open a new tab.
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  // Request a new credential with direct attestation and wait for the prompt.
  let active = true;
  let promise = promiseWebAuthnMakeCredential(tab, "direct")
    .then(arrivingHereIsBad)
    .catch(expectNotAllowedError)
    .then(() => (active = false));
  await promiseNotification("webauthn-prompt-register-direct");

  // Cancel the request.
  ok(active, "request should still be active");
  PopupNotifications.panel.firstElementChild.secondaryButton.click();
  await promise;

  // Close tab.
  await BrowserTestUtils.removeTab(tab);
}

async function test_register_direct_presence() {
  // Open a new tab.
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  // Request a new credential with direct attestation and wait for the prompt.
  let active = true;
  let promise = promiseWebAuthnMakeCredential(tab, "direct")
    .then(arrivingHereIsBad)
    .catch(expectNotAllowedError)
    .then(() => (active = false));
  await promiseNotification("webauthn-prompt-register-direct");

  // Click "proceed" and wait for presence prompt
  let presence = promiseNotification("webauthn-prompt-presence");
  PopupNotifications.panel.firstElementChild.button.click();
  await presence;

  // Cancel the request.
  ok(active, "request should still be active");
  PopupNotifications.panel.firstElementChild.button.click();
  await promise;

  // Close tab.
  await BrowserTestUtils.removeTab(tab);
}

// Add two tabs, open WebAuthn in the first, switch, assert the prompt is
// not visible, switch back, assert the prompt is there and cancel it.
async function test_tab_switching() {
  // Open a new tab.
  let tab_one = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  // Request a new credential and wait for the prompt.
  let active = true;
  let request = promiseWebAuthnMakeCredential(tab_one)
    .then(arrivingHereIsBad)
    .catch(expectNotAllowedError)
    .then(() => (active = false));
  await promiseNotification("webauthn-prompt-presence");
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

  await promiseNotification("webauthn-prompt-presence");

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
}

// Add two tabs, open WebAuthn in the first, switch, assert the prompt is
// not visible, switch back, assert the prompt is there and cancel it.
async function test_window_switching() {
  // Open a new tab.
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  // Request a new credential and wait for the prompt.
  let active = true;
  let request = promiseWebAuthnMakeCredential(tab)
    .then(arrivingHereIsBad)
    .catch(expectNotAllowedError)
    .then(() => (active = false));
  await promiseNotification("webauthn-prompt-presence");

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
}

async function test_register_direct_proceed() {
  // Open a new tab.
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  // Request a new credential with direct attestation and wait for the prompt.
  let request = promiseWebAuthnMakeCredential(tab, "direct");
  await promiseNotification("webauthn-prompt-register-direct");

  // Proceed.
  PopupNotifications.panel.firstElementChild.button.click();

  // Ensure we got "direct" attestation.
  await request.then(verifyDirectCertificate);

  // Close tab.
  await BrowserTestUtils.removeTab(tab);
}

async function test_register_direct_proceed_anon() {
  // Open a new tab.
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  // Request a new credential with direct attestation and wait for the prompt.
  let request = promiseWebAuthnMakeCredential(tab, "direct");
  await promiseNotification("webauthn-prompt-register-direct");

  // Check "anonymize anyway" and proceed.
  PopupNotifications.panel.firstElementChild.checkbox.checked = true;
  PopupNotifications.panel.firstElementChild.button.click();

  // Ensure we got "none" attestation.
  await request.then(verifyAnonymizedCertificate);

  // Close tab.
  await BrowserTestUtils.removeTab(tab);
}

async function test_select_sign_result() {
  // Open a new tab.
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  // Make two discoverable credentials for the same RP ID so that
  // the user has to select one to return.
  let cred1 = await addCredential(gAuthenticatorId, "example.com");
  let cred2 = await addCredential(gAuthenticatorId, "example.com");

  let active = true;
  let request = promiseWebAuthnGetAssertionDiscoverable(tab)
    .then(arrivingHereIsBad)
    .catch(expectNotAllowedError)
    .then(() => (active = false));

  // Ensure the selection prompt is shown
  await promiseNotification("webauthn-prompt-select-sign-result");

  ok(active, "request is active");

  // Cancel the request
  PopupNotifications.panel.firstElementChild.button.click();
  await request;

  await removeCredential(gAuthenticatorId, cred1);
  await removeCredential(gAuthenticatorId, cred2);
  await BrowserTestUtils.removeTab(tab);
}

async function test_fullscreen_show_nav_toolbar() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  // Start with the window fullscreen and the nav toolbox hidden
  let fullscreenState = window.fullScreen;

  let navToolboxHiddenPromise = promiseNavToolboxStatus("hidden");

  window.fullScreen = true;
  FullScreen.hideNavToolbox(false);

  await navToolboxHiddenPromise;

  // Request a new credential and wait for the direct attestation consent
  // prompt.
  let promptPromise = promiseNotification("webauthn-prompt-register-direct");
  let navToolboxShownPromise = promiseNavToolboxStatus("shown");

  let active = true;
  let requestPromise = promiseWebAuthnMakeCredential(tab, "direct")
    .then(arrivingHereIsBad)
    .catch(expectNotAllowedError)
    .then(() => (active = false));

  await Promise.all([promptPromise, navToolboxShownPromise]);

  ok(active, "request is active");
  ok(window.fullScreen, "window is fullscreen");

  // Cancel the request.
  PopupNotifications.panel.firstElementChild.secondaryButton.click();
  await requestPromise;

  window.fullScreen = fullscreenState;

  // Close tab.
  await BrowserTestUtils.removeTab(tab);
}

async function test_no_fullscreen_dom() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  let fullScreenPaintPromise = promiseFullScreenPaint();
  // Make a DOM element fullscreen
  await ContentTask.spawn(tab.linkedBrowser, [], () => {
    return content.document.body.requestFullscreen();
  });
  await fullScreenPaintPromise;
  ok(!!document.fullscreenElement, "a DOM element is fullscreen");

  // Request a new credential and wait for the direct attestation consent
  // prompt.
  let promptPromise = promiseNotification("webauthn-prompt-register-direct");
  fullScreenPaintPromise = promiseFullScreenPaint();

  let active = true;
  let requestPromise = promiseWebAuthnMakeCredential(tab, "direct")
    .then(arrivingHereIsBad)
    .catch(expectNotAllowedError)
    .then(() => (active = false));

  await Promise.all([promptPromise, fullScreenPaintPromise]);

  ok(active, "request is active");
  ok(!document.fullscreenElement, "no DOM element is fullscreen");

  // Cancel the request.
  await waitForPopupNotificationSecurityDelay();
  PopupNotifications.panel.firstElementChild.secondaryButton.click();
  await requestPromise;

  // Close tab.
  await BrowserTestUtils.removeTab(tab);
}
