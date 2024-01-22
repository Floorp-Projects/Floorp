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

const { AddressesEngine } = ChromeUtils.importESModule(
  "resource://autofill/FormAutofillSync.sys.mjs"
);

Services.prefs.setCharPref("extensions.formautofill.loglevel", "Debug");
initTestLogging("Trace");

const TEST_STORE_FILE_NAME = "test-profile.json";

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
  const profiles = await profileStorage.addresses.getAll({
    rawData: true,
    includeDeleted: true,
  });
  expectProfiles(profiles, expected);
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
        global: {
          engines: { addresses: { version: engine.version, syncID } },
        },
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

function setupServerRecords(server, records) {
  for (const record of records) {
    server.insertWBO(
      "foo",
      "addresses",
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

function assertNumberRecordUploadedBySync(engine, expectedNumber) {
  const uploadOutgoing = engine._uploadOutgoing;
  engine._uploadOutgoing = async function () {
    engine._uploadOutgoing = uploadOutgoing;
    try {
      await uploadOutgoing.call(this);
    } finally {
      Assert.equal(this._modified.ids().length, expectedNumber);
    }
  };
}

/**
 * The following tests test uploading. Ensure `given-name`, `additional-name`, and `family-name` fields are included
 * in the sync payload.
 */

add_task(async function test_local_upload() {
  const { collection, profileStorage, server, engine } = await setup();

  try {
    const localGuid = await profileStorage.addresses.add({
      name: "Mr John William Doe",
      "street-address": "Some Address",
    });

    await engine.setLastSync(0);
    await engine.sync();

    await expectServerProfiles(collection, [
      {
        guid: localGuid,
        version: 1,
        name: "Mr John William Doe",
        "given-name": "John",
        "additional-name": "William",
        "family-name": "Doe",
        "street-address": "Some Address",
      },
    ]);
  } finally {
    await cleanup(server);
  }
});

add_task(async function test_local_upload_no_addtional_name() {
  const { collection, profileStorage, server, engine } = await setup();

  try {
    const localGuid = await profileStorage.addresses.add({
      name: "Timothy Berners-Lee",
      "street-address": "Some Address",
    });

    await engine.setLastSync(0);
    await engine.sync();

    await expectServerProfiles(collection, [
      {
        guid: localGuid,
        version: 1,
        name: "Timothy Berners-Lee",
        "given-name": "Timothy",
        "additional-name": undefined,
        "family-name": "Berners-Lee",
        "street-address": "Some Address",
      },
    ]);
  } finally {
    await cleanup(server);
  }
});

add_task(async function test_local_upload_no_name() {
  const { collection, profileStorage, server, engine } = await setup();

  try {
    const localGuid = await profileStorage.addresses.add({
      "street-address": "Some Address",
    });

    await engine.setLastSync(0);
    await engine.sync();

    await expectServerProfiles(collection, [
      {
        guid: localGuid,
        version: 1,
        name: undefined,
        "given-name": undefined,
        "additional-name": undefined,
        "family-name": undefined,
        "street-address": "Some Address",
      },
    ]);
  } finally {
    await cleanup(server);
  }
});

/**
 * The following tasks test cases where no matching local record is found while applying an
 * incoming record. We test:
 * 1. An incoming record with `name` field and `*-name` fields, indicating this is a new record.
 * 2. An incoming record with only `*-name` fields, indicating this is an old record.
 * 3. An incoming record without any name fields, leaving it unknown whether this record comes
 *    from a new or an old device.
 *
 * The expected result for each task is:
 * 1. The `name` field should NOT be rebuilt based on the deprecated `*-name` fields.
 * 2. The `name` field should be rebuilt based on the deprecated `*-name` fields.
 * 3. All the name related fields should remain empty.
 */

// 1. Remote record is a new record
add_task(async function test_apply_incoming() {
  const { profileStorage, server, engine } = await setup();

  try {
    setupServerRecords(server, [
      {
        guid: "86d961c7717a",
        version: 1,
        name: "Mr. John William Doe",
        "given-name": "John",
        "additional-name": "William",
        "family-name": "Doe",
        organization: "Mozilla",
      },
    ]);

    assertNumberRecordUploadedBySync(engine, 0);
    await engine.setLastSync(0);
    await engine.sync();

    await expectLocalProfiles(profileStorage, [
      {
        guid: "86d961c7717a",
        version: 1,
        name: "Mr. John William Doe", // Prefix `Mr.` remains
        organization: "Mozilla",
      },
    ]);
  } finally {
    await cleanup(server);
  }
});

// 2. Remote record is an old record
add_task(async function test_apply_incoming_legacy() {
  const { profileStorage, server, engine } = await setup();

  try {
    setupServerRecords(server, [
      {
        guid: "86d961c7717a",
        version: 1,
        "given-name": "John",
        "additional-name": "William",
        "family-name": "Doe",
        organization: "Mozilla",
      },
    ]);

    assertNumberRecordUploadedBySync(engine, 0);
    await engine.setLastSync(0);
    await engine.sync();

    await expectLocalProfiles(profileStorage, [
      {
        guid: "86d961c7717a",
        version: 1,
        name: "John William Doe", // rebuild name field based on `*-name` fields
        organization: "Mozilla",
      },
    ]);
  } finally {
    await cleanup(server);
  }
});

// 3. Remote record does not have `name`
add_task(async function test_apply_incoming_without_name() {
  const { profileStorage, server, engine } = await setup();

  try {
    setupServerRecords(server, [
      {
        guid: "86d961c7717a",
        version: 1,
        "street-address": "Some Address",
      },
    ]);

    assertNumberRecordUploadedBySync(engine, 0);
    await engine.setLastSync(0);
    await engine.sync();

    await expectLocalProfiles(profileStorage, [
      {
        guid: "86d961c7717a",
        version: 1,
        "street-address": "Some Address",
        name: undefined,
        "given-name": undefined,
        "additional-name": undefined,
        "family-name": undefined,
      },
    ]);
  } finally {
    await cleanup(server);
  }
});

/**
 * The following tasks test cases where a matching local record is found by matching GUID while applying an
 * incoming record. We test:
 * 1. An incoming record with `name` field and `*-name` fields, indicating this is a new record.
 * 2. An incoming record with only `*-name` fields, indicating this is an old record, `*-name` is not updated
 * 3. An incoming record with only `*-name` fields, indicating this is an old record, `*-name` is updated
 * 4. An incoming record with only `*-name` fields, indicating this is an old record. Local record does not have `name`.
 * 5. An incoming record without any name fields, leaving it unknown whether this record comes
 *    from a new or an old device.
 *
 * The expected result for each task is:
 * 1. The `name` field of the local record should be replaced by the remote record.
 * 2. The `name` field of the local record should NOT be replaced by the remote record, `name` remains the same.
 * 3. The `name` field of the local record should be replaced by the remote record.
 * 4. The `name` field of the local record should be replaced by the remote record.
 * 5. The `name` field of the local record should be replaced by the remote record.
 */

// 1. Remote record is a new record
add_task(async function test_apply_incoming_repalce() {
  const { profileStorage, server, engine } = await setup();

  try {
    // Setup the local record and sync the record to sync
    const localGuid = await profileStorage.addresses.add({
      name: "Mr John William Doe",
      "street-address": "Some Address",
      email: "john.doe@mozilla.org",
    });
    await engine.setLastSync(0);
    await engine.sync();

    // Remote record is updated by a new device
    setupServerRecords(server, [
      {
        guid: localGuid,
        version: 1,
        name: "Dr Timothy Berners Lee",
        "given-name": "Timothy",
        "additional-name": "Berners",
        "family-name": "Lee",
        "street-address": "32 Vassar Street", // updated
        organization: "Mozilla", // added
        email: undefined, // removed
      },
    ]);

    assertNumberRecordUploadedBySync(engine, 0);
    await engine.setLastSync(0);
    await engine.sync();

    await expectLocalProfiles(profileStorage, [
      {
        guid: localGuid,
        version: 1,
        name: "Dr Timothy Berners Lee", // Should be replaced!
        "street-address": "32 Vassar Street",
        organization: "Mozilla",
        email: undefined,
      },
    ]);
  } finally {
    await cleanup(server);
  }
});

// 2. Remote record is an old record, `*-name` is not updated
add_task(async function test_apply_incoming_legacy_replace_name_is_updated() {
  const { profileStorage, server, engine } = await setup();

  try {
    // Setup the local record and sync the record to sync
    const localGuid = await profileStorage.addresses.add({
      name: "Mr John William Doe",
      "street-address": "Some Address",
      email: "john.doe@mozilla.org",
    });
    await engine.setLastSync(0);
    await engine.sync();

    // Remote record is updated by an old device
    setupServerRecords(server, [
      {
        guid: localGuid,
        version: 1,
        "given-name": "John",
        "additional-name": "William",
        "family-name": "Doe",
        "street-address": "32 Vassar Street", // updated
        organization: "Mozilla", // added
        email: undefined, // removed
      },
    ]);

    assertNumberRecordUploadedBySync(engine, 0);
    await engine.setLastSync(0);
    await engine.sync();

    await expectLocalProfiles(profileStorage, [
      {
        guid: localGuid,
        version: 1,
        name: "Mr John William Doe", // Shoult not be replaced!
        "street-address": "32 Vassar Street",
        organization: "Mozilla",
        email: undefined,
      },
    ]);
  } finally {
    await cleanup(server);
  }
});

// 3. Remote record is an old record, `*-name` is updated
add_task(
  async function test_apply_incoming_legacy_replace_name_is_not_updated() {
    const { profileStorage, server, engine } = await setup();

    try {
      // Setup the local record and sync the record to sync
      const localGuid = await profileStorage.addresses.add({
        name: "Mr John William Doe",
        "street-address": "Some Address",
        email: "john.doe@mozilla.org",
      });
      await engine.setLastSync(0);
      await engine.sync();

      // Remote record is updated by an old device
      setupServerRecords(server, [
        {
          guid: localGuid,
          version: 1,
          "given-name": "Timothy",
          "additional-name": "Berners",
          "family-name": "Lee",
          "street-address": "32 Vassar Street", // updated
          organization: "Mozilla", // added
          email: undefined, // removed
        },
      ]);

      assertNumberRecordUploadedBySync(engine, 0);
      await engine.setLastSync(0);
      await engine.sync();

      await expectLocalProfiles(profileStorage, [
        {
          guid: localGuid,
          version: 1,
          name: "Timothy Berners Lee", // Shoult be replaced!
          "street-address": "32 Vassar Street",
          organization: "Mozilla",
          email: undefined,
        },
      ]);
    } finally {
      await cleanup(server);
    }
  }
);

// 4. Remote record is an old record. Local record does not have `name`
add_task(
  async function test_apply_incoming_legacy_repalce_local_without_name() {
    const { profileStorage, server, engine } = await setup();

    try {
      // Setup the local record and sync the record to sync
      const localGuid = await profileStorage.addresses.add({
        "street-address": "Some Address",
        email: "john.doe@mozilla.org",
      });
      await engine.setLastSync(0);
      await engine.sync();

      // Remote record is updated by an old device
      setupServerRecords(server, [
        {
          guid: localGuid,
          version: 1,
          "given-name": "Timothy",
          "additional-name": "Berners",
          "family-name": "Lee",
          "street-address": "32 Vassar Street", // updated
          organization: "Mozilla", // added
          email: undefined, // removed
        },
      ]);

      assertNumberRecordUploadedBySync(engine, 0);
      await engine.setLastSync(0);
      await engine.sync();

      await expectLocalProfiles(profileStorage, [
        {
          guid: localGuid,
          version: 1,
          name: "Timothy Berners Lee", // Shoult be replaced!
          "street-address": "32 Vassar Street",
          organization: "Mozilla",
          email: undefined,
        },
      ]);
    } finally {
      await cleanup(server);
    }
  }
);

// 5. Remote record doesn't have `name`
add_task(async function test_apply_incoming_without_name_replace() {
  const { profileStorage, server, engine } = await setup();

  try {
    // Setup the local record and sync the record to sync
    const localGuid = await profileStorage.addresses.add({
      name: "Mr. John William Doe",
      "street-address": "Some Address",
      email: "john.doe@mozilla.org",
    });
    await engine.setLastSync(0);
    await engine.sync();

    // Remote record is updated by a  device
    setupServerRecords(server, [
      {
        guid: localGuid,
        version: 1,
        "street-address": "32 Vassar Street", // updated
        organization: "Mozilla", // added
        email: undefined, // removed
      },
    ]);

    assertNumberRecordUploadedBySync(engine, 0);
    await engine.setLastSync(0);
    await engine.sync();

    // `name` field should be removed
    await expectLocalProfiles(profileStorage, [
      {
        guid: localGuid,
        version: 1,
        name: undefined, // Should be replaced!
        "street-address": "32 Vassar Street",
        organization: "Mozilla",
        email: undefined,
      },
    ]);
  } finally {
    await cleanup(server);
  }
});

/**
 * The following tasks test cases where a matching local record is found by matching GUID while applying an
 * incoming record. And both the local and remote records are modified.
 *
 * We test:
 * 1. An incoming record with `name` field and `*-name` fields, indicating this is a new record.
 * 2. An incoming record with only `*-name` fields, indicating this is an old record, `*-name` is not updated
 * 3. An incoming record with only `*-name` fields, indicating this is an old record, `*-name` is updated
 * 4. An incoming record with only `*-name` fields, indicating this is an old record. Local record does not have `name`.
 * 5. An incoming record without any name fields, leaving it unknown whether this record comes
 *    from a new or an old device.
 *
 * The expected result for each task is:
 * 1. The `name` field of the local record should be replaced by the remote record
 * 2. The `name` field of the local record should NOT be replaced by the remote record, `name` remains the same.
 * 3. The `name` field of the local record should be replaced by the remote record.
 * 4. The `name` field of the local record should be replaced by the remote record.
 * 5. The `name` field of the local record should be replaced by the remote record
 */

// 1. Remote record is a new record
add_task(async function test_apply_incoming_merge() {
  const { collection, profileStorage, server, engine } = await setup();
  const LOCAL_ENTRY = {
    name: "Mr John William Doe",
    "street-address": "Some Address",
    email: "john.doe@mozilla.org",
  };

  try {
    const guid = await profileStorage.addresses.add(LOCAL_ENTRY);
    await engine.setLastSync(0);
    await engine.sync();

    // local modifies "street-address"
    let localCopy = Object.assign({}, LOCAL_ENTRY);
    localCopy["street-address"] = "I moved!";
    await profileStorage.addresses.update(guid, localCopy);

    setupServerRecords(server, [
      {
        guid,
        version: 1,
        name: "Dr Timothy Berners Lee", // `name` is modified
        "given-name": "Timothy",
        "additional-name": "Berners",
        "family-name": "Lee",
        "street-address": "Some Address",
        organization: "Mozilla", // `organization` is added
        email: undefined, // `email` is removed
      },
    ]);

    await engine.setLastSync(0);
    await engine.sync();

    await expectLocalProfiles(profileStorage, [
      {
        guid,
        version: 1,
        name: "Dr Timothy Berners Lee", // Name should be replaced!
        "street-address": "I moved!",
        organization: "Mozilla",
        email: undefined,
      },
    ]);
    await expectServerProfiles(collection, [
      {
        guid,
        version: 1,
        name: "Dr Timothy Berners Lee", // Name should be replaced!
        "given-name": "Timothy",
        "additional-name": "Berners",
        "family-name": "Lee",
        "street-address": "I moved!",
        organization: "Mozilla",
        email: undefined,
      },
    ]);
  } finally {
    await cleanup(server);
  }
});

// 2. Remote record is an old record, `*-name` is not updated
add_task(async function test_apply_incoming_legacy_merge_name_is_not_updated() {
  const { collection, profileStorage, server, engine } = await setup();
  const LOCAL_ENTRY = {
    name: "Mr John William Doe",
    "street-address": "Some Address",
    email: "john.doe@mozilla.org",
  };

  try {
    const guid = await profileStorage.addresses.add(LOCAL_ENTRY);
    await engine.setLastSync(0);
    await engine.sync();

    // local modifies "street-address"
    let localCopy = Object.assign({}, LOCAL_ENTRY);
    localCopy["street-address"] = "I moved!";
    await profileStorage.addresses.update(guid, localCopy);

    setupServerRecords(server, [
      {
        guid,
        version: 1,
        "given-name": "John",
        "additional-name": "William",
        "family-name": "Doe",
        "street-address": "Some Address",
        organization: "Mozilla", // `organization` is added
        email: undefined, // `email` is removed
      },
    ]);

    await engine.setLastSync(0);
    await engine.sync();

    await expectLocalProfiles(profileStorage, [
      {
        guid,
        version: 1,
        name: "Mr John William Doe", // `name` should NOT be replaced!
        "street-address": "I moved!",
        organization: "Mozilla",
        email: undefined,
      },
    ]);
    await expectServerProfiles(collection, [
      {
        guid,
        version: 1,
        name: "Mr John William Doe",
        "given-name": "John",
        "additional-name": "William",
        "family-name": "Doe",
        "street-address": "I moved!",
        organization: "Mozilla",
        email: undefined,
      },
    ]);
  } finally {
    await cleanup(server);
  }
});

// 3. Remote record is an old record, `*-name` is updated
add_task(async function test_apply_incoming_legacy_merge_name_is_updated() {
  const { collection, profileStorage, server, engine } = await setup();
  const LOCAL_ENTRY = {
    name: "Mr John William Doe",
    "street-address": "Some Address",
    email: "john.doe@mozilla.org",
  };

  try {
    const guid = await profileStorage.addresses.add(LOCAL_ENTRY);
    await engine.setLastSync(0);
    await engine.sync();

    // local modifies "street-address"
    let localCopy = Object.assign({}, LOCAL_ENTRY);
    localCopy["street-address"] = "I moved!";
    await profileStorage.addresses.update(guid, localCopy);

    setupServerRecords(server, [
      {
        guid,
        version: 1,
        "given-name": "Timothy", // `name` is modified
        "additional-name": "Berners",
        "family-name": "Lee",
        "street-address": "Some Address",
        organization: "Mozilla", // `organization` is added
        email: undefined, // `email` is removed
      },
    ]);

    await engine.setLastSync(0);
    await engine.sync();

    await expectLocalProfiles(profileStorage, [
      {
        guid,
        version: 1,
        name: "Timothy Berners Lee", // `name` should be replaced!
        "street-address": "I moved!",
        organization: "Mozilla",
        email: undefined,
      },
    ]);
    await expectServerProfiles(collection, [
      {
        guid,
        version: 1,
        name: "Timothy Berners Lee",
        "given-name": "Timothy",
        "additional-name": "Berners",
        "family-name": "Lee",
        "street-address": "I moved!",
        organization: "Mozilla",
        email: undefined,
      },
    ]);
  } finally {
    await cleanup(server);
  }
});

// 4. Remote record is an old record. Local record does not have `name`
add_task(async function test_apply_incoming_legacy_merge_local_without_name() {
  const { collection, profileStorage, server, engine } = await setup();
  const LOCAL_ENTRY = {
    "street-address": "Some Address",
    email: "john.doe@mozilla.org",
  };

  try {
    const guid = await profileStorage.addresses.add(LOCAL_ENTRY);
    await engine.setLastSync(0);
    await engine.sync();

    // local modifies "street-address"
    let localCopy = Object.assign({}, LOCAL_ENTRY);
    localCopy["street-address"] = "I moved!";
    await profileStorage.addresses.update(guid, localCopy);

    setupServerRecords(server, [
      {
        guid,
        version: 1,
        "given-name": "Timothy", // `name` is modified
        "additional-name": "Berners",
        "family-name": "Lee",
        "street-address": "Some Address",
        organization: "Mozilla", // `organization` is added
        email: undefined, // `email` is removed
      },
    ]);

    await engine.setLastSync(0);
    await engine.sync();

    await expectLocalProfiles(profileStorage, [
      {
        guid,
        version: 1,
        name: "Timothy Berners Lee", // `name` should be replaced!
        "street-address": "I moved!",
        organization: "Mozilla",
        email: undefined,
      },
    ]);
    await expectServerProfiles(collection, [
      {
        guid,
        version: 1,
        name: "Timothy Berners Lee",
        "given-name": "Timothy",
        "additional-name": "Berners",
        "family-name": "Lee",
        "street-address": "I moved!",
        organization: "Mozilla",
        email: undefined,
      },
    ]);
  } finally {
    await cleanup(server);
  }
});

// 5. Remote record does not have `name`
add_task(async function test_apply_incoming_without_name_merge() {
  const { collection, profileStorage, server, engine } = await setup();
  const LOCAL_ENTRY = {
    "street-address": "Some Address",
    email: "john.doe@mozilla.org",
  };

  try {
    const guid = await profileStorage.addresses.add(LOCAL_ENTRY);
    await engine.setLastSync(0);
    await engine.sync();

    // local modifies "street-address"
    let localCopy = Object.assign({}, LOCAL_ENTRY);
    localCopy["street-address"] = "I moved!";
    await profileStorage.addresses.update(guid, localCopy);

    setupServerRecords(server, [
      {
        guid,
        version: 1,
        "street-address": "Some Address",
        organization: "Mozilla",
      },
    ]);

    await engine.setLastSync(0);
    await engine.sync();

    await expectLocalProfiles(profileStorage, [
      {
        guid,
        version: 1,
        "street-address": "I moved!",
        organization: "Mozilla", // `organization` is added
        email: undefined, // `email` is removed
      },
    ]);
    await expectServerProfiles(collection, [
      {
        guid,
        version: 1,
        "street-address": "I moved!",
        organization: "Mozilla",
        email: undefined,
      },
    ]);
  } finally {
    await cleanup(server);
  }
});

/**
 * The following tasks test cases where a matching local record is found by running dedupe algorithm while applying an
 * incoming record. We test:
 * 1. An incoming record with `name` field and `*-name` fields, indicating this is a new record.
 * 2. An incoming record with only `*-name` fields, indicating this is an old record.
 * 3. An incoming record without any name fields, leaving it unknown whether this record comes
 *    from a new or an old device.
 *
 * The expected result is still one record in the local after merging the incoming record
 */

// 1. Remote record is a new record
add_task(async function test_apply_incoming_dedupe() {
  const { collection, profileStorage, server, engine } = await setup();
  const LOCAL_ENTRY = {
    name: "Mr John William Doe",
    "street-address": "Some Address",
    country: "US",
  };

  try {
    const localGuid = await profileStorage.addresses.add(LOCAL_ENTRY);

    const remoteGuid = Utils.makeGUID();
    notEqual(localGuid, remoteGuid);

    setupServerRecords(server, [
      {
        guid: remoteGuid,
        version: 1,
        name: "Mr John William Doe",
        "given-name": "John",
        "additional-name": "William",
        "family-name": "Doe",
        "street-address": "Some Address",
        country: "US",
      },
    ]);

    // Local duplicated record has not been synced before, so will trigger
    // sync upload after merging.
    assertNumberRecordUploadedBySync(engine, 1);
    await engine.setLastSync(0);
    await engine.sync();

    await expectLocalProfiles(profileStorage, [
      {
        guid: remoteGuid,
        version: 1,
        name: "Mr John William Doe",
        "street-address": "Some Address",
        country: "US",
      },
    ]);
    await expectServerProfiles(collection, [
      {
        guid: remoteGuid,
        version: 1,
        name: "Mr John William Doe",
        "given-name": "John",
        "additional-name": "William",
        "family-name": "Doe",
        "street-address": "Some Address",
        country: "US",
      },
    ]);
  } finally {
    await cleanup(server);
  }
});

// 2. Remote record is an old record
add_task(async function test_apply_incoming_legacy_dedupe() {
  const { collection, profileStorage, server, engine } = await setup();
  const LOCAL_ENTRY = {
    name: "John William Doe",
    "street-address": "Some Address",
    country: "US",
  };

  try {
    const localGuid = await profileStorage.addresses.add(LOCAL_ENTRY);

    const remoteGuid = Utils.makeGUID();
    notEqual(localGuid, remoteGuid);

    setupServerRecords(server, [
      {
        guid: remoteGuid,
        version: 1,
        "given-name": "John",
        "additional-name": "William",
        "family-name": "Doe",
        "street-address": "Some Address",
        country: "US",
      },
    ]);

    // Local duplicated record has not been synced before, so will trigger
    // sync upload after merging.
    assertNumberRecordUploadedBySync(engine, 1);
    await engine.setLastSync(0);
    await engine.sync();

    await expectLocalProfiles(profileStorage, [
      {
        guid: remoteGuid,
        version: 1,
        name: "John William Doe",
        "street-address": "Some Address",
        country: "US",
      },
    ]);
    await expectServerProfiles(collection, [
      {
        guid: remoteGuid,
        version: 1,
        name: "John William Doe",
        "given-name": "John",
        "additional-name": "William",
        "family-name": "Doe",
        "street-address": "Some Address",
        country: "US",
      },
    ]);
  } finally {
    await cleanup(server);
  }
});

// 3. Remote record does not have `name`
add_task(async function test_apply_incoming_without_name_dedupe() {
  const { collection, profileStorage, server, engine } = await setup();
  const LOCAL_ENTRY = {
    "street-address": "Some Address",
    country: "US",
  };

  try {
    const localGuid = await profileStorage.addresses.add(LOCAL_ENTRY);

    const remoteGuid = Utils.makeGUID();
    notEqual(localGuid, remoteGuid);

    setupServerRecords(server, [
      {
        guid: remoteGuid,
        version: 1,
        "street-address": "Some Address",
        country: "US",
      },
    ]);

    assertNumberRecordUploadedBySync(engine, 1);
    await engine.setLastSync(0);
    await engine.sync();

    await expectLocalProfiles(profileStorage, [
      {
        guid: remoteGuid,
        version: 1,
        "street-address": "Some Address",
        country: "US",
      },
    ]);
    await expectServerProfiles(collection, [
      {
        guid: remoteGuid,
        version: 1,
        "street-address": "Some Address",
        country: "US",
      },
    ]);
  } finally {
    await cleanup(server);
  }
});
