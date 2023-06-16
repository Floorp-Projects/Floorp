/**
 * Tests sync functionality.
 */

/* import-globals-from ../../../../../services/sync/tests/unit/head_appinfo.js */
/* import-globals-from ../../../../../services/common/tests/unit/head_helpers.js */
/* import-globals-from ../../../../../services/sync/tests/unit/head_helpers.js */
/* import-globals-from ../../../../../services/sync/tests/unit/head_http_server.js */

"use strict";

const { Service } = ChromeUtils.importESModule(
  "resource://services-sync/service.sys.mjs"
);

const { CreditCardsEngine } = ChromeUtils.importESModule(
  "resource://autofill/FormAutofillSync.sys.mjs"
);

Services.prefs.setCharPref("extensions.formautofill.loglevel", "Trace");
initTestLogging("Trace");

const TEST_STORE_FILE_NAME = "test-profile.json";

const TEST_CREDIT_CARD_1 = {
  guid: "86d961c7717a",
  "cc-name": "John Doe",
  "cc-number": "4111111111111111",
  "cc-exp-month": 4,
  "cc-exp-year": new Date().getFullYear(),
};

const TEST_CREDIT_CARD_2 = {
  guid: "cf57a7ac3539",
  "cc-name": "Timothy Berners-Lee",
  "cc-number": "4929001587121045",
  "cc-exp-month": 12,
  "cc-exp-year": new Date().getFullYear() + 10,
};

function expectProfiles(profiles, expected) {
  expected.sort((a, b) => a.guid.localeCompare(b.guid));
  profiles.sort((a, b) => a.guid.localeCompare(b.guid));
  try {
    deepEqual(
      profiles.map(p => p.guid),
      expected.map(p => p.guid)
    );
    for (let i = 0; i < expected.length; i++) {
      let thisExpected = expected[i];
      let thisGot = profiles[i];
      // always check "deleted".
      equal(thisExpected.deleted, thisGot.deleted);
      ok(objectMatches(thisGot, thisExpected));
    }
  } catch (ex) {
    info("Comparing expected profiles:");
    info(JSON.stringify(expected, undefined, 2));
    info("against actual profiles:");
    info(JSON.stringify(profiles, undefined, 2));
    throw ex;
  }
}

async function expectServerProfiles(collection, expected) {
  const profiles = collection
    .payloads()
    .map(payload => Object.assign({ guid: payload.id }, payload.entry));
  expectProfiles(profiles, expected);
}

async function expectLocalProfiles(profileStorage, expected) {
  const profiles = await profileStorage.creditCards.getAll({
    rawData: true,
    includeDeleted: true,
  });
  expectProfiles(profiles, expected);
}

async function setup() {
  let profileStorage = await initProfileStorage(TEST_STORE_FILE_NAME);
  // should always start with no profiles.
  Assert.equal(
    (await profileStorage.creditCards.getAll({ includeDeleted: true })).length,
    0
  );

  Services.prefs.setCharPref(
    "services.sync.log.logger.engine.CreditCards",
    "Trace"
  );
  let engine = new CreditCardsEngine(Service);
  await engine.initialize();
  // Avoid accidental automatic sync due to our own changes
  Service.scheduler.syncThreshold = 10000000;
  let syncID = await engine.resetLocalSyncID();
  let server = serverForUsers(
    { foo: "password" },
    {
      meta: {
        global: {
          engines: { creditcards: { version: engine.version, syncID } },
        },
      },
      creditcards: {},
    }
  );

  Service.engineManager._engines.creditcards = engine;
  engine.enabled = true;
  engine._store._storage = profileStorage.creditCards;

  generateNewKeys(Service.collectionKeys);

  await SyncTestingInfrastructure(server);

  let collection = server.user("foo").collection("creditcards");

  return { profileStorage, server, collection, engine };
}

