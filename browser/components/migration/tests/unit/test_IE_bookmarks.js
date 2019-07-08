"use strict";

add_task(async function() {
  let migrator = await MigrationUtils.getMigrator("ie");
  // Sanity check for the source.
  Assert.ok(await migrator.isSourceAvailable());

  // Wait for the imported bookmarks.  Check that "From Internet Explorer"
  // folders are created in the menu and on the toolbar.
  let source = MigrationUtils.getLocalizedString("sourceNameIE");
  let label = MigrationUtils.getLocalizedString("importedBookmarksFolder", [
    source,
  ]);

  let expectedParents = [
    PlacesUtils.bookmarksMenuFolderId,
    PlacesUtils.toolbarFolderId,
  ];

  let itemCount = 0;
  let listener = events => {
    for (let event of events) {
      if (event.title != label) {
        itemCount++;
      }
      if (expectedParents.length > 0 && event.title == label) {
        let index = expectedParents.indexOf(event.parentId);
        Assert.notEqual(index, -1);
        expectedParents.splice(index, 1);
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

  // Check the bookmarks have been imported to all the expected parents.
  Assert.equal(expectedParents.length, 0, "Got all the expected parents");
});
