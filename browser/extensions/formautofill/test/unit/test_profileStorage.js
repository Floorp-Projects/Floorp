/**
 * Tests ProfileStorage object.
 */

"use strict";

Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://formautofill/ProfileStorage.jsm");

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

let prepareTestRecords = Task.async(function* (path) {
  let profileStorage = new ProfileStorage(path);
  yield profileStorage.initialize();

  let onChanged = TestUtils.topicObserved("formautofill-storage-changed",
                                          (subject, data) => data == "add");
  profileStorage.add(TEST_ADDRESS_1);
  yield onChanged;
  profileStorage.add(TEST_ADDRESS_2);
  yield profileStorage._saveImmediately();
});

let do_check_record_matches = (recordWithMeta, record) => {
  for (let key in record) {
    do_check_eq(recordWithMeta[key], record[key]);
  }
};

add_task(function* test_initialize() {
  let path = getTempFile(TEST_STORE_FILE_NAME).path;
  let profileStorage = new ProfileStorage(path);
  yield profileStorage.initialize();

  do_check_eq(profileStorage._store.data.version, 1);
  do_check_eq(profileStorage._store.data.addresses.length, 0);

  let data = profileStorage._store.data;

  yield profileStorage._saveImmediately();

  profileStorage = new ProfileStorage(path);
  yield profileStorage.initialize();

  Assert.deepEqual(profileStorage._store.data, data);
});

add_task(function* test_getAll() {
  let path = getTempFile(TEST_STORE_FILE_NAME).path;
  yield prepareTestRecords(path);

  let profileStorage = new ProfileStorage(path);
  yield profileStorage.initialize();

  let addresses = profileStorage.getAll();

  do_check_eq(addresses.length, 2);
  do_check_record_matches(addresses[0], TEST_ADDRESS_1);
  do_check_record_matches(addresses[1], TEST_ADDRESS_2);

  // Modifying output shouldn't affect the storage.
  addresses[0].organization = "test";
  do_check_record_matches(profileStorage.getAll()[0], TEST_ADDRESS_1);
});

add_task(function* test_get() {
  let path = getTempFile(TEST_STORE_FILE_NAME).path;
  yield prepareTestRecords(path);

  let profileStorage = new ProfileStorage(path);
  yield profileStorage.initialize();

  let addresses = profileStorage.getAll();
  let guid = addresses[0].guid;

  let address = profileStorage.get(guid);
  do_check_record_matches(address, TEST_ADDRESS_1);

  // Modifying output shouldn't affect the storage.
  address.organization = "test";
  do_check_record_matches(profileStorage.get(guid), TEST_ADDRESS_1);

  Assert.throws(() => profileStorage.get("INVALID_GUID"),
    /No matching record\./);
});

add_task(function* test_getByFilter() {
  let path = getTempFile(TEST_STORE_FILE_NAME).path;
  yield prepareTestRecords(path);

  let profileStorage = new ProfileStorage(path);
  yield profileStorage.initialize();

  let filter = {info: {fieldName: "street-address"}, searchString: "Some"};
  let addresses = profileStorage.getByFilter(filter);
  do_check_eq(addresses.length, 1);
  do_check_record_matches(addresses[0], TEST_ADDRESS_2);

  filter = {info: {fieldName: "country"}, searchString: "u"};
  addresses = profileStorage.getByFilter(filter);
  do_check_eq(addresses.length, 2);
  do_check_record_matches(addresses[0], TEST_ADDRESS_1);
  do_check_record_matches(addresses[1], TEST_ADDRESS_2);

  filter = {info: {fieldName: "street-address"}, searchString: "test"};
  addresses = profileStorage.getByFilter(filter);
  do_check_eq(addresses.length, 0);

  filter = {info: {fieldName: "street-address"}, searchString: ""};
  addresses = profileStorage.getByFilter(filter);
  do_check_eq(addresses.length, 2);

  // Check if the filtering logic is free from searching special chars.
  filter = {info: {fieldName: "street-address"}, searchString: ".*"};
  addresses = profileStorage.getByFilter(filter);
  do_check_eq(addresses.length, 0);
});

