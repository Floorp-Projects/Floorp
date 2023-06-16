"use strict";

add_task(async function () {
  let migrator = await MigrationUtils.getMigrator("ie");
  // Sanity check for the source.
  Assert.ok(await migrator.isSourceAvailable(), "Check migrator source");

  // Since this test doesn't mock out the favorites, execution is dependent
  // on the actual favorites stored on the local machine's IE favorites database.
  // As such, we can't assert that bookmarks were migrated to both the bookmarks
  // menu and the bookmarks toolbar.
  let itemCount = 0;
  let listener = events => {
    for (let event of events) {
      if (event.itemType == PlacesUtils.bookmarks.TYPE_BOOKMARK) {
        info("bookmark added: " + event.parentGuid);
        itemCount++;
      }
    }
  };
  PlacesUtils.observers.addListener(["bookmark-added"], listener);

  await promiseMigration(migrator, MigrationUtils.resourceTypes.BOOKMARKS);
  PlacesUtils.observers.removeListener(["bookmark-added"], listener);
  Assert.equal(
    MigrationUtils._importQuantities.bookmarks,
    itemCount,
    "Ensure telemetry matches actual number of imported items."
  );
});
