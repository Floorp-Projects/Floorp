"use strict";

const { CustomizableUI } = ChromeUtils.import(
  "resource:///modules/CustomizableUI.jsm"
);

add_task(async function() {
  registerFakePath("ULibDir", do_get_file("Library/"));

  let migrator = await MigrationUtils.getMigrator("safari");
  // Sanity check for the source.
  Assert.ok(await migrator.isSourceAvailable());

  // Wait for the imported bookmarks.  Check that "From Safari"
  // folders are created on the toolbar.
  let source = await MigrationUtils.getLocalizedString("source-name-safari");
  let label = await MigrationUtils.getLocalizedString(
    "imported-bookmarks-source",
    { source }
  );

  let expectedParents = [PlacesUtils.toolbarFolderId];
  let itemCount = 0;

  let gotFolder = false;
  let listener = events => {
    for (let event of events) {
      if (event.title != label) {
        itemCount++;
      }
      if (
        event.itemType == PlacesUtils.bookmarks.TYPE_FOLDER &&
        event.title == "Stuff"
      ) {
        gotFolder = true;
      }
      if (expectedParents.length && event.title == label) {
        let index = expectedParents.indexOf(event.parentId);
        Assert.ok(index != -1, "Found expected parent");
        expectedParents.splice(index, 1);
      }
    }
  };
  PlacesUtils.observers.addListener(["bookmark-added"], listener);
  let observerNotified = false;
  Services.obs.addObserver((aSubject, aTopic, aData) => {
    let [toolbar, visibility] = JSON.parse(aData);
    Assert.equal(
      toolbar,
      CustomizableUI.AREA_BOOKMARKS,
      "Notification should be received for bookmarks toolbar"
    );
    Assert.equal(
      visibility,
      "true",
      "Notification should say to reveal the bookmarks toolbar"
    );
    observerNotified = true;
  }, "browser-set-toolbar-visibility");

  await promiseMigration(migrator, MigrationUtils.resourceTypes.BOOKMARKS);
  PlacesUtils.observers.removeListener(["bookmark-added"], listener);

  // Check the bookmarks have been imported to all the expected parents.
  Assert.ok(!expectedParents.length, "No more expected parents");
  Assert.ok(gotFolder, "Should have seen the folder get imported");
  Assert.equal(itemCount, 13, "Should import all 13 items.");
  // Check that the telemetry matches:
  Assert.equal(
    MigrationUtils._importQuantities.bookmarks,
    itemCount,
    "Telemetry reporting correct."
  );
  Assert.ok(observerNotified, "The observer should be notified upon migration");
});
