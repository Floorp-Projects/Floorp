"use strict";

add_task(function* () {
  registerFakePath("ULibDir", do_get_file("Library/"));

  let migrator = MigrationUtils.getMigrator("safari");
  // Sanity check for the source.
  Assert.ok(migrator.sourceExists);

  // Wait for the imported bookmarks.  Check that "From Safari"
  // folders are created on the toolbar.
  let source = MigrationUtils.getLocalizedString("sourceNameSafari");
  let label = MigrationUtils.getLocalizedString("importedBookmarksFolder", [source]);

  let expectedParents = [ PlacesUtils.toolbarFolderId ];
  let itemCount = 0;

  let gotFolder = false;
  let bmObserver = {
    onItemAdded(aItemId, aParentId, aIndex, aItemType, aURI, aTitle) {
      if (aTitle != label) {
        itemCount++;
      }
      if (aItemType == PlacesUtils.bookmarks.TYPE_FOLDER && aTitle == "Stuff") {
        gotFolder = true;
      }
      if (expectedParents.length > 0 && aTitle == label) {
        let index = expectedParents.indexOf(aParentId);
        Assert.ok(index != -1, "Found expected parent");
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
  PlacesUtils.bookmarks.addObserver(bmObserver);

  yield promiseMigration(migrator, MigrationUtils.resourceTypes.BOOKMARKS);
  PlacesUtils.bookmarks.removeObserver(bmObserver);

  // Check the bookmarks have been imported to all the expected parents.
  Assert.ok(!expectedParents.length, "No more expected parents");
  Assert.ok(gotFolder, "Should have seen the folder get imported");
  Assert.equal(itemCount, 13, "Should import all 13 items.");
  // Check that the telemetry matches:
  Assert.equal(MigrationUtils._importQuantities.bookmarks, itemCount, "Telemetry reporting correct.");
});
