/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// This test tends to trigger a race in the fullscreen time telemetry,
// where the fullscreen enter and fullscreen exit events (which use the
// same histogram ID) overlap. That causes TelemetryStopwatch to log an
// error.
SimpleTest.ignoreAllUncaughtExceptions(true);

SimpleTest.requestCompleteLog();

async function requestNotificationPermission(browser) {
  return ContentTask.spawn(browser, null, () => {
    return content.Notification.requestPermission();
  });
}

async function requestCameraPermission(browser) {
  return ContentTask.spawn(browser, null, () => {
    return new Promise(resolve => {
      content.navigator.mediaDevices
        .getUserMedia({ video: true, fake: true })
        .catch(resolve(false))
        .then(resolve(true));
    });
  });
}

add_task(async function test_fullscreen_closes_permissionui_prompt() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.webnotifications.requireuserinteraction", false],
      ["permissions.fullscreen.allowed", false],
    ],
  });

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com"
  );
  let browser = tab.linkedBrowser;

  let popupShown, requestResult, popupHidden;

  popupShown = BrowserTestUtils.waitForEvent(
    window.PopupNotifications.panel,
    "popupshown"
  );

  info("Requesting notification permission");
  requestResult = requestNotificationPermission(browser);
  await popupShown;

  info("Entering DOM full-screen");
  popupHidden = BrowserTestUtils.waitForEvent(
    window.PopupNotifications.panel,
    "popuphidden"
  );

  await changeFullscreen(browser, true);

  await popupHidden;

  is(
    await requestResult,
    "default",
    "Expect permission request to be cancelled"
  );

  await changeFullscreen(browser, false);

  BrowserTestUtils.removeTab(tab);
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_fullscreen_closes_webrtc_permission_prompt() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["media.navigator.permission.fake", true],
      ["media.navigator.permission.force", true],
      ["permissions.fullscreen.allowed", false],
    ],
  });

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com"
  );
  let browser = tab.linkedBrowser;
  let popupShown, requestResult, popupHidden;

  popupShown = BrowserTestUtils.waitForEvent(
    window.PopupNotifications.panel,
    "popupshown"
  );

  info("Requesting camera permission");
  requestResult = requestCameraPermission(browser);

  await popupShown;

  info("Entering DOM full-screen");
  popupHidden = BrowserTestUtils.waitForEvent(
    window.PopupNotifications.panel,
    "popuphidden"
  );
  await changeFullscreen(browser, true);

  await popupHidden;

  is(
    await requestResult,
    false,
    "Expect webrtc permission request to be cancelled"
  );

  await changeFullscreen(browser, false);

  BrowserTestUtils.removeTab(tab);
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_permission_prompt_closes_fullscreen() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.webnotifications.requireuserinteraction", false],
      ["permissions.fullscreen.allowed", false],
    ],
  });

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com"
  );
  let browser = tab.linkedBrowser;
  info("Entering DOM full-screen");
  await changeFullscreen(browser, true);

  let popupShown = BrowserTestUtils.waitForEvent(
    window.PopupNotifications.panel,
    "popupshown"
  );
  let fullScreenExit = waitForFullScreenState(browser, false);

  info("Requesting notification permission");
  requestNotificationPermission(browser);
  await popupShown;

  info("Waiting for full-screen exit");
  await fullScreenExit;

  BrowserTestUtils.removeTab(tab);
  await SpecialPowers.popPrefEnv();
});
