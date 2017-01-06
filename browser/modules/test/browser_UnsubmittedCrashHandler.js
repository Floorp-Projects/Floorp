"use strict";

/**
 * This suite tests the "unsubmitted crash report" notification
 * that is seen when we detect pending crash reports on startup.
 */

const { UnsubmittedCrashHandler } =
  Cu.import("resource:///modules/ContentCrashHandlers.jsm", {});
const { FileUtils } =
  Cu.import("resource://gre/modules/FileUtils.jsm", {});
const { makeFakeAppDir }  =
  Cu.import("resource://testing-common/AppData.jsm", {});
const { OS } =
  Cu.import("resource://gre/modules/osfile.jsm", {});

const DAY = 24 * 60 * 60 * 1000; // milliseconds
const SERVER_URL = "http://example.com/browser/toolkit/crashreporter/test/browser/crashreport.sjs";

/**
 * Returns the directly where the browsing is storing the
 * pending crash reports.
 *
 * @returns nsIFile
 */
function getPendingCrashReportDir() {
  // The fake UAppData directory that makeFakeAppDir provides
  // is just UAppData under the profile directory.
  return FileUtils.getDir("ProfD", [
    "UAppData",
    "Crash Reports",
    "pending",
  ], false);
}

/**
 * Synchronously deletes all entries inside the pending
 * crash report directory.
 */
function clearPendingCrashReports() {
  let dir = getPendingCrashReportDir();
  let entries = dir.directoryEntries;

  while (entries.hasMoreElements()) {
    let entry = entries.getNext().QueryInterface(Ci.nsIFile);
    if (entry.isFile()) {
      entry.remove(false);
    }
  }
}

/**
 * Randomly generates howMany crash report .dmp and .extra files
 * to put into the pending crash report directory. We're not
 * actually creating real crash reports here, just stubbing
 * out enough of the files to satisfy our notification and
 * submission code.
 *
 * @param howMany (int)
 *        How many pending crash reports to put in the pending
 *        crash report directory.
 * @param accessDate (Date, optional)
 *        What date to set as the last accessed time on the created
 *        crash reports. This defaults to the current date and time.
 * @returns Promise
 */
function* createPendingCrashReports(howMany, accessDate) {
  let dir = getPendingCrashReportDir();
  if (!accessDate) {
    accessDate = new Date();
  }

  /**
   * Helper function for creating a file in the pending crash report
   * directory.
   *
   * @param fileName (string)
   *        The filename for the crash report, not including the
   *        extension. This is usually a UUID.
   * @param extension (string)
   *        The file extension for the created file.
   * @param accessDate (Date)
   *        The date to set lastAccessed to.
   * @param contents (string, optional)
   *        Set this to whatever the file needs to contain, if anything.
   * @returns Promise
   */
  let createFile = (fileName, extension, lastAccessedDate, contents) => {
    let file = dir.clone();
    file.append(fileName + "." + extension);
    file.create(Ci.nsILocalFile.NORMAL_FILE_TYPE, FileUtils.PERMS_FILE);
    let promises = [OS.File.setDates(file.path, lastAccessedDate)];

    if (contents) {
      let encoder = new TextEncoder();
      let array = encoder.encode(contents);
      promises.push(OS.File.writeAtomic(file.path, array, {
        tmpPath: file.path + ".tmp",
      }));
    }
    return Promise.all(promises);
  }

  let uuidGenerator = Cc["@mozilla.org/uuid-generator;1"]
                      .getService(Ci.nsIUUIDGenerator);
  // CrashSubmit expects there to be a ServerURL key-value
  // pair in the .extra file, so we'll satisfy it.
  let extraFileContents = "ServerURL=" + SERVER_URL;

  return Task.spawn(function*() {
    let uuids = [];
    for (let i = 0; i < howMany; ++i) {
      let uuid = uuidGenerator.generateUUID().toString();
      // Strip the {}...
      uuid = uuid.substring(1, uuid.length - 1);
      yield createFile(uuid, "dmp", accessDate);
      yield createFile(uuid, "extra", accessDate, extraFileContents);
      uuids.push(uuid);
    }
    return uuids;
  });
}

