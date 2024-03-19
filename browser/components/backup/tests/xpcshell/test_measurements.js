/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { BackupService } = ChromeUtils.importESModule(
  "resource:///modules/backup/BackupService.sys.mjs"
);

const { PlacesBackupResource } = ChromeUtils.importESModule(
  "resource:///modules/backup/PlacesBackupResource.sys.mjs"
);

const { BackupResource } = ChromeUtils.importESModule(
  "resource:///modules/backup/BackupResource.sys.mjs"
);

const { TelemetryTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TelemetryTestUtils.sys.mjs"
);

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

const EXPECTED_PLACES_DB_SIZE = 5240;
const EXPECTED_FAVICONS_DB_SIZE = 5240;

add_setup(() => {
  do_get_profile();
  // FOG needs to be initialized in order for data to flow.
  Services.fog.initializeFOG();
  Services.telemetry.clearScalars();
});

/**
 * Tests that calling `BackupService.takeMeasurements` will call the measure
 * method of all registered BackupResource classes.
 */
add_task(async function test_takeMeasurements() {
  /**
   * Some fake backup resource classes to test with.
   */
  class FakeBackupResource1 extends BackupResource {
    static get key() {
      return "fake1";
    }
    measure() {}
  }

  /**
   * Another fake backup resource class to test with.
   */
  class FakeBackupResource2 extends BackupResource {
    static get key() {
      return "fake2";
    }
    measure() {}
  }

  let sandbox = sinon.createSandbox();
  sandbox.stub(FakeBackupResource1.prototype, "measure").resolves();
  sandbox
    .stub(FakeBackupResource2.prototype, "measure")
    .rejects(new Error("Some failure to measure"));

  let bs = new BackupService({ FakeBackupResource1, FakeBackupResource2 });
  await bs.takeMeasurements();

  for (let backupResourceClass of [FakeBackupResource1, FakeBackupResource2]) {
    Assert.ok(
      backupResourceClass.prototype.measure.calledOnce,
      "Measure was called"
    );
    Assert.ok(
      backupResourceClass.prototype.measure.calledWith(PathUtils.profileDir),
      "Measure was called with the profile directory argument"
    );
  }

  sandbox.restore();
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

/**
 * Tests that we can measure Places DB related files in the profile directory.
 */
add_task(async function test_placesBackupResource() {
  Services.fog.testResetFOG();

  // Create resource files in temporary directory
  let tempDir = PathUtils.tempDir;
  let tempPlacesDBPath = PathUtils.join(tempDir, "places.sqlite");
  let tempFaviconsDBPath = PathUtils.join(tempDir, "favicons.sqlite");
  await createKilobyteSizedFile(tempPlacesDBPath, EXPECTED_PLACES_DB_SIZE);
  await createKilobyteSizedFile(tempFaviconsDBPath, EXPECTED_FAVICONS_DB_SIZE);

  let placesBackupResource = new PlacesBackupResource();
  await placesBackupResource.measure(tempDir);

  let placesMeasurement = Glean.browserBackup.placesSize.testGetValue();
  let faviconsMeasurement = Glean.browserBackup.faviconsSize.testGetValue();
  let scalars = TelemetryTestUtils.getProcessScalars("parent", false, false);

  // Compare glean vs telemetry measurements
  TelemetryTestUtils.assertScalar(
    scalars,
    "browser.backup.places_size",
    placesMeasurement,
    "Glean and telemetry measurements for places.sqlite should be equal"
  );
  TelemetryTestUtils.assertScalar(
    scalars,
    "browser.backup.favicons_size",
    faviconsMeasurement,
    "Glean and telemetry measurements for favicons.sqlite should be equal"
  );

  // Compare glean measurements vs actual file sizes
  Assert.equal(
    placesMeasurement,
    EXPECTED_PLACES_DB_SIZE,
    "Should have collected the correct glean measurement for places.sqlite"
  );
  Assert.equal(
    faviconsMeasurement,
    EXPECTED_FAVICONS_DB_SIZE,
    "Should have collected the correct glean measurement for favicons.sqlite"
  );

  await IOUtils.remove(tempPlacesDBPath);
  await IOUtils.remove(tempFaviconsDBPath);
});
