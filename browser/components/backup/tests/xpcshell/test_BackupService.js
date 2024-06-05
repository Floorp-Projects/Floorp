/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs"
);
const { JsonSchemaValidator } = ChromeUtils.importESModule(
  "resource://gre/modules/components-utils/JsonSchemaValidator.sys.mjs"
);
const { UIState } = ChromeUtils.importESModule(
  "resource://services-sync/UIState.sys.mjs"
);
const { ClientID } = ChromeUtils.importESModule(
  "resource://gre/modules/ClientID.sys.mjs"
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
 * A utility function for testing BackupService.createBackup. This helper
 * function:
 *
 * 1. Ensures that `backup` will be called on BackupResources with the service
 * 2. Ensures that a backup-manifest.json will be written and contain the
 *    ManifestEntry data returned by each BackupResource.
 * 3. Ensures that a `staging` folder will be written to and renamed properly
 *    once the backup creation is complete.
 *
 * Once this is done, a task function can be run. The task function is passed
 * the parsed backup-manifest.json object as its only argument.
 *
 * @param {object} sandbox
 *   The Sinon sandbox to be used stubs and mocks. The test using this helper
 *   is responsible for creating and resetting this sandbox.
 * @param {Function} taskFn
 *   A function that is run once all default checks are done on the manifest
 *   and staging folder. After this function returns, the staging folder will
 *   be cleaned up.
 * @returns {Promise<undefined>}
 */
async function testCreateBackupHelper(sandbox, taskFn) {
  const EXPECTED_CLIENT_ID = await ClientID.getClientID();

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
  let backupsFolderPath = PathUtils.join(
    fakeProfilePath,
    BackupService.PROFILE_FOLDER_NAME
  );
  let stagingPath = PathUtils.join(backupsFolderPath, "staging");

  // For now, we expect a single backup only to be saved. There should also be
  // a single compressed file for the staging folder.
  let backupsChildren = await IOUtils.getChildren(backupsFolderPath);
  Assert.equal(
    backupsChildren.length,
    2,
    "There should only be 2 items in the backups folder"
  );

  // The folder and the compressed file should have the same filename, but
  // the compressed file should have a `.zip` file extension. We sort the
  // list of directory children to make sure that the folder is first in
  // the array.
  backupsChildren.sort();

  let renamedFilename = await PathUtils.filename(backupsChildren[0]);
  let expectedFormatRegex = /^\d{4}(-\d{2}){2}T(\d{2}-){2}\d{2}Z$/;
  Assert.ok(
    renamedFilename.match(expectedFormatRegex),
    "Renamed staging folder should have format YYYY-MM-DDTHH-mm-ssZ"
  );

  // We also expect a zipped version of that same folder to exist in the
  // directory, with the same name along with a .zip extension.
  let archiveFilename = await PathUtils.filename(backupsChildren[1]);
  Assert.equal(
    archiveFilename,
    `${renamedFilename}.zip`,
    "Compressed staging folder exists."
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

  // Check that resources were called from highest to lowest backup priority.
  sinon.assert.callOrder(
    FakeBackupResource3.prototype.backup,
    FakeBackupResource2.prototype.backup,
    FakeBackupResource1.prototype.backup
  );

  let manifestPath = PathUtils.join(
    stagingPathRenamed,
    BackupService.MANIFEST_FILE_NAME
  );

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
  Assert.equal(
    manifest.meta.legacyClientID,
    EXPECTED_CLIENT_ID,
    "The client ID was stored properly."
  );

  taskFn(manifest);

  // After createBackup is more fleshed out, we're going to want to make sure
  // that we're writing the manifest file and that it contains the expected
  // ManifestEntry objects, and that the staging folder was successfully
  // renamed with the current date.
  await IOUtils.remove(fakeProfilePath, { recursive: true });
}

/**
 * Tests that calling BackupService.createBackup will call backup on each
 * registered BackupResource, and that each BackupResource will have a folder
 * created for them to write into. Tests in the signed-out state.
 */
add_task(async function test_createBackup_signed_out() {
  let sandbox = sinon.createSandbox();

  sandbox
    .stub(UIState, "get")
    .returns({ status: UIState.STATUS_NOT_CONFIGURED });
  await testCreateBackupHelper(sandbox, manifest => {
    Assert.equal(
      manifest.meta.accountID,
      undefined,
      "Account ID should be undefined."
    );
    Assert.equal(
      manifest.meta.accountEmail,
      undefined,
      "Account email should be undefined."
    );
  });

  sandbox.restore();
});

/**
 * Tests that calling BackupService.createBackup will call backup on each
 * registered BackupResource, and that each BackupResource will have a folder
 * created for them to write into. Tests in the signed-in state.
 */
add_task(async function test_createBackup_signed_in() {
  let sandbox = sinon.createSandbox();

  const TEST_UID = "ThisIsMyTestUID";
  const TEST_EMAIL = "foxy@mozilla.org";

  sandbox.stub(UIState, "get").returns({
    status: UIState.STATUS_SIGNED_IN,
    uid: TEST_UID,
    email: TEST_EMAIL,
  });

  await testCreateBackupHelper(sandbox, manifest => {
    Assert.equal(
      manifest.meta.accountID,
      TEST_UID,
      "Account ID should be set properly."
    );
    Assert.equal(
      manifest.meta.accountEmail,
      TEST_EMAIL,
      "Account email should be set properly."
    );
  });

  sandbox.restore();
});

/**
 * Creates a directory that looks a lot like a decompressed backup archive,
 * and then tests that BackupService.recoverFromBackup can create a new profile
 * and recover into it.
 */
add_task(async function test_recoverFromBackup() {
  let sandbox = sinon.createSandbox();
  let fakeEntryMap = new Map();
  let backupResourceClasses = [
    FakeBackupResource1,
    FakeBackupResource2,
    FakeBackupResource3,
  ];

  let i = 1;
  for (let backupResourceClass of backupResourceClasses) {
    let fakeManifestEntry = { [`fake${i}`]: `hello from backup - ${i}` };
    sandbox
      .stub(backupResourceClass.prototype, "backup")
      .resolves(fakeManifestEntry);

    let fakePostRecoveryEntry = { [`fake${i}`]: `hello from recover - ${i}` };
    sandbox
      .stub(backupResourceClass.prototype, "recover")
      .resolves(fakePostRecoveryEntry);

    fakeEntryMap.set(backupResourceClass, {
      manifestEntry: fakeManifestEntry,
      postRecoveryEntry: fakePostRecoveryEntry,
    });

    ++i;
  }

  let bs = new BackupService({
    FakeBackupResource1,
    FakeBackupResource2,
    FakeBackupResource3,
  });

  let oldProfilePath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "recoverFromBackupTest"
  );
  let newProfileRootPath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "recoverFromBackupTest-newProfileRoot"
  );

  let { stagingPath } = await bs.createBackup({ profilePath: oldProfilePath });

  let testTelemetryStateObject = {
    clientID: "ed209123-04a1-04a1-04a1-c0ffeec0ffee",
  };
  await IOUtils.writeJSON(
    PathUtils.join(PathUtils.profileDir, "datareporting", "state.json"),
    testTelemetryStateObject
  );

  let profile = await bs.recoverFromBackup(
    stagingPath,
    false /* shouldLaunch */,
    newProfileRootPath
  );
  Assert.ok(profile, "An nsIToolkitProfile was created.");
  let newProfilePath = profile.rootDir.path;

  let postRecoveryFilePath = PathUtils.join(
    newProfilePath,
    "post-recovery.json"
  );
  let postRecovery = await IOUtils.readJSON(postRecoveryFilePath);

  for (let backupResourceClass of backupResourceClasses) {
    let expectedResourceFolder = PathUtils.join(
      stagingPath,
      backupResourceClass.key
    );

    let { manifestEntry, postRecoveryEntry } =
      fakeEntryMap.get(backupResourceClass);

    Assert.ok(
      backupResourceClass.prototype.recover.calledOnce,
      `Recover was called for ${backupResourceClass.key}`
    );
    Assert.ok(
      backupResourceClass.prototype.recover.calledWith(
        manifestEntry,
        expectedResourceFolder,
        newProfilePath
      ),
      `Recover was passed the right arguments for ${backupResourceClass.key}`
    );
    Assert.deepEqual(
      postRecoveryEntry,
      postRecovery[backupResourceClass.key],
      "The post recovery data is as expected"
    );
  }

  let newProfileTelemetryStateObject = await IOUtils.readJSON(
    PathUtils.join(newProfileRootPath, "datareporting", "state.json")
  );
  Assert.deepEqual(
    testTelemetryStateObject,
    newProfileTelemetryStateObject,
    "Recovered profile inherited telemetry state from the profile that " +
      "initiated recovery"
  );

  await IOUtils.remove(oldProfilePath, { recursive: true });
  await IOUtils.remove(newProfileRootPath, { recursive: true });
  sandbox.restore();
});