/**
 * Returns a Promise that resolves once CrashSubmit starts sending
 * success notifications for crash submission matching the reportIDs
 * being passed in.
 *
 * @param reportIDs (Array<string>)
 *        The IDs for the reports that we expect CrashSubmit to have sent.
 * @returns Promise
 */
function waitForSubmittedReports(reportIDs) {
  let promises = [];
  for (let reportID of reportIDs) {
    let promise = TestUtils.topicObserved("crash-report-status", (subject, data) => {
      if (data == "success") {
        let propBag = subject.QueryInterface(Ci.nsIPropertyBag2);
        let dumpID = propBag.getPropertyAsAString("minidumpID");
        if (dumpID == reportID) {
          return true;
        }
      }
      return false;
    });
    promises.push(promise);
  }
  return Promise.all(promises);
}

/**
 * Returns a Promise that resolves once a .dmp.ignore file is created for
 * the crashes in the pending directory matching the reportIDs being
 * passed in.
 *
 * @param reportIDs (Array<string>)
 *        The IDs for the reports that we expect CrashSubmit to have been
 *        marked for ignoring.
 * @returns Promise
 */
function waitForIgnoredReports(reportIDs) {
  let dir = getPendingCrashReportDir();
  let promises = [];
  for (let reportID of reportIDs) {
    let file = dir.clone();
    file.append(reportID + ".dmp.ignore");
    promises.push(OS.File.exists(file.path));
  }
  return Promise.all(promises);
}

let gNotificationBox;

add_task(function* setup() {
  // Pending crash reports are stored in the UAppData folder,
  // which exists outside of the profile folder. In order to
  // not overwrite / clear pending crash reports for the poor
  // soul who runs this test, we use AppData.jsm to point to
  // a special made-up directory inside the profile
  // directory.
  yield makeFakeAppDir();
  // We'll assume that the notifications will be shown in the current
  // browser window's global notification box.
  gNotificationBox = document.getElementById("global-notificationbox");

  // If we happen to already be seeing the unsent crash report
  // notification, it's because the developer running this test
  // happened to have some unsent reports in their UAppDir.
  // We'll remove the notification without touching those reports.
  let notification =
    gNotificationBox.getNotificationWithValue("pending-crash-reports");
  if (notification) {
    notification.close();
  }

  let env = Cc["@mozilla.org/process/environment;1"]
              .getService(Components.interfaces.nsIEnvironment);
  let oldServerURL = env.get("MOZ_CRASHREPORTER_URL");
  env.set("MOZ_CRASHREPORTER_URL", SERVER_URL);

  // nsBrowserGlue starts up UnsubmittedCrashHandler automatically
  // so at this point, it is initialized. It's possible that it
  // was initialized, but is preffed off, so it's inert, so we
  // shut it down, make sure it's preffed on, and then restart it.
  // Note that making the component initialize even when it's
  // disabled is an intentional choice, as this allows for easier
  // simulation of startup and shutdown.
  UnsubmittedCrashHandler.uninit();
  yield SpecialPowers.pushPrefEnv({
    set: [
      ["browser.crashReports.unsubmittedCheck.enabled", true],
    ],
  });
  UnsubmittedCrashHandler.init();

  registerCleanupFunction(function() {
    gNotificationBox = null;
    clearPendingCrashReports();
    env.set("MOZ_CRASHREPORTER_URL", oldServerURL);
  });
});

/**
 * Tests that if there are no pending crash reports, then the
 * notification will not show up.
 */
add_task(function* test_no_pending_no_notification() {
  // Make absolutely sure there are no pending crash reports first...
  clearPendingCrashReports();
  let notification =
    yield UnsubmittedCrashHandler.checkForUnsubmittedCrashReports();
  Assert.equal(notification, null,
               "There should not be a notification if there are no " +
               "pending crash reports");
});

/**
 * Tests that there is a notification if there is one pending
 * crash report.
 */
