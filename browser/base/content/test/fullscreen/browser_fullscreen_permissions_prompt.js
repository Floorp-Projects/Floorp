/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// This test tends to trigger a race in the fullscreen time telemetry,
// where the fullscreen enter and fullscreen exit events (which use the
// same histogram ID) overlap. That causes TelemetryStopwatch to log an
// error.
SimpleTest.ignoreAllUncaughtExceptions(true);
const { PromiseTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PromiseTestUtils.sys.mjs"
);
PromiseTestUtils.allowMatchingRejectionsGlobally(/Not in fullscreen mode/);

SimpleTest.requestCompleteLog();

async function requestNotificationPermission(browser) {
  return SpecialPowers.spawn(browser, [], () => {
    return content.Notification.requestPermission();
  });
}

async function requestCameraPermission(browser) {
  return SpecialPowers.spawn(browser, [], () =>
    content.navigator.mediaDevices
      .getUserMedia({ video: true, fake: true })
      .then(
        () => true,
        () => false
      )
  );
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
  requestNotificationPermission(browser).catch(() => {});
  await popupShown;

  info("Waiting for full-screen exit");
  await fullScreenExit;

  BrowserTestUtils.removeTab(tab);
  await SpecialPowers.popPrefEnv();
});

function triggerMainCommand(popup) {
  let notifications = popup.childNodes;
  ok(!!notifications.length, "at least one notification displayed");
  let notification = notifications[0];
  info("Triggering main command for notification " + notification.id);
  EventUtils.synthesizeMouseAtCenter(notification.button, {});
}

add_task(
  async function test_permission_prompt_closes_fullscreen_and_extends_security_delay() {
    const TEST_SECURITY_DELAY = 500;
    await SpecialPowers.pushPrefEnv({
      set: [
        ["dom.webnotifications.requireuserinteraction", false],
        ["permissions.fullscreen.allowed", false],
        ["security.notification_enable_delay", TEST_SECURITY_DELAY],
        // macOS is not affected by the sec delay bug because it uses the native
        // macOS full screen API. Revert back to legacy behavior so we can also
        // test on macOS. If this pref is removed in the future we can consider
        // skipping the testcase for macOS altogether.
        ["full-screen-api.macos-native-full-screen", false],
      ],
    });

    let tab = await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      "https://example.com"
    );
    let browser = tab.linkedBrowser;
    info("Entering DOM full-screen");
    await changeFullscreen(browser, true);

    let popupShown = BrowserTestUtils.waitForPopupEvent(
      window.PopupNotifications.panel,
      "shown"
    );
    let fullScreenExit = waitForFullScreenState(browser, false);

    info("Requesting notification permission");
    requestNotificationPermission(browser).catch(() => {});
    await popupShown;

    let notificationHiddenPromise = BrowserTestUtils.waitForPopupEvent(
      window.PopupNotifications.panel,
      "hidden"
    );

    info("Waiting for full-screen exit");
    await fullScreenExit;

    info("Wait for original security delay to expire.");
    SimpleTest.requestFlakyTimeout(
      "Wait for original security delay to expire."
    );
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    await new Promise(resolve => setTimeout(resolve, TEST_SECURITY_DELAY));

    info(
      "Trigger main action via button click during the extended security delay"
    );
    triggerMainCommand(PopupNotifications.panel);

    let notification = PopupNotifications.getNotification(
      "web-notifications",
      gBrowser.selectedBrowser
    );

    // Linux in CI seems to skip the full screen animation, which means its not
    // affected by the bug and we can't test extension of the sec delay here.
    if (Services.appinfo.OS == "Linux") {
      todo(
        notification &&
          !notification.dismissed &&
          BrowserTestUtils.isVisible(PopupNotifications.panel.firstChild),
        "Notification should still be open because we clicked during the security delay."
      );
    } else {
      ok(
        notification &&
          !notification.dismissed &&
          BrowserTestUtils.isVisible(PopupNotifications.panel.firstChild),
        "Notification should still be open because we clicked during the security delay."
      );
    }

    // If the notification is no longer shown (test failure) skip the remaining
    // checks.
    if (!notification) {
      // Cleanup
      BrowserTestUtils.removeTab(tab);
      await SpecialPowers.popPrefEnv();
      // Remove the granted notification permission.
      Services.perms.removeAll();
      return;
    }

    Assert.greater(
      notification.timeShown,
      performance.now(),
      "Notification timeShown property should be in the future, because the security delay was extended."
    );

    // Ensure that once the security delay has passed the notification can be
    // closed again.
    let fakeTimeShown = TEST_SECURITY_DELAY + 500;
    info(`Manually set timeShown to ${fakeTimeShown}ms in the past.`);
    notification.timeShown = performance.now() - fakeTimeShown;

    info("Trigger main action via button click outside security delay");
    triggerMainCommand(PopupNotifications.panel);

    info("Wait for panel to be hidden.");
    await notificationHiddenPromise;

    ok(
      !PopupNotifications.getNotification(
        "web-notifications",
        gBrowser.selectedBrowser
      ),
      "Should not longer see the notification."
    );

    // Cleanup
    BrowserTestUtils.removeTab(tab);
    await SpecialPowers.popPrefEnv();
    // Remove the granted notification permission.
    Services.perms.removeAll();
  }
);
