"use strict";

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

  let migrator = await MigrationUtils.getMigrator("chrome");
  Assert.ok(
    !migrator,
    "Migrator should not be available since there are no passwords"
  );
});
