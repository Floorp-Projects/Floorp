/**
 * Tests FormAutofillStorage objects support for sync related fields.
 */

"use strict";

// The duplication of some of these fixtures between tests is unfortunate.
const TEST_STORE_FILE_NAME = "test-profile.json";

const TEST_ADDRESS_1 = {
  name: "Timothy John Berners-Lee",
  organization: "World Wide Web Consortium",
  "street-address": "32 Vassar Street\nMIT Room 32-G524",
  "address-level2": "Cambridge",
  "address-level1": "MA",
  "postal-code": "02139",
  country: "US",
  tel: "+1 617 253 5702",
  email: "timbl@w3.org",
  "unknown-1": "an unknown field we roundtrip",
};

const TEST_ADDRESS_2 = {
  "street-address": "Some Address",
  country: "US",
};

const TEST_ADDRESS_3 = {
  "street-address": "Other Address",
  "postal-code": "12345",
};

// storage.get() doesn't support getting deleted items. However, this test
// wants to do that, so rather than making .get() support that just for this
// test, we use this helper.
async function findGUID(storage, guid, options) {
  let all = await storage.getAll(options);
  let records = all.filter(r => r.guid == guid);
  equal(records.length, 1);
  return records[0];
}

add_task(async function test_changeCounter() {
  let profileStorage = await initProfileStorage(TEST_STORE_FILE_NAME, [
    TEST_ADDRESS_1,
  ]);

  let [address] = await profileStorage.addresses.getAll();
  // new records don't get the sync metadata.
  equal(getSyncChangeCounter(profileStorage.addresses, address.guid), -1);
  // But we can force one.
  profileStorage.addresses.pullSyncChanges();
  equal(getSyncChangeCounter(profileStorage.addresses, address.guid), 1);
});

add_task(async function test_pushChanges() {
  let profileStorage = await initProfileStorage(TEST_STORE_FILE_NAME, [
    TEST_ADDRESS_1,
    TEST_ADDRESS_2,
  ]);

  profileStorage.addresses.pullSyncChanges(); // force sync metadata for all items

  let [, address] = await profileStorage.addresses.getAll();
  let guid = address.guid;
  let changeCounter = getSyncChangeCounter(profileStorage.addresses, guid);

  // Pretend we're doing a sync now, and an update occured mid-sync.
  let changes = {
    [guid]: {
      profile: address,
      counter: changeCounter,
      modified: address.timeLastModified,
      synced: true,
    },
  };

  let onChanged = TestUtils.topicObserved(
    "formautofill-storage-changed",
    (subject, data) => data == "update"
  );
  await profileStorage.addresses.update(guid, TEST_ADDRESS_3);
  await onChanged;

  changeCounter = getSyncChangeCounter(profileStorage.addresses, guid);
  Assert.equal(changeCounter, 2);

  profileStorage.addresses.pushSyncChanges(changes);
  address = await profileStorage.addresses.get(guid);
  changeCounter = getSyncChangeCounter(profileStorage.addresses, guid);

  // Counter should still be 1, since our sync didn't record the mid-sync change
  Assert.equal(
    changeCounter,
    1,
    "Counter shouldn't be zero because it didn't record update"
  );

  // now, push a new set of changes, which should make the changeCounter 0
  profileStorage.addresses.pushSyncChanges({
    [guid]: {
      profile: address,
      counter: changeCounter,
      modified: address.timeLastModified,
      synced: true,
    },
  });

  changeCounter = getSyncChangeCounter(profileStorage.addresses, guid);
  Assert.equal(changeCounter, 0);
});

async function checkingSyncChange(action, callback) {
  let onChanged = TestUtils.topicObserved(
    "formautofill-storage-changed",
    (subject, data) => data == action
  );
  await callback();
  let [subject] = await onChanged;
  ok(
    subject.wrappedJSObject.sourceSync,
    "change notification should have source sync"
  );
}