add_task(function* test_one_pending() {
  yield createPendingCrashReports(1);
  let notification =
    yield UnsubmittedCrashHandler.checkForUnsubmittedCrashReports();
  Assert.ok(notification, "There should be a notification");

  gNotificationBox.removeNotification(notification, true);
  clearPendingCrashReports();
});

/**
 * Tests that there is a notification if there is more than one
 * pending crash report.
 */
add_task(function* test_several_pending() {
  yield createPendingCrashReports(3);
  let notification =
    yield UnsubmittedCrashHandler.checkForUnsubmittedCrashReports();
  Assert.ok(notification, "There should be a notification");

  gNotificationBox.removeNotification(notification, true);
  clearPendingCrashReports();
});

/**
 * Tests that there is no notification if the only pending crash
 * reports are over 28 days old. Also checks that if we put a newer
 * crash with that older set, that we can still get a notification.
 */
add_task(function* test_several_pending() {
  // Let's create some crash reports from 30 days ago.
  let oldDate = new Date(Date.now() - (30 * DAY));
  yield createPendingCrashReports(3, oldDate);
  let notification =
    yield UnsubmittedCrashHandler.checkForUnsubmittedCrashReports();
  Assert.equal(notification, null,
               "There should not be a notification if there are only " +
               "old pending crash reports");
  // Now let's create a new one and check again
  yield createPendingCrashReports(1);
  notification =
    yield UnsubmittedCrashHandler.checkForUnsubmittedCrashReports();
  Assert.ok(notification, "There should be a notification");

  gNotificationBox.removeNotification(notification, true);
  clearPendingCrashReports();
});

/**
 * Tests that the notification can submit a report.
 */
add_task(function* test_can_submit() {
  let reportIDs = yield createPendingCrashReports(1);
  let notification =
    yield UnsubmittedCrashHandler.checkForUnsubmittedCrashReports();
  Assert.ok(notification, "There should be a notification");

  // Attempt to submit the notification by clicking on the submit
  // button
  let buttons = notification.querySelectorAll(".notification-button");
  // ...which should be the first button.
  let submit = buttons[0];

  let promiseReports = waitForSubmittedReports(reportIDs);
  info("Sending crash report");
  submit.click();
  info("Sent!");
  // We'll not wait for the notification to finish its transition -
  // we'll just remove it right away.
  gNotificationBox.removeNotification(notification, true);

  info("Waiting on reports to be received.");
  yield promiseReports;
  info("Received!");
  clearPendingCrashReports();
});

/**
 * Tests that the notification can submit multiple reports.
 */
add_task(function* test_can_submit_several() {
  let reportIDs = yield createPendingCrashReports(3);
  let notification =
    yield UnsubmittedCrashHandler.checkForUnsubmittedCrashReports();
  Assert.ok(notification, "There should be a notification");

  // Attempt to submit the notification by clicking on the submit
  // button
  let buttons = notification.querySelectorAll(".notification-button");
  // ...which should be the first button.
  let submit = buttons[0];

  let promiseReports = waitForSubmittedReports(reportIDs);
  info("Sending crash reports");
  submit.click();
  info("Sent!");
  // We'll not wait for the notification to finish its transition -
  // we'll just remove it right away.
  gNotificationBox.removeNotification(notification, true);

  info("Waiting on reports to be received.");
  yield promiseReports;
  info("Received!");
  clearPendingCrashReports();
});

/**
 * Tests that choosing "Send Always" flips the autoSubmit pref
 * and sends the pending crash reports.
 */
