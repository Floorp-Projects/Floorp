/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test() {
  do_test_pending();

  let migrator = MigrationUtils.getMigrator("ie");

  // Sanity check for the source.
  do_check_true(migrator.sourceExists);

  // Ensure bookmarks migration is available.
  let availableSources = migrator.getMigrateData("FieldOfFlowers", false);
  do_check_true((availableSources & MigrationUtils.resourceTypes.BOOKMARKS) > 0);

  // Wait for the imported bookmarks.  Check that "From Internet Explorer"
  // folders are created in the menu and on the toolbar.
  let source = MigrationUtils.getLocalizedString("sourceNameIE");
  let label = MigrationUtils.getLocalizedString("importedBookmarksFolder", [source]);

  let expectedParents = [ PlacesUtils.bookmarksMenuFolderId,
                          PlacesUtils.toolbarFolderId ];

  PlacesUtils.bookmarks.addObserver({
    onItemAdded: function onItemAdded(aItemId, aParentId, aIndex, aItemType,
                                      aURI, aTitle) {
      if (aTitle == label) {
        let index = expectedParents.indexOf(aParentId);
        do_check_neq(index, -1);
        expectedParents.splice(index, 1);
        if (expectedParents.length == 0)
          PlacesUtils.bookmarks.removeObserver(this);
      }
    },
    onBeginUpdateBatch: function () {},
    onEndUpdateBatch: function () {},
    onBeforeItemRemoved: function () {},
    onItemRemoved: function () {},
    onItemChanged: function () {},
    onItemVisited: function () {},
    onItemMoved: function () {},
  }, false);

  // Wait for migration.
  Services.obs.addObserver(function onMigrationEnded() {
    Services.obs.removeObserver(onMigrationEnded, "Migration:Ended");

    // Check the bookmarks have been imported to all the expected parents.
    do_check_eq(expectedParents.length, 0);

    do_test_finished();
  }, "Migration:Ended", false);

  migrator.migrate(MigrationUtils.resourceTypes.BOOKMARKS, null,
                   "FieldOfFlowers");
}
