"use strict";

// These match what we add to IE via InsertIEHistory.exe.
const TEST_ENTRIES = [
  {
    url: "http://www.mozilla.org/1",
    title: "Mozilla HTTP Test",
  },
  {
    url: "https://www.mozilla.org/2",
    // Test character encoding with a fox emoji:
    title: "Mozilla HTTPS Test 🦊",
  },
];

function insertIEHistory() {
  let file = do_get_file("InsertIEHistory.exe", false);
  let process = Cc["@mozilla.org/process/util;1"].createInstance(Ci.nsIProcess);
  process.init(file);

  let args = [];
  process.run(true, args, args.length);

  Assert.ok(!process.isRunning, "Should be done running");
  Assert.equal(process.exitValue, 0, "Check exit code");
}

add_task(async function setup() {
  await PlacesUtils.history.clear();

  insertIEHistory();

  registerCleanupFunction(async () => {
    await PlacesUtils.history.clear();
  });
});

add_task(async function test_IE_history() {
  let migrator = await MigrationUtils.getMigrator("ie");
  Assert.ok(await migrator.isSourceAvailable(), "Source is available");

  await promiseMigration(migrator, MigrationUtils.resourceTypes.HISTORY);

  for (let { url, title } of TEST_ENTRIES) {
    let entry = await PlacesUtils.history.fetch(url, { includeVisits: true });
    Assert.equal(entry.url, url, "Should have the correct URL");
    Assert.equal(entry.title, title, "Should have the correct title");
    Assert.greater(entry.visits.length, 0, "Should have some visits");
  }
});
