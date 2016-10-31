/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Pass an empty scope object to the import to prevent "leaked window property"
// errors in tests.
var Preferences = Cu.import("resource://gre/modules/Preferences.jsm", {}).Preferences;
var TelemetryReportingPolicy =
  Cu.import("resource://gre/modules/TelemetryReportingPolicy.jsm", {}).TelemetryReportingPolicy;

const PREF_BRANCH = "datareporting.policy.";
const PREF_BYPASS_NOTIFICATION = PREF_BRANCH + "dataSubmissionPolicyBypassNotification";
const PREF_CURRENT_POLICY_VERSION = PREF_BRANCH + "currentPolicyVersion";
const PREF_ACCEPTED_POLICY_VERSION = PREF_BRANCH + "dataSubmissionPolicyAcceptedVersion";
const PREF_ACCEPTED_POLICY_DATE = PREF_BRANCH + "dataSubmissionPolicyNotifiedTime";

const TEST_POLICY_VERSION = 37;

function fakeShowPolicyTimeout(set, clear) {
  let reportingPolicy =
    Cu.import("resource://gre/modules/TelemetryReportingPolicy.jsm", {}).Policy;
  reportingPolicy.setShowInfobarTimeout = set;
  reportingPolicy.clearShowInfobarTimeout = clear;
}

function sendSessionRestoredNotification() {
  let reportingPolicyImpl =
    Cu.import("resource://gre/modules/TelemetryReportingPolicy.jsm", {}).TelemetryReportingPolicyImpl;
  reportingPolicyImpl.observe(null, "sessionstore-windows-restored", null);
}

/**
 * Wait for a tick.
 */
function promiseNextTick() {
  return new Promise(resolve => executeSoon(resolve));
}

/**
 * Wait for a notification to be shown in a notification box.
 * @param {Object} aNotificationBox The notification box.
 * @return {Promise} Resolved when the notification is displayed.
 */
function promiseWaitForAlertActive(aNotificationBox) {
  let deferred = PromiseUtils.defer();
  aNotificationBox.addEventListener("AlertActive", function onActive() {
    aNotificationBox.removeEventListener("AlertActive", onActive, true);
    deferred.resolve();
  });
  return deferred.promise;
}

/**
 * Wait for a notification to be closed.
 * @param {Object} aNotification The notification.
 * @return {Promise} Resolved when the notification is closed.
 */
function promiseWaitForNotificationClose(aNotification) {
  let deferred = PromiseUtils.defer();
  waitForNotificationClose(aNotification, deferred.resolve);
  return deferred.promise;
}

function triggerInfoBar(expectedTimeoutMs) {
  let showInfobarCallback = null;
  let timeoutMs = null;
  fakeShowPolicyTimeout((callback, timeout) => {
    showInfobarCallback = callback;
    timeoutMs = timeout;
  }, () => {});
  sendSessionRestoredNotification();
  Assert.ok(!!showInfobarCallback, "Must have a timer callback.");
  if (expectedTimeoutMs !== undefined) {
    Assert.equal(timeoutMs, expectedTimeoutMs, "Timeout should match");
  }
  showInfobarCallback();
}

var checkInfobarButton = Task.async(function* (aNotification) {
  // Check that the button on the data choices infobar does the right thing.
  let buttons = aNotification.getElementsByTagName("button");
  Assert.equal(buttons.length, 1, "There is 1 button in the data reporting notification.");
  let button = buttons[0];

  // Add an observer to ensure the "advanced" pane opened (but don't bother
  // closing it - we close the entire window when done.)
  let paneLoadedPromise = promiseTopicObserved("advanced-pane-loaded");

  // Click on the button.
  button.click();

  // Wait for the preferences panel to open.
  yield paneLoadedPromise;
  yield promiseNextTick();
});

add_task(function* setup() {
  const bypassNotification = Preferences.get(PREF_BYPASS_NOTIFICATION, true);
  const currentPolicyVersion = Preferences.get(PREF_CURRENT_POLICY_VERSION, 1);

  // Register a cleanup function to reset our preferences.
  registerCleanupFunction(() => {
    Preferences.set(PREF_BYPASS_NOTIFICATION, bypassNotification);
    Preferences.set(PREF_CURRENT_POLICY_VERSION, currentPolicyVersion);

    return closeAllNotifications();
  });

  // Don't skip the infobar visualisation.
  Preferences.set(PREF_BYPASS_NOTIFICATION, false);
  // Set the current policy version.
  Preferences.set(PREF_CURRENT_POLICY_VERSION, TEST_POLICY_VERSION);
});

