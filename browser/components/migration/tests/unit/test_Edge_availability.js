const EDGE_AVAILABLE_MIGRATIONS = 
  MigrationUtils.resourceTypes.COOKIES |
  MigrationUtils.resourceTypes.BOOKMARKS;

add_task(function* () {
  let migrator = MigrationUtils.getMigrator("edge");
  Cu.import("resource://gre/modules/AppConstants.jsm");
  Assert.equal(!!(migrator && migrator.sourceExists), AppConstants.isPlatformAndVersionAtLeast("win", "10"),
               "Edge should be available for migration if and only if we're on Win 10+");
  if (migrator) {
    let migratableData = migrator.getMigrateData(null, false);
    Assert.equal(migratableData, EDGE_AVAILABLE_MIGRATIONS,
                 "All the data types we expect should be available");
  }
});

