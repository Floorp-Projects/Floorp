/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const kPref = "browser.bookmarks.addedImportButton";

/**
 * Verify that we add the import button only if there aren't enough bookmarks
 * in the toolbar.
 */
add_task(async function test_bookmark_import_button() {
  let bookmarkCount = (
    await PlacesUtils.bookmarks.fetch(PlacesUtils.bookmarks.toolbarGuid)
  ).childCount;
  Assert.less(bookmarkCount, 3, "we should start with less than 3 bookmarks");

  ok(
    !document.getElementById("import-button"),
    "Shouldn't have button to start with."
  );
  await PlacesUIUtils.maybeAddImportButton();
  ok(document.getElementById("import-button"), "Button should be added.");
  is(Services.prefs.getBoolPref(kPref), true, "Pref should be set.");

  CustomizableUI.reset();

  // Add some bookmarks. This should stop the import button from being inserted.
  let parentGuid = PlacesUtils.bookmarks.toolbarGuid;
  let bookmarks = await Promise.all(
    ["firefox", "rules", "yo"].map(n =>
      PlacesUtils.bookmarks.insert({
        parentGuid,
        url: `https://example.com/${n}`,
        title: n.toString(),
      })
    )
  );

  // Ensure we remove items after this task, or worst-case after this test
  // file has completed.
  let removeAllBookmarks = () => {
    let removals = bookmarks.map(b => PlacesUtils.bookmarks.remove(b.guid));
    bookmarks = [];
    return Promise.all(removals);
  };
  registerCleanupFunction(removeAllBookmarks);

  await PlacesUIUtils.maybeAddImportButton();
  ok(
    !document.getElementById("import-button"),
    "Button should not be added if we have bookmarks."
  );

  // Just in case, for future tests we run:
  CustomizableUI.reset();

  await removeAllBookmarks();
});

/**
 * Verify the button gets removed when we import bookmarks successfully.
 */
add_task(async function test_bookmark_import_button_removal() {
  let bookmarkCount = (
    await PlacesUtils.bookmarks.fetch(PlacesUtils.bookmarks.toolbarGuid)
  ).childCount;
  Assert.less(bookmarkCount, 3, "we should start with less than 3 bookmarks");

  ok(
    !document.getElementById("import-button"),
    "Shouldn't have button to start with."
  );
  await PlacesUIUtils.maybeAddImportButton();
  ok(document.getElementById("import-button"), "Button should be added.");
  is(Services.prefs.getBoolPref(kPref), true, "Pref should be set.");

  PlacesUIUtils.removeImportButtonWhenImportSucceeds();

  Services.obs.notifyObservers(
    null,
    "Migration:ItemAfterMigrate",
    MigrationUtils.resourceTypes.BOOKMARKS
  );

  is(
    Services.prefs.getBoolPref(kPref, false),
    true,
    "Pref should stay without import."
  );
  ok(document.getElementById("import-button"), "Button should still be there.");

  // OK, actually add some bookmarks:
  MigrationUtils._importQuantities.bookmarks = 5;
  Services.obs.notifyObservers(
    null,
    "Migration:ItemAfterMigrate",
    MigrationUtils.resourceTypes.BOOKMARKS
  );

  is(Services.prefs.prefHasUserValue(kPref), false, "Pref should be removed.");
  ok(
    !document.getElementById("import-button"),
    "Button should have been removed."
  );

  // Reset this, otherwise subsequent tests are going to have a bad time.
  MigrationUtils._importQuantities.bookmarks = 0;
});

/**
 * Check that if the user removes the button, the next startup
 * we clear the pref and stop monitoring to remove the item.
 */
add_task(async function test_bookmark_import_button_removal_cleanup() {
  let bookmarkCount = (
    await PlacesUtils.bookmarks.fetch(PlacesUtils.bookmarks.toolbarGuid)
  ).childCount;
  Assert.less(bookmarkCount, 3, "we should start with less than 3 bookmarks");

  ok(
    !document.getElementById("import-button"),
    "Shouldn't have button to start with."
  );
  await PlacesUIUtils.maybeAddImportButton();
  ok(document.getElementById("import-button"), "Button should be added.");
  is(Services.prefs.getBoolPref(kPref), true, "Pref should be set.");

  // Simulate the user removing the item.
  CustomizableUI.removeWidgetFromArea("import-button");

  // We'll call this next startup:
  PlacesUIUtils.removeImportButtonWhenImportSucceeds();

  // And it should clean up the pref:
  is(Services.prefs.prefHasUserValue(kPref), false, "Pref should be removed.");
});

/**
 * Check that if migration (silently) errors, we still remove the button
 * _if_ we imported any bookmarks.
 */
add_task(async function test_bookmark_import_button_errors() {
  let bookmarkCount = (
    await PlacesUtils.bookmarks.fetch(PlacesUtils.bookmarks.toolbarGuid)
  ).childCount;
  Assert.less(bookmarkCount, 3, "we should start with less than 3 bookmarks");

  ok(
    !document.getElementById("import-button"),
    "Shouldn't have button to start with."
  );
  await PlacesUIUtils.maybeAddImportButton();
  ok(document.getElementById("import-button"), "Button should be added.");
  is(Services.prefs.getBoolPref(kPref), true, "Pref should be set.");

  PlacesUIUtils.removeImportButtonWhenImportSucceeds();

  Services.obs.notifyObservers(
    null,
    "Migration:ItemError",
    MigrationUtils.resourceTypes.BOOKMARKS
  );

  is(
    Services.prefs.getBoolPref(kPref, false),
    true,
    "Pref should stay when fatal error happens."
  );
  ok(document.getElementById("import-button"), "Button should still be there.");

  // OK, actually add some bookmarks:
  MigrationUtils._importQuantities.bookmarks = 5;
  Services.obs.notifyObservers(
    null,
    "Migration:ItemError",
    MigrationUtils.resourceTypes.BOOKMARKS
  );

  is(Services.prefs.prefHasUserValue(kPref), false, "Pref should be removed.");
  ok(
    !document.getElementById("import-button"),
    "Button should have been removed."
  );

  MigrationUtils._importQuantities.bookmarks = 0;
});
