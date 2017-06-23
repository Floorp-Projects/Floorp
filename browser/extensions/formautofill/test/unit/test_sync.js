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

function expectLocalProfiles(expected) {
  let profiles = profileStorage.addresses.getAll({
    rawData: true,
    includeDeleted: true,
  });
  expected.sort((a, b) => a.guid.localeCompare(b.guid));
  profiles.sort((a, b) => a.guid.localeCompare(b.guid));
  let doCheckObject = (a, b) => {
    for (let key of Object.keys(a)) {
      if (typeof a[key] == "object") {
        doCheckObject(a[key], b[key]);
      } else {
        equal(a[key], b[key]);
      }
    }
  };
  try {
    deepEqual(profiles.map(p => p.guid), expected.map(p => p.guid));
    for (let i = 0; i < expected.length; i++) {
      let thisExpected = expected[i];
      let thisGot = profiles[i];
      // always check "deleted".
      equal(thisExpected.deleted, thisGot.deleted);
      doCheckObject(thisExpected, thisGot);
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

    await engine.sync();

    Assert.equal(collection.count(), 2);
    Assert.ok(collection.wbo(existingGUID));
    Assert.ok(collection.wbo(deletedGUID));

    expectLocalProfiles([
      {
        guid: existingGUID,
        _sync: {changeCounter: 0},
      },
      {
        guid: deletedGUID,
        _sync: {changeCounter: 0},
        deleted: true,
      },
    ]);
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

    await engine.sync();

    expectLocalProfiles([
      {
        guid: profileID,
        _sync: {changeCounter: 0},
      }, {
        guid: deletedID,
        _sync: {changeCounter: 0},
        deleted: true,
      },
    ]);

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

    await engine.sync();

    expectLocalProfiles([{guid}]);
  } finally {
    await cleanup(server);
  }
});

add_task(async function test_dedupe_identical() {
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

    await engine.sync();

    // Should no longer have the local guid
    expectLocalProfiles([
      {
        guid: remoteGuid,
      },
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

    let aRecord = {
      "given-name": "Mark",
      "family-name": "Hammond",
      "organization": "Mozilla",
      "country": "AU",
      "tel": "+12345678910",
    };
    // We don't pass `sourceSync` so that the records are marked as NEW.
    let aGuid = profileStorage.addresses.add(aRecord);

    let bRecord = Cu.cloneInto(aRecord, {});
    let bGuid = profileStorage.addresses.add(bRecord);

    // Insert B before A.
    server.insertWBO("foo", "addresses", new ServerWBO(bGuid, encryptPayload({
      id: bGuid,
      entry: bRecord,
    }), Date.now() / 1000));
    server.insertWBO("foo", "addresses", new ServerWBO(aGuid, encryptPayload({
      id: aGuid,
      entry: aRecord,
    }), Date.now() / 1000));

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
    let addrA = profileStorage.addresses.get(aGuid, {rawData: true});
    ok(addrA._sync);

    let addrB = profileStorage.addresses.get(bGuid, {rawData: true});
    ok(addrB._sync);
  } finally {
    await cleanup(server);
  }
});
