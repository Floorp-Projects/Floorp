/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs"
);
const { OSKeyStoreTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/OSKeyStoreTestUtils.sys.mjs"
);

const TEST_PASSWORD = "This is some test password.";

add_setup(async () => {
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

  // Ensure that enabling encryption doesn't cause the native OSKeyStore to
  // require authentication.
  OSKeyStoreTestUtils.setup();
  registerCleanupFunction(async () => {
    await OSKeyStoreTestUtils.cleanup();
  });
});

/**
 * Tests that if encryption is disabled that only BackupResource's with
 * requiresEncryption set to `false` will have `backup()` called on them.
 */
add_task(async function test_disabled_encryption() {
  let sandbox = sinon.createSandbox();

  let bs = new BackupService({
    FakeBackupResource1,
    FakeBackupResource2,
    FakeBackupResource3,
  });

  Assert.ok(
    !bs.state.encryptionEnabled,
    "State should indicate that encryption is disabled."
  );

  let testProfilePath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "testDisabledEncryption"
  );
  let encState = await bs.loadEncryptionState(testProfilePath);
  Assert.ok(!encState, "Should not find an ArchiveEncryptionState.");

  // Override FakeBackupResource2 so that it requires encryption.
  sandbox.stub(FakeBackupResource2, "requiresEncryption").get(() => {
    return true;
  });
  Assert.ok(
    FakeBackupResource2.requiresEncryption,
    "FakeBackupResource2 requires encryption."
  );

  // This is how these FakeBackupResource's are defined in head.js
  Assert.ok(
    !FakeBackupResource1.requiresEncryption,
    "FakeBackupResource1 does not require encryption."
  );
  Assert.ok(
    !FakeBackupResource3.requiresEncryption,
    "FakeBackupResource3 does not require encryption."
  );

  let resourceWithoutEncryptionStubs = [
    sandbox.stub(FakeBackupResource1.prototype, "backup").resolves(null),
    sandbox.stub(FakeBackupResource3.prototype, "backup").resolves(null),
  ];

  let resourceWithEncryptionStub = sandbox
    .stub(FakeBackupResource2.prototype, "backup")
    .resolves(null);

  await bs.createBackup({ profilePath: testProfilePath });
  Assert.ok(
    resourceWithEncryptionStub.notCalled,
    "FakeBackupResource2.backup should not have been called"
  );

  for (let resourceWithoutEncryptionStub of resourceWithoutEncryptionStubs) {
    Assert.ok(
      resourceWithoutEncryptionStub.calledOnce,
      "backup called on resource that didn't require encryption"
    );
  }

  await IOUtils.remove(testProfilePath, { recursive: true });
  sandbox.restore();
});

/**
 * Tests that encryption cannot be disabled if it's already disabled.
 */
add_task(async function test_already_disabled_encryption() {
  let bs = new BackupService();

  Assert.ok(
    !bs.state.encryptionEnabled,
    "State should indicate that encryption is disabled."
  );

  let encState = await bs.loadEncryptionState();
  Assert.ok(!encState, "Should not find an ArchiveEncryptionState.");

  await Assert.rejects(
    bs.disableEncryption(),
    /already disabled/,
    "It should not be possible to disable encryption if it's already disabled"
  );
});

/**
 * Tests that if encryption is enabled from a non-enabled state, that an
 * ArchiveEncryptionState is created, and state is written to the profile
 * directory. Also tests that this allows BackupResource's with
 * requiresEncryption set to `true` to have `backup()` called on them.
 */