add_task(function* test_add() {
  let path = getTempFile(TEST_STORE_FILE_NAME).path;
  yield prepareTestRecords(path);

  let profileStorage = new ProfileStorage(path);
  yield profileStorage.initialize();

  let addresses = profileStorage.getAll();

  do_check_eq(addresses.length, 2);

  do_check_record_matches(addresses[0], TEST_ADDRESS_1);
  do_check_record_matches(addresses[1], TEST_ADDRESS_2);

  do_check_neq(addresses[0].guid, undefined);
  do_check_neq(addresses[0].timeCreated, undefined);
  do_check_eq(addresses[0].timeLastModified, addresses[0].timeCreated);
  do_check_eq(addresses[0].timeLastUsed, 0);
  do_check_eq(addresses[0].timesUsed, 0);

  Assert.throws(() => profileStorage.add(TEST_ADDRESS_WITH_INVALID_FIELD),
    /"invalidField" is not a valid field\./);
});

add_task(function* test_update() {
  let path = getTempFile(TEST_STORE_FILE_NAME).path;
  yield prepareTestRecords(path);

  let profileStorage = new ProfileStorage(path);
  yield profileStorage.initialize();

  let addresses = profileStorage.getAll();
  let guid = addresses[1].guid;
  let timeLastModified = addresses[1].timeLastModified;

  let onChanged = TestUtils.topicObserved("formautofill-storage-changed",
                                          (subject, data) => data == "update");

  do_check_neq(addresses[1].country, undefined);

  profileStorage.update(guid, TEST_ADDRESS_3);
  yield onChanged;
  yield profileStorage._saveImmediately();

  profileStorage = new ProfileStorage(path);
  yield profileStorage.initialize();

  let address = profileStorage.get(guid);

  do_check_eq(address.country, undefined);
  do_check_neq(address.timeLastModified, timeLastModified);
  do_check_record_matches(address, TEST_ADDRESS_3);

  Assert.throws(
    () => profileStorage.update("INVALID_GUID", TEST_ADDRESS_3),
    /No matching record\./
  );

  Assert.throws(
    () => profileStorage.update(guid, TEST_ADDRESS_WITH_INVALID_FIELD),
    /"invalidField" is not a valid field\./
  );
});

add_task(function* test_notifyUsed() {
  let path = getTempFile(TEST_STORE_FILE_NAME).path;
  yield prepareTestRecords(path);

  let profileStorage = new ProfileStorage(path);
  yield profileStorage.initialize();

  let addresses = profileStorage.getAll();
  let guid = addresses[1].guid;
  let timeLastUsed = addresses[1].timeLastUsed;
  let timesUsed = addresses[1].timesUsed;

  let onChanged = TestUtils.topicObserved("formautofill-storage-changed",
                                          (subject, data) => data == "notifyUsed");

  profileStorage.notifyUsed(guid);
  yield onChanged;
  yield profileStorage._saveImmediately();

  profileStorage = new ProfileStorage(path);
  yield profileStorage.initialize();

  let address = profileStorage.get(guid);

  do_check_eq(address.timesUsed, timesUsed + 1);
  do_check_neq(address.timeLastUsed, timeLastUsed);

  Assert.throws(() => profileStorage.notifyUsed("INVALID_GUID"),
    /No matching record\./);
});

add_task(function* test_remove() {
  let path = getTempFile(TEST_STORE_FILE_NAME).path;
  yield prepareTestRecords(path);

  let profileStorage = new ProfileStorage(path);
  yield profileStorage.initialize();

  let addresses = profileStorage.getAll();
  let guid = addresses[1].guid;

  let onChanged = TestUtils.topicObserved("formautofill-storage-changed",
                                          (subject, data) => data == "remove");

  do_check_eq(addresses.length, 2);

  profileStorage.remove(guid);
  yield onChanged;
  yield profileStorage._saveImmediately();

  profileStorage = new ProfileStorage(path);
  yield profileStorage.initialize();

  addresses = profileStorage.getAll();

  do_check_eq(addresses.length, 1);

  Assert.throws(() => profileStorage.get(guid), /No matching record\./);
});
