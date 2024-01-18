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
const { SCORE_INCREMENT_XLARGE } = ChromeUtils.importESModule(
  "resource://services-sync/constants.sys.mjs"
);

const { sanitizeStorageObject, AutofillRecord, AddressesEngine } =
  ChromeUtils.importESModule("resource://autofill/FormAutofillSync.sys.mjs");

Services.prefs.setCharPref("extensions.formautofill.loglevel", "Trace");
initTestLogging("Trace");

const TEST_STORE_FILE_NAME = "test-profile.json";

const TEST_PROFILE_1 = {
  name: "Timothy John Berners-Lee",
  organization: "World Wide Web Consortium",
  "street-address": "32 Vassar Street\nMIT Room 32-G524",
  "address-level2": "Cambridge",
  "address-level1": "MA",
  "postal-code": "02139",
  country: "US",
  tel: "+16172535702",
  email: "timbl@w3.org",
  // A field this client doesn't "understand" from another client
  "unknown-1": "some unknown data from another client",
};

const TEST_PROFILE_2 = {
  "street-address": "Some Address",
  country: "US",
};

async function expectLocalProfiles(profileStorage, expected) {
  let profiles = await profileStorage.addresses.getAll({
    rawData: true,
    includeDeleted: true,
  });
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

async function setup() {
  let profileStorage = await initProfileStorage(TEST_STORE_FILE_NAME);
  // should always start with no profiles.
  Assert.equal(
    (await profileStorage.addresses.getAll({ includeDeleted: true })).length,
    0
  );

  Services.prefs.setCharPref(
    "services.sync.log.logger.engine.addresses",
    "Trace"
  );
  let engine = new AddressesEngine(Service);
  await engine.initialize();
  // Avoid accidental automatic sync due to our own changes
  Service.scheduler.syncThreshold = 10000000;
  let syncID = await engine.resetLocalSyncID();
  let server = serverForUsers(
    { foo: "password" },
    {
      meta: {
        global: { engines: { addresses: { version: engine.version, syncID } } },
      },
      addresses: {},
    }
  );

  Service.engineManager._engines.addresses = engine;
  engine.enabled = true;
  engine._store._storage = profileStorage.addresses;

  generateNewKeys(Service.collectionKeys);

  await SyncTestingInfrastructure(server);

  let collection = server.user("foo").collection("addresses");

  return { profileStorage, server, collection, engine };
}

async function cleanup(server) {
  let promiseStartOver = promiseOneObserver("weave:service:start-over:finish");
  await Service.startOver();
  await promiseStartOver;
  await promiseStopServer(server);
}

add_task(async function test_log_sanitization() {
  let sanitized = sanitizeStorageObject(TEST_PROFILE_1);
  // all strings have been mangled.
  for (let key of Object.keys(TEST_PROFILE_1)) {
    let val = TEST_PROFILE_1[key];
    if (typeof val == "string") {
      notEqual(sanitized[key], val);
    }
  }
  // And check that stringifying a sync record is sanitized.
  let record = new AutofillRecord("collection", "some-id");
  record.entry = TEST_PROFILE_1;
  let serialized = record.toString();
  // None of the string values should appear in the output.
  for (let key of Object.keys(TEST_PROFILE_1)) {
    let val = TEST_PROFILE_1[key];
    if (typeof val == "string") {
      ok(!serialized.includes(val), `"${val}" shouldn't be in: ${serialized}`);
    }
  }
});

add_task(async function test_outgoing() {
  let { profileStorage, server, collection, engine } = await setup();
  try {
    equal(engine._tracker.score, 0);
    let existingGUID = await profileStorage.addresses.add(TEST_PROFILE_1);
    // And a deleted item.
    let deletedGUID = profileStorage.addresses._generateGUID();
    await profileStorage.addresses.add({ guid: deletedGUID, deleted: true });

    await expectLocalProfiles(profileStorage, [
      {
        guid: existingGUID,
      },
      {
        guid: deletedGUID,
        deleted: true,
      },
    ]);

    await engine._tracker.asyncObserver.promiseObserversComplete();
    // The tracker should have a score recorded for the 2 additions we had.
    equal(engine._tracker.score, SCORE_INCREMENT_XLARGE * 2);

    await engine.setLastSync(0);
    await engine.sync();

    Assert.equal(collection.count(), 2);
    Assert.ok(collection.wbo(existingGUID));
    Assert.ok(collection.wbo(deletedGUID));

    await expectLocalProfiles(profileStorage, [
      {
        guid: existingGUID,
      },
      {
        guid: deletedGUID,
        deleted: true,
      },
    ]);

    strictEqual(
      getSyncChangeCounter(profileStorage.addresses, existingGUID),
      0
    );
    strictEqual(getSyncChangeCounter(profileStorage.addresses, deletedGUID), 0);
  } finally {
    await cleanup(server);
  }
});

add_task(async function test_incoming_new() {
  let { profileStorage, server, engine } = await setup();
  try {
    let profileID = Utils.makeGUID();
    let deletedID = Utils.makeGUID();

    server.insertWBO(
      "foo",
      "addresses",
      new ServerWBO(
        profileID,
        encryptPayload({
          id: profileID,
          entry: Object.assign(
            {
              version: 1,
            },
            TEST_PROFILE_1
          ),
        }),
        getDateForSync()
      )
    );
    server.insertWBO(
      "foo",
      "addresses",
      new ServerWBO(
        deletedID,
        encryptPayload({
          id: deletedID,
          deleted: true,
        }),
        getDateForSync()
      )
    );

    // The tracker should start with no score.
    equal(engine._tracker.score, 0);

    await engine.setLastSync(0);
    await engine.sync();

    await expectLocalProfiles(profileStorage, [
      {
        guid: profileID,
      },
      {
        guid: deletedID,
        deleted: true,
      },
    ]);

    strictEqual(getSyncChangeCounter(profileStorage.addresses, profileID), 0);
    strictEqual(getSyncChangeCounter(profileStorage.addresses, deletedID), 0);

    // Validate incoming records with unknown fields get stored
    let localRecord = await profileStorage.addresses.get(profileID);
    equal(localRecord["unknown-1"], TEST_PROFILE_1["unknown-1"]);

    // The sync applied new records - ensure our tracker knew it came from
    // sync and didn't bump the score.
    equal(engine._tracker.score, 0);
  } finally {
    await cleanup(server);
  }
});

add_task(async function test_incoming_existing() {
  let { profileStorage, server, engine } = await setup();
  try {
    let guid1 = await profileStorage.addresses.add(TEST_PROFILE_1);
    let guid2 = await profileStorage.addresses.add(TEST_PROFILE_2);

    // an initial sync so we don't think they are locally modified.
    await engine.setLastSync(0);
    await engine.sync();

    // now server records that modify the existing items.
    let modifiedEntry1 = Object.assign({}, TEST_PROFILE_1, {
      version: 1,
      name: "NewName",
    });

    let lastSync = await engine.getLastSync();
    server.insertWBO(
      "foo",
      "addresses",
      new ServerWBO(
        guid1,
        encryptPayload({
          id: guid1,
          entry: modifiedEntry1,
        }),
        lastSync + 10
      )
    );
    server.insertWBO(
      "foo",
      "addresses",
      new ServerWBO(
        guid2,
        encryptPayload({
          id: guid2,
          deleted: true,
        }),
        lastSync + 10
      )
    );

    await engine.sync();

    await expectLocalProfiles(profileStorage, [
      Object.assign({}, modifiedEntry1, { guid: guid1 }),
      { guid: guid2, deleted: true },
    ]);
  } finally {
    await cleanup(server);
  }
});

add_task(async function test_tombstones() {
  let { profileStorage, server, collection, engine } = await setup();
  try {
    let existingGUID = await profileStorage.addresses.add(TEST_PROFILE_1);

    await engine.setLastSync(0);
    await engine.sync();

    Assert.equal(collection.count(), 1);
    let payload = collection.payloads()[0];
    equal(payload.id, existingGUID);
    equal(payload.deleted, undefined);

    profileStorage.addresses.remove(existingGUID);
    await engine.sync();

    // should still exist, but now be a tombstone.
    Assert.equal(collection.count(), 1);
    payload = collection.payloads()[0];
    equal(payload.id, existingGUID);
    equal(payload.deleted, true);
  } finally {
    await cleanup(server);
  }
});

add_task(async function test_applyIncoming_both_deleted() {
  let { profileStorage, server, engine } = await setup();
  try {
    let guid = await profileStorage.addresses.add(TEST_PROFILE_1);

    await engine.setLastSync(0);
    await engine.sync();

    // Delete synced record locally.
    profileStorage.addresses.remove(guid);

    // Delete same record remotely.
    let lastSync = await engine.getLastSync();
    let collection = server.user("foo").collection("addresses");
    collection.insert(
      guid,
      encryptPayload({
        id: guid,
        deleted: true,
      }),
      lastSync + 10
    );

    await engine.sync();

    ok(
      !(await await profileStorage.addresses.get(guid)),
      "Should not return record for locally deleted item"
    );

    let localRecords = await profileStorage.addresses.getAll({
      includeDeleted: true,
    });
    equal(localRecords.length, 1, "Only tombstone should exist locally");

    equal(collection.count(), 1, "Only tombstone should exist on server");
  } finally {
    await cleanup(server);
  }
});

add_task(async function test_applyIncoming_nonexistent_tombstone() {
  let { profileStorage, server, engine } = await setup();
  try {
    let guid = profileStorage.addresses._generateGUID();
    let collection = server.user("foo").collection("addresses");
    collection.insert(
      guid,
      encryptPayload({
        id: guid,
        deleted: true,
      }),
      getDateForSync()
    );

    await engine.setLastSync(0);
    await engine.sync();

    ok(
      !(await profileStorage.addresses.get(guid)),
      "Should not return record for uknown deleted item"
    );
    let localTombstone = (
      await profileStorage.addresses.getAll({
        includeDeleted: true,
      })
    ).find(record => record.guid == guid);
    ok(localTombstone, "Should store tombstone for unknown item");
  } finally {
    await cleanup(server);
  }
});

add_task(async function test_applyIncoming_incoming_deleted() {
  let { profileStorage, server, engine } = await setup();
  try {
    let guid = await profileStorage.addresses.add(TEST_PROFILE_1);

    await engine.setLastSync(0);
    await engine.sync();

    // Delete the record remotely.
    let lastSync = await engine.getLastSync();
    let collection = server.user("foo").collection("addresses");
    collection.insert(
      guid,
      encryptPayload({
        id: guid,
        deleted: true,
      }),
      lastSync + 10
    );

    await engine.sync();

    ok(
      !(await profileStorage.addresses.get(guid)),
      "Should delete unmodified item locally"
    );

    let localTombstone = (
      await profileStorage.addresses.getAll({
        includeDeleted: true,
      })
    ).find(record => record.guid == guid);
    ok(localTombstone, "Should keep local tombstone for remotely deleted item");
    strictEqual(
      getSyncChangeCounter(profileStorage.addresses, guid),
      0,
      "Local tombstone should be marked as syncing"
    );
  } finally {
    await cleanup(server);
  }
});

add_task(async function test_applyIncoming_incoming_restored() {
  let { profileStorage, server, engine } = await setup();
  try {
    let guid = await profileStorage.addresses.add(TEST_PROFILE_1);

    // Upload the record to the server.
    await engine.setLastSync(0);
    await engine.sync();

    // Removing a synced record should write a tombstone.
    profileStorage.addresses.remove(guid);

    // Modify the deleted record remotely.
    let collection = server.user("foo").collection("addresses");
    let serverPayload = JSON.parse(
      JSON.parse(collection.payload(guid)).ciphertext
    );
    serverPayload.entry["street-address"] = "I moved!";
    let lastSync = await engine.getLastSync();
    collection.insert(guid, encryptPayload(serverPayload), lastSync + 10);

    // Sync again.
    await engine.sync();

    // We should replace our tombstone with the server's version.
    let localRecord = await profileStorage.addresses.get(guid);
    ok(
      objectMatches(localRecord, {
        name: "Timothy John Berners-Lee",
        "street-address": "I moved!",
      })
    );

    let maybeNewServerPayload = JSON.parse(
      JSON.parse(collection.payload(guid)).ciphertext
    );
    deepEqual(
      maybeNewServerPayload,
      serverPayload,
      "Should not change record on server"
    );
  } finally {
    await cleanup(server);
  }
});

add_task(async function test_applyIncoming_outgoing_restored() {
  let { profileStorage, server, engine } = await setup();
  try {
    let guid = await profileStorage.addresses.add(TEST_PROFILE_1);

    // Upload the record to the server.
    await engine.setLastSync(0);
    await engine.sync();

    // Modify the local record.
    let localCopy = Object.assign({}, TEST_PROFILE_1);
    localCopy["street-address"] = "I moved!";
    await profileStorage.addresses.update(guid, localCopy);

    // Replace the record with a tombstone on the server.
    let lastSync = await engine.getLastSync();
    let collection = server.user("foo").collection("addresses");
    collection.insert(
      guid,
      encryptPayload({
        id: guid,
        deleted: true,
      }),
      lastSync + 10
    );

    // Sync again.
    await engine.sync();

    // We should resurrect the record on the server.
    let serverPayload = JSON.parse(
      JSON.parse(collection.payload(guid)).ciphertext
    );
    ok(!serverPayload.deleted, "Should resurrect record on server");
    ok(
      objectMatches(serverPayload.entry, {
        name: "Timothy John Berners-Lee",
        "street-address": "I moved!",
        // resurrection also beings back any unknown fields we had
        "unknown-1": "some unknown data from another client",
      })
    );

    let localRecord = await profileStorage.addresses.get(guid);
    ok(localRecord, "Modified record should not be deleted locally");
  } finally {
    await cleanup(server);
  }
});

// Unlike most sync engines, we want "both modified" to inspect the records,
// and if materially different, create a duplicate.
add_task(async function test_reconcile_both_modified_identical() {
  let { profileStorage, server, engine } = await setup();
  try {
    // create a record locally.
    let guid = await profileStorage.addresses.add(TEST_PROFILE_1);

    // and an identical record on the server.
    server.insertWBO(
      "foo",
      "addresses",
      new ServerWBO(
        guid,
        encryptPayload({
          id: guid,
          entry: TEST_PROFILE_1,
        }),
        getDateForSync()
      )
    );

    await engine.setLastSync(0);
    await engine.sync();

    await expectLocalProfiles(profileStorage, [{ guid }]);
  } finally {
    await cleanup(server);
  }
});

add_task(async function test_incoming_dupes() {
  let { profileStorage, server, engine } = await setup();
  try {
    // Create a profile locally, then sync to upload the new profile to the
    // server.
    let guid1 = await profileStorage.addresses.add(TEST_PROFILE_1);

    await engine.setLastSync(0);
    await engine.sync();

    // Create another profile locally, but don't sync it yet.
    await profileStorage.addresses.add(TEST_PROFILE_2);

    // Now create two records on the server with the same contents as our local
    // profiles, but different GUIDs.
    let lastSync = await engine.getLastSync();
    let guid1_dupe = Utils.makeGUID();
    server.insertWBO(
      "foo",
      "addresses",
      new ServerWBO(
        guid1_dupe,
        encryptPayload({
          id: guid1_dupe,
          entry: Object.assign(
            {
              version: 1,
            },
            TEST_PROFILE_1
          ),
        }),
        lastSync + 10
      )
    );
    let guid2_dupe = Utils.makeGUID();
    server.insertWBO(
      "foo",
      "addresses",
      new ServerWBO(
        guid2_dupe,
        encryptPayload({
          id: guid2_dupe,
          entry: Object.assign(
            {
              version: 1,
            },
            TEST_PROFILE_2
          ),
        }),
        lastSync + 10
      )
    );

    // Sync again. We should download `guid1_dupe` and `guid2_dupe`, then
    // reconcile changes.
    await engine.sync();

    await expectLocalProfiles(profileStorage, [
      // We uploaded `guid1` during the first sync. Even though its contents
      // are the same as `guid1_dupe`, we keep both.
      Object.assign({}, TEST_PROFILE_1, { guid: guid1 }),
      Object.assign({}, TEST_PROFILE_1, { guid: guid1_dupe }),
      // However, we didn't upload `guid2` before downloading `guid2_dupe`, so
      // we *should* dedupe `guid2` to `guid2_dupe`.
      Object.assign({}, TEST_PROFILE_2, { guid: guid2_dupe }),
    ]);
  } finally {
    await cleanup(server);
  }
});

add_task(async function test_dedupe_identical_unsynced() {
  let { profileStorage, server, engine } = await setup();
  try {
    // create a record locally.
    let localGuid = await profileStorage.addresses.add(TEST_PROFILE_1);

    // and an identical record on the server but different GUID.
    let remoteGuid = Utils.makeGUID();
    notEqual(localGuid, remoteGuid);
    server.insertWBO(
      "foo",
      "addresses",
      new ServerWBO(
        remoteGuid,
        encryptPayload({
          id: remoteGuid,
          entry: Object.assign(
            {
              version: 1,
            },
            TEST_PROFILE_1
          ),
        }),
        getDateForSync()
      )
    );

    await engine.setLastSync(0);
    await engine.sync();

    // Should have 1 item locally with GUID changed to the remote one.
    // There's no tombstone as the original was unsynced.
    await expectLocalProfiles(profileStorage, [
      {
        guid: remoteGuid,
      },
    ]);
  } finally {
    await cleanup(server);
  }
});

add_task(async function test_dedupe_identical_synced() {
  let { profileStorage, server, engine } = await setup();
  try {
    // create a record locally.
    let localGuid = await profileStorage.addresses.add(TEST_PROFILE_1);

    // sync it - it will no longer be a candidate for de-duping.
    await engine.setLastSync(0);
    await engine.sync();

    // and an identical record on the server but different GUID.
    let lastSync = await engine.getLastSync();
    let remoteGuid = Utils.makeGUID();
    server.insertWBO(
      "foo",
      "addresses",
      new ServerWBO(
        remoteGuid,
        encryptPayload({
          id: remoteGuid,
          entry: Object.assign(
            {
              version: 1,
            },
            TEST_PROFILE_1
          ),
        }),
        lastSync + 10
      )
    );

    await engine.sync();

    // Should have 2 items locally, since the first was synced.
    await expectLocalProfiles(profileStorage, [
      { guid: localGuid },
      { guid: remoteGuid },
    ]);
  } finally {
    await cleanup(server);
  }
});

add_task(async function test_dedupe_multiple_candidates() {
  let { profileStorage, server, engine } = await setup();
  try {
    // It's possible to have duplicate local profiles, with the same fields but
    // different GUIDs. After a node reassignment, or after disconnecting and
    // reconnecting to Sync, we might dedupe a local record A to a remote record
    // B, if we see B before we download and apply A. Since A and B are dupes,
    // that's OK. We'll write a tombstone for A when we dedupe A to B, and
    // overwrite that tombstone when we see A.

    let localRecord = {
      name: "Mark Hammond",
      organization: "Mozilla",
      country: "AU",
      tel: "+12345678910",
    };
    let serverRecord = Object.assign(
      {
        version: 1,
      },
      localRecord
    );

    // We don't pass `sourceSync` so that the records are marked as NEW.
    let aGuid = await profileStorage.addresses.add(localRecord);
    let bGuid = await profileStorage.addresses.add(localRecord);

    // Insert B before A.
    server.insertWBO(
      "foo",
      "addresses",
      new ServerWBO(
        bGuid,
        encryptPayload({
          id: bGuid,
          entry: serverRecord,
        }),
        getDateForSync()
      )
    );
    server.insertWBO(
      "foo",
      "addresses",
      new ServerWBO(
        aGuid,
        encryptPayload({
          id: aGuid,
          entry: serverRecord,
        }),
        getDateForSync()
      )
    );

    await engine.setLastSync(0);
    await engine.sync();

    await expectLocalProfiles(profileStorage, [
      {
        guid: aGuid,
        name: "Mark Hammond",
        organization: "Mozilla",
        country: "AU",
        tel: "+12345678910",
      },
      {
        guid: bGuid,
        name: "Mark Hammond",
        organization: "Mozilla",
        country: "AU",
        tel: "+12345678910",
      },
    ]);
    // Make sure these are both syncing.
    strictEqual(
      getSyncChangeCounter(profileStorage.addresses, aGuid),
      0,
      "A should be marked as syncing"
    );
    strictEqual(
      getSyncChangeCounter(profileStorage.addresses, bGuid),
      0,
      "B should be marked as syncing"
    );
  } finally {
    await cleanup(server);
  }
});

// Unlike most sync engines, we want "both modified" to inspect the records,
// and if materially different, create a duplicate.
add_task(async function test_reconcile_both_modified_conflict() {
  let { profileStorage, server, engine } = await setup();
  try {
    // create a record locally.
    let guid = await profileStorage.addresses.add(TEST_PROFILE_1);

    // Upload the record to the server.
    await engine.setLastSync(0);
    await engine.sync();

    strictEqual(
      getSyncChangeCounter(profileStorage.addresses, guid),
      0,
      "Original record should be marked as syncing"
    );

    // Change the same field locally and on the server.
    let localCopy = Object.assign({}, TEST_PROFILE_1);
    localCopy["street-address"] = "I moved!";
    await profileStorage.addresses.update(guid, localCopy);

    let lastSync = await engine.getLastSync();
    let collection = server.user("foo").collection("addresses");
    let serverPayload = JSON.parse(
      JSON.parse(collection.payload(guid)).ciphertext
    );
    serverPayload.entry["street-address"] = "I moved, too!";
    collection.insert(guid, encryptPayload(serverPayload), lastSync + 10);

    // Sync again.
    await engine.sync();

    // Since we wait to pull changes until we're ready to upload, both records
    // should now exist on the server; we don't need a follow-up sync.
    let serverPayloads = collection.payloads();
    equal(serverPayloads.length, 2, "Both records should exist on server");

    let forkedPayload = serverPayloads.find(payload => payload.id != guid);
    ok(forkedPayload, "Forked record should exist on server");

    await expectLocalProfiles(profileStorage, [
      {
        guid,
        name: "Timothy John Berners-Lee",
        "street-address": "I moved, too!",
      },
      {
        guid: forkedPayload.id,
        name: "Timothy John Berners-Lee",
        "street-address": "I moved!",
      },
    ]);

    let changeCounter = getSyncChangeCounter(
      profileStorage.addresses,
      forkedPayload.id
    );
    strictEqual(changeCounter, 0, "Forked record should be marked as syncing");
  } finally {
    await cleanup(server);
  }
});

add_task(async function test_wipe() {
  let { profileStorage, server, engine } = await setup();
  try {
    let guid = await profileStorage.addresses.add(TEST_PROFILE_1);

    await expectLocalProfiles(profileStorage, [{ guid }]);

    let promiseObserved = promiseOneObserver("formautofill-storage-changed");

    await engine._wipeClient();

    let { subject, data } = await promiseObserved;
    Assert.equal(
      subject.wrappedJSObject.sourceSync,
      true,
      "it should be noted this came from sync"
    );
    Assert.equal(
      subject.wrappedJSObject.collectionName,
      "addresses",
      "got the correct collection"
    );
    Assert.equal(data, "removeAll", "a removeAll should be noted");

    await expectLocalProfiles(profileStorage, []);
  } finally {
    await cleanup(server);
  }
});

// Other clients might have data that we aren't able to process/understand yet
// We should keep that data and ensure when we sync we don't lose that data
add_task(async function test_full_roundtrip_unknown_data() {
  let { profileStorage, server, engine } = await setup();
  try {
    let profileID = Utils.makeGUID();

    info("Incoming records with unknown fields are properly stored");
    // Insert a record onto the server
    server.insertWBO(
      "foo",
      "addresses",
      new ServerWBO(
        profileID,
        encryptPayload({
          id: profileID,
          entry: Object.assign(
            {
              version: 1,
            },
            TEST_PROFILE_1
          ),
        }),
        getDateForSync()
      )
    );

    // The tracker should start with no score.
    equal(engine._tracker.score, 0);

    await engine.setLastSync(0);
    await engine.sync();

    await expectLocalProfiles(profileStorage, [
      {
        guid: profileID,
      },
    ]);

    strictEqual(getSyncChangeCounter(profileStorage.addresses, profileID), 0);

    // The sync applied new records - ensure our tracker knew it came from
    // sync and didn't bump the score.
    equal(engine._tracker.score, 0);

    // Validate incoming records with unknown fields are correctly stored
    let localRecord = await profileStorage.addresses.get(profileID);
    equal(localRecord["unknown-1"], TEST_PROFILE_1["unknown-1"]);

    let onChanged = TestUtils.topicObserved(
      "formautofill-storage-changed",
      (subject, data) => data == "update"
    );

    // Validate we can update the records locally and not drop any unknown fields
    info("Unknown fields are sent back up to the server");

    // Modify the local copy
    let localCopy = Object.assign({}, TEST_PROFILE_1);
    localCopy["street-address"] = "I moved!";
    await profileStorage.addresses.update(profileID, localCopy);
    await onChanged;
    await profileStorage._saveImmediately();

    let updatedCopy = await profileStorage.addresses.get(profileID);
    equal(updatedCopy["street-address"], "I moved!");

    // Sync our changes to the server
    await engine.setLastSync(0);
    await engine.sync();

    let collection = server.user("foo").collection("addresses");

    Assert.ok(collection.wbo(profileID));
    let serverPayload = JSON.parse(
      JSON.parse(collection.payload(profileID)).ciphertext
    );

    // The server has the updated field as well as any unknown fields
    equal(
      serverPayload.entry["unknown-1"],
      "some unknown data from another client"
    );
    equal(serverPayload.entry["street-address"], "I moved!");
  } finally {
    await cleanup(server);
  }
});
