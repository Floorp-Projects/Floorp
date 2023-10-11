"use strict";

const { ChromeProfileMigrator } = ChromeUtils.importESModule(
  "resource:///modules/ChromeProfileMigrator.sys.mjs"
);

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

add_task(async function test_importEmptyDBWithoutAuthPrompts() {
  let dirSvcPath;
  let pathId;

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

  let sandbox = sinon.createSandbox();
  sandbox
    .stub(ChromeProfileMigrator.prototype, "canGetPermissions")
    .resolves(true);
  sandbox
    .stub(ChromeProfileMigrator.prototype, "hasPermissions")
    .resolves(true);
  registerCleanupFunction(() => {
    sandbox.restore();
  });

  let migrator = await MigrationUtils.getMigrator("chrome");
  Assert.ok(
    !migrator,
    "Migrator should not be available since there are no passwords"
  );
});
