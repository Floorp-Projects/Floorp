/**
 * Tests ProfileStorage object with addresses records.
 */

"use strict";

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
  tel: "+16172535702",
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

const TEST_ADDRESS_4 = {
  "given-name": "Timothy",
  "additional-name": "John",
  "family-name": "Berners-Lee",
  organization: "World Wide Web Consortium",
};

const TEST_ADDRESS_WITH_INVALID_FIELD = {
  "street-address": "Another Address",
  invalidField: "INVALID",
};

const MERGE_TESTCASES = [
  {
    description: "Merge a superset",
    addressInStorage: {
      "given-name": "Timothy",
      "street-address": "331 E. Evelyn Avenue",
      "tel": "+16509030800",
    },
    addressToMerge: {
      "given-name": "Timothy",
      "street-address": "331 E. Evelyn Avenue",
      "tel": "+16509030800",
      country: "US",
    },
    expectedAddress: {
      "given-name": "Timothy",
      "street-address": "331 E. Evelyn Avenue",
      "tel": "+16509030800",
      country: "US",
    },
  },
  {
    description: "Merge a subset",
    addressInStorage: {
      "given-name": "Timothy",
      "street-address": "331 E. Evelyn Avenue",
      "tel": "+16509030800",
      country: "US",
    },
    addressToMerge: {
      "given-name": "Timothy",
      "street-address": "331 E. Evelyn Avenue",
      "tel": "+16509030800",
    },
    expectedAddress: {
      "given-name": "Timothy",
      "street-address": "331 E. Evelyn Avenue",
      "tel": "+16509030800",
      country: "US",
    },
  },
  {
    description: "Merge an address with partial overlaps",
    addressInStorage: {
      "given-name": "Timothy",
      "street-address": "331 E. Evelyn Avenue",
      "tel": "+16509030800",
    },
    addressToMerge: {
      "street-address": "331 E. Evelyn Avenue",
      "tel": "+16509030800",
      country: "US",
    },
    expectedAddress: {
      "given-name": "Timothy",
      "street-address": "331 E. Evelyn Avenue",
      "tel": "+16509030800",
      country: "US",
    },
  },
];

let do_check_record_matches = (recordWithMeta, record) => {
  for (let key in record) {
    do_check_eq(recordWithMeta[key], record[key]);
  }
};

add_task(async function test_initialize() {
  let profileStorage = await initProfileStorage(TEST_STORE_FILE_NAME);

  do_check_eq(profileStorage._store.data.version, 1);
  do_check_eq(profileStorage._store.data.addresses.length, 0);

  let data = profileStorage._store.data;
  Assert.deepEqual(data.addresses, []);

  await profileStorage._saveImmediately();

  profileStorage = await initProfileStorage(TEST_STORE_FILE_NAME);

  Assert.deepEqual(profileStorage._store.data, data);
  for (let {_sync} of profileStorage._store.data.addresses) {
    Assert.ok(_sync);
    Assert.equal(_sync.changeCounter, 1);
  }
});

add_task(async function test_getAll() {
  let profileStorage = await initProfileStorage(TEST_STORE_FILE_NAME,
                                                [TEST_ADDRESS_1, TEST_ADDRESS_2]);

  let addresses = profileStorage.addresses.getAll();

  do_check_eq(addresses.length, 2);
  do_check_record_matches(addresses[0], TEST_ADDRESS_1);
  do_check_record_matches(addresses[1], TEST_ADDRESS_2);

  // Check computed fields.
  do_check_eq(addresses[0].name, "Timothy John Berners-Lee");
  do_check_eq(addresses[0]["address-line1"], "32 Vassar Street");
  do_check_eq(addresses[0]["address-line2"], "MIT Room 32-G524");

  // Test with rawData set.
  addresses = profileStorage.addresses.getAll({rawData: true});
  do_check_eq(addresses[0].name, undefined);
  do_check_eq(addresses[0]["address-line1"], undefined);
  do_check_eq(addresses[0]["address-line2"], undefined);

  // Modifying output shouldn't affect the storage.
  addresses[0].organization = "test";
  do_check_record_matches(profileStorage.addresses.getAll()[0], TEST_ADDRESS_1);
});

