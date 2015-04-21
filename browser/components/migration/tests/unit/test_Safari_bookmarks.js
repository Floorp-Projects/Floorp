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

  PlacesUtils.bookmarks.addObserver({
    onItemAdded(aItemId, aParentId, aIndex, aItemType, aURI, aTitle) {
      if (aTitle == label) {
        let index = expectedParents.indexOf(aParentId);
        Assert.notEqual(index, -1);
        expectedParents.splice(index, 1);
        if (expectedParents.length == 0)
          PlacesUtils.bookmarks.removeObserver(this);
      }
    },
    onBeginUpdateBatch() {},
    onEndUpdateBatch() {},
    onItemRemoved() {},
    onItemChanged() {},
    onItemVisited() {},
    onItemMoved() {},
  }, false);

  yield promiseMigration(migrator, MigrationUtils.resourceTypes.BOOKMARKS);

  // Check the bookmarks have been imported to all the expected parents.
  Assert.equal(expectedParents.length, 0);
});
