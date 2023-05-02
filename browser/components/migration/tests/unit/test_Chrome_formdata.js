/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { FormHistory } = ChromeUtils.importESModule(
  "resource://gre/modules/FormHistory.sys.mjs"
);

let rootDir = do_get_file("chromefiles/", true);

add_setup(async function setup_fakePaths() {
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

/**
 * This function creates a testing database in the default profile,
 * populates it with 10 example data entries,migrates the database,
 * and then searches for each entry to ensure it exists in the FormHistory.
 *
 * @async
 * @param {string} migratorKey
 *    A string that identifies the type of migrator object to be retrieved.
 * @param {Array<string>} subDirs
 *    An array of strings that specifies the subdirectories for the target profile directory.
 * @returns {Promise<undefined>}
 *    A Promise that resolves when the migration is completed.
 */
async function testFormdata(migratorKey, subDirs) {
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

  target.append("Web Data");
  await IOUtils.remove(target.path, { ignoreAbsent: true });

  // Clear any search history results
  await FormHistory.update({ op: "remove" });

  let dbConn = await Sqlite.openConnection({ path: target.path });

  await dbConn.execute(
    `CREATE TABLE "autofill" (name VARCHAR, value VARCHAR, value_lower VARCHAR, date_created INTEGER DEFAULT 0, date_last_used INTEGER DEFAULT 0, count INTEGER DEFAULT 1, PRIMARY KEY (name, value))`
  );
  for (let i = 0; i < 10; i++) {
    await dbConn.execute(
      `INSERT INTO autofill VALUES (:name, :value, :value_lower, :date_created, :date_last_used, :count)`,
      {
        name: `name${i}`,
        value: `example${i}`,
        value_lower: `example${i}`,
        date_created: Math.round(Date.now() / 1000) - i * 10000,
        date_last_used: Date.now(),
        count: i,
      }
    );
  }
  await dbConn.close();

  let migrator = await MigrationUtils.getMigrator(migratorKey);
  // Sanity check for the source.
  Assert.ok(await migrator.isSourceAvailable());

  await promiseMigration(migrator, MigrationUtils.resourceTypes.FORMDATA, {
    id: "Default",
    name: "Person 1",
  });

  for (let i = 0; i < 10; i++) {
    let results = await FormHistory.search(["fieldname", "value"], {
      fieldname: `name${i}`,
      value: `example${i}`,
    });
    Assert.ok(results.length, `Should have item${i} in FormHistory`);
  }
}

add_task(async function test_Chrome() {
  let subDirs =
    AppConstants.platform == "linux" ? ["google-chrome"] : ["Google", "Chrome"];
  await testFormdata("chrome", subDirs);
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
  await testFormdata("chromium-edge", subDirs);
});
