/**
 * Tests sync functionality.
 */

/* import-globals-from ../../../../../services/sync/tests/unit/head_appinfo.js */
/* import-globals-from ../../../../../services/common/tests/unit/head_helpers.js */
/* import-globals-from ../../../../../services/sync/tests/unit/head_helpers.js */
/* import-globals-from ../../../../../services/sync/tests/unit/head_http_server.js */

"use strict";

Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://testing-common/services/sync/utils.js");
Cu.import("resource://formautofill/ProfileStorage.jsm");

let {sanitizeStorageObject, AutofillRecord, AddressesEngine} =
  Cu.import("resource://formautofill/FormAutofillSync.jsm", {});


Services.prefs.setCharPref("extensions.formautofill.loglevel", "Trace");
initTestLogging("Trace");

const TEST_PROFILE_1 = {
  "given-name": "Timothy",
  "additional-name": "John",
  "family-name": "Berners-Lee",
  organization: "World Wide Web Consortium",
  "street-address": "32 Vassar Street\nMIT Room 32-G524",
  "address-level2": "Cambridge",
  "address-level1": "MA",
  "postal-code": "02139",
  country: "US",
  tel: "+16172535702",
  email: "timbl@w3.org",
};

const TEST_PROFILE_2 = {
  "street-address": "Some Address",
  country: "US",
};

function expectLocalProfiles(expected) {
  let profiles = profileStorage.addresses.getAll({
    rawData: true,
    includeDeleted: true,
  });
  expected.sort((a, b) => a.guid.localeCompare(b.guid));
  profiles.sort((a, b) => a.guid.localeCompare(b.guid));
  try {
    deepEqual(profiles.map(p => p.guid), expected.map(p => p.guid));
    for (let i = 0; i < expected.length; i++) {
      let thisExpected = expected[i];
      let thisGot = profiles[i];
      // always check "deleted".
      equal(thisExpected.deleted, thisGot.deleted);
      ok(objectMatches(thisGot, thisExpected));
    }
  } catch (ex) {
    do_print("Comparing expected profiles:");
    do_print(JSON.stringify(expected, undefined, 2));
    do_print("against actual profiles:");
    do_print(JSON.stringify(profiles, undefined, 2));
    throw ex;
  }
}

async function setup() {
  await profileStorage.initialize();
  // should always start with no profiles.
  Assert.equal(profileStorage.addresses.getAll({includeDeleted: true}).length, 0);

  Services.prefs.setCharPref("services.sync.log.logger.engine.addresses", "Trace");
  let engine = new AddressesEngine(Service);
  await engine.initialize();
  // Avoid accidental automatic sync due to our own changes
  Service.scheduler.syncThreshold = 10000000;
  let server = serverForUsers({"foo": "password"}, {
    meta: {global: {engines: {addresses: {version: engine.version, syncID: engine.syncID}}}},
    addresses: {},
  });

  Service.engineManager._engines.addresses = engine;
  engine.enabled = true;

  generateNewKeys(Service.collectionKeys);

  await SyncTestingInfrastructure(server);

  let collection = server.user("foo").collection("addresses");

  return {server, collection, engine};
}

async function cleanup(server) {
  let promiseStartOver = promiseOneObserver("weave:service:start-over:finish");
  await Service.startOver();
  await promiseStartOver;
  await promiseStopServer(server);
  profileStorage.addresses._nukeAllRecords();
}

add_task(async function test_log_sanitization() {
  await profileStorage.initialize();
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
  profileStorage.addresses._nukeAllRecords();
});

add_task(async function test_outgoing() {
  let {server, collection, engine} = await setup();
  try {
    equal(engine._tracker.score, 0);
    let existingGUID = profileStorage.addresses.add(TEST_PROFILE_1);
    // And a deleted item.
    let deletedGUID = profileStorage.addresses._generateGUID();
    profileStorage.addresses.add({guid: deletedGUID, deleted: true});

    expectLocalProfiles([
      {
        guid: existingGUID,
      },
      {
        guid: deletedGUID,
        deleted: true,
      },
    ]);

    // The tracker should have a score recorded for the 2 additions we had.
    equal(engine._tracker.score, SCORE_INCREMENT_XLARGE * 2);

    engine.lastSync = 0;
    await engine.sync();

    Assert.equal(collection.count(), 2);
    Assert.ok(collection.wbo(existingGUID));
    Assert.ok(collection.wbo(deletedGUID));

    expectLocalProfiles([
      {
        guid: existingGUID,
      },
      {
        guid: deletedGUID,
        deleted: true,
      },
    ]);

    strictEqual(getSyncChangeCounter(profileStorage.addresses, existingGUID), 0);
    strictEqual(getSyncChangeCounter(profileStorage.addresses, deletedGUID), 0);
  } finally {
    await cleanup(server);
  }
});

