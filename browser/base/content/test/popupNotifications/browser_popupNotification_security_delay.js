/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_SECURITY_DELAY = 5000;

/**
 * Shows a test PopupNotification.
 */
function showNotification() {
  PopupNotifications.show(
    gBrowser.selectedBrowser,
    "foo",
    "Hello, World!",
    "default-notification-icon",
    {
      label: "ok",
      accessKey: "o",
      callback: () => {},
    },
    [
      {
        label: "cancel",
        accessKey: "c",
        callback: () => {},
      },
    ],
    {
      // Make test notifications persistent to ensure they are only closed
      // explicitly by test actions and survive tab switches.
      persistent: true,
    }
  );
}

add_setup(async function () {
  // Set a longer security delay for PopupNotification actions so we can test
  // the delay even if the test runs slowly.
  await SpecialPowers.pushPrefEnv({
    set: [["security.notification_enable_delay", TEST_SECURITY_DELAY]],
  });
});

async function ensureSecurityDelayReady() {
  /**
   * The security delay calculation in PopupNotification.sys.mjs is dependent on
   * the monotonically increasing value of performance.now. This timestamp is
   * not relative to a fixed date, but to runtime.
   * We need to wait for the value performance.now() to be larger than the
   * security delay in order to observe the bug. Only then does the
   * timeSinceShown check in PopupNotifications.sys.mjs lead to a timeSinceShown
   * value that is unconditionally greater than lazy.buttonDelay for
   * notification.timeShown = null = 0.
   * See: https://searchfox.org/mozilla-central/rev/f32d5f3949a3f4f185122142b29f2e3ab776836e/toolkit/modules/PopupNotifications.sys.mjs#1870-1872
   *
   * When running in automation as part of a larger test suite performance.now()
   * should usually be already sufficiently high in which case this check should
   * directly resolve.
   */
  await TestUtils.waitForCondition(
    () => performance.now() > TEST_SECURITY_DELAY,
    "Wait for performance.now() > SECURITY_DELAY",
    500,
    50
  );
}

/**
 * Tests that when we show a second notification while the panel is open the
 * timeShown attribute is correctly set and the security delay is enforced
 * properly.
 */
add_task(async function test_timeShownMultipleNotifications() {
  await ensureSecurityDelayReady();

  ok(
    !PopupNotifications.isPanelOpen,
    "PopupNotification panel should not be open initially."
  );

  info("Open the first notification.");
  let popupShownPromise = waitForNotificationPanel();
  showNotification();
  await popupShownPromise;
  ok(
    PopupNotifications.isPanelOpen,
    "PopupNotification should be open after first show call."
  );

  is(
    PopupNotifications._currentNotifications.length,
    1,
    "There should only be one notification"
  );

  let notification = PopupNotifications.getNotification(
    "foo",
    gBrowser.selectedBrowser
  );
  is(notification?.id, "foo", "There should be a notification with id foo");
  ok(notification.timeShown, "The notification should have timeShown set");

  info(
    "Call show again with the same notification id while the PopupNotification panel is still open."
  );
  showNotification();
  ok(
    PopupNotifications.isPanelOpen,
    "PopupNotification should still open after second show call."
  );
  notification = PopupNotifications.getNotification(
    "foo",
    gBrowser.selectedBrowser
  );
  is(
    PopupNotifications._currentNotifications.length,
    1,
    "There should still only be one notification"
  );

  is(
    notification?.id,
    "foo",
    "There should still be a notification with id foo"
  );
  ok(notification.timeShown, "The notification should have timeShown set");

  let notificationHiddenPromise = waitForNotificationPanelHidden();

  info("Trigger main action via button click during security delay");

  // Wait for a tick of the event loop to ensure the button we're clicking
  // has been slotted into moz-button-group
  await new Promise(resolve => setTimeout(resolve, 0));

  triggerMainCommand(PopupNotifications.panel);

  await new Promise(resolve => setTimeout(resolve, 0));

  ok(PopupNotifications.isPanelOpen, "PopupNotification should still be open.");
  notification = PopupNotifications.getNotification(
    "foo",
    gBrowser.selectedBrowser
  );
  ok(
    notification,
    "Notification should still be open because we clicked during the security delay."
  );

  // If the notification is no longer shown (test failure) skip the remaining
  // checks.
  if (!notification) {
    return;
  }

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
    !PopupNotifications.getNotification("foo", gBrowser.selectedBrowser),
    "Should not longer see the notification."
  );
});

/**
 * Tests that when we reshow a notification after a tab switch the timeShown
 * attribute is correctly reset and the security delay is enforced.
 */
add_task(async function test_notificationReshowTabSwitch() {
  await ensureSecurityDelayReady();

  ok(
    !PopupNotifications.isPanelOpen,
    "PopupNotification panel should not be open initially."
  );

  info("Open the first notification.");
  let popupShownPromise = waitForNotificationPanel();
  showNotification();
  await popupShownPromise;
  ok(
    PopupNotifications.isPanelOpen,
    "PopupNotification should be open after first show call."
  );

  let notification = PopupNotifications.getNotification(
    "foo",
    gBrowser.selectedBrowser
  );
  is(notification?.id, "foo", "There should be a notification with id foo");
  ok(notification.timeShown, "The notification should have timeShown set");

  info("Trigger main action via button click during security delay");
  triggerMainCommand(PopupNotifications.panel);

  await new Promise(resolve => setTimeout(resolve, 0));

  ok(PopupNotifications.isPanelOpen, "PopupNotification should still be open.");
  notification = PopupNotifications.getNotification(
    "foo",
    gBrowser.selectedBrowser
  );
  ok(
    notification,
    "Notification should still be open because we clicked during the security delay."
  );

  // If the notification is no longer shown (test failure) skip the remaining
  // checks.
  if (!notification) {
    return;
  }

  let panelHiddenPromise = waitForNotificationPanelHidden();
  let panelShownPromise;

  info("Open a new tab which hides the notification panel.");
  await BrowserTestUtils.withNewTab("https://example.com", async browser => {
    info("Wait for panel to be hidden by tab switch.");
    await panelHiddenPromise;
    info(
      "Keep the tab open until the security delay for the original notification show has expired."
    );
    await new Promise(resolve =>
      // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
      setTimeout(resolve, TEST_SECURITY_DELAY + 500)
    );

    panelShownPromise = waitForNotificationPanel();
  });
  info(
    "Wait for the panel to show again after the tab close. We're showing the original tab again."
  );
  await panelShownPromise;

  ok(
    PopupNotifications.isPanelOpen,
    "PopupNotification should be shown after tab close."
  );
  notification = PopupNotifications.getNotification(
    "foo",
    gBrowser.selectedBrowser
  );
  is(
    notification?.id,
    "foo",
    "There should still be a notification with id foo"
  );

  let notificationHiddenPromise = waitForNotificationPanelHidden();

  info(
    "Because we re-show the panel after tab close / switch the security delay should have reset."
  );
  info("Trigger main action via button click during the new security delay.");
  triggerMainCommand(PopupNotifications.panel);

  await new Promise(resolve => setTimeout(resolve, 0));

  ok(PopupNotifications.isPanelOpen, "PopupNotification should still be open.");
  notification = PopupNotifications.getNotification(
    "foo",
    gBrowser.selectedBrowser
  );
  ok(
    notification,
    "Notification should still be open because we clicked during the security delay."
  );
  // If the notification is no longer shown (test failure) skip the remaining
  // checks.
  if (!notification) {
    return;
  }

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
    !PopupNotifications.getNotification("foo", gBrowser.selectedBrowser),
    "Should not longer see the notification."
  );
});
