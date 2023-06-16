/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const HISTORY_TEMPLATE_FILE_PATH = "Library/Safari/HistoryTemplate.db";
const HISTORY_FILE_PATH = "Library/Safari/History.db";

// We want this to be some recent time, so we'll always add some time to our
// dates to keep them ~ five days ago.
const MS_FROM_REFERENCE_TIME =
  new Date() - new Date("May 31, 2023 00:00:00 UTC");

const TEST_URLS = [
  {
    url: "http://example.com/",
    title: "Example Domain",
    time: 706743588.04751,
    jsTime: 1685050788047 + MS_FROM_REFERENCE_TIME,
    visits: 1,
  },
  {
    url: "http://mozilla.org/",
    title: "",
    time: 706743581.133386,
    jsTime: 1685050781133 + MS_FROM_REFERENCE_TIME,
    visits: 1,
  },
  {
    url: "https://www.mozilla.org/en-CA/",
    title: "Internet for people, not profit - Mozilla",
    time: 706743581.133679,
    jsTime: 1685050781133 + MS_FROM_REFERENCE_TIME,
    visits: 1,
  },
];

async function setupHistoryFile() {
  removeHistoryFile();
  let file = do_get_file(HISTORY_TEMPLATE_FILE_PATH);
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

async function updateVisitTimes() {
  let cocoaDifference = MS_FROM_REFERENCE_TIME / 1000;
  let historyFile = do_get_file(HISTORY_FILE_PATH);
  let dbConn = await Sqlite.openConnection({ path: historyFile.path });
  await dbConn.execute(
    "UPDATE history_visits SET visit_time = visit_time + :difference;",
    { difference: cocoaDifference }
  );
  await dbConn.close();
}

add_setup(async function setup() {
  registerFakePath("ULibDir", do_get_file("Library/"));
  await setupHistoryFile();
  registerCleanupFunction(async () => {
    await PlacesUtils.history.clear();
    removeHistoryFile();
  });
});

add_task(async function testHistoryImport() {
  await PlacesUtils.history.clear();

  let migrator = await MigrationUtils.getMigrator("safari");
  await promiseMigration(migrator, MigrationUtils.resourceTypes.HISTORY);

  for (let urlInfo of TEST_URLS) {
    let entry = await PlacesUtils.history.fetch(urlInfo.url, {
      includeVisits: true,
    });

    Assert.equal(entry.url, urlInfo.url, "Should have the correct URL");
    Assert.equal(entry.title, urlInfo.title, "Should have the correct title");
    Assert.equal(
      entry.visits.length,
      urlInfo.visits,
      "Should have the correct number of visits"
    );
    Assert.equal(
      entry.visits[0].date.getTime(),
      urlInfo.jsTime,
      "Should have the correct date"
    );
  }
});