function clearAcceptedPolicy() {
  // Reset the accepted policy.
  Preferences.reset(PREF_ACCEPTED_POLICY_VERSION);
  Preferences.reset(PREF_ACCEPTED_POLICY_DATE);
}

add_task(function* test_single_window() {
  clearAcceptedPolicy();

  // Close all the notifications, then try to trigger the data choices infobar.
  yield closeAllNotifications();

  let notificationBox = document.getElementById("global-notificationbox");

  // Make sure that we have a coherent initial state.
  Assert.equal(Preferences.get(PREF_ACCEPTED_POLICY_VERSION, 0), 0,
               "No version should be set on init.");
  Assert.equal(Preferences.get(PREF_ACCEPTED_POLICY_DATE, 0), 0,
               "No date should be set on init.");
  Assert.ok(!TelemetryReportingPolicy.testIsUserNotified(),
            "User not notified about datareporting policy.");

  let alertShownPromise = promiseWaitForAlertActive(notificationBox);
  Assert.ok(!TelemetryReportingPolicy.canUpload(),
            "User should not be allowed to upload.");

  // Wait for the infobar to be displayed.
  triggerInfoBar(10 * 1000);
  yield alertShownPromise;

  Assert.equal(notificationBox.allNotifications.length, 1, "Notification Displayed.");
  Assert.ok(TelemetryReportingPolicy.canUpload(), "User should be allowed to upload now.");

  yield promiseNextTick();
  let promiseClosed = promiseWaitForNotificationClose(notificationBox.currentNotification);
  yield checkInfobarButton(notificationBox.currentNotification);
  yield promiseClosed;

  Assert.equal(notificationBox.allNotifications.length, 0, "No notifications remain.");

  // Check that we are still clear to upload and that the policy data is saved.
  Assert.ok(TelemetryReportingPolicy.canUpload());
  Assert.equal(TelemetryReportingPolicy.testIsUserNotified(), true,
               "User notified about datareporting policy.");
  Assert.equal(Preferences.get(PREF_ACCEPTED_POLICY_VERSION, 0), TEST_POLICY_VERSION,
               "Version pref set.");
  Assert.greater(parseInt(Preferences.get(PREF_ACCEPTED_POLICY_DATE, null), 10), -1,
                 "Date pref set.");
});

add_task(function* test_multiple_windows() {
  clearAcceptedPolicy();

  // Close all the notifications, then try to trigger the data choices infobar.
  yield closeAllNotifications();

  // Ensure we see the notification on all windows and that action on one window
  // results in dismiss on every window.
  let otherWindow = yield BrowserTestUtils.openNewBrowserWindow();

  // Get the notification box for both windows.
  let notificationBoxes = [
    document.getElementById("global-notificationbox"),
    otherWindow.document.getElementById("global-notificationbox")
  ];

  Assert.ok(notificationBoxes[1], "2nd window has a global notification box.");

  // Make sure that we have a coherent initial state.
  Assert.equal(Preferences.get(PREF_ACCEPTED_POLICY_VERSION, 0), 0, "No version should be set on init.");
  Assert.equal(Preferences.get(PREF_ACCEPTED_POLICY_DATE, 0), 0, "No date should be set on init.");
  Assert.ok(!TelemetryReportingPolicy.testIsUserNotified(), "User not notified about datareporting policy.");

  let showAlertPromises = [
    promiseWaitForAlertActive(notificationBoxes[0]),
    promiseWaitForAlertActive(notificationBoxes[1])
  ];

  Assert.ok(!TelemetryReportingPolicy.canUpload(),
            "User should not be allowed to upload.");

  // Wait for the infobars.
  triggerInfoBar(10 * 1000);
  yield Promise.all(showAlertPromises);

  // Both notification were displayed. Close one and check that both gets closed.
  let closeAlertPromises = [
    promiseWaitForNotificationClose(notificationBoxes[0].currentNotification),
    promiseWaitForNotificationClose(notificationBoxes[1].currentNotification)
  ];
  notificationBoxes[0].currentNotification.close();
  yield Promise.all(closeAlertPromises);

  // Close the second window we opened.
  yield BrowserTestUtils.closeWindow(otherWindow);

  // Check that we are clear to upload and that the policy data us saved.
  Assert.ok(TelemetryReportingPolicy.canUpload(), "User should be allowed to upload now.");
  Assert.equal(TelemetryReportingPolicy.testIsUserNotified(), true,
               "User notified about datareporting policy.");
  Assert.equal(Preferences.get(PREF_ACCEPTED_POLICY_VERSION, 0), TEST_POLICY_VERSION,
               "Version pref set.");
  Assert.greater(parseInt(Preferences.get(PREF_ACCEPTED_POLICY_DATE, null), 10), -1,
                 "Date pref set.");
});
