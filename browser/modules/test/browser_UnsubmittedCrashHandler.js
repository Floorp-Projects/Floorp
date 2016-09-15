"use strict";

/**
 * This suite tests the "unsubmitted crash report" notification
 * that is seen when we detect pending crash reports on startup.
 */

const { UnsubmittedCrashHandler } =
  Cu.import("resource:///modules/ContentCrashHandlers.jsm", this);
const { FileUtils } =
  Cu.import("resource://gre/modules/FileUtils.jsm", this);
const { makeFakeAppDir }  =
  Cu.import("resource://testing-common/AppData.jsm", this);
const { OS } =
  Cu.import("resource://gre/modules/osfile.jsm", this);

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
  let createFile = (fileName, extension, accessDate, contents) => {
    let file = dir.clone();
    file.append(fileName + "." + extension);
    file.create(Ci.nsILocalFile.NORMAL_FILE_TYPE, FileUtils.PERMS_FILE);
    let promises = [OS.File.setDates(file.path, accessDate)];

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
  yield waitForIgnoredReports(reportIDs);

  notification =
    yield UnsubmittedCrashHandler.checkForUnsubmittedCrashReports();
  Assert.equal(notification, null, "There should be no notification");

  clearPendingCrashReports();
});