add_task(async function test_get() {
  let profileStorage = await initProfileStorage(TEST_STORE_FILE_NAME,
                                                [TEST_ADDRESS_1, TEST_ADDRESS_2]);

  let addresses = profileStorage.addresses.getAll();
  let guid = addresses[0].guid;

  let address = profileStorage.addresses.get(guid);
  do_check_record_matches(address, TEST_ADDRESS_1);

  // Test with rawData set.
  address = profileStorage.addresses.get(guid, {rawData: true});
  do_check_eq(address.name, undefined);
  do_check_eq(address["address-line1"], undefined);
  do_check_eq(address["address-line2"], undefined);

  // Modifying output shouldn't affect the storage.
  address.organization = "test";
  do_check_record_matches(profileStorage.addresses.get(guid), TEST_ADDRESS_1);

  do_check_eq(profileStorage.addresses.get("INVALID_GUID"), null);
});

add_task(async function test_getByFilter() {
  let profileStorage = await initProfileStorage(TEST_STORE_FILE_NAME,
                                                [TEST_ADDRESS_1, TEST_ADDRESS_2]);

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
  let profileStorage = await initProfileStorage(TEST_STORE_FILE_NAME,
                                                [TEST_ADDRESS_1, TEST_ADDRESS_2]);

  let addresses = profileStorage.addresses.getAll();

  do_check_eq(addresses.length, 2);

  do_check_record_matches(addresses[0], TEST_ADDRESS_1);
  do_check_record_matches(addresses[1], TEST_ADDRESS_2);

  do_check_neq(addresses[0].guid, undefined);
  do_check_eq(addresses[0].version, 1);
  do_check_neq(addresses[0].timeCreated, undefined);
  do_check_eq(addresses[0].timeLastModified, addresses[0].timeCreated);
  do_check_eq(addresses[0].timeLastUsed, 0);
  do_check_eq(addresses[0].timesUsed, 0);

  Assert.throws(() => profileStorage.addresses.add(TEST_ADDRESS_WITH_INVALID_FIELD),
    /"invalidField" is not a valid field\./);
});

add_task(async function test_update() {
  let profileStorage = await initProfileStorage(TEST_STORE_FILE_NAME,
                                                [TEST_ADDRESS_1, TEST_ADDRESS_2]);

  let addresses = profileStorage.addresses.getAll();
  let guid = addresses[1].guid;
  let timeLastModified = addresses[1].timeLastModified;

  let onChanged = TestUtils.topicObserved("formautofill-storage-changed",
                                          (subject, data) => data == "update");

  do_check_neq(addresses[1].country, undefined);

  profileStorage.addresses.update(guid, TEST_ADDRESS_3);
  await onChanged;
  await profileStorage._saveImmediately();

  profileStorage.addresses.pullSyncChanges(); // force sync metadata, which we check below.

  let address = profileStorage.addresses.get(guid, {rawData: true});

  do_check_eq(address.country, undefined);
  do_check_neq(address.timeLastModified, timeLastModified);
  do_check_record_matches(address, TEST_ADDRESS_3);
  do_check_eq(getSyncChangeCounter(profileStorage.addresses, guid), 1);

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
  let profileStorage = await initProfileStorage(TEST_STORE_FILE_NAME,
                                                [TEST_ADDRESS_1, TEST_ADDRESS_2]);

  let addresses = profileStorage.addresses.getAll();
  let guid = addresses[1].guid;
  let timeLastUsed = addresses[1].timeLastUsed;
  let timesUsed = addresses[1].timesUsed;

  let onChanged = TestUtils.topicObserved("formautofill-storage-changed",
                                          (subject, data) => data == "notifyUsed");

  profileStorage.addresses.notifyUsed(guid);
  await onChanged;

  let address = profileStorage.addresses.get(guid);

  do_check_eq(address.timesUsed, timesUsed + 1);
  do_check_neq(address.timeLastUsed, timeLastUsed);

  Assert.throws(() => profileStorage.addresses.notifyUsed("INVALID_GUID"),
    /No matching record\./);
});

