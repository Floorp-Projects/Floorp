/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_SECURITY_DELAY = 5000;

SimpleTest.requestCompleteLog();

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

/**
 * Test helper for security delay tests which performs the following steps:
 * 1. Shows a test notification.
 * 2. Waits for the notification panel to be shown and checks that the initial
 *    security delay blocks clicks.
 * 3. Waits for the security delay to expire. Clicks should now be allowed.
 * 4. Executes the provided onSecurityDelayExpired function. This function
 *    should renew the security delay.
 * 5. Tests that the security delay was renewed.
 * 6. Ensures that after the security delay the notification can be closed.
 *
 * @param {*} options
 * @param {function} options.onSecurityDelayExpired - Function to run after the
 *  security delay has expired. This function should trigger a renew of the
 *  security delay.
 * @param {function} options.cleanupFn - Optional cleanup function to run after
 * the test has completed.
 * @returns {Promise<void>} - Resolves when the test has completed.
 */
async function runPopupNotificationSecurityDelayTest({
  onSecurityDelayExpired,
  cleanupFn = () => {},
}) {
  await ensureSecurityDelayReady();

  info("Open a notification.");
  let popupShownPromise = waitForNotificationPanel();
  showNotification();
  await popupShownPromise;
  ok(
    PopupNotifications.isPanelOpen,
    "PopupNotification should be open after show call."
  );

  // Test that the initial security delay works.
  info(
    "Trigger main action via button click during the initial security delay."
  );
  triggerMainCommand(PopupNotifications.panel);

  await new Promise(resolve => setTimeout(resolve, 0));

  ok(PopupNotifications.isPanelOpen, "PopupNotification should still be open.");
  let notification = PopupNotifications.getNotification(
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
    await cleanupFn();
    return;
  }

  info("Wait for security delay to expire.");
  await new Promise(resolve =>
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    setTimeout(resolve, TEST_SECURITY_DELAY + 500)
  );

  info("Run test specific actions which restarts the security delay.");
  await onSecurityDelayExpired();

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
    await cleanupFn();
    return;
  }

  // Ensure that once the security delay has passed the notification can be
  // closed again.
  let fakeTimeShown = TEST_SECURITY_DELAY + 500;
  info(`Manually set timeShown to ${fakeTimeShown}ms in the past.`);
  notification.timeShown = performance.now() - fakeTimeShown;

  info("Trigger main action via button click outside security delay");
  let notificationHiddenPromise = waitForNotificationPanelHidden();
  triggerMainCommand(PopupNotifications.panel);

  info("Wait for panel to be hidden.");
  await notificationHiddenPromise;

  ok(
    !PopupNotifications.getNotification("foo", gBrowser.selectedBrowser),
    "Should no longer see the notification."
  );

  info("Cleanup.");
  await cleanupFn();
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
    "Should no longer see the notification."
  );
});

/**
 * Tests that when we reshow a notification after a tab switch the timeShown
 * attribute is correctly reset and the security delay is enforced.
 */
