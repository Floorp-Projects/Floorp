/**
 * Tests ProfileStorage object with addresses records.
 */

"use strict";

const {ProfileStorage} = Cu.import("resource://formautofill/ProfileStorage.jsm", {});

const TEST_STORE_FILE_NAME = "test-profile.json";

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

const TEST_ADDRESS_2 = {
  "street-address": "Some Address",
  country: "US",
};

const TEST_ADDRESS_3 = {
  "street-address": "Other Address",
  "postal-code": "12345",
};

const TEST_ADDRESS_WITH_INVALID_FIELD = {
  "street-address": "Another Address",
  invalidField: "INVALID",
};

let prepareTestRecords = async function(path) {
  let profileStorage = new ProfileStorage(path);
  await profileStorage.initialize();

  let onChanged = TestUtils.topicObserved("formautofill-storage-changed",
                                          (subject, data) => data == "add");
  do_check_true(profileStorage.addresses.add(TEST_ADDRESS_1));
  await onChanged;
  do_check_true(profileStorage.addresses.add(TEST_ADDRESS_2));
  await profileStorage._saveImmediately();
};

let do_check_record_matches = (recordWithMeta, record) => {
  for (let key in record) {
    do_check_eq(recordWithMeta[key], record[key]);
  }
};

add_task(async function test_initialize() {
  let path = getTempFile(TEST_STORE_FILE_NAME).path;
  let profileStorage = new ProfileStorage(path);
  await profileStorage.initialize();

  do_check_eq(profileStorage._store.data.version, 1);
  do_check_eq(profileStorage._store.data.addresses.length, 0);

  let data = profileStorage._store.data;
  Assert.deepEqual(data.addresses, []);

  await profileStorage._saveImmediately();

  profileStorage = new ProfileStorage(path);
  await profileStorage.initialize();

  Assert.deepEqual(profileStorage._store.data, data);
});

add_task(async function test_getAll() {
  let path = getTempFile(TEST_STORE_FILE_NAME).path;
  await prepareTestRecords(path);

  let profileStorage = new ProfileStorage(path);
  await profileStorage.initialize();

  let addresses = profileStorage.addresses.getAll();

  do_check_eq(addresses.length, 2);
  do_check_record_matches(addresses[0], TEST_ADDRESS_1);
  do_check_record_matches(addresses[1], TEST_ADDRESS_2);

  // Check computed fields.
  do_check_eq(addresses[0].name, "Timothy John Berners-Lee");
  do_check_eq(addresses[0]["address-line1"], "32 Vassar Street");
  do_check_eq(addresses[0]["address-line2"], "MIT Room 32-G524");

  // Test with noComputedFields set.
  addresses = profileStorage.addresses.getAll({noComputedFields: true});
  do_check_eq(addresses[0].name, undefined);
  do_check_eq(addresses[0]["address-line1"], undefined);
  do_check_eq(addresses[0]["address-line2"], undefined);

  // Modifying output shouldn't affect the storage.
  addresses[0].organization = "test";
  do_check_record_matches(profileStorage.addresses.getAll()[0], TEST_ADDRESS_1);
});

add_task(async function test_get() {
  let path = getTempFile(TEST_STORE_FILE_NAME).path;
  await prepareTestRecords(path);

  let profileStorage = new ProfileStorage(path);
  await profileStorage.initialize();

  let addresses = profileStorage.addresses.getAll();
  let guid = addresses[0].guid;

  let address = profileStorage.addresses.get(guid);
  do_check_record_matches(address, TEST_ADDRESS_1);

  // Modifying output shouldn't affect the storage.
  address.organization = "test";
  do_check_record_matches(profileStorage.addresses.get(guid), TEST_ADDRESS_1);

  do_check_eq(profileStorage.addresses.get("INVALID_GUID"), null);
});

add_task(async function test_getByFilter() {
  let path = getTempFile(TEST_STORE_FILE_NAME).path;
  await prepareTestRecords(path);

  let profileStorage = new ProfileStorage(path);
  await profileStorage.initialize();

  let filter = {info: {fieldName: "street-address"}, searchString: "Some"};
  let addresses = profileStorage.addresses.getByFilter(filter);
  do_check_eq(addresses.length, 1);
  do_check_record_matches(addresses[0], TEST_ADDRESS_2);

  filter = {info: {fieldName: "country"}, searchString: "u"};
  addresses = profileStorage.addresses.getByFilter(filter);
  do_check_eq(addresses.length, 2);
  do_check_record_matches(addresses[0], TEST_ADDRESS_1);
  do_check_record_matches(addresses[1], TEST_ADDRESS_2);

  filter = {info: {fieldName: "street-address"}, searchString: "test"};
  addresses = profileStorage.addresses.getByFilter(filter);
  do_check_eq(addresses.length, 0);

  filter = {info: {fieldName: "street-address"}, searchString: ""};
  addresses = profileStorage.addresses.getByFilter(filter);
  do_check_eq(addresses.length, 2);

  // Check if the filtering logic is free from searching special chars.
  filter = {info: {fieldName: "street-address"}, searchString: ".*"};
  addresses = profileStorage.addresses.getByFilter(filter);
  do_check_eq(addresses.length, 0);

  // Prevent broken while searching the property that does not exist.
  filter = {info: {fieldName: "tel"}, searchString: "1"};
  addresses = profileStorage.addresses.getByFilter(filter);
  do_check_eq(addresses.length, 0);
});

