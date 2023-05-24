"use strict";

const { CustomizableUI } = ChromeUtils.importESModule(
  "resource:///modules/CustomizableUI.sys.mjs"
);

add_task(async function () {
  registerFakePath("ULibDir", do_get_file("Library/"));
  const faviconPath = do_get_file(
    "Library/Safari/Favicon Cache/favicons.db"
  ).path;

  let migrator = await MigrationUtils.getMigrator("safari");
  // Sanity check for the source.
  Assert.ok(await migrator.isSourceAvailable());

  // Wait for the imported bookmarks. We don't check that "From Safari"
  // folders are created on the toolbar since the profile
  // we're importing to has less than 3 bookmarks in the destination
  // so a "From Safari" folder isn't created.
  let expectedParentGuids = [PlacesUtils.bookmarks.toolbarGuid];
  let itemCount = 0;

  let gotFolder = false;
  let listener = events => {
    for (let event of events) {
      itemCount++;
      if (
        event.itemType == PlacesUtils.bookmarks.TYPE_FOLDER &&
        event.title == "Food and Travel"
      ) {
        gotFolder = true;
      }
      if (expectedParentGuids.length) {
        let index = expectedParentGuids.indexOf(event.parentGuid);
        Assert.ok(index != -1, "Found expected parent");
        expectedParentGuids.splice(index, 1);
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
  Assert.ok(!expectedParentGuids.length, "No more expected parents");
  Assert.ok(gotFolder, "Should have seen the folder get imported");
  Assert.equal(itemCount, 14, "Should import all 14 items.");
  // Check that the telemetry matches:
  Assert.equal(
    MigrationUtils._importQuantities.bookmarks,
    itemCount,
    "Telemetry reporting correct."
  );

  // Check that favicons migrated
  let faviconURIs = await MigrationUtils.getRowsFromDBWithoutLocks(
    faviconPath,
    "Safari Bookmark Favicons",
    `SELECT I.uuid, I.url AS favicon_url, P.url 
    FROM icon_info I 
    INNER JOIN page_url P ON I.uuid = P.uuid;`
  );
  let pageUrls = Array.from(faviconURIs, row =>
    Services.io.newURI(row.getResultByName("url"))
  );
  await assertFavicons(pageUrls);
  Assert.ok(observerNotified, "The observer should be notified upon migration");
});
