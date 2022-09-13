/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

var EXPORTED_SYMBOLS = ["runBackgroundTask"];
async function runBackgroundTask(commandLine) {
  if (AppConstants.platform !== "win") {
    console.log("Not a Windows install, skipping `uninstall` background task.");
    return;
  }
  console.log("Running BackgroundTask_uninstall.");

  removeNotifications();
}

function removeNotifications() {
  console.log("Removing Windows toast notifications.");

  if (!("nsIWindowsAlertsService" in Ci)) {
    console.log("nsIWindowsAlertService not present.");
    return;
  }

  let alertsService;
  try {
    alertsService = Cc["@mozilla.org/system-alerts-service;1"]
      .getService(Ci.nsIAlertsService)
      .QueryInterface(Ci.nsIWindowsAlertsService);
  } catch (e) {
    console.error("Error retrieving nsIWindowsAlertService: " + e.message);
    return;
  }

  alertsService.removeAllNotificationsForInstall();
  console.log("Finished removing Windows toast notifications.");
}