add_task(async function test_add_sourceSync() {
  let profileStorage = await initProfileStorage(TEST_STORE_FILE_NAME, []);

  // Hardcode a guid so that we don't need to generate a dynamic regex
  let guid = "aaaaaaaaaaaa";
  let testAddr = Object.assign({ guid, version: 1 }, TEST_ADDRESS_1);

  await checkingSyncChange("add", async () =>
    profileStorage.addresses.add(testAddr, { sourceSync: true })
  );

  let changeCounter = getSyncChangeCounter(profileStorage.addresses, guid);
  equal(changeCounter, 0);

  await Assert.rejects(
    profileStorage.addresses.add({ guid, deleted: true }, { sourceSync: true }),
    /Record aaaaaaaaaaaa already exists/
  );
});

add_task(async function test_add_tombstone_sourceSync() {
  let profileStorage = await initProfileStorage(TEST_STORE_FILE_NAME, []);

  let guid = profileStorage.addresses._generateGUID();
  let testAddr = { guid, deleted: true };
  await checkingSyncChange("add", async () =>
    profileStorage.addresses.add(testAddr, { sourceSync: true })
  );

  let added = await findGUID(profileStorage.addresses, guid, {
    includeDeleted: true,
  });
  ok(added);
  equal(getSyncChangeCounter(profileStorage.addresses, guid), 0);
  ok(added.deleted);

  // Adding same record again shouldn't throw (or change anything)
  await checkingSyncChange("add", async () =>
    profileStorage.addresses.add(testAddr, { sourceSync: true })
  );

  added = await findGUID(profileStorage.addresses, guid, {
    includeDeleted: true,
  });
  equal(getSyncChangeCounter(profileStorage.addresses, guid), 0);
  ok(added.deleted);
});

add_task(async function test_add_resurrects_tombstones() {
  let profileStorage = await initProfileStorage(TEST_STORE_FILE_NAME, []);

  let guid = profileStorage.addresses._generateGUID();

  // Add a tombstone.
  await profileStorage.addresses.add({ guid, deleted: true });

  // You can't re-add an item with an explicit GUID.
  let resurrected = Object.assign({}, TEST_ADDRESS_1, { guid, version: 1 });
  await Assert.rejects(
    profileStorage.addresses.add(resurrected),
    /"(guid|version)" is not a valid field/
  );

  // But Sync can!
  let guid3 = await profileStorage.addresses.add(resurrected, {
    sourceSync: true,
  });
  equal(guid, guid3);

  let got = await profileStorage.addresses.get(guid);
  equal(got.name, TEST_ADDRESS_1.name);
});

add_task(async function test_remove_sourceSync_localChanges() {
  let profileStorage = await initProfileStorage(TEST_STORE_FILE_NAME, [
    TEST_ADDRESS_1,
  ]);
  profileStorage.addresses.pullSyncChanges(); // force sync metadata

  let [{ guid }] = await profileStorage.addresses.getAll();

  equal(getSyncChangeCounter(profileStorage.addresses, guid), 1);
  // try and remove a record stored locally with local changes
  await checkingSyncChange("remove", async () =>
    profileStorage.addresses.remove(guid, { sourceSync: true })
  );

  let record = await profileStorage.addresses.get(guid);
  ok(record);
  equal(getSyncChangeCounter(profileStorage.addresses, guid), 1);
});

add_task(async function test_remove_sourceSync_unknown() {
  // remove a record not stored locally
  let profileStorage = await initProfileStorage(TEST_STORE_FILE_NAME, []);

  let guid = profileStorage.addresses._generateGUID();
  await checkingSyncChange("remove", async () =>
    profileStorage.addresses.remove(guid, { sourceSync: true })
  );

  let tombstone = await findGUID(profileStorage.addresses, guid, {
    includeDeleted: true,
  });
  ok(tombstone.deleted);
  equal(getSyncChangeCounter(profileStorage.addresses, guid), 0);
});

add_task(async function test_remove_sourceSync_unchanged() {
  // Remove a local record without a change counter.
  let profileStorage = await initProfileStorage(TEST_STORE_FILE_NAME, []);

  let guid = profileStorage.addresses._generateGUID();
  let addr = Object.assign({ guid, version: 1 }, TEST_ADDRESS_1);
  // add a record with sourceSync to guarantee changeCounter == 0
  await checkingSyncChange("add", async () =>
    profileStorage.addresses.add(addr, { sourceSync: true })
  );

  equal(getSyncChangeCounter(profileStorage.addresses, guid), 0);

  await checkingSyncChange("remove", async () =>
    profileStorage.addresses.remove(guid, { sourceSync: true })
  );

  let tombstone = await findGUID(profileStorage.addresses, guid, {
    includeDeleted: true,
  });
  ok(tombstone.deleted);
  equal(getSyncChangeCounter(profileStorage.addresses, guid), 0);
});

