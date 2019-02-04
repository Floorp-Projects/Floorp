/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {ChromeMigrationUtils} =
  ChromeUtils.import("resource:///modules/ChromeMigrationUtils.jsm", null);

const SOURCE_PROFILE_DIR = "Library/Application Support/Google/Chrome/Default/";

const PROFILE = {
  id: "Default",
  name: "Person 1",
};

/**
 * TEST_URLS reflects the data stored in '${SOURCE_PROFILE_DIR}HistoryMaster'.
 * The main object reflects the data in the 'urls' table. The visits property
 * reflects the associated data in the 'visits' table.
 */
const TEST_URLS = [{
  id: 1,
  url: "http://example.com/",
  title: "test",
  visit_count: 1,
  typed_count: 0,
  last_visit_time: 13193151310368000,
  hidden: 0,
  visits: [{
    id: 1,
    url: 1,
    visit_time: 13193151310368000,
    from_visit: 0,
    transition: 805306370,
    segment_id: 0,
    visit_duration: 10745006,
    incremented_omnibox_typed_score: 0,
  }],
}, {
  id: 2,
  url: "http://invalid.com/",
  title: "test2",
  visit_count: 1,
  typed_count: 0,
  last_visit_time: 13193154948901000,
  hidden: 0,
  visits: [{
    id: 2,
    url: 2,
    visit_time: 13193154948901000,
    from_visit: 0,
    transition: 805306376,
    segment_id: 0,
    visit_duration: 6568270,
    incremented_omnibox_typed_score: 0,
  }],
}];

async function setVisitTimes(time) {
  let loginDataFile = do_get_file(`${SOURCE_PROFILE_DIR}History`);
  let dbConn = await Sqlite.openConnection({ path: loginDataFile.path });

  await dbConn.execute(`UPDATE urls SET last_visit_time = :last_visit_time`, {
    last_visit_time: time,
  });
  await dbConn.execute(`UPDATE visits SET visit_time = :visit_time`, {
    visit_time: time,
  });

  await dbConn.close();
}

function assertEntryMatches(entry, urlInfo, dateWasInFuture = false) {
  info(`Checking url: ${urlInfo.url}`);
  Assert.ok(entry, `Should have stored an entry`);

  Assert.equal(entry.url, urlInfo.url, "Should have the correct URL");
  Assert.equal(entry.title, urlInfo.title, "Should have the correct title");
  Assert.equal(entry.visits.length, urlInfo.visits.length,
    "Should have the correct number of visits");

  for (let index in urlInfo.visits) {
    Assert.equal(entry.visits[index].transition,
      PlacesUtils.history.TRANSITIONS.LINK,
      "Should have Link type transition");

    if (dateWasInFuture) {
      Assert.lessOrEqual(entry.visits[index].date.getTime(), new Date().getTime(),
        "Should have moved the date to no later than the current date.");
    } else {
      Assert.equal(
        entry.visits[index].date.getTime(),
        ChromeMigrationUtils.chromeTimeToDate(urlInfo.visits[index].visit_time).getTime(),
        "Should have the correct date");
    }
  }
}

function setupHistoryFile() {
  removeHistoryFile();
  let file = do_get_file(`${SOURCE_PROFILE_DIR}HistoryMaster`);
  file.copyTo(file.parent, "History");
}

function removeHistoryFile() {
  let file = do_get_file(`${SOURCE_PROFILE_DIR}History`, true);
  try {
    file.remove(false);
  } catch (ex) {
    // It is ok if this doesn't exist.
    if (ex.result != Cr.NS_ERROR_FILE_TARGET_DOES_NOT_EXIST) {
      throw ex;
    }
  }
}

add_task(async function setup() {
  registerFakePath("ULibDir", do_get_file("Library/"));

  registerCleanupFunction(async () => {
    await PlacesUtils.history.clear();
    removeHistoryFile();
  });
});

add_task(async function test_import() {
  setupHistoryFile();
  await PlacesUtils.history.clear();

  let migrator = await MigrationUtils.getMigrator("chrome");
  Assert.ok(await migrator.isSourceAvailable(), "Sanity check the source exists");

  await promiseMigration(migrator, MigrationUtils.resourceTypes.HISTORY, PROFILE);

  for (let urlInfo of TEST_URLS) {
    let entry = await PlacesUtils.history.fetch(urlInfo.url, {includeVisits: true});
    assertEntryMatches(entry, urlInfo);
  }
});

add_task(async function test_import_future_date() {
  setupHistoryFile();
  await PlacesUtils.history.clear();
  const futureDate = new Date().getTime() + 6000 * 60 * 24;
  await setVisitTimes(ChromeMigrationUtils.dateToChromeTime(futureDate));

  let migrator = await MigrationUtils.getMigrator("chrome");
  Assert.ok(await migrator.isSourceAvailable(), "Sanity check the source exists");

  await promiseMigration(migrator, MigrationUtils.resourceTypes.HISTORY, PROFILE);

  for (let urlInfo of TEST_URLS) {
    let entry = await PlacesUtils.history.fetch(urlInfo.url, {includeVisits: true});
    assertEntryMatches(entry, urlInfo, true);
  }
});
