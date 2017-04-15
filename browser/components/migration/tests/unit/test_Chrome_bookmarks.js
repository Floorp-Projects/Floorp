"use strict";

Cu.import("resource://gre/modules/AppConstants.jsm");

add_task(function* () {
  let rootDir = do_get_file("chromefiles/");
  let pathId;
  let subDirs = ["Google", "Chrome"];
  if (AppConstants.platform == "macosx") {
    subDirs.unshift("Application Support");
    pathId = "ULibDir";
  } else if (AppConstants.platform == "win") {
    subDirs.push("User Data");
    pathId = "LocalAppData";
  } else {
    subDirs = [".config", "google-chrome"];
    pathId = "Home";
  }
  registerFakePath(pathId, rootDir);

  let target = rootDir.clone();
  // Pretend this is the default profile
  subDirs.push("Default");
  while (subDirs.length) {
    target.append(subDirs.shift());
  }
  // We don't import osfile.jsm until after registering the fake path, because
  // importing osfile will sometimes greedily fetch certain path identifiers
  // from the dir service, which means they get cached, which means we can't
  // register a fake path for them anymore.
  Cu.import("resource://gre/modules/osfile.jsm"); /* globals OS */
  yield OS.File.makeDir(target.path, {from: rootDir.parent.path, ignoreExisting: true});

  target.append("Bookmarks");
  yield OS.File.remove(target.path, {ignoreAbsent: true});

  let bookmarksData = {roots: {bookmark_bar: {children: []}, other: {children: []}}};
  const MAX_BMS = 100;
  let barKids = bookmarksData.roots.bookmark_bar.children;
  let menuKids = bookmarksData.roots.other.children;
  let currentMenuKids = menuKids;
  let currentBarKids = barKids;
  for (let i = 0; i < MAX_BMS; i++) {
    currentBarKids.push({
      url: "https://www.chrome-bookmark-bar-bookmark" + i + ".com",
      name: "bookmark " + i,
      type: "url",
    });
    currentMenuKids.push({
      url: "https://www.chrome-menu-bookmark" + i + ".com",
      name: "bookmark for menu " + i,
      type: "url",
    });
    if (i % 20 == 19) {
      let nextFolder = {
        name: "toolbar folder " + Math.ceil(i / 20),
        type: "folder",
        children: [],
      };
      currentBarKids.push(nextFolder);
      currentBarKids = nextFolder.children;

      nextFolder = {
        name: "menu folder " + Math.ceil(i / 20),
        type: "folder",
        children: [],
      };
      currentMenuKids.push(nextFolder);
      currentMenuKids = nextFolder.children;
    }
  }

  yield OS.File.writeAtomic(target.path, JSON.stringify(bookmarksData), {encoding: "utf-8"});

  let migrator = MigrationUtils.getMigrator("chrome");
  // Sanity check for the source.
  Assert.ok(migrator.sourceExists);

  let itemsSeen = {bookmarks: 0, folders: 0};
  let bmObserver = {
    onItemAdded(aItemId, aParentId, aIndex, aItemType, aURI, aTitle) {
      if (!aTitle.includes("Chrome")) {
        itemsSeen[aItemType == PlacesUtils.bookmarks.TYPE_FOLDER ? "folders" : "bookmarks"]++;
      }
    },
    onBeginUpdateBatch() {},
    onEndUpdateBatch() {},
    onItemRemoved() {},
    onItemChanged() {},
    onItemVisited() {},
    onItemMoved() {},
  };

  PlacesUtils.bookmarks.addObserver(bmObserver);
  const PROFILE = {
    id: "Default",
    name: "Default",
  };
  yield promiseMigration(migrator, MigrationUtils.resourceTypes.BOOKMARKS, PROFILE);
  PlacesUtils.bookmarks.removeObserver(bmObserver);

  Assert.equal(itemsSeen.bookmarks, 200, "Should have seen 200 bookmarks.");
  Assert.equal(itemsSeen.folders, 10, "Should have seen 10 folders.");
  Assert.equal(MigrationUtils._importQuantities.bookmarks, itemsSeen.bookmarks + itemsSeen.folders, "Telemetry reporting correct.");
});
