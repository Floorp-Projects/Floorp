/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { PlacesQuery } = ChromeUtils.importESModule(
  "resource://gre/modules/PlacesQuery.sys.mjs"
);

const HISTORY_FILE_PATH = "Library/Safari/History.db";
const HISTORY_STRANGE_ENTRIES_FILE_PATH =
  "Library/Safari/HistoryStrangeEntries.db";

// By default, our migrators will cut off migrating any history older than
// 180 days. In order to make sure this test continues to run correctly
// in the future, we copy the reference database to History.db, and then
// use Sqlite.sys.mjs to connect to it and manually update all of the visit
// times to be "now", so that they all fall within the 180 day window. The
// Nov 10th date below is right around when the reference database visit
// entries were created.
//
// This update occurs in `updateVisitTimes`.
const MS_SINCE_SNAPSHOT_TIME =
  new Date() - new Date("Nov 10, 2022 00:00:00 UTC");

async function setupHistoryFile() {
  removeHistoryFile();
  let file = do_get_file(HISTORY_STRANGE_ENTRIES_FILE_PATH);
  file.copyTo(file.parent, "History.db");
  await updateVisitTimes();
}

function removeHistoryFile() {
  let file = do_get_file(HISTORY_FILE_PATH, true);
  try {
    file.remove(false);
  } catch (ex) {
    // It is ok if this doesn't exist.
    if (ex.result != Cr.NS_ERROR_FILE_NOT_FOUND) {
      throw ex;
    }
  }
}

add_setup(async function setup() {
  registerFakePath("ULibDir", do_get_file("Library/"));
  await setupHistoryFile();
  registerCleanupFunction(async () => {
    await PlacesUtils.history.clear();
    removeHistoryFile();
  });
});

async function updateVisitTimes() {
  let cocoaSnapshotDelta = MS_SINCE_SNAPSHOT_TIME / 1000;
  let historyFile = do_get_file(HISTORY_FILE_PATH);
  let dbConn = await Sqlite.openConnection({ path: historyFile.path });

  await dbConn.execute(
    "UPDATE history_visits SET visit_time = visit_time + :cocoaSnapshotDelta;",
    {
      cocoaSnapshotDelta,
    }
  );

  await dbConn.close();
}

/**
 * Tests that we can import successfully from Safari when Safari's history
 * database contains malformed URLs.
 */
add_task(async function testHistoryImportStrangeEntries() {
  await PlacesUtils.history.clear();

  let placesQuery = new PlacesQuery();
  let emptyHistory = await placesQuery.getHistory();
  Assert.equal(emptyHistory.size, 0, "Empty history should indeed be empty.");

  const EXPECTED_MIGRATED_SITES = 10;
  const EXPECTED_MIGRATED_VISTS = 23;

  let historyFile = do_get_file(HISTORY_FILE_PATH);
  let dbConn = await Sqlite.openConnection({ path: historyFile.path });
  let [rowCountResult] = await dbConn.execute(
    "SELECT COUNT(*) FROM history_visits"
  );
  Assert.greater(
    rowCountResult.getResultByName("COUNT(*)"),
    EXPECTED_MIGRATED_VISTS,
    "There are more total rows than valid rows"
  );
  await dbConn.close();

  let migrator = await MigrationUtils.getMigrator("safari");
  await promiseMigration(migrator, MigrationUtils.resourceTypes.HISTORY);
  let migratedHistory = await placesQuery.getHistory({ sortBy: "site" });
  let siteCount = migratedHistory.size;
  let visitCount = 0;
  for (let [, visits] of migratedHistory) {
    visitCount += visits.length;
  }
  Assert.equal(
    siteCount,
    EXPECTED_MIGRATED_SITES,
    "Should have migrated all valid history sites"
  );
  Assert.equal(
    visitCount,
    EXPECTED_MIGRATED_VISTS,
    "Should have migrated all valid history visits"
  );

  placesQuery.close();
});