add_task(async function test_notificationReshowTabSwitch() {
  await runPopupNotificationSecurityDelayTest({
    onSecurityDelayExpired: async () => {
      let panelHiddenPromise = waitForNotificationPanelHidden();
      let panelShownPromise;

      info("Open a new tab which hides the notification panel.");
      await BrowserTestUtils.withNewTab("https://example.com", async () => {
        info("Wait for panel to be hidden by tab switch.");
        await panelHiddenPromise;
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
      let notification = PopupNotifications.getNotification(
        "foo",
        gBrowser.selectedBrowser
      );
      is(
        notification?.id,
        "foo",
        "There should still be a notification with id foo"
      );

      info(
        "Because we re-show the panel after tab close / switch the security delay should have reset."
      );
    },
  });
});

/**
 * Tests that the security delay gets reset when a window is repositioned and
 * the PopupNotifications panel position is updated.
 */
add_task(async function test_notificationWindowMove() {
  let screenX, screenY;

  await runPopupNotificationSecurityDelayTest({
    onSecurityDelayExpired: async () => {
      info("Reposition the window");
      // Remember original window position.
      screenX = window.screenX;
      screenY = window.screenY;

      let promisePopupPositioned = BrowserTestUtils.waitForEvent(
        PopupNotifications.panel,
        "popuppositioned"
      );

      // Move the window.
      window.moveTo(200, 200);

      // Wait for the panel to reposition and the PopupNotifications listener to run.
      await promisePopupPositioned;
      await new Promise(resolve => setTimeout(resolve, 0));
    },
    cleanupFn: async () => {
      // Reset window position
      window.moveTo(screenX, screenY);
    },
  });
});

/**
 * Tests that the security delay gets extended if a notification is shown during
 * a full screen transition.
 */
add_task(async function test_notificationDuringFullScreenTransition() {
  // Log full screen transition messages.
  let loggingObserver = {
    observe(subject, topic) {
      info("Observed topic: " + topic);
    },
  };
  Services.obs.addObserver(loggingObserver, "fullscreen-transition-start");
  Services.obs.addObserver(loggingObserver, "fullscreen-transition-end");
  // Unregister observers when the test ends:
  registerCleanupFunction(() => {
    Services.obs.removeObserver(loggingObserver, "fullscreen-transition-start");
    Services.obs.removeObserver(loggingObserver, "fullscreen-transition-end");
  });

  if (Services.appinfo.OS == "Linux") {
    ok(
      "Skipping test on Linux because of disabled full screen transition in CI."
    );
    return;
  }
  // Bug 1882527: Intermittent failures on macOS.
  if (Services.appinfo.OS == "Darwin") {
    ok("Skipping test on macOS because of intermittent failures.");
    return;
  }

  await BrowserTestUtils.withNewTab("https://example.com", async browser => {
    await SpecialPowers.pushPrefEnv({
      set: [
        // Set a short security delay so we can observe it being extended.
        ["security.notification_enable_delay", 1],
        // Set a longer full screen exit transition so the test works on slow builds.
        ["full-screen-api.transition-duration.leave", "1000 1000"],
        // Waive the user activation requirement for full screen requests.
        // The PoC this test is based on relies on spam clicking which grants
        // user activation in the popup that requests full screen.
        // This isn't reliable in automation.
        ["full-screen-api.allow-trusted-requests-only", false],
        // macOS native full screen is not affected by the full screen
        // transition overlap. Test with the old full screen implementation.
        ["full-screen-api.macos-native-full-screen", false],
      ],
    });

    await ensureSecurityDelayReady();

    ok(
      !PopupNotifications.isPanelOpen,
      "PopupNotification panel should not be open initially."
    );

    info("Open a notification.");
    let popupShownPromise = waitForNotificationPanel();
    showNotification();
    await popupShownPromise;
    ok(
      PopupNotifications.isPanelOpen,
      "PopupNotification should be open after show call."
    );

    let notification = PopupNotifications.getNotification("foo", browser);
    is(notification?.id, "foo", "There should be a notification with id foo");

    info(
      "Open a new tab via window.open, enter full screen and remove the tab."
    );

    // There are two transitions, one for full screen entry and one for full screen exit.
    let transitionStartCount = 0;
    let transitionEndCount = 0;
    let promiseFullScreenTransitionStart = TestUtils.topicObserved(
      "fullscreen-transition-start",
      () => {
        transitionStartCount++;
        return transitionStartCount == 2;
      }
    );
    let promiseFullScreenTransitionEnd = TestUtils.topicObserved(
      "fullscreen-transition-end",
      () => {
        transitionEndCount++;
        return transitionEndCount == 2;
      }
    );
    let notificationShownPromise = waitForNotificationPanel();

    await SpecialPowers.spawn(browser, [], () => {
      // Use eval to execute in the privilege context of the website.
      content.eval(`
           let button = document.createElement("button");
           button.id = "triggerBtn";
           button.innerText = "Open Popup";
           button.addEventListener("click", () => {
             let popup = window.open("about:blank");
             popup.document.write(
               "<script>setTimeout(() => document.documentElement.requestFullscreen(), 500)</script>"
             );
             popup.document.write(
               "<script>setTimeout(() => window.close(), 1500)</script>"
             );
           });
           // Insert button at the top so the synthesized click works. Otherwise
           // the button may be outside of the viewport.
           document.body.prepend(button);
         `);
    });

    let timeClick = performance.now();
    await BrowserTestUtils.synthesizeMouseAtCenter("#triggerBtn", {}, browser);

    info("Wait for the exit transition to start. It's the second transition.");
    await promiseFullScreenTransitionStart;
    info("Full screen transition start");
    ok(true, "Full screen transition started");
    ok(
      window.isInFullScreenTransition,
      "Full screen transition is still running."
    );

    info(
      "Wait for notification to re-show on tab switch, after the popup has been closed"
    );
    await notificationShownPromise;
    ok(
      window.isInFullScreenTransition,
      "Full screen transition is still running."
    );
    info(
      "about to trigger notification. time between btn click and notification show: " +
        (performance.now() - timeClick)
    );

    info(
      "Trigger main action via button click during the extended security delay."
    );
    triggerMainCommand(PopupNotifications.panel);

    await new Promise(resolve => setTimeout(resolve, 0));

    ok(
      PopupNotifications.isPanelOpen,
      "PopupNotification should still be open."
    );
    notification = PopupNotifications.getNotification(
      "foo",
      gBrowser.selectedBrowser
    );
    ok(
      notification,
      "Notification should still be open because we clicked during the security delay."
    );

    info("Wait for full screen transition end.");
    await promiseFullScreenTransitionEnd;
    info("Full screen transition end");

    await SpecialPowers.popPrefEnv();
  });
});