add_task(async function test_add() {
  let path = getTempFile(TEST_STORE_FILE_NAME).path;
  await prepareTestRecords(path);

  let profileStorage = new ProfileStorage(path);
  await profileStorage.initialize();

  let addresses = profileStorage.addresses.getAll();

  do_check_eq(addresses.length, 2);

  do_check_record_matches(addresses[0], TEST_ADDRESS_1);
  do_check_record_matches(addresses[1], TEST_ADDRESS_2);

  do_check_neq(addresses[0].guid, undefined);
  do_check_neq(addresses[0].timeCreated, undefined);
  do_check_eq(addresses[0].timeLastModified, addresses[0].timeCreated);
  do_check_eq(addresses[0].timeLastUsed, 0);
  do_check_eq(addresses[0].timesUsed, 0);

  Assert.throws(() => profileStorage.addresses.add(TEST_ADDRESS_WITH_INVALID_FIELD),
    /"invalidField" is not a valid field\./);
});

add_task(async function test_update() {
  let path = getTempFile(TEST_STORE_FILE_NAME).path;
  await prepareTestRecords(path);

  let profileStorage = new ProfileStorage(path);
  await profileStorage.initialize();

  let addresses = profileStorage.addresses.getAll();
  let guid = addresses[1].guid;
  let timeLastModified = addresses[1].timeLastModified;

  let onChanged = TestUtils.topicObserved("formautofill-storage-changed",
                                          (subject, data) => data == "update");

  do_check_neq(addresses[1].country, undefined);

  profileStorage.addresses.update(guid, TEST_ADDRESS_3);
  await onChanged;
  await profileStorage._saveImmediately();

  profileStorage = new ProfileStorage(path);
  await profileStorage.initialize();

  let address = profileStorage.addresses.get(guid);

  do_check_eq(address.country, undefined);
  do_check_neq(address.timeLastModified, timeLastModified);
  do_check_record_matches(address, TEST_ADDRESS_3);

  Assert.throws(
    () => profileStorage.addresses.update("INVALID_GUID", TEST_ADDRESS_3),
    /No matching record\./
  );

  Assert.throws(
    () => profileStorage.addresses.update(guid, TEST_ADDRESS_WITH_INVALID_FIELD),
    /"invalidField" is not a valid field\./
  );
});

add_task(async function test_notifyUsed() {
  let path = getTempFile(TEST_STORE_FILE_NAME).path;
  await prepareTestRecords(path);

  let profileStorage = new ProfileStorage(path);
  await profileStorage.initialize();

  let addresses = profileStorage.addresses.getAll();
  let guid = addresses[1].guid;
  let timeLastUsed = addresses[1].timeLastUsed;
  let timesUsed = addresses[1].timesUsed;

  let onChanged = TestUtils.topicObserved("formautofill-storage-changed",
                                          (subject, data) => data == "notifyUsed");

  profileStorage.addresses.notifyUsed(guid);
  await onChanged;
  await profileStorage._saveImmediately();

  profileStorage = new ProfileStorage(path);
  await profileStorage.initialize();

  let address = profileStorage.addresses.get(guid);

  do_check_eq(address.timesUsed, timesUsed + 1);
  do_check_neq(address.timeLastUsed, timeLastUsed);

  Assert.throws(() => profileStorage.addresses.notifyUsed("INVALID_GUID"),
    /No matching record\./);
});

add_task(async function test_remove() {
  let path = getTempFile(TEST_STORE_FILE_NAME).path;
  await prepareTestRecords(path);

  let profileStorage = new ProfileStorage(path);
  await profileStorage.initialize();

  let addresses = profileStorage.addresses.getAll();
  let guid = addresses[1].guid;

  let onChanged = TestUtils.topicObserved("formautofill-storage-changed",
                                          (subject, data) => data == "remove");

  do_check_eq(addresses.length, 2);

  profileStorage.addresses.remove(guid);
  await onChanged;
  await profileStorage._saveImmediately();

  profileStorage = new ProfileStorage(path);
  await profileStorage.initialize();

  addresses = profileStorage.addresses.getAll();

  do_check_eq(addresses.length, 1);

  do_check_eq(profileStorage.addresses.get(guid), null);
});
