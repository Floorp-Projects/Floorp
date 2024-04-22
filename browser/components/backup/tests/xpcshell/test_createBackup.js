/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs"
);
const { JsonSchemaValidator } = ChromeUtils.importESModule(
  "resource://gre/modules/components-utils/JsonSchemaValidator.sys.mjs"
);

add_setup(function () {
  // Much of this setup is copied from toolkit/profile/xpcshell/head.js. It is
  // needed in order to put the xpcshell test environment into the state where
  // it thinks its profile is the one pointed at by
  // nsIToolkitProfileService.currentProfile.
  let gProfD = do_get_profile();
  let gDataHome = gProfD.clone();
  gDataHome.append("data");
  gDataHome.createUnique(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
  let gDataHomeLocal = gProfD.clone();
  gDataHomeLocal.append("local");
  gDataHomeLocal.createUnique(Ci.nsIFile.DIRECTORY_TYPE, 0o755);

  let xreDirProvider = Cc["@mozilla.org/xre/directory-provider;1"].getService(
    Ci.nsIXREDirProvider
  );
  xreDirProvider.setUserDataDirectory(gDataHome, false);
  xreDirProvider.setUserDataDirectory(gDataHomeLocal, true);

  let profileSvc = Cc["@mozilla.org/toolkit/profile-service;1"].getService(
    Ci.nsIToolkitProfileService
  );

  let createdProfile = {};
  let didCreate = profileSvc.selectStartupProfile(
    ["xpcshell"],
    false,
    AppConstants.UPDATE_CHANNEL,
    "",
    {},
    {},
    createdProfile
  );
  Assert.ok(didCreate, "Created a testing profile and set it to current.");
  Assert.equal(
    profileSvc.currentProfile,
    createdProfile.value,
    "Profile set to current"
  );
});

/**
 * Tests that calling BackupService.createBackup will call backup on each
 * registered BackupResource, and that each BackupResource will have a folder
 * created for them to write into.
 */
add_task(async function test_createBackup() {
  let sandbox = sinon.createSandbox();
  let fake1ManifestEntry = { fake1: "hello from 1" };
  sandbox
    .stub(FakeBackupResource1.prototype, "backup")
    .resolves(fake1ManifestEntry);

  sandbox
    .stub(FakeBackupResource2.prototype, "backup")
    .rejects(new Error("Some failure to backup"));

  let fake3ManifestEntry = { fake3: "hello from 3" };
  sandbox
    .stub(FakeBackupResource3.prototype, "backup")
    .resolves(fake3ManifestEntry);

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

  // We expect the staging folder to exist then be renamed under the fakeProfilePath.
  // We should also find a folder for each fake BackupResource.
  let backupsFolderPath = PathUtils.join(fakeProfilePath, "backups");
  let stagingPath = PathUtils.join(backupsFolderPath, "staging");

  // For now, we expect a single backup only to be saved.
  let backups = await IOUtils.getChildren(backupsFolderPath);
  Assert.equal(
    backups.length,
    1,
    "There should only be 1 backup in the backups folder"
  );

  let renamedFilename = await PathUtils.filename(backups[0]);
  let expectedFormatRegex = /^\d{4}(-\d{2}){2}T(\d{2}-){2}\d{2}Z$/;
  Assert.ok(
    renamedFilename.match(expectedFormatRegex),
    "Renamed staging folder should have format YYYY-MM-DDTHH-mm-ssZ"
  );

  let stagingPathRenamed = PathUtils.join(backupsFolderPath, renamedFilename);

  for (let backupResourceClass of [
    FakeBackupResource1,
    FakeBackupResource2,
    FakeBackupResource3,
  ]) {
    let expectedResourceFolderBeforeRename = PathUtils.join(
      stagingPath,
      backupResourceClass.key
    );
    let expectedResourceFolderAfterRename = PathUtils.join(
      stagingPathRenamed,
      backupResourceClass.key
    );

    Assert.ok(
      await IOUtils.exists(expectedResourceFolderAfterRename),
      `BackupResource folder exists for ${backupResourceClass.key} after rename`
    );
    Assert.ok(
      backupResourceClass.prototype.backup.calledOnce,
      `Backup was called for ${backupResourceClass.key}`
    );
    Assert.ok(
      backupResourceClass.prototype.backup.calledWith(
        expectedResourceFolderBeforeRename,
        fakeProfilePath
      ),
      `Backup was called in the staging folder for ${backupResourceClass.key} before rename`
    );
  }

  let manifestPath = PathUtils.join(stagingPathRenamed, "backup-manifest.json");
  Assert.ok(await IOUtils.exists(manifestPath), "Manifest file exists");
  let manifest = await IOUtils.readJSON(manifestPath);

  let schema = await BackupService.MANIFEST_SCHEMA;
  let validationResult = JsonSchemaValidator.validate(manifest, schema);
  Assert.ok(validationResult.valid, "Schema matches manifest");
  Assert.deepEqual(
    Object.keys(manifest.resources).sort(),
    ["fake1", "fake3"],
    "Manifest contains all expected BackupResource keys"
  );
  Assert.deepEqual(
    manifest.resources.fake1,
    fake1ManifestEntry,
    "Manifest contains the expected entry for FakeBackupResource1"
  );
  Assert.deepEqual(
    manifest.resources.fake3,
    fake3ManifestEntry,
    "Manifest contains the expected entry for FakeBackupResource3"
  );

  // After createBackup is more fleshed out, we're going to want to make sure
  // that we're writing the manifest file and that it contains the expected
  // ManifestEntry objects, and that the staging folder was successfully
  // renamed with the current date.
  await IOUtils.remove(fakeProfilePath, { recursive: true });

  sandbox.restore();
});
