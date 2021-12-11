/**
 * Tests tombstones in address/creditcard records.
 */

"use strict";

let FormAutofillStorage;
add_task(async function setup() {
  ({ FormAutofillStorage } = ChromeUtils.import(
    "resource://autofill/FormAutofillStorage.jsm"
  ));
});

const TEST_STORE_FILE_NAME = "test-tombstones.json";

const TEST_ADDRESS_1 = {
  "given-name": "Timothy",
  "additional-name": "John",
  "family-name": "Berners-Lee",
  organization: "World Wide Web Consortium",
  "street-address": "32 Vassar Street\nMIT Room 32-G524",
  "address-level2": "Cambridge",
  "address-level1": "MA",
  "postal-code": "02139",
  country: "US",
  tel: "+1 617 253 5702",
  email: "timbl@w3.org",
};

const TEST_CC_1 = {
  "cc-name": "John Doe",
  "cc-number": "4111111111111111",
  "cc-exp-month": 4,
  "cc-exp-year": 2017,
};

let do_check_tombstone_record = profile => {
  Assert.ok(profile.deleted);
  Assert.deepEqual(
    Object.keys(profile).sort(),
    ["guid", "timeLastModified", "deleted"].sort()
  );
};

// Like add_task, but actually adds 2 - one for addresses and one for cards.
function add_storage_task(test_function) {
  add_task(async function() {
    let path = getTempFile(TEST_STORE_FILE_NAME).path;
    let profileStorage = new FormAutofillStorage(path);
    let testCC1 = Object.assign({}, TEST_CC_1);
    await profileStorage.initialize();

    for (let [storage, record] of [
      [profileStorage.addresses, TEST_ADDRESS_1],
      [profileStorage.creditCards, testCC1],
    ]) {
      await test_function(storage, record);
    }
  });
}

add_storage_task(async function test_simple_tombstone(storage, record) {
  info("check simple tombstone semantics");

  let guid = await storage.add(record);
  Assert.equal((await storage.getAll()).length, 1);

  storage.remove(guid);

  // should be unable to get it normally.
  Assert.equal(await storage.get(guid), null);
  // and getAll should also not return it.
  Assert.equal((await storage.getAll()).length, 0);

  // but getAll allows us to access deleted items - but we didn't create
  // a tombstone here, so even that will not get it.
  let all = await storage.getAll({ includeDeleted: true });
  Assert.equal(all.length, 0);
});

add_storage_task(async function test_simple_synctombstone(storage, record) {
  info("check simple tombstone semantics for synced records");

  let guid = await storage.add(record);
  Assert.equal((await storage.getAll()).length, 1);

  storage.pullSyncChanges(); // force sync metadata, which triggers tombstone behaviour.

  storage.remove(guid);

  // should be unable to get it normally.
  Assert.equal(await storage.get(guid), null);
  // and getAll should also not return it.
  Assert.equal((await storage.getAll()).length, 0);

  // but getAll allows us to access deleted items.
  let all = await storage.getAll({ includeDeleted: true });
  Assert.equal(all.length, 1);

  do_check_tombstone_record(all[0]);

  // a tombstone got from API should look exactly the same as it got from the
  // disk (besides "_sync").
  let tombstoneInDisk = Object.assign(
    {},
    storage._store.data[storage._collectionName][0]
  );
  delete tombstoneInDisk._sync;
  do_check_tombstone_record(tombstoneInDisk);
});

add_storage_task(async function test_add_tombstone(storage, record) {
  info("Should be able to add a new tombstone");
  let guid = await storage.add({ guid: "test-guid-1", deleted: true });

  // should be unable to get it normally.
  Assert.equal(await storage.get(guid), null);
  // and getAll should also not return it.
  Assert.equal((await storage.getAll()).length, 0);

  // but getAll allows us to access deleted items.
  let all = await storage.getAll({ rawData: true, includeDeleted: true });
  Assert.equal(all.length, 1);

  do_check_tombstone_record(all[0]);

  // a tombstone got from API should look exactly the same as it got from the
  // disk (besides "_sync").
  let tombstoneInDisk = Object.assign(
    {},
    storage._store.data[storage._collectionName][0]
  );
  delete tombstoneInDisk._sync;
  do_check_tombstone_record(tombstoneInDisk);
});

add_storage_task(async function test_add_tombstone_without_guid(
  storage,
  record
) {
  info("Should not be able to add a new tombstone without specifying the guid");
  await Assert.rejects(storage.add({ deleted: true }), /Record missing GUID/);
  Assert.equal((await storage.getAll({ includeDeleted: true })).length, 0);
});

add_storage_task(async function test_add_tombstone_existing_guid(
  storage,
  record
) {
  info(
    "Should not be able to add a new tombstone when a record with that ID exists"
  );
  let guid = await storage.add(record);
  await Assert.rejects(
    storage.add({ guid, deleted: true }),
    /a record with this GUID already exists/
  );

  // same if the existing item is already a tombstone.
  await storage.add({ guid: "test-guid-1", deleted: true });
  await Assert.rejects(
    storage.add({ guid: "test-guid-1", deleted: true }),
    /a record with this GUID already exists/
  );
});

add_storage_task(async function test_update_tombstone(storage, record) {
  info("Updating a tombstone should fail");
  let guid = await storage.add({ guid: "test-guid-1", deleted: true });
  await Assert.rejects(storage.update(guid, {}), /No matching record./);
});

add_storage_task(async function test_remove_existing_tombstone(
  storage,
  record
) {
  info("Removing a record that's already a tombstone should be a no-op");
  let guid = await storage.add({
    guid: "test-guid-1",
    deleted: true,
    timeLastModified: 1234,
  });

  storage.remove(guid);
  let all = await storage.getAll({ rawData: true, includeDeleted: true });
  Assert.equal(all.length, 1);

  do_check_tombstone_record(all[0]);
  equal(all[0].timeLastModified, 1234); // should not be updated to now().
});
