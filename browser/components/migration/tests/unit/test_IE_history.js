"use strict";

const { MockRegistrar } = ChromeUtils.importESModule(
  "resource://testing-common/MockRegistrar.sys.mjs"
);

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
    Assert.ok(!!entry.visits.length, "Should have some visits");
  }

  await PlacesUtils.history.clear();
});

/**
 * Tests that history visits from IE that have a visit date older than
 * maxAgeInDays days do not get imported.
 */
add_task(async function test_IE_history_past_max_days() {
  // The InsertIEHistory program inserts two history visits using the MS COM
  // IUrlHistoryStg interface. That interface does not allow us to dictate
  // the visit times of those history visits. Thankfully, we can temporarily
  // mock out the @mozilla.org/profile/migrator/iehistoryenumerator;1 to return
  // some entries that we expect to expire.

  /**
   * An implmentation of nsISimpleEnumerator that wraps a JavaScript Array.
   */
  class nsSimpleEnumerator {
    #items;
    #nextIndex;

    constructor(items) {
      this.#items = items;
      this.#nextIndex = 0;
    }

    hasMoreElements() {
      return this.#nextIndex < this.#items.length;
    }

    getNext() {
      if (!this.hasMoreElements()) {
        throw Components.Exception("", Cr.NS_ERROR_NOT_AVAILABLE);
      }

      return this.#items[this.#nextIndex++];
    }

    [Symbol.iterator]() {
      return this.#items.values();
    }

    QueryInterface = ChromeUtils.generateQI(["nsISimpleEnumerator"]);
  }

  Assert.ok(
    MigrationUtils.HISTORY_MAX_AGE_IN_DAYS < 300,
    "This test expects the current pref to be less than the youngest expired visit."
  );
  Assert.ok(
    MigrationUtils.HISTORY_MAX_AGE_IN_DAYS > 160,
    "This test expects the current pref to be greater than the oldest unexpired visit."
  );

  const EXPIRED_VISITS = [
    new Map([
      ["uri", Services.io.newURI("https://test1.invalid")],
      ["title", "Test history visit 1"],
      ["time", PRTimeDaysAgo(500)],
    ]),
    new Map([
      ["uri", Services.io.newURI("https://test2.invalid")],
      ["title", "Test history visit 2"],
      ["time", PRTimeDaysAgo(450)],
    ]),
    new Map([
      ["uri", Services.io.newURI("https://test3.invalid")],
      ["title", "Test history visit 3"],
      ["time", PRTimeDaysAgo(300)],
    ]),
  ];

  const UNEXPIRED_VISITS = [
    new Map([
      ["uri", Services.io.newURI("https://test4.invalid")],
      ["title", "Test history visit 4"],
    ]),
    new Map([
      ["uri", Services.io.newURI("https://test5.invalid")],
      ["title", "Test history visit 5"],
      ["time", PRTimeDaysAgo(160)],
    ]),
    new Map([
      ["uri", Services.io.newURI("https://test6.invalid")],
      ["title", "Test history visit 6"],
      ["time", PRTimeDaysAgo(50)],
    ]),
    new Map([
      ["uri", Services.io.newURI("https://test7.invalid")],
      ["title", "Test history visit 7"],
      ["time", PRTimeDaysAgo(0)],
    ]),
  ];

  let fakeIEHistoryEnumerator = MockRegistrar.register(
    "@mozilla.org/profile/migrator/iehistoryenumerator;1",
    new nsSimpleEnumerator([...EXPIRED_VISITS, ...UNEXPIRED_VISITS])
  );
  registerCleanupFunction(() => {
    MockRegistrar.unregister(fakeIEHistoryEnumerator);
  });

  let migrator = await MigrationUtils.getMigrator("ie");
  Assert.ok(await migrator.isSourceAvailable(), "Source is available");

  await promiseMigration(migrator, MigrationUtils.resourceTypes.HISTORY);

  for (let visit of EXPIRED_VISITS) {
    let entry = await PlacesUtils.history.fetch(visit.get("uri").spec, {
      includeVisits: true,
    });
    Assert.equal(entry, null, "Should not have found an entry.");
  }

  for (let visit of UNEXPIRED_VISITS) {
    let entry = await PlacesUtils.history.fetch(visit.get("uri"), {
      includeVisits: true,
    });
    Assert.equal(
      entry.url,
      visit.get("uri").spec,
      "Should have the correct URL"
    );
    Assert.equal(
      entry.title,
      visit.get("title"),
      "Should have the correct title"
    );
    Assert.ok(!!entry.visits.length, "Should have some visits");
  }

  await PlacesUtils.history.clear();
});