add_task(async function test_enable_encryption() {
  let sandbox = sinon.createSandbox();

  let bs = new BackupService({
    FakeBackupResource1,
    FakeBackupResource2,
    FakeBackupResource3,
  });

  Assert.ok(
    !bs.state.encryptionEnabled,
    "State should initially indicate that encryption is disabled."
  );

  let testProfilePath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "testEnableEncryption"
  );
  let encState = await bs.loadEncryptionState(testProfilePath);
  Assert.ok(!encState, "Should not find an ArchiveEncryptionState.");

  // Now enable encryption.
  let stateUpdatePromise = new Promise(resolve => {
    bs.addEventListener("BackupService:StateUpdate", resolve, { once: true });
  });
  await bs.enableEncryption(TEST_PASSWORD, testProfilePath);
  await stateUpdatePromise;
  Assert.ok(
    bs.state.encryptionEnabled,
    "State should indicate that encryption is enabled."
  );

  Assert.ok(
    await IOUtils.exists(
      PathUtils.join(
        testProfilePath,
        BackupService.PROFILE_FOLDER_NAME,
        BackupService.ARCHIVE_ENCRYPTION_STATE_FILE
      )
    ),
    "Encryption state file should exist."
  );

  // Override FakeBackupResource2 so that it requires encryption.
  sandbox.stub(FakeBackupResource2, "requiresEncryption").get(() => {
    return true;
  });
  Assert.ok(
    FakeBackupResource2.requiresEncryption,
    "FakeBackupResource2 requires encryption."
  );

  // This is how these FakeBackupResource's are defined in head.js
  Assert.ok(
    !FakeBackupResource1.requiresEncryption,
    "FakeBackupResource1 does not require encryption."
  );
  Assert.ok(
    !FakeBackupResource3.requiresEncryption,
    "FakeBackupResource3 does not require encryption."
  );

  let allResourceBackupStubs = [
    sandbox.stub(FakeBackupResource1.prototype, "backup").resolves(null),
    sandbox.stub(FakeBackupResource3.prototype, "backup").resolves(null),
    sandbox.stub(FakeBackupResource2.prototype, "backup").resolves(null),
  ];

  await bs.createBackup({
    profilePath: testProfilePath,
  });

  for (let resourceBackupStub of allResourceBackupStubs) {
    Assert.ok(resourceBackupStub.calledOnce, "backup called on resource");
  }

  Assert.ok(
    await IOUtils.exists(
      PathUtils.join(
        testProfilePath,
        BackupService.PROFILE_FOLDER_NAME,
        BackupService.ARCHIVE_ENCRYPTION_STATE_FILE
      )
    ),
    "Encryption state file should still exist."
  );

  await IOUtils.remove(testProfilePath, { recursive: true });
  sandbox.restore();
});

/**
 * Tests that encryption cannot be enabled if it's already enabled.
 */
add_task(async function test_already_enabled_encryption() {
  let bs = new BackupService();

  let testProfilePath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "testAlreadyEnabledEncryption"
  );

  // Enable encryption.
  await bs.enableEncryption(TEST_PASSWORD, testProfilePath);

  let encState = await bs.loadEncryptionState(testProfilePath);
  Assert.ok(encState, "ArchiveEncryptionState is available.");

  await Assert.rejects(
    bs.enableEncryption(TEST_PASSWORD, testProfilePath),
    /already enabled/,
    "It should not be possible to enable encryption if it's already enabled"
  );

  await IOUtils.remove(testProfilePath, { recursive: true });
});

/**
 * Tests that if encryption is enabled that it can be disabled.
 */
add_task(async function test_disabling_encryption() {
  let bs = new BackupService();

  let testProfilePath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "testDisableEncryption"
  );

  // Enable encryption.
  await bs.enableEncryption(TEST_PASSWORD, testProfilePath);

  let encState = await bs.loadEncryptionState(testProfilePath);
  Assert.ok(encState, "ArchiveEncryptionState is available.");
  Assert.ok(
    bs.state.encryptionEnabled,
    "State should indicate that encryption is enabled."
  );

  Assert.ok(
    await IOUtils.exists(
      PathUtils.join(
        testProfilePath,
        BackupService.PROFILE_FOLDER_NAME,
        BackupService.ARCHIVE_ENCRYPTION_STATE_FILE
      )
    ),
    "Encryption state file should exist."
  );

  // Now disable encryption.
  let stateUpdatePromise = new Promise(resolve => {
    bs.addEventListener("BackupService:StateUpdate", resolve, { once: true });
  });
  await bs.disableEncryption(testProfilePath);
  await stateUpdatePromise;
  Assert.ok(
    !bs.state.encryptionEnabled,
    "State should indicate that encryption is now disabled."
  );

  Assert.ok(
    !(await IOUtils.exists(
      PathUtils.join(
        testProfilePath,
        BackupService.PROFILE_FOLDER_NAME,
        BackupService.ARCHIVE_ENCRYPTION_STATE_FILE
      )
    )),
    "Encryption state file should have been removed."
  );

  await IOUtils.remove(testProfilePath, { recursive: true });
});