add_task(async function test_pullSyncChanges() {
  let profileStorage = await initProfileStorage(TEST_STORE_FILE_NAME, [
    TEST_ADDRESS_1,
    TEST_ADDRESS_2,
  ]);

  let startAddresses = await profileStorage.addresses.getAll();
  equal(startAddresses.length, 2);
  // All should start without sync metadata
  for (let { guid } of profileStorage.addresses._store.data.addresses) {
    let changeCounter = getSyncChangeCounter(profileStorage.addresses, guid);
    equal(changeCounter, -1);
  }
  profileStorage.addresses.pullSyncChanges(); // force sync metadata

  let addedDirectGUID = profileStorage.addresses._generateGUID();
  let testAddr = Object.assign(
    { guid: addedDirectGUID, version: 1 },
    TEST_ADDRESS_1,
    TEST_ADDRESS_3
  );

  await checkingSyncChange("add", async () =>
    profileStorage.addresses.add(testAddr, { sourceSync: true })
  );

  let tombstoneGUID = profileStorage.addresses._generateGUID();
  await checkingSyncChange("add", async () =>
    profileStorage.addresses.add(
      { guid: tombstoneGUID, deleted: true },
      { sourceSync: true }
    )
  );

  let onChanged = TestUtils.topicObserved(
    "formautofill-storage-changed",
    (subject, data) => data == "remove"
  );

  profileStorage.addresses.remove(startAddresses[0].guid);
  await onChanged;

  let addresses = await profileStorage.addresses.getAll({
    includeDeleted: true,
  });

  // Should contain changes with a change counter
  let changes = profileStorage.addresses.pullSyncChanges();
  equal(Object.keys(changes).length, 2);

  ok(changes[startAddresses[0].guid].profile.deleted);
  equal(changes[startAddresses[0].guid].counter, 2);

  ok(!changes[startAddresses[1].guid].profile.deleted);
  equal(changes[startAddresses[1].guid].counter, 1);

  ok(
    !changes[tombstoneGUID],
    "Missing because it's a tombstone from sourceSync"
  );
  ok(!changes[addedDirectGUID], "Missing because it was added with sourceSync");

  for (let address of addresses) {
    let change = changes[address.guid];
    if (!change) {
      continue;
    }
    equal(change.profile.guid, address.guid);
    let changeCounter = getSyncChangeCounter(
      profileStorage.addresses,
      change.profile.guid
    );
    equal(change.counter, changeCounter);
    ok(!change.synced);
  }
});

add_task(async function test_pullPushChanges() {
  // round-trip changes between pull and push
  let profileStorage = await initProfileStorage(TEST_STORE_FILE_NAME, []);
  let psa = profileStorage.addresses;

  let guid1 = await psa.add(TEST_ADDRESS_1);
  let guid2 = await psa.add(TEST_ADDRESS_2);
  let guid3 = await psa.add(TEST_ADDRESS_3);

  let changes = psa.pullSyncChanges();

  equal(getSyncChangeCounter(psa, guid1), 1);
  equal(getSyncChangeCounter(psa, guid2), 1);
  equal(getSyncChangeCounter(psa, guid3), 1);

  // between the pull and the push we change the second.
  await psa.update(guid2, Object.assign({}, TEST_ADDRESS_2, { country: "AU" }));
  equal(getSyncChangeCounter(psa, guid2), 2);
  // and update the changeset to indicated we did update the first 2, but failed
  // to update the 3rd for some reason.
  changes[guid1].synced = true;
  changes[guid2].synced = true;

  psa.pushSyncChanges(changes);

  // first was synced correctly.
  equal(getSyncChangeCounter(psa, guid1), 0);
  // second was synced correctly, but it had a change while syncing.
  equal(getSyncChangeCounter(psa, guid2), 1);
  // 3rd wasn't marked as having synced.
  equal(getSyncChangeCounter(psa, guid3), 1);
});

