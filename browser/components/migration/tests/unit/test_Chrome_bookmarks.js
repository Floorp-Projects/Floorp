"use strict";

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { CustomizableUI } = ChromeUtils.import(
  "resource:///modules/CustomizableUI.jsm"
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

async function testBookmarks(migratorKey, subDirs, folderName) {
  if (AppConstants.platform == "macosx") {
    subDirs.unshift("Application Support");
  } else if (AppConstants.platform == "win") {
    subDirs.push("User Data");
  } else {
    subDirs.unshift(".config");
  }

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
  const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");
  await OS.File.makeDir(target.path, {
    from: rootDir.parent.path,
    ignoreExisting: true,
  });

  target.append("Bookmarks");
  await OS.File.remove(target.path, { ignoreAbsent: true });

  let bookmarksData = {
    roots: { bookmark_bar: { children: [] }, other: { children: [] } },
  };
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

  await OS.File.writeAtomic(target.path, JSON.stringify(bookmarksData), {
    encoding: "utf-8",
  });

  let migrator = await MigrationUtils.getMigrator(migratorKey);
  // Sanity check for the source.
  Assert.ok(await migrator.isSourceAvailable());

  let itemsSeen = { bookmarks: 0, folders: 0 };
  let listener = events => {
    for (let event of events) {
      // "From " comes from the string `importedBookmarksFolder`
      if (
        event.title.startsWith("From ") &&
        event.itemType == PlacesUtils.bookmarks.TYPE_FOLDER
      ) {
        Assert.equal(event.title, folderName, "Bookmark folder name");
      } else {
        itemsSeen[
          event.itemType == PlacesUtils.bookmarks.TYPE_FOLDER
            ? "folders"
            : "bookmarks"
        ]++;
      }
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
  await promiseMigration(
    migrator,
    MigrationUtils.resourceTypes.BOOKMARKS,
    PROFILE
  );
  PlacesUtils.observers.removeListener(["bookmark-added"], listener);

  Assert.equal(itemsSeen.bookmarks, 200, "Should have seen 200 bookmarks.");
  Assert.equal(itemsSeen.folders, 10, "Should have seen 10 folders.");
  Assert.equal(
    MigrationUtils._importQuantities.bookmarks,
    itemsSeen.bookmarks + itemsSeen.folders,
    "Telemetry reporting correct."
  );
  Assert.ok(observerNotified, "The observer should be notified upon migration");
}

add_task(async function test_Chrome() {
  let subDirs =
    AppConstants.platform == "linux" ? ["google-chrome"] : ["Google", "Chrome"];
  await testBookmarks("chrome", subDirs, "From Google Chrome");
});

add_task(async function test_ChromiumEdge() {
  if (AppConstants.platform == "linux") {
    // Edge isn't available on Linux.
    return;
  }
  let subDirs =
    AppConstants.platform == "macosx"
      ? ["Microsoft Edge"]
      : ["Microsoft", "Edge"];
  await testBookmarks("chromium-edge", subDirs, "From Microsoft Edge");
});
