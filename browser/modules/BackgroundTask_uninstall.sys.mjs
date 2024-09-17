/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This task ought to have an ephemeral profile and should not apply updates.
 * These settings are controlled externally, by
 * `BackgroundTasks::IsUpdatingTaskName` and
 * `BackgroundTasks::IsEphemeralProfileTaskName`.
 */

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";

export async function runBackgroundTask() {
  console.log("Running BackgroundTask_uninstall.");

  if (AppConstants.platform === "win") {
    try {
      removeNotifications();
    } catch (ex) {
      console.error(ex);
    }
  } else {
    console.log("Not a Windows install. Skipping notification removal.");
  }

  console.log("Cleaning up update files.");
  try {
    await Cc["@mozilla.org/updates/update-manager;1"]
      .getService(Ci.nsIUpdateManager)
      .doUninstallCleanup();
  } catch (ex) {
    console.error(ex);
  }
}

function removeNotifications() {
  console.log("Removing Windows toast notifications.");

  if (!("nsIWindowsAlertsService" in Ci)) {
    console.log("nsIWindowsAlertsService not present.");
    return;
  }

  let alertsService;
  try {
    alertsService = Cc["@mozilla.org/system-alerts-service;1"]
      .getService(Ci.nsIAlertsService)
      .QueryInterface(Ci.nsIWindowsAlertsService);
  } catch (e) {
    console.error("Error retrieving nsIWindowsAlertsService: " + e.message);
    return;
  }

  alertsService.removeAllNotificationsForInstall();
  console.log("Finished removing Windows toast notifications.");
}