add_task(async function test_remove() {
  let profileStorage = await initProfileStorage(TEST_STORE_FILE_NAME,
                                                [TEST_ADDRESS_1, TEST_ADDRESS_2]);

  let addresses = profileStorage.addresses.getAll();
  let guid = addresses[1].guid;

  let onChanged = TestUtils.topicObserved("formautofill-storage-changed",
                                          (subject, data) => data == "remove");

  do_check_eq(addresses.length, 2);

  profileStorage.addresses.remove(guid);
  await onChanged;

  addresses = profileStorage.addresses.getAll();

  do_check_eq(addresses.length, 1);

  do_check_eq(profileStorage.addresses.get(guid), null);
});

MERGE_TESTCASES.forEach((testcase) => {
  add_task(async function test_merge() {
    do_print("Starting testcase: " + testcase.description);
    let profileStorage = await initProfileStorage(TEST_STORE_FILE_NAME,
                                                  [testcase.addressInStorage]);
    let addresses = profileStorage.addresses.getAll();
    // Merge address and verify the guid in notifyObservers subject
    let onMerged = TestUtils.topicObserved(
      "formautofill-storage-changed",
      (subject, data) =>
        data == "merge" && subject.QueryInterface(Ci.nsISupportsString).data == addresses[0].guid
    );
    let timeLastModified = addresses[0].timeLastModified;
    Assert.equal(
      profileStorage.addresses.mergeIfPossible(addresses[0].guid, testcase.addressToMerge),
      true);
    await onMerged;
    addresses = profileStorage.addresses.getAll();
    Assert.equal(addresses.length, 1);
    Assert.notEqual(addresses[0].timeLastModified, timeLastModified);
    do_check_record_matches(addresses[0], testcase.expectedAddress);
  });
});

add_task(async function test_merge_same_address() {
  let profileStorage = await initProfileStorage(TEST_STORE_FILE_NAME, [TEST_ADDRESS_1]);
  let addresses = profileStorage.addresses.getAll();
  let timeLastModified = addresses[0].timeLastModified;
  // Merge same address will still return true but it won't update timeLastModified.
  Assert.equal(profileStorage.addresses.mergeIfPossible(addresses[0].guid, TEST_ADDRESS_1), true);
  Assert.equal(addresses[0].timeLastModified, timeLastModified);
});

add_task(async function test_merge_unable_merge() {
  let profileStorage = await initProfileStorage(TEST_STORE_FILE_NAME,
                                                [TEST_ADDRESS_1, TEST_ADDRESS_2]);

  let addresses = profileStorage.addresses.getAll();
  // Unable to merge because of conflict
  do_check_eq(profileStorage.addresses.mergeIfPossible(addresses[1].guid, TEST_ADDRESS_3), false);

  // Unable to merge because no overlap
  do_check_eq(profileStorage.addresses.mergeIfPossible(addresses[1].guid, TEST_ADDRESS_4), false);
});

add_task(async function test_mergeToStorage() {
  let profileStorage = await initProfileStorage(TEST_STORE_FILE_NAME,
                                                [TEST_ADDRESS_1, TEST_ADDRESS_2]);
  // Merge an address to storage
  let anotherAddress = profileStorage.addresses._clone(TEST_ADDRESS_2);
  profileStorage.addresses.add(anotherAddress);
  anotherAddress.email = "timbl@w3.org";
  do_check_eq(profileStorage.addresses.mergeToStorage(anotherAddress).length, 2);
  do_check_eq(profileStorage.addresses.getAll()[1].email, anotherAddress.email);
  do_check_eq(profileStorage.addresses.getAll()[2].email, anotherAddress.email);
});
