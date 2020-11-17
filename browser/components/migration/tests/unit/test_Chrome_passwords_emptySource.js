"use strict";

/**
 * Ensure that there is no dialog for OS crypto that blocks a migration when
 * importing from an empty Login Data DB.
 */

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

const PROFILE = {
  id: "Default",
  name: "Person With No Data",
};

add_task(async function setup() {
  let dirSvcPath;
  let pathId;

  // Use a mock service and account name to avoid a Keychain auth. prompt that
  // would block the test from finishing if Chrome has already created a matching
  // Keychain entry. This allows us to still exercise the keychain lookup code.
  // The mock encryption passphrase is used when the Keychain item isn't found.
  let mockMacOSKeychain = {
    passphrase: "bW96aWxsYWZpcmVmb3g=",
    serviceName: "TESTING Chrome Safe Storage",
    accountName: "TESTING Chrome",
  };
  if (AppConstants.platform == "macosx") {
    dirSvcPath = "LibraryWithNoData/";
    pathId = "ULibDir";
  } else if (AppConstants.platform == "win") {
    dirSvcPath = "AppData/LocalWithNoData/";
    pathId = "LocalAppData";
  } else {
    throw new Error("Not implemented");
  }
  let dirSvcFile = do_get_file(dirSvcPath);
  registerFakePath(pathId, dirSvcFile);

  if (AppConstants.platform == "macosx") {
    let migrator = await MigrationUtils.getMigrator("chrome");
    Object.assign(migrator.wrappedJSObject, {
      _keychainServiceName: mockMacOSKeychain.serviceName,
      _keychainAccountName: mockMacOSKeychain.accountName,
      // No `_keychainMockPassphrase` as we don't want to mock the OS dialog as
      // it shouldn't appear.
    });
  }

  registerCleanupFunction(() => {
    Services.logins.removeAllUserFacingLogins();
  });
});

add_task(async function test_importEmptyDBWithoutAuthPrompts() {
  let migrator = await MigrationUtils.getMigrator("chrome");
  Assert.ok(
    await migrator.isSourceAvailable(),
    "Sanity check the source exists"
  );

  let logins = Services.logins.getAllLogins();
  Assert.equal(logins.length, 0, "There are no logins initially");

  // Migrate the logins. If an OS dialog (e.g. Keychain) blocks this the test
  // would timeout.
  await promiseMigration(
    migrator,
    MigrationUtils.resourceTypes.PASSWORDS,
    PROFILE,
    true
  );

  logins = Services.logins.getAllLogins();
  Assert.equal(logins.length, 0, "Check login count after importing the data");
  Assert.equal(
    logins.length,
    MigrationUtils._importQuantities.logins,
    "Check telemetry matches the actual import."
  );
});