add_task(function* test_can_submit_always() {
  let pref = "browser.crashReports.unsubmittedCheck.autoSubmit";
  Assert.equal(Services.prefs.getBoolPref(pref), false,
               "We should not be auto-submitting by default");

  let reportIDs = yield createPendingCrashReports(1);
  let notification =
    yield UnsubmittedCrashHandler.checkForUnsubmittedCrashReports();
  Assert.ok(notification, "There should be a notification");

  // Attempt to submit the notification by clicking on the send all
  // button
  let buttons = notification.querySelectorAll(".notification-button");
  // ...which should be the second button.
  let sendAll = buttons[1];

  let promiseReports = waitForSubmittedReports(reportIDs);
  info("Sending crash reports");
  sendAll.click();
  info("Sent!");
  // We'll not wait for the notification to finish its transition -
  // we'll just remove it right away.
  gNotificationBox.removeNotification(notification, true);

  info("Waiting on reports to be received.");
  yield promiseReports;
  info("Received!");

  // Make sure the pref was set
  Assert.equal(Services.prefs.getBoolPref(pref), true,
               "The autoSubmit pref should have been set");

  // And revert back to default now.
  Services.prefs.clearUserPref(pref);

  clearPendingCrashReports();
});

/**
 * Tests that if the user has chosen to automatically send
 * crash reports that no notification is displayed to the
 * user.
 */
add_task(function* test_can_auto_submit() {
  yield SpecialPowers.pushPrefEnv({ set: [
    ["browser.crashReports.unsubmittedCheck.autoSubmit", true],
  ]});

  let reportIDs = yield createPendingCrashReports(3);
  let promiseReports = waitForSubmittedReports(reportIDs);
  let notification =
    yield UnsubmittedCrashHandler.checkForUnsubmittedCrashReports();
  Assert.equal(notification, null, "There should be no notification");
  info("Waiting on reports to be received.");
  yield promiseReports;
  info("Received!");

  clearPendingCrashReports();
  yield SpecialPowers.popPrefEnv();
});

/**
 * Tests that if the user chooses to dismiss the notification,
 * then the current pending requests won't cause the notification
 * to appear again in the future.
 */
add_task(function* test_can_ignore() {
  let reportIDs = yield createPendingCrashReports(3);
  let notification =
    yield UnsubmittedCrashHandler.checkForUnsubmittedCrashReports();
  Assert.ok(notification, "There should be a notification");

  // Dismiss the notification by clicking on the "X" button.
  let anonyNodes = document.getAnonymousNodes(notification)[0];
  let closeButton = anonyNodes.querySelector(".close-icon");
  closeButton.click();
  // We'll not wait for the notification to finish its transition -
  // we'll just remove it right away.
  gNotificationBox.removeNotification(notification, true);
  yield waitForIgnoredReports(reportIDs);

  notification =
    yield UnsubmittedCrashHandler.checkForUnsubmittedCrashReports();
  Assert.equal(notification, null, "There should be no notification");

  clearPendingCrashReports();
});

/**
 * Tests that if the notification is shown, then the
 * lastShownDate is set for today.
 */
add_task(function* test_last_shown_date() {
  yield createPendingCrashReports(1);
  let notification =
    yield UnsubmittedCrashHandler.checkForUnsubmittedCrashReports();
  Assert.ok(notification, "There should be a notification");

  let today = UnsubmittedCrashHandler.dateString(new Date());
  let lastShownDate =
    UnsubmittedCrashHandler.prefs.getCharPref("lastShownDate");
  Assert.equal(today, lastShownDate,
               "Last shown date should be today.");

  UnsubmittedCrashHandler.prefs.clearUserPref("lastShownDate");
  gNotificationBox.removeNotification(notification, true);
  clearPendingCrashReports();
});

/**
 * Tests that if UnsubmittedCrashHandler is uninit with a
 * notification still being shown, that
 * browser.crashReports.unsubmittedCheck.shutdownWhileShowing is
 * set to true.
 */
add_task(function* test_shutdown_while_showing() {
  yield createPendingCrashReports(1);
  let notification =
    yield UnsubmittedCrashHandler.checkForUnsubmittedCrashReports();
  Assert.ok(notification, "There should be a notification");

  UnsubmittedCrashHandler.uninit();
  let shutdownWhileShowing =
    UnsubmittedCrashHandler.prefs.getBoolPref("shutdownWhileShowing");
  Assert.ok(shutdownWhileShowing,
            "We should have noticed that we uninitted while showing " +
            "the notification.");
  UnsubmittedCrashHandler.prefs.clearUserPref("shutdownWhileShowing");
  UnsubmittedCrashHandler.init();

  gNotificationBox.removeNotification(notification, true);
  clearPendingCrashReports();
});

