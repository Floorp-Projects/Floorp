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

// This happens synchronously during installation. It shouldn't take that long
// and if something goes wrong we really don't want to sit around waiting for
// it.
export const backgroundTaskTimeoutSec = 30;

export async function runBackgroundTask() {
  console.log("Running BackgroundTask_install.");

  console.log("Cleaning up update files.");
  try {
    await Cc["@mozilla.org/updates/update-manager;1"]
      .getService(Ci.nsIUpdateManager)
      .doInstallCleanup();
  } catch (ex) {
    console.error(ex);
  }
}