async function cleanup(server) {
  let promiseStartOver = promiseOneObserver("weave:service:start-over:finish");
  await Service.startOver();
  await promiseStartOver;
  await promiseStopServer(server);
}

function getTestRecords(profileStorage, version) {
  return [
    Object.assign({ version }, TEST_CREDIT_CARD_1),
    Object.assign({ version }, TEST_CREDIT_CARD_2),
  ];
}

function setupServerRecords(server, records) {
  for (const record of records) {
    server.insertWBO(
      "foo",
      "creditcards",
      new ServerWBO(
        record.guid,
        encryptPayload({
          id: record.guid,
          entry: Object.assign({}, record),
        }),
        getDateForSync()
      )
    );
  }
}

/**
 * We want to setup old records and run init() to migrate records.
 * However, We don't have an easy way to setup an older version record with
 * init() function now.
 * So as a workaround, we simulate the behavior by directly setting data and then
 * run migration.
 */
async function setupLocalProfilesAndRunMigration(profileStorage, records) {
  for (const record of records) {
    profileStorage._store.data.creditCards.push(Object.assign({}, record));
  }
  await Promise.all(
    profileStorage.creditCards._data.map(async (record, index) =>
      profileStorage.creditCards._migrateRecord(record, index)
    )
  );
}

// local v3, server v4
add_task(async function test_local_v3_server_v4() {
  let { collection, profileStorage, server, engine } = await setup();

  const V3_RECORDS = getTestRecords(profileStorage, 3);
  const V4_RECORDS = getTestRecords(profileStorage, 4);

  await setupLocalProfilesAndRunMigration(profileStorage, V3_RECORDS);
  setupServerRecords(server, V4_RECORDS);

  await engine.setLastSync(0);
  await engine.sync();

  await expectServerProfiles(collection, V3_RECORDS);
  await expectLocalProfiles(profileStorage, V3_RECORDS);

  await cleanup(server);
});

// local v4, server empty
add_task(async function test_local_v4_server_empty() {
  let { collection, profileStorage, server, engine } = await setup();
  const V3_RECORDS = getTestRecords(profileStorage, 3);
  const V4_RECORDS = getTestRecords(profileStorage, 4);

  await setupLocalProfilesAndRunMigration(profileStorage, V4_RECORDS);

  await engine.setLastSync(0);
  await engine.sync();

  await expectServerProfiles(collection, V3_RECORDS);
  await expectLocalProfiles(profileStorage, V3_RECORDS);

  await cleanup(server);
});

// local v4, server v3
add_task(async function test_local_v4_server_v3() {
  let { collection, profileStorage, server, engine } = await setup();
  const V3_RECORDS = getTestRecords(profileStorage, 3);
  const V4_RECORDS = getTestRecords(profileStorage, 4);

  await setupLocalProfilesAndRunMigration(profileStorage, V4_RECORDS);
  setupServerRecords(server, V3_RECORDS);

  // local should be v3 before syncing.
  await expectLocalProfiles(profileStorage, V3_RECORDS);

  await engine.setLastSync(0);
  await engine.sync();

  await expectServerProfiles(collection, V3_RECORDS);
  await expectLocalProfiles(profileStorage, V3_RECORDS);

  await cleanup(server);
});

// local v4, server v4
add_task(async function test_local_v4_server_v4() {
  let { collection, profileStorage, server, engine } = await setup();
  const V3_RECORDS = getTestRecords(profileStorage, 3);
  const V4_RECORDS = getTestRecords(profileStorage, 4);

  await setupLocalProfilesAndRunMigration(profileStorage, V4_RECORDS);
  setupServerRecords(server, V4_RECORDS);

  // local should be v3 before syncing and then we ignore
  // incoming v4 from server
  await expectLocalProfiles(profileStorage, V3_RECORDS);

  await engine.setLastSync(0);
  await engine.sync();

  await expectServerProfiles(collection, V3_RECORDS);
  await expectLocalProfiles(profileStorage, V3_RECORDS);

  await cleanup(server);
});
