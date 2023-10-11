/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that if the migrator does not have the permission to
 * read from the Safari data directory, that it can request
 * permission to read from it, if the system allows.
 */

const { SafariProfileMigrator } = ChromeUtils.importESModule(
  "resource:///modules/SafariProfileMigrator.sys.mjs"
);

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

const { MockFilePicker } = ChromeUtils.importESModule(
  "resource://testing-common/MockFilePicker.sys.mjs"
);

let gDataDir;

add_setup(async () => {
  let tempDir = do_get_tempdir();
  gDataDir = PathUtils.join(tempDir.path, "Safari");
  await IOUtils.makeDirectory(gDataDir);

  registerFakePath("ULibDir", tempDir);

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

  let migrator = new SafariProfileMigrator();
  // Not being able to get a folder picker is not a problem on macOS, but
  // we'll test that case anyways.
  let canGetPermissionsStub = sandbox
    .stub(MigrationUtils, "canGetPermissionsOnPlatform")
    .resolves(false);

  Assert.ok(
    !(await migrator.canGetPermissions()),
    "Should not be able to get permissions."
  );

  canGetPermissionsStub.resolves(true);

  Assert.equal(
    await migrator.canGetPermissions(),
    gDataDir,
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

  let migrator = new SafariProfileMigrator();
  sandbox.stub(MigrationUtils, "canGetPermissionsOnPlatform").resolves(true);
  let hasPermissionsStub = sandbox
    .stub(migrator, "hasPermissions")
    .resolves(false);

  Assert.equal(
    await migrator.canGetPermissions(),
    gDataDir,
    "Should be able to get the permissions path."
  );

  let filePickerSeenCount = 0;

  let filePickerShownPromise = new Promise(resolve => {
    MockFilePicker.showCallback = () => {
      Assert.ok(true, "Filepicker shown.");
      MockFilePicker.useDirectory([gDataDir]);
      filePickerSeenCount++;
      if (filePickerSeenCount > 3) {
        Assert.ok(true, "File picker looped 3 times.");
        hasPermissionsStub.resolves(true);
        resolve();
      }
    };
  });
  MockFilePicker.returnValue = MockFilePicker.returnOK;

  // This is a little awkward, but we need to ensure that the
  // filePickerShownPromise resolves first before we await
  // the getPermissionsPromise in order to get the correct
  // filePickerSeenCount.
  let getPermissionsPromise = migrator.getPermissions();
  await filePickerShownPromise;
  Assert.ok(
    await getPermissionsPromise,
    "Should report that we got permissions."
  );

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