/**
 * Tests that if UnsubmittedCrashHandler is uninit after
 * the notification has been closed, that
 * browser.crashReports.unsubmittedCheck.shutdownWhileShowing is
 * not set in prefs.
 */
add_task(function* test_shutdown_while_not_showing() {
  let reportIDs = yield createPendingCrashReports(1);
  let notification =
    yield UnsubmittedCrashHandler.checkForUnsubmittedCrashReports();
  Assert.ok(notification, "There should be a notification");

  // Dismiss the notification by clicking on the "X" button.
  let anonyNodes = document.getAnonymousNodes(notification)[0];
  let closeButton = anonyNodes.querySelector(".close-icon");
  closeButton.click();
  // We'll not wait for the notification to finish its transition -
  // we'll just remove it right away.
  gNotificationBox.removeNotification(notification, true);

  yield waitForIgnoredReports(reportIDs);

  UnsubmittedCrashHandler.uninit();
  Assert.throws(() => {
    UnsubmittedCrashHandler.prefs.getBoolPref("shutdownWhileShowing");
  }, "We should have noticed that the notification had closed before " +
     "uninitting.");
  UnsubmittedCrashHandler.init();

  clearPendingCrashReports();
});

/**
 * Tests that if
 * browser.crashReports.unsubmittedCheck.shutdownWhileShowing is
 * set and the lastShownDate is today, then we don't decrement
 * browser.crashReports.unsubmittedCheck.chancesUntilSuppress.
 */
add_task(function* test_dont_decrement_chances_on_same_day() {
  let initChances =
    UnsubmittedCrashHandler.prefs.getIntPref("chancesUntilSuppress");
  Assert.ok(initChances > 1, "We should start with at least 1 chance.");

  yield createPendingCrashReports(1);
  let notification =
    yield UnsubmittedCrashHandler.checkForUnsubmittedCrashReports();
  Assert.ok(notification, "There should be a notification");

  UnsubmittedCrashHandler.uninit();

  gNotificationBox.removeNotification(notification, true);

  let shutdownWhileShowing =
    UnsubmittedCrashHandler.prefs.getBoolPref("shutdownWhileShowing");
  Assert.ok(shutdownWhileShowing,
            "We should have noticed that we uninitted while showing " +
            "the notification.");

  let today = UnsubmittedCrashHandler.dateString(new Date());
  let lastShownDate =
    UnsubmittedCrashHandler.prefs.getCharPref("lastShownDate");
  Assert.equal(today, lastShownDate,
               "Last shown date should be today.");

  UnsubmittedCrashHandler.init();

  notification =
      yield UnsubmittedCrashHandler.checkForUnsubmittedCrashReports();
  Assert.ok(notification, "There should still be a notification");

  let chances =
    UnsubmittedCrashHandler.prefs.getIntPref("chancesUntilSuppress");

  Assert.equal(initChances, chances,
               "We should not have decremented chances.");

  gNotificationBox.removeNotification(notification, true);
  clearPendingCrashReports();
});

/**
 * Tests that if
 * browser.crashReports.unsubmittedCheck.shutdownWhileShowing is
 * set and the lastShownDate is before today, then we decrement
 * browser.crashReports.unsubmittedCheck.chancesUntilSuppress.
 */
