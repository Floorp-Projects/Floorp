/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { BookmarksFileMigrator } = ChromeUtils.importESModule(
  "resource:///modules/FileMigrators.sys.mjs"
);

const { MigrationWizardConstants } = ChromeUtils.importESModule(
  "chrome://browser/content/migration/migration-wizard-constants.mjs"
);

/**
 * Tests that the BookmarksFileMigrator properly subclasses FileMigratorBase
 * and delegates to BookmarkHTMLUtils or BookmarkJSONUtils.
 *
 * Normally, we'd override the BookmarkHTMLUtils and BookmarkJSONUtils methods
 * in our test here so that we just ensure that they're called with the
 * right arguments, rather than testing their behaviour. Unfortunately, both
 * BookmarkHTMLUtils and BookmarkJSONUtils are frozen with Object.freeze, which
 * prevents sinon from stubbing out any of their methods. Rather than unfreezing
 * those objects just for testing, we test the whole flow end-to-end, including
 * the import to Places.
 */

add_setup(() => {
  Services.prefs.setBoolPref("browser.migrate.bookmarks-file.enabled", true);
  registerCleanupFunction(async () => {
    await PlacesUtils.bookmarks.eraseEverything();
    Services.prefs.clearUserPref("browser.migrate.bookmarks-file.enabled");
  });
});

/**
 * First check that the BookmarksFileMigrator implements the required parts
 * of the parent class.
 */
add_task(async function test_BookmarksFileMigrator_members() {
  let migrator = new BookmarksFileMigrator();
  Assert.ok(
    migrator.constructor.key,
    `BookmarksFileMigrator implements static getter 'key'`
  );

  Assert.ok(
    migrator.constructor.displayNameL10nID,
    `BookmarksFileMigrator implements static getter 'displayNameL10nID'`
  );

  Assert.ok(
    migrator.constructor.brandImage,
    `BookmarksFileMigrator implements static getter 'brandImage'`
  );

  Assert.ok(
    migrator.progressHeaderL10nID,
    `BookmarksFileMigrator implements getter 'progressHeaderL10nID'`
  );

  Assert.ok(
    migrator.successHeaderL10nID,
    `BookmarksFileMigrator implements getter 'successHeaderL10nID'`
  );

  Assert.ok(
    await migrator.getFilePickerConfig(),
    `BookmarksFileMigrator implements method 'getFilePickerConfig'`
  );

  Assert.ok(
    migrator.displayedResourceTypes,
    `BookmarksFileMigrator implements getter 'displayedResourceTypes'`
  );

  Assert.ok(migrator.enabled, `BookmarksFileMigrator is enabled`);
});

add_task(async function test_BookmarksFileMigrator_HTML() {
  let migrator = new BookmarksFileMigrator();
  const EXPECTED_SUCCESS_STATE = {
    [MigrationWizardConstants.DISPLAYED_FILE_RESOURCE_TYPES
      .BOOKMARKS_FROM_FILE]: "8 bookmarks",
  };

  const BOOKMARKS_PATH = PathUtils.join(
    do_get_cwd().path,
    "bookmarks.exported.html"
  );

  let result = await migrator.migrate(BOOKMARKS_PATH);

  Assert.deepEqual(
    result,
    EXPECTED_SUCCESS_STATE,
    "Returned the expected success state."
  );
});

add_task(async function test_BookmarksFileMigrator_JSON() {
  let migrator = new BookmarksFileMigrator();

  const EXPECTED_SUCCESS_STATE = {
    [MigrationWizardConstants.DISPLAYED_FILE_RESOURCE_TYPES
      .BOOKMARKS_FROM_FILE]: "10 bookmarks",
  };

  const BOOKMARKS_PATH = PathUtils.join(
    do_get_cwd().path,
    "bookmarks.exported.json"
  );

  let result = await migrator.migrate(BOOKMARKS_PATH);

  Assert.deepEqual(
    result,
    EXPECTED_SUCCESS_STATE,
    "Returned the expected success state."
  );
});

add_task(async function test_BookmarksFileMigrator_invalid() {
  let migrator = new BookmarksFileMigrator();

  const INVALID_FILE_PATH = PathUtils.join(
    do_get_cwd().path,
    "bookmarks.invalid.html"
  );

  await Assert.rejects(
    migrator.migrate(INVALID_FILE_PATH),
    /Pick another file/
  );
});