add_task(async function test_changeGUID() {
  let profileStorage = await initProfileStorage(TEST_STORE_FILE_NAME, []);

  let newguid = () => profileStorage.addresses._generateGUID();

  let guid_synced = await profileStorage.addresses.add(TEST_ADDRESS_1);

  // pullSyncChanges so guid_synced is flagged as syncing.
  profileStorage.addresses.pullSyncChanges();

  // and 2 items that haven't been synced.
  let guid_u1 = await profileStorage.addresses.add(TEST_ADDRESS_2);
  let guid_u2 = await profileStorage.addresses.add(TEST_ADDRESS_3);

  // Change a non-existing guid
  Assert.throws(
    () => profileStorage.addresses.changeGUID(newguid(), newguid()),
    /changeGUID: no source record/
  );
  // Change to a guid that already exists.
  Assert.throws(
    () => profileStorage.addresses.changeGUID(guid_u1, guid_u2),
    /changeGUID: record with destination id exists already/
  );
  // Try and change a guid that's already been synced.
  Assert.throws(
    () => profileStorage.addresses.changeGUID(guid_synced, newguid()),
    /changeGUID: existing record has already been synced/
  );

  // Change an item to itself makes no sense.
  Assert.throws(
    () => profileStorage.addresses.changeGUID(guid_u1, guid_u1),
    /changeGUID: old and new IDs are the same/
  );

  // and one that works.
  equal(
    (await profileStorage.addresses.getAll({ includeDeleted: true })).length,
    3
  );
  let targetguid = newguid();
  profileStorage.addresses.changeGUID(guid_u1, targetguid);
  equal(
    (await profileStorage.addresses.getAll({ includeDeleted: true })).length,
    3
  );

  ok(
    await profileStorage.addresses.get(guid_synced),
    "synced item still exists."
  );
  ok(
    await profileStorage.addresses.get(guid_u2),
    "guid we didn't touch still exists."
  );
  ok(await profileStorage.addresses.get(targetguid), "target guid exists.");
  ok(
    !(await profileStorage.addresses.get(guid_u1)),
    "old guid no longer exists."
  );
});

add_task(async function test_findDuplicateGUID() {
  let profileStorage = await initProfileStorage(TEST_STORE_FILE_NAME, [
    TEST_ADDRESS_1,
  ]);

  let [record] = await profileStorage.addresses.getAll({ rawData: true });
  await Assert.rejects(
    profileStorage.addresses.findDuplicateGUID(record),
    /Record \w+ already exists/,
    "Should throw if the GUID already exists"
  );

  // Add a malformed record, passing `sourceSync` to work around the record
  // normalization logic that would prevent this.
  let timeLastModified = Date.now();
  let timeCreated = timeLastModified - 60 * 1000;

  await profileStorage.addresses.add(
    {
      guid: profileStorage.addresses._generateGUID(),
      version: 1,
      timeCreated,
      timeLastModified,
    },
    { sourceSync: true }
  );

  strictEqual(
    await profileStorage.addresses.findDuplicateGUID({
      guid: profileStorage.addresses._generateGUID(),
      version: 1,
      timeCreated,
      timeLastModified,
    }),
    null,
    "Should ignore internal fields and malformed records"
  );
});

add_task(async function test_reset() {
  let profileStorage = await initProfileStorage(TEST_STORE_FILE_NAME, [
    TEST_ADDRESS_1,
    TEST_ADDRESS_2,
  ]);

  let addresses = await profileStorage.addresses.getAll();
  // All should start without sync metadata
  for (let { guid } of addresses) {
    let changeCounter = getSyncChangeCounter(profileStorage.addresses, guid);
    equal(changeCounter, -1);
  }
  // pullSyncChanges should create the metadata.
  profileStorage.addresses.pullSyncChanges();
  addresses = await profileStorage.addresses.getAll();
  for (let { guid } of addresses) {
    let changeCounter = getSyncChangeCounter(profileStorage.addresses, guid);
    equal(changeCounter, 1);
  }
  // and resetSync should wipe it.
  profileStorage.addresses.resetSync();
  addresses = await profileStorage.addresses.getAll();
  for (let { guid } of addresses) {
    let changeCounter = getSyncChangeCounter(profileStorage.addresses, guid);
    equal(changeCounter, -1);
  }
});
