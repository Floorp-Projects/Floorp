/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { CredentialsAndSecurityBackupResource } = ChromeUtils.importESModule(
  "resource:///modules/backup/CredentialsAndSecurityBackupResource.sys.mjs"
);

/**
 * Tests that we can measure credentials related files in the profile directory.
 */
add_task(async function test_measure() {
  Services.fog.testResetFOG();

  const EXPECTED_CREDENTIALS_KILOBYTES_SIZE = 413;
  const EXPECTED_SECURITY_KILOBYTES_SIZE = 231;

  // Create resource files in temporary directory
  const tempDir = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "CredentialsAndSecurityBackupResource-measurement-test"
  );

  const mockFiles = [
    // Set up credentials files
    { path: "key4.db", sizeInKB: 300 },
    { path: "logins.json", sizeInKB: 1 },
    { path: "logins-backup.json", sizeInKB: 1 },
    { path: "autofill-profiles.json", sizeInKB: 1 },
    { path: "credentialstate.sqlite", sizeInKB: 100 },
    { path: "signedInUser.json", sizeInKB: 5 },
    // Set up security files
    { path: "cert9.db", sizeInKB: 230 },
    { path: "pkcs11.txt", sizeInKB: 1 },
  ];

  await createTestFiles(tempDir, mockFiles);

  let credentialsAndSecurityBackupResource =
    new CredentialsAndSecurityBackupResource();
  await credentialsAndSecurityBackupResource.measure(tempDir);

  let credentialsMeasurement =
    Glean.browserBackup.credentialsDataSize.testGetValue();
  let securityMeasurement = Glean.browserBackup.securityDataSize.testGetValue();
  let scalars = TelemetryTestUtils.getProcessScalars("parent", false, false);

  // Credentials measurements
  TelemetryTestUtils.assertScalar(
    scalars,
    "browser.backup.credentials_data_size",
    credentialsMeasurement,
    "Glean and telemetry measurements for credentials data should be equal"
  );

  Assert.equal(
    credentialsMeasurement,
    EXPECTED_CREDENTIALS_KILOBYTES_SIZE,
    "Should have collected the correct glean measurement for credentials files"
  );

  // Security measurements
  TelemetryTestUtils.assertScalar(
    scalars,
    "browser.backup.security_data_size",
    securityMeasurement,
    "Glean and telemetry measurements for security data should be equal"
  );
  Assert.equal(
    securityMeasurement,
    EXPECTED_SECURITY_KILOBYTES_SIZE,
    "Should have collected the correct glean measurement for security files"
  );

  // Cleanup
  await maybeRemovePath(tempDir);
});

/**
 * Test that the backup method correctly copies items from the profile directory
 * into the staging directory.
 */
add_task(async function test_backup() {
  let sandbox = sinon.createSandbox();

  let credentialsAndSecurityBackupResource =
    new CredentialsAndSecurityBackupResource();
  let sourcePath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "CredentialsAndSecurityBackupResource-source-test"
  );
  let stagingPath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "CredentialsAndSecurityBackupResource-staging-test"
  );

  const simpleCopyFiles = [
    { path: "logins.json", sizeInKB: 1 },
    { path: "logins-backup.json", sizeInKB: 1 },
    { path: "autofill-profiles.json", sizeInKB: 1 },
    { path: "signedInUser.json", sizeInKB: 5 },
    { path: "pkcs11.txt", sizeInKB: 1 },
  ];
  await createTestFiles(sourcePath, simpleCopyFiles);

  // Create our fake database files. We don't expect these to be copied to the
  // staging directory in this test due to our stubbing of the backup method, so
  // we don't include it in `simpleCopyFiles`.
  await createTestFiles(sourcePath, [
    { path: "cert9.db" },
    { path: "key4.db" },
    { path: "credentialstate.sqlite" },
  ]);

  // We have no need to test that Sqlite.sys.mjs's backup method is working -
  // this is something that is tested in Sqlite's own tests. We can just make
  // sure that it's being called using sinon. Unfortunately, we cannot do the
  // same thing with IOUtils.copy, as its methods are not stubbable.
  let fakeConnection = {
    backup: sandbox.stub().resolves(true),
    close: sandbox.stub().resolves(true),
  };
  sandbox.stub(Sqlite, "openConnection").returns(fakeConnection);

  let manifestEntry = await credentialsAndSecurityBackupResource.backup(
    stagingPath,
    sourcePath
  );

  Assert.equal(
    manifestEntry,
    null,
    "CredentialsAndSecurityBackupResource.backup should return null as its ManifestEntry"
  );

  await assertFilesExist(stagingPath, simpleCopyFiles);

  // Next, we'll make sure that the Sqlite connection had `backup` called on it
  // with the right arguments.
  Assert.ok(
    fakeConnection.backup.calledThrice,
    "Called backup the expected number of times for all connections"
  );
  Assert.ok(
    fakeConnection.backup.firstCall.calledWith(
      PathUtils.join(stagingPath, "cert9.db")
    ),
    "Called backup on cert9.db connection first"
  );
  Assert.ok(
    fakeConnection.backup.secondCall.calledWith(
      PathUtils.join(stagingPath, "key4.db")
    ),
    "Called backup on key4.db connection second"
  );
  Assert.ok(
    fakeConnection.backup.thirdCall.calledWith(
      PathUtils.join(stagingPath, "credentialstate.sqlite")
    ),
    "Called backup on credentialstate.sqlite connection third"
  );

  await maybeRemovePath(stagingPath);
  await maybeRemovePath(sourcePath);

  sandbox.restore();
});

/**
 * Test that the recover method correctly copies items from the recovery
 * directory into the destination profile directory.
 */
add_task(async function test_recover() {
  let credentialsAndSecurityBackupResource =
    new CredentialsAndSecurityBackupResource();
  let recoveryPath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "CredentialsAndSecurityBackupResource-recovery-test"
  );
  let destProfilePath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "CredentialsAndSecurityBackupResource-test-profile"
  );

  const files = [
    { path: "logins.json" },
    { path: "logins-backup.json" },
    { path: "autofill-profiles.json" },
    { path: "credentialstate.sqlite" },
    { path: "signedInUser.json" },
    { path: "cert9.db" },
    { path: "key4.db" },
    { path: "pkcs11.txt" },
  ];
  await createTestFiles(recoveryPath, files);

  // The backup method is expected to have returned a null ManifestEntry
  let postRecoveryEntry = await credentialsAndSecurityBackupResource.recover(
    null /* manifestEntry */,
    recoveryPath,
    destProfilePath
  );
  Assert.equal(
    postRecoveryEntry,
    null,
    "CredentialsAndSecurityBackupResource.recover should return null as its post " +
      "recovery entry"
  );

  await assertFilesExist(destProfilePath, files);

  await maybeRemovePath(recoveryPath);
  await maybeRemovePath(destProfilePath);
});
