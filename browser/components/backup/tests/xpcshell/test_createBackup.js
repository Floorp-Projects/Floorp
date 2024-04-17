/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that calling BackupService.createBackup will call backup on each
 * registered BackupResource, and that each BackupResource will have a folder
 * created for them to write into.
 */
add_task(async function test_createBackup() {
  let sandbox = sinon.createSandbox();
  sandbox
    .stub(FakeBackupResource1.prototype, "backup")
    .resolves({ fake1: "hello from 1" });
  sandbox
    .stub(FakeBackupResource2.prototype, "backup")
    .rejects(new Error("Some failure to backup"));
  sandbox
    .stub(FakeBackupResource3.prototype, "backup")
    .resolves({ fake3: "hello from 3" });

  let bs = new BackupService({
    FakeBackupResource1,
    FakeBackupResource2,
    FakeBackupResource3,
  });

  let fakeProfilePath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "createBackupTest"
  );

  await bs.createBackup({ profilePath: fakeProfilePath });

  // For now, we expect a staging folder to exist under the fakeProfilePath,
  // and we should find a folder for each fake BackupResource.
  let stagingPath = PathUtils.join(fakeProfilePath, "backups", "staging");
  Assert.ok(await IOUtils.exists(stagingPath), "Staging folder exists");

  for (let backupResourceClass of [
    FakeBackupResource1,
    FakeBackupResource2,
    FakeBackupResource3,
  ]) {
    let expectedResourceFolder = PathUtils.join(
      stagingPath,
      backupResourceClass.key
    );
    Assert.ok(
      await IOUtils.exists(expectedResourceFolder),
      `BackupResource staging folder exists for ${backupResourceClass.key}`
    );
    Assert.ok(
      backupResourceClass.prototype.backup.calledOnce,
      `Backup was called for ${backupResourceClass.key}`
    );
    Assert.ok(
      backupResourceClass.prototype.backup.calledWith(
        expectedResourceFolder,
        fakeProfilePath
      ),
      `Backup was passed the right paths for ${backupResourceClass.key}`
    );
  }

  // After createBackup is more fleshed out, we're going to want to make sure
  // that we're writing the manifest file and that it contains the expected
  // ManifestEntry objects, and that the staging folder was successfully
  // renamed with the current date.
  await IOUtils.remove(fakeProfilePath, { recursive: true });

  sandbox.restore();
});
