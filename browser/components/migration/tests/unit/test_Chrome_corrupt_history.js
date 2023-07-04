/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let rootDir = do_get_file("chromefiles/", true);

const SOURCE_PROFILE_DIR = "Library/Application Support/Google/Chrome/Default/";
const PROFILE = {
  id: "Default",
  name: "Person 1",
};

add_setup(async function setup_fake_paths() {
  let pathId;
  if (AppConstants.platform == "macosx") {
    pathId = "ULibDir";
  } else if (AppConstants.platform == "win") {
    pathId = "LocalAppData";
  } else {
    pathId = "Home";
  }
  registerFakePath(pathId, rootDir);

  let file = do_get_file(`${SOURCE_PROFILE_DIR}HistoryCorrupt`);
  file.copyTo(file.parent, "History");

  registerCleanupFunction(() => {
    let historyFile = do_get_file(`${SOURCE_PROFILE_DIR}History`, true);
    try {
      historyFile.remove(false);
    } catch (ex) {
      // It is ok if this doesn't exist.
      if (ex.result != Cr.NS_ERROR_FILE_NOT_FOUND) {
        throw ex;
      }
    }
  });

  let subDirs =
    AppConstants.platform == "linux" ? ["google-chrome"] : ["Google", "Chrome"];

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

  await IOUtils.makeDirectory(target.path, {
    createAncestor: true,
    ignoreExisting: true,
  });

  target.append("Bookmarks");
  await IOUtils.remove(target.path, { ignoreAbsent: true });

  let bookmarksData = createChromeBookmarkStructure();
  await IOUtils.writeJSON(target.path, bookmarksData);
});

add_task(async function test_corrupt_history() {
  let migrator = await MigrationUtils.getMigrator("chrome");
  Assert.ok(await migrator.isSourceAvailable());

  let data = await migrator.getMigrateData(PROFILE);
  Assert.ok(
    data & MigrationUtils.resourceTypes.BOOKMARKS,
    "Bookmarks resource available."
  );
  Assert.ok(
    !(data & MigrationUtils.resourceTypes.HISTORY),
    "Corrupt history resource unavailable."
  );
});