add_task(function* test_decrement_chances_on_other_day() {
  let initChances =
    UnsubmittedCrashHandler.prefs.getIntPref("chancesUntilSuppress");
  Assert.ok(initChances > 1, "We should start with at least 1 chance.");

  yield createPendingCrashReports(1);
  let notification =
    yield UnsubmittedCrashHandler.checkForUnsubmittedCrashReports();
  Assert.ok(notification, "There should be a notification");

  UnsubmittedCrashHandler.uninit();

  gNotificationBox.removeNotification(notification, true);

  let shutdownWhileShowing =
    UnsubmittedCrashHandler.prefs.getBoolPref("shutdownWhileShowing");
  Assert.ok(shutdownWhileShowing,
            "We should have noticed that we uninitted while showing " +
            "the notification.");

  // Now pretend that the notification was shown yesterday.
  let yesterday = UnsubmittedCrashHandler.dateString(new Date(Date.now() - DAY));
  UnsubmittedCrashHandler.prefs.setCharPref("lastShownDate", yesterday);

  UnsubmittedCrashHandler.init();

  notification =
    yield UnsubmittedCrashHandler.checkForUnsubmittedCrashReports();
  Assert.ok(notification, "There should still be a notification");

  let chances =
    UnsubmittedCrashHandler.prefs.getIntPref("chancesUntilSuppress");

  Assert.equal(initChances - 1, chances,
               "We should have decremented our chances.");
  UnsubmittedCrashHandler.prefs.clearUserPref("chancesUntilSuppress");

  gNotificationBox.removeNotification(notification, true);
  clearPendingCrashReports();
});

/**
 * Tests that if we've shutdown too many times showing the
 * notification, and we've run out of chances, then
 * browser.crashReports.unsubmittedCheck.suppressUntilDate is
 * set for some days into the future.
 */
add_task(function* test_can_suppress_after_chances() {
  // Pretend that a notification was shown yesterday.
  let yesterday = UnsubmittedCrashHandler.dateString(new Date(Date.now() - DAY));
  UnsubmittedCrashHandler.prefs.setCharPref("lastShownDate", yesterday);
  UnsubmittedCrashHandler.prefs.setBoolPref("shutdownWhileShowing", true);
  UnsubmittedCrashHandler.prefs.setIntPref("chancesUntilSuppress", 0);

  yield createPendingCrashReports(1);
  let notification =
    yield UnsubmittedCrashHandler.checkForUnsubmittedCrashReports();
  Assert.equal(notification, null,
               "There should be no notification if we've run out of chances");

  // We should have set suppressUntilDate into the future
  let suppressUntilDate =
    UnsubmittedCrashHandler.prefs.getCharPref("suppressUntilDate");

  let today = UnsubmittedCrashHandler.dateString(new Date());
  Assert.ok(suppressUntilDate > today,
            "We should be suppressing until some days into the future.");

  UnsubmittedCrashHandler.prefs.clearUserPref("chancesUntilSuppress");
  UnsubmittedCrashHandler.prefs.clearUserPref("suppressUntilDate");
  UnsubmittedCrashHandler.prefs.clearUserPref("lastShownDate");
  clearPendingCrashReports();
});

/**
 * Tests that if there's a suppression date set, then no notification
 * will be shown even if there are pending crash reports.
 */
add_task(function* test_suppression() {
  let future = UnsubmittedCrashHandler.dateString(new Date(Date.now() + (DAY * 5)));
  UnsubmittedCrashHandler.prefs.setCharPref("suppressUntilDate", future);
  UnsubmittedCrashHandler.uninit();
  UnsubmittedCrashHandler.init();

  Assert.ok(UnsubmittedCrashHandler.suppressed,
            "The UnsubmittedCrashHandler should be suppressed.");
  UnsubmittedCrashHandler.prefs.clearUserPref("suppressUntilDate");

  UnsubmittedCrashHandler.uninit();
  UnsubmittedCrashHandler.init();
});

/**
 * Tests that if there's a suppression date set, but we've exceeded
 * it, then we can show the notification again.
 */
add_task(function* test_end_suppression() {
  let yesterday = UnsubmittedCrashHandler.dateString(new Date(Date.now() - DAY));
  UnsubmittedCrashHandler.prefs.setCharPref("suppressUntilDate", yesterday);
  UnsubmittedCrashHandler.uninit();
  UnsubmittedCrashHandler.init();

  Assert.ok(!UnsubmittedCrashHandler.suppressed,
            "The UnsubmittedCrashHandler should not be suppressed.");
  Assert.ok(!UnsubmittedCrashHandler.prefs.prefHasUserValue("suppressUntilDate"),
            "The suppression date should been cleared from preferences.");

  UnsubmittedCrashHandler.uninit();
  UnsubmittedCrashHandler.init();
});
