"use strict";

const { CustomizableUI } = ChromeUtils.import(
  "resource:///modules/CustomizableUI.jsm"
);

add_task(async function() {
  let migrator = await MigrationUtils.getMigrator("ie");
  // Sanity check for the source.
  Assert.ok(await migrator.isSourceAvailable(), "Check migrator source");

  let expectedParents = [
    PlacesUtils.bookmarksMenuFolderId,
    PlacesUtils.toolbarFolderId,
  ];

  let itemCount = 0;
  let listener = events => {
    for (let event of events) {
      if (event.itemType == PlacesUtils.bookmarks.TYPE_BOOKMARK) {
        itemCount++;
      }
      if (expectedParents.length && expectedParents.includes(event.parentId)) {
        let index = expectedParents.indexOf(event.parentId);
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
  Assert.equal(
    MigrationUtils._importQuantities.bookmarks,
    itemCount,
    "Ensure telemetry matches actual number of imported items."
  );

  // Check the bookmarks have been imported to all the expected parents.
  Assert.equal(expectedParents.length, 0, "Got all the expected parents");
  Assert.ok(observerNotified, "The observer should be notified upon migration");
});
