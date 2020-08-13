"use strict";

const { CustomizableUI } = ChromeUtils.import(
  "resource:///modules/CustomizableUI.jsm"
);

add_task(async function() {
  registerFakePath("AppData", do_get_file("AppData/Roaming/"));

  let migrator = await MigrationUtils.getMigrator("360se");
  // Sanity check for the source.
  Assert.ok(await migrator.isSourceAvailable());

  let profiles = await migrator.getSourceProfiles();
  Assert.equal(profiles.length, 2, "Should present two profiles");
  Assert.equal(
    profiles[0].name,
    "test@firefox.com.cn",
    "Current logged in user should be the first"
  );
  Assert.equal(
    profiles[profiles.length - 1].name,
    "Default",
    "Default user should be the last"
  );

  let importedToBookmarksToolbar = false;
  let itemsSeen = { bookmarks: 0, folders: 0 };

  let listener = events => {
    for (let event of events) {
      itemsSeen[
        event.itemType == PlacesUtils.bookmarks.TYPE_FOLDER
          ? "folders"
          : "bookmarks"
      ]++;
      if (event.parentId == PlacesUtils.toolbarFolderId) {
        importedToBookmarksToolbar = true;
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

  await promiseMigration(migrator, MigrationUtils.resourceTypes.BOOKMARKS, {
    id: "default",
  });
  PlacesUtils.observers.removeListener(["bookmark-added"], listener);

  // Check the bookmarks have been imported to all the expected parents.
  Assert.ok(importedToBookmarksToolbar, "Bookmarks imported in the toolbar");
  Assert.equal(itemsSeen.bookmarks, 8, "Should import all bookmarks.");
  Assert.equal(itemsSeen.folders, 2, "Should import all folders.");
  // Check that the telemetry matches:
  Assert.equal(
    MigrationUtils._importQuantities.bookmarks,
    itemsSeen.bookmarks + itemsSeen.folders,
    "Telemetry reporting correct."
  );
  Assert.ok(observerNotified, "The observer should be notified upon migration");
});
