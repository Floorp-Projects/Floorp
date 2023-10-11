"use strict";

const { CustomizableUI } = ChromeUtils.importESModule(
  "resource:///modules/CustomizableUI.sys.mjs"
);

const { PlacesUIUtils } = ChromeUtils.importESModule(
  "resource:///modules/PlacesUIUtils.sys.mjs"
);

let rootDir = do_get_file("chromefiles/", true);

add_task(async function setup_fakePaths() {
  let pathId;
  if (AppConstants.platform == "macosx") {
    pathId = "ULibDir";
  } else if (AppConstants.platform == "win") {
    pathId = "LocalAppData";
  } else {
    pathId = "Home";
  }
  registerFakePath(pathId, rootDir);
});

add_task(async function setup_initialBookmarks() {
  let bookmarks = [];
  for (let i = 0; i < PlacesUIUtils.NUM_TOOLBAR_BOOKMARKS_TO_UNHIDE + 1; i++) {
    bookmarks.push({ url: "https://example.com/" + i, title: "" + i });
  }

  // Ensure we have enough items in both the menu and toolbar to trip creating a "from" folder.
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.toolbarGuid,
    children: bookmarks,
  });
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.menuGuid,
    children: bookmarks,
  });
});

async function testBookmarks(migratorKey, subDirs) {
  if (AppConstants.platform == "macosx") {
    subDirs.unshift("Application Support");
  } else if (AppConstants.platform == "win") {
    subDirs.push("User Data");
  } else {
    subDirs.unshift(".config");
  }

  let target = rootDir.clone();

  // Pretend this is the default profile
  while (subDirs.length) {
    target.append(subDirs.shift());
  }

  let localStatePath = PathUtils.join(target.path, "Local State");
  await IOUtils.writeJSON(localStatePath, []);

  target.append("Default");

  await IOUtils.makeDirectory(target.path, {
    createAncestor: true,
    ignoreExisting: true,
  });

  // Copy Favicons database into Default profile
  const sourcePath = do_get_file(
    "AppData/Local/Google/Chrome/User Data/Default/Favicons"
  ).path;
  await IOUtils.copy(sourcePath, target.path);

  // Get page url for each favicon
  let faviconURIs = await MigrationUtils.getRowsFromDBWithoutLocks(
    sourcePath,
    "Chrome Bookmark Favicons",
    `select page_url from icon_mapping`
  );

  target.append("Bookmarks");
  await IOUtils.remove(target.path, { ignoreAbsent: true });

  let bookmarksData = createChromeBookmarkStructure();
  await IOUtils.writeJSON(target.path, bookmarksData);

  let migrator = await MigrationUtils.getMigrator(migratorKey);
  Assert.ok(await migrator.hasPermissions(), "Has permissions");
  // Sanity check for the source.
  Assert.ok(await migrator.isSourceAvailable());

  let itemsSeen = { bookmarks: 0, folders: 0 };
  let listener = events => {
    for (let event of events) {
      itemsSeen[
        event.itemType == PlacesUtils.bookmarks.TYPE_FOLDER
          ? "folders"
          : "bookmarks"
      ]++;
    }
  };

  PlacesUtils.observers.addListener(["bookmark-added"], listener);
  const PROFILE = {
    id: "Default",
    name: "Default",
  };
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
  const initialToolbarCount = await getFolderItemCount(
    PlacesUtils.bookmarks.toolbarGuid
  );
  const initialUnfiledCount = await getFolderItemCount(
    PlacesUtils.bookmarks.unfiledGuid
  );
  const initialmenuCount = await getFolderItemCount(
    PlacesUtils.bookmarks.menuGuid
  );

  await promiseMigration(
    migrator,
    MigrationUtils.resourceTypes.BOOKMARKS,
    PROFILE
  );
  const postToolbarCount = await getFolderItemCount(
    PlacesUtils.bookmarks.toolbarGuid
  );
  const postUnfiledCount = await getFolderItemCount(
    PlacesUtils.bookmarks.unfiledGuid
  );
  const postmenuCount = await getFolderItemCount(
    PlacesUtils.bookmarks.menuGuid
  );

  Assert.equal(
    postUnfiledCount - initialUnfiledCount,
    210,
    "Should have seen 210 items in unsorted bookmarks"
  );
  Assert.equal(
    postToolbarCount - initialToolbarCount,
    105,
    "Should have seen 105 items in toolbar"
  );
  Assert.equal(
    postmenuCount - initialmenuCount,
    0,
    "Should have seen 0 items in menu toolbar"
  );

  PlacesUtils.observers.removeListener(["bookmark-added"], listener);

  Assert.equal(itemsSeen.bookmarks, 300, "Should have seen 300 bookmarks.");
  Assert.equal(itemsSeen.folders, 15, "Should have seen 15 folders.");
  Assert.equal(
    MigrationUtils._importQuantities.bookmarks,
    itemsSeen.bookmarks + itemsSeen.folders,
    "Telemetry reporting correct."
  );
  Assert.ok(observerNotified, "The observer should be notified upon migration");
  let pageUrls = Array.from(faviconURIs, f =>
    Services.io.newURI(f.getResultByName("page_url"))
  );
  await assertFavicons(pageUrls);
}

add_task(async function test_Chrome() {
  // Expire all favicons before the test to make sure favicons are imported
  PlacesUtils.favicons.expireAllFavicons();
  let subDirs =
    AppConstants.platform == "linux" ? ["google-chrome"] : ["Google", "Chrome"];
  await testBookmarks("chrome", subDirs);
});

add_task(async function test_ChromiumEdge() {
  PlacesUtils.favicons.expireAllFavicons();
  if (AppConstants.platform == "linux") {
    // Edge isn't available on Linux.
    return;
  }
  let subDirs =
    AppConstants.platform == "macosx"
      ? ["Microsoft Edge"]
      : ["Microsoft", "Edge"];
  await testBookmarks("chromium-edge", subDirs);
});

async function getFolderItemCount(guid) {
  let results = await PlacesUtils.promiseBookmarksTree(guid);

  return results.itemsCount;
}
