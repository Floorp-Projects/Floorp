/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that if the migrator does not have the permission to
 * read from the Chrome data directory, that it can request
 * permission to read from it, if the system allows.
 */

const { ChromeMigrationUtils } = ChromeUtils.importESModule(
  "resource:///modules/ChromeMigrationUtils.sys.mjs"
);

const { ChromeProfileMigrator } = ChromeUtils.importESModule(
  "resource:///modules/ChromeProfileMigrator.sys.mjs"
);

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

const { MockFilePicker } = ChromeUtils.importESModule(
  "resource://testing-common/MockFilePicker.sys.mjs"
);

let gTempDir;

add_setup(async () => {
  Services.prefs.setBoolPref(
    "browser.migrate.chrome.get_permissions.enabled",
    true
  );
  gTempDir = do_get_tempdir();
  await IOUtils.writeJSON(PathUtils.join(gTempDir.path, "Local State"), []);

  MockFilePicker.init(globalThis);
  registerCleanupFunction(() => {
    MockFilePicker.cleanup();
  });
});

/**
 * Tests that canGetPermissions will return false if the platform does
 * not allow for folder selection in the native file picker, and returns
 * the data path otherwise.
 */
add_task(async function test_canGetPermissions() {
  let sandbox = sinon.createSandbox();
  registerCleanupFunction(() => {
    sandbox.restore();
  });

  let migrator = new ChromeProfileMigrator();
  let canGetPermissionsStub = sandbox
    .stub(MigrationUtils, "canGetPermissionsOnPlatform")
    .resolves(false);
  sandbox.stub(ChromeMigrationUtils, "getDataPath").resolves(gTempDir.path);

  Assert.ok(
    !(await migrator.canGetPermissions()),
    "Should not be able to get permissions."
  );

  canGetPermissionsStub.resolves(true);

  Assert.equal(
    await migrator.canGetPermissions(),
    gTempDir.path,
    "Should be able to get the permissions path."
  );

  sandbox.restore();
});

/**
 * Tests that getPermissions will show the native file picker in a
 * loop until either the user cancels or selects a folder that grants
 * read permissions to the data directory.
 */
add_task(async function test_getPermissions() {
  let sandbox = sinon.createSandbox();
  registerCleanupFunction(() => {
    sandbox.restore();
  });

  let migrator = new ChromeProfileMigrator();
  sandbox.stub(MigrationUtils, "canGetPermissionsOnPlatform").resolves(true);
  sandbox.stub(ChromeMigrationUtils, "getDataPath").resolves(gTempDir.path);
  let hasPermissionsStub = sandbox
    .stub(migrator, "hasPermissions")
    .resolves(false);

  Assert.equal(
    await migrator.canGetPermissions(),
    gTempDir.path,
    "Should be able to get the permissions path."
  );

  let filePickerSeenCount = 0;

  let filePickerShownPromise = new Promise(resolve => {
    MockFilePicker.showCallback = () => {
      Assert.ok(true, "Filepicker shown.");
      MockFilePicker.useDirectory([gTempDir.path]);
      filePickerSeenCount++;
      if (filePickerSeenCount > 3) {
        Assert.ok(true, "File picker looped 3 times.");
        hasPermissionsStub.resolves(true);
        resolve();
      }
    };
  });
  MockFilePicker.returnValue = MockFilePicker.returnOK;
  Assert.ok(
    await migrator.getPermissions(),
    "Should report that we got permissions."
  );

  await filePickerShownPromise;

  // Make sure that the user can also hit "cancel" and that we
  // file picker loop.

  hasPermissionsStub.resolves(false);

  filePickerSeenCount = 0;
  filePickerShownPromise = new Promise(resolve => {
    MockFilePicker.showCallback = () => {
      Assert.ok(true, "Filepicker shown.");
      filePickerSeenCount++;
      Assert.equal(filePickerSeenCount, 1, "Saw the picker once.");
      resolve();
    };
  });
  MockFilePicker.returnValue = MockFilePicker.returnCancel;
  Assert.ok(
    !(await migrator.getPermissions()),
    "Should report that we didn't get permissions."
  );
  await filePickerShownPromise;

  sandbox.restore();
});

/**
 * Tests that if the native file picker chooses a different directory
 * than the one we originally asked for, that we remap attempts to
 * read profiles from that new directory. This is because Ubuntu Snaps
 * will return us paths from the native file picker that are symlinks
 * to the original directories.
 */
add_task(async function test_remapDirectories() {
  let remapDir = new FileUtils.File(
    await IOUtils.createUniqueDirectory(
      PathUtils.tempDir,
      "test-chrome-migration"
    )
  );
  let localStatePath = PathUtils.join(remapDir.path, "Local State");
  await IOUtils.writeJSON(localStatePath, []);

  let sandbox = sinon.createSandbox();
  registerCleanupFunction(() => {
    sandbox.restore();
  });

  let migrator = new ChromeProfileMigrator();
  sandbox.stub(MigrationUtils, "canGetPermissionsOnPlatform").resolves(true);
  sandbox.stub(ChromeMigrationUtils, "getDataPath").resolves(gTempDir.path);
  let hasPermissionsStub = sandbox
    .stub(migrator, "hasPermissions")
    .resolves(false);

  Assert.equal(
    await migrator.canGetPermissions(),
    gTempDir.path,
    "Should be able to get the permissions path."
  );

  let filePickerShownPromise = new Promise(resolve => {
    MockFilePicker.showCallback = () => {
      Assert.ok(true, "Filepicker shown.");
      MockFilePicker.useDirectory([remapDir.path]);
      hasPermissionsStub.resolves(true);
      resolve();
    };
  });
  MockFilePicker.returnValue = MockFilePicker.returnOK;
  Assert.ok(
    await migrator.getPermissions(),
    "Should report that we got permissions."
  );

  Assert.equal(
    PathUtils.normalize(await migrator.canGetPermissions()),
    PathUtils.normalize(remapDir.path),
    "Should be able to get the remapped permissions path."
  );

  await filePickerShownPromise;

  sandbox.restore();
});