/**
 * Tests that if there's a post-recovery.json file in the profile directory
 * when checkForPostRecovery() is called, that it is processed, and the
 * postRecovery methods on the associated BackupResources are called with the
 * entry values from the file.
 */
add_task(async function test_checkForPostRecovery() {
  let sandbox = sinon.createSandbox();

  let testProfilePath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "checkForPostRecoveryTest"
  );
  let fakePostRecoveryObject = {
    [FakeBackupResource1.key]: "test 1",
    [FakeBackupResource3.key]: "test 3",
  };
  await IOUtils.writeJSON(
    PathUtils.join(testProfilePath, BackupService.POST_RECOVERY_FILE_NAME),
    fakePostRecoveryObject
  );

  sandbox.stub(FakeBackupResource1.prototype, "postRecovery").resolves();
  sandbox.stub(FakeBackupResource2.prototype, "postRecovery").resolves();
  sandbox.stub(FakeBackupResource3.prototype, "postRecovery").resolves();

  let bs = new BackupService({
    FakeBackupResource1,
    FakeBackupResource2,
    FakeBackupResource3,
  });

  await bs.checkForPostRecovery(testProfilePath);
  await bs.postRecoveryComplete;

  Assert.ok(
    FakeBackupResource1.prototype.postRecovery.calledOnce,
    "FakeBackupResource1.postRecovery was called once"
  );
  Assert.ok(
    FakeBackupResource2.prototype.postRecovery.notCalled,
    "FakeBackupResource2.postRecovery was not called"
  );
  Assert.ok(
    FakeBackupResource3.prototype.postRecovery.calledOnce,
    "FakeBackupResource3.postRecovery was called once"
  );
  Assert.ok(
    FakeBackupResource1.prototype.postRecovery.calledWith(
      fakePostRecoveryObject[FakeBackupResource1.key]
    ),
    "FakeBackupResource1.postRecovery was called with the expected argument"
  );
  Assert.ok(
    FakeBackupResource3.prototype.postRecovery.calledWith(
      fakePostRecoveryObject[FakeBackupResource3.key]
    ),
    "FakeBackupResource3.postRecovery was called with the expected argument"
  );

  await IOUtils.remove(testProfilePath, { recursive: true });
  sandbox.restore();
});
