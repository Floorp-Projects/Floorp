/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { BackupService } = ChromeUtils.importESModule(
  "resource:///modules/backup/BackupService.sys.mjs"
);

const { TelemetryTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TelemetryTestUtils.sys.mjs"
);

add_setup(() => {
  do_get_profile();
  // FOG needs to be initialized in order for data to flow.
  Services.fog.initializeFOG();
  Services.telemetry.clearScalars();
});

/**
 * Tests that we can measure the disk space available in the profile directory.
 */
add_task(async function test_profDDiskSpace() {
  let bs = new BackupService();
  await bs.takeMeasurements();
  let measurement = Glean.browserBackup.profDDiskSpace.testGetValue();
  TelemetryTestUtils.assertScalar(
    TelemetryTestUtils.getProcessScalars("parent", false, true),
    "browser.backup.prof_d_disk_space",
    measurement
  );

  Assert.greater(
    measurement,
    0,
    "Should have collected a measurement for the profile directory storage " +
      "device"
  );
});
