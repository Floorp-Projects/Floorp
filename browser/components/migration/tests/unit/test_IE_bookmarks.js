"use strict";

add_task(function* () {
  let migrator = MigrationUtils.getMigrator("ie");
  // Sanity check for the source.
  Assert.ok(migrator.sourceExists);

  // Wait for the imported bookmarks.  Check that "From Internet Explorer"
  // folders are created in the menu and on the toolbar.
  let source = MigrationUtils.getLocalizedString("sourceNameIE");
  let label = MigrationUtils.getLocalizedString("importedBookmarksFolder", [source]);

  let expectedParents = [ PlacesUtils.bookmarksMenuFolderId,
                          PlacesUtils.toolbarFolderId ];

  let itemCount = 0;
  let bmObserver = {
    onItemAdded(aItemId, aParentId, aIndex, aItemType, aURI, aTitle) {
      if (aTitle != label) {
        itemCount++;
      }
      if (expectedParents.length > 0 && aTitle == label) {
        let index = expectedParents.indexOf(aParentId);
        Assert.notEqual(index, -1);
        expectedParents.splice(index, 1);
      }
    },
    onBeginUpdateBatch() {},
    onEndUpdateBatch() {},
    onItemRemoved() {},
    onItemChanged() {},
    onItemVisited() {},
    onItemMoved() {},
  };
  PlacesUtils.bookmarks.addObserver(bmObserver, false);

  yield promiseMigration(migrator, MigrationUtils.resourceTypes.BOOKMARKS);
  PlacesUtils.bookmarks.removeObserver(bmObserver);
  Assert.equal(MigrationUtils._importQuantities.bookmarks, itemCount,
               "Ensure telemetry matches actual number of imported items.");

  // Check the bookmarks have been imported to all the expected parents.
  Assert.equal(expectedParents.length, 0, "Got all the expected parents");
});
