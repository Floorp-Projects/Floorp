"use strict";

const { Qihoo360seMigrationUtils } = ChromeUtils.importESModule(
  "resource:///modules/360seMigrationUtils.sys.mjs"
);

const parentPath = do_get_file("AppData/Roaming/360se6/User Data").path;
const loggedInPath = "0f3ab103a522f4463ecacc36d34eb996";
const loggedInBackup = PathUtils.join(
  parentPath,
  "Default",
  loggedInPath,
  "DailyBackup",
  "360default_ori_2021_12_02.favdb"
);
const loggedOutBackup = PathUtils.join(
  parentPath,
  "Default",
  "DailyBackup",
  "360default_ori_2021_12_02.favdb"
);

function getSqlitePath(profileId) {
  return PathUtils.join(parentPath, profileId, loggedInPath, "360sefav.dat");
}

add_task(async function test_360se10_logged_in() {
  const sqlitePath = getSqlitePath("Default");
  await IOUtils.setModificationTime(sqlitePath);
  await IOUtils.copy(
    PathUtils.join(parentPath, "Default", "360Bookmarks"),
    PathUtils.join(parentPath, "Default", loggedInPath)
  );
  await IOUtils.copy(loggedOutBackup, loggedInBackup);

  const alternativeBookmarks =
    await Qihoo360seMigrationUtils.getAlternativeBookmarks({
      bookmarksPath: PathUtils.join(parentPath, "Default", "Bookmarks"),
      localState: {
        sync_login_info: {
          filepath: loggedInPath,
        },
      },
    });
  Assert.ok(
    alternativeBookmarks.resource && alternativeBookmarks.resource.exists,
    "Should return the legacy bookmark resource."
  );
  Assert.strictEqual(
    alternativeBookmarks.path,
    undefined,
    "Should not return any path to plain text bookmarks."
  );
});

add_task(async function test_360se10_logged_in_outdated_sqlite() {
  const sqlitePath = getSqlitePath("Default");
  await IOUtils.setModificationTime(
    sqlitePath,
    new Date("2020-08-18").valueOf()
  );

  const alternativeBookmarks =
    await Qihoo360seMigrationUtils.getAlternativeBookmarks({
      bookmarksPath: PathUtils.join(parentPath, "Default", "Bookmarks"),
      localState: {
        sync_login_info: {
          filepath: loggedInPath,
        },
      },
    });
  Assert.strictEqual(
    alternativeBookmarks.resource,
    undefined,
    "Should not return the outdated legacy bookmark resource."
  );
  Assert.strictEqual(
    alternativeBookmarks.path,
    loggedInBackup,
    "Should return path to the most recent plain text bookmarks backup."
  );

  await IOUtils.setModificationTime(sqlitePath);
});

add_task(async function test_360se10_logged_out() {
  const alternativeBookmarks =
    await Qihoo360seMigrationUtils.getAlternativeBookmarks({
      bookmarksPath: PathUtils.join(parentPath, "Default", "Bookmarks"),
      localState: {
        sync_login_info: {
          filepath: "",
        },
      },
    });
  Assert.strictEqual(
    alternativeBookmarks.resource,
    undefined,
    "Should not return the legacy bookmark resource."
  );
  Assert.strictEqual(
    alternativeBookmarks.path,
    loggedOutBackup,
    "Should return path to the most recent plain text bookmarks backup."
  );
});

add_task(async function test_360se9_logged_in_outdated_sqlite() {
  const sqlitePath = getSqlitePath("Default4SE9Test");
  await IOUtils.setModificationTime(
    sqlitePath,
    new Date("2020-08-18").valueOf()
  );

  const alternativeBookmarks =
    await Qihoo360seMigrationUtils.getAlternativeBookmarks({
      bookmarksPath: PathUtils.join(parentPath, "Default4SE9Test", "Bookmarks"),
      localState: {
        sync_login_info: {
          filepath: loggedInPath,
        },
      },
    });
  Assert.strictEqual(
    alternativeBookmarks.resource,
    undefined,
    "Should not return the legacy bookmark resource."
  );
  Assert.strictEqual(
    alternativeBookmarks.path,
    PathUtils.join(
      parentPath,
      "Default4SE9Test",
      loggedInPath,
      "DailyBackup",
      "360sefav_2020_08_28.favdb"
    ),
    "Should return path to the most recent plain text bookmarks backup."
  );

  await IOUtils.setModificationTime(sqlitePath);
});

add_task(async function test_360se9_logged_out() {
  const alternativeBookmarks =
    await Qihoo360seMigrationUtils.getAlternativeBookmarks({
      bookmarksPath: PathUtils.join(parentPath, "Default4SE9Test", "Bookmarks"),
      localState: {
        sync_login_info: {
          filepath: "",
        },
      },
    });
  Assert.strictEqual(
    alternativeBookmarks.resource,
    undefined,
    "Should not return the legacy bookmark resource."
  );
  Assert.strictEqual(
    alternativeBookmarks.path,
    PathUtils.join(parentPath, "Default4SE9Test", "Bookmarks"),
    "Should return path to the plain text canonical bookmarks."
  );
});
