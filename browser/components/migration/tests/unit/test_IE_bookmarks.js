/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test() {
  let migrator = newMigratorFor("ie");

  // Sanity check for the source.
  do_check_true(migrator.sourceExists);

  // Ensure bookmarks migration is available.
  let availableSources = migrator.getMigrateData("FieldOfFlowers", false);
  do_check_true((availableSources & IMIGRATOR.BOOKMARKS) > 0);

  // Needed to enforce bookmarks replacing.
  let startup = {
    doStartup: function () {},
    get directory() do_get_profile()
  }
  migrator.migrate(IMIGRATOR.BOOKMARKS, startup, "FieldOfFlowers");

  // Check that at least two bookmark have been added to the menu and the
  // toolbar.  The first one comes from bookmarks.html, the others from IE.
  do_check_true(PlacesUtils.bookmarks
                           .getIdForItemAt(PlacesUtils.bookmarksMenuFolderId, 1) > 0);
  do_check_true(PlacesUtils.bookmarks
                           .getIdForItemAt(PlacesUtils.toolbarFolderId, 1) > 0);
}