add_task(async function test_incoming_new() {
  let {server, engine} = await setup();
  try {
    let profileID = Utils.makeGUID();
    let deletedID = Utils.makeGUID();

    server.insertWBO("foo", "addresses", new ServerWBO(profileID, encryptPayload({
      id: profileID,
      entry: TEST_PROFILE_1,
    }), Date.now() / 1000));
    server.insertWBO("foo", "addresses", new ServerWBO(deletedID, encryptPayload({
      id: deletedID,
      deleted: true,
    }), Date.now() / 1000));

    // The tracker should start with no score.
    equal(engine._tracker.score, 0);

    engine.lastSync = 0;
    await engine.sync();

    expectLocalProfiles([
      {
        guid: profileID,
      }, {
        guid: deletedID,
        deleted: true,
      },
    ]);

    strictEqual(getSyncChangeCounter(profileStorage.addresses, profileID), 0);
    strictEqual(getSyncChangeCounter(profileStorage.addresses, deletedID), 0);

    // The sync applied new records - ensure our tracker knew it came from
    // sync and didn't bump the score.
    equal(engine._tracker.score, 0);
  } finally {
    await cleanup(server);
  }
});

add_task(async function test_tombstones() {
  let {server, collection, engine} = await setup();
  try {
    let existingGUID = profileStorage.addresses.add(TEST_PROFILE_1);

    engine.lastSync = 0;
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

// Unlike most sync engines, we want "both modified" to inspect the records,
// and if materially different, create a duplicate.
add_task(async function test_reconcile_both_modified_identical() {
  let {server, engine} = await setup();
  try {
    // create a record locally.
    let guid = profileStorage.addresses.add(TEST_PROFILE_1);

    // and an identical record on the server.
    server.insertWBO("foo", "addresses", new ServerWBO(guid, encryptPayload({
      id: guid,
      entry: TEST_PROFILE_1,
    }), Date.now() / 1000));

    engine.lastSync = 0;
    await engine.sync();

    expectLocalProfiles([{guid}]);
  } finally {
    await cleanup(server);
  }
});

add_task(async function test_incoming_dupes() {
  let {server, engine} = await setup();
  try {
    // Create a profile locally, then sync to upload the new profile to the
    // server.
    let guid1 = profileStorage.addresses.add(TEST_PROFILE_1);

    engine.lastSync = 0;
    await engine.sync();

    // Create another profile locally, but don't sync it yet.
    profileStorage.addresses.add(TEST_PROFILE_2);

    // Now create two records on the server with the same contents as our local
    // profiles, but different GUIDs.
    let guid1_dupe = Utils.makeGUID();
    server.insertWBO("foo", "addresses", new ServerWBO(guid1_dupe, encryptPayload({
      id: guid1_dupe,
      entry: TEST_PROFILE_1,
    }), engine.lastSync + 10));
    let guid2_dupe = Utils.makeGUID();
    server.insertWBO("foo", "addresses", new ServerWBO(guid2_dupe, encryptPayload({
      id: guid2_dupe,
      entry: TEST_PROFILE_2,
    }), engine.lastSync + 10));

    // Sync again. We should download `guid1_dupe` and `guid2_dupe`, then
    // reconcile changes.
    await engine.sync();

    expectLocalProfiles([
      // We uploaded `guid1` during the first sync. Even though its contents
      // are the same as `guid1_dupe`, we keep both.
      Object.assign({}, TEST_PROFILE_1, {guid: guid1}),
      Object.assign({}, TEST_PROFILE_1, {guid: guid1_dupe}),
      // However, we didn't upload `guid2` before downloading `guid2_dupe`, so
      // we *should* dedupe `guid2` to `guid2_dupe`.
      Object.assign({}, TEST_PROFILE_2, {guid: guid2_dupe}),
    ]);
  } finally {
    await cleanup(server);
  }
});

add_task(async function test_dedupe_identical_unsynced() {
  let {server, engine} = await setup();
  try {
    // create a record locally.
    let localGuid = profileStorage.addresses.add(TEST_PROFILE_1);

    // and an identical record on the server but different GUID.
    let remoteGuid = Utils.makeGUID();
    notEqual(localGuid, remoteGuid);
    server.insertWBO("foo", "addresses", new ServerWBO(remoteGuid, encryptPayload({
      id: remoteGuid,
      entry: TEST_PROFILE_1,
    }), Date.now() / 1000));

    engine.lastSync = 0;
    await engine.sync();

    // Should have 1 item locally with GUID changed to the remote one.
    // There's no tombstone as the original was unsynced.
    expectLocalProfiles([
      {
        guid: remoteGuid,
      },
    ]);
  } finally {
    await cleanup(server);
  }
});

add_task(async function test_dedupe_identical_synced() {
  let {server, engine} = await setup();
  try {
    // create a record locally.
    let localGuid = profileStorage.addresses.add(TEST_PROFILE_1);

    // sync it - it will no longer be a candidate for de-duping.
    engine.lastSync = 0;
    await engine.sync();

    // and an identical record on the server but different GUID.
    let remoteGuid = Utils.makeGUID();
    server.insertWBO("foo", "addresses", new ServerWBO(remoteGuid, encryptPayload({
      id: remoteGuid,
      entry: TEST_PROFILE_1,
    }), engine.lastSync + 10));

    await engine.sync();

    // Should have 2 items locally, since the first was synced.
    expectLocalProfiles([
      {guid: localGuid},
      {guid: remoteGuid},
    ]);
  } finally {
    await cleanup(server);
  }
});

add_task(async function test_dedupe_multiple_candidates() {
  let {server, engine} = await setup();
  try {
    // It's possible to have duplicate local profiles, with the same fields but
    // different GUIDs. After a node reassignment, or after disconnecting and
    // reconnecting to Sync, we might dedupe a local record A to a remote record
    // B, if we see B before we download and apply A. Since A and B are dupes,
    // that's OK. We'll write a tombstone for A when we dedupe A to B, and
    // overwrite that tombstone when we see A.

    let localRecord = {
      "given-name": "Mark",
      "family-name": "Hammond",
      "organization": "Mozilla",
      "country": "AU",
      "tel": "+12345678910",
    };
    let serverRecord = Object.assign({
      "version": 1,
    }, localRecord);

    // We don't pass `sourceSync` so that the records are marked as NEW.
    let aGuid = profileStorage.addresses.add(localRecord);
    let bGuid = profileStorage.addresses.add(localRecord);

    // Insert B before A.
    server.insertWBO("foo", "addresses", new ServerWBO(bGuid, encryptPayload({
      id: bGuid,
      entry: serverRecord,
    }), Date.now() / 1000));
    server.insertWBO("foo", "addresses", new ServerWBO(aGuid, encryptPayload({
      id: aGuid,
      entry: serverRecord,
    }), Date.now() / 1000));

    engine.lastSync = 0;
    await engine.sync();

    expectLocalProfiles([
      {
        "guid": aGuid,
        "given-name": "Mark",
        "family-name": "Hammond",
        "organization": "Mozilla",
        "country": "AU",
        "tel": "+12345678910",
      },
      {
        "guid": bGuid,
        "given-name": "Mark",
        "family-name": "Hammond",
        "organization": "Mozilla",
        "country": "AU",
        "tel": "+12345678910",
      },
    ]);
    // Make sure these are both syncing.
    strictEqual(getSyncChangeCounter(profileStorage.addresses, aGuid), 0,
      "A should be marked as syncing");
    strictEqual(getSyncChangeCounter(profileStorage.addresses, bGuid), 0,
      "B should be marked as syncing");
  } finally {
    await cleanup(server);
  }
});
