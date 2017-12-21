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
  "given-name": "Timothy",
  "family-name": "Berners-Lee",
  "street-address": "Other Address",
  "postal-code": "12345",
};

const TEST_ADDRESS_4 = {
  "given-name": "Timothy",
  "additional-name": "John",
  "family-name": "Berners-Lee",
  organization: "World Wide Web Consortium",
};

const TEST_ADDRESS_WITH_EMPTY_FIELD = {
  name: "Tim Berners",
  "street-address": "",
};

const TEST_ADDRESS_WITH_EMPTY_COMPUTED_FIELD = {
  name: "",
  "address-line1": "",
  "address-line2": "",
  "address-line3": "",
  "country-name": "",
  "tel-country-code": "",
  "tel-national": "",
  "tel-area-code": "",
  "tel-local": "",
  "tel-local-prefix": "",
  "tel-local-suffix": "",
  email: "timbl@w3.org",
};

const TEST_ADDRESS_WITH_INVALID_FIELD = {
  "street-address": "Another Address",
  invalidField: "INVALID",
};

const TEST_ADDRESS_EMPTY_AFTER_NORMALIZE = {
  country: "XXXXXX",
};

const TEST_ADDRESS_EMPTY_AFTER_UPDATE_ADDRESS_2 = {
  "street-address": "",
  country: "XXXXXX",
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
    description: "Loose merge a subset",
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
    noNeedToUpdate: true,
  },
  {
    description: "Strict merge a subset without empty string",
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
    strict: true,
    noNeedToUpdate: true,
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
  {
    description: "Merge an address with multi-line street-address in storage and single-line incoming one",
    addressInStorage: {
      "given-name": "Timothy",
      "street-address": "331 E. Evelyn Avenue\nLine2",
      "tel": "+16509030800",
    },
    addressToMerge: {
      "street-address": "331 E. Evelyn Avenue Line2",
      "tel": "+16509030800",
      country: "US",
    },
    expectedAddress: {
      "given-name": "Timothy",
      "street-address": "331 E. Evelyn Avenue\nLine2",
      "tel": "+16509030800",
      country: "US",
    },
  },
  {
    description: "Merge an address with 3-line street-address in storage and 2-line incoming one",
    addressInStorage: {
      "given-name": "Timothy",
      "street-address": "331 E. Evelyn Avenue\nLine2\nLine3",
      "tel": "+16509030800",
    },
    addressToMerge: {
      "street-address": "331 E. Evelyn Avenue\nLine2 Line3",
      "tel": "+16509030800",
      country: "US",
    },
    expectedAddress: {
      "given-name": "Timothy",
      "street-address": "331 E. Evelyn Avenue\nLine2\nLine3",
      "tel": "+16509030800",
      country: "US",
    },
  },
  {
    description: "Merge an address with single-line street-address in storage and multi-line incoming one",
    addressInStorage: {
      "given-name": "Timothy",
      "street-address": "331 E. Evelyn Avenue Line2",
      "tel": "+16509030800",
    },
    addressToMerge: {
      "street-address": "331 E. Evelyn Avenue\nLine2",
      "tel": "+16509030800",
      country: "US",
    },
    expectedAddress: {
      "given-name": "Timothy",
      "street-address": "331 E. Evelyn Avenue\nLine2",
      "tel": "+16509030800",
      country: "US",
    },
  },
  {
    description: "Merge an address with 2-line street-address in storage and 3-line incoming one",
    addressInStorage: {
      "given-name": "Timothy",
      "street-address": "331 E. Evelyn Avenue\nLine2 Line3",
      "tel": "+16509030800",
    },
    addressToMerge: {
      "street-address": "331 E. Evelyn Avenue\nLine2\nLine3",
      "tel": "+16509030800",
      country: "US",
    },
    expectedAddress: {
      "given-name": "Timothy",
      "street-address": "331 E. Evelyn Avenue\nLine2\nLine3",
      "tel": "+16509030800",
      country: "US",
    },
  },
  {
    description: "Merge an address with the same amount of lines",
    addressInStorage: {
      "given-name": "Timothy",
      "street-address": "331 E. Evelyn Avenue\nLine2\nLine3",
      "tel": "+16509030800",
    },
    addressToMerge: {
      "street-address": "331 E. Evelyn\nAvenue Line2\nLine3",
      "tel": "+16509030800",
      country: "US",
    },
    expectedAddress: {
      "given-name": "Timothy",
      "street-address": "331 E. Evelyn Avenue\nLine2\nLine3",
      "tel": "+16509030800",
      country: "US",
    },
  },
];

let do_check_record_matches = (recordWithMeta, record) => {
  for (let key in record) {
    Assert.equal(recordWithMeta[key], record[key]);
  }
};

add_task(async function test_initialize() {
  let profileStorage = await initProfileStorage(TEST_STORE_FILE_NAME);

  Assert.equal(profileStorage._store.data.version, 1);
  Assert.equal(profileStorage._store.data.addresses.length, 0);

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

  Assert.equal(addresses.length, 2);
  do_check_record_matches(addresses[0], TEST_ADDRESS_1);
  do_check_record_matches(addresses[1], TEST_ADDRESS_2);

  // Check computed fields.
  Assert.equal(addresses[0].name, "Timothy John Berners-Lee");
  Assert.equal(addresses[0]["address-line1"], "32 Vassar Street");
  Assert.equal(addresses[0]["address-line2"], "MIT Room 32-G524");

  // Test with rawData set.
  addresses = profileStorage.addresses.getAll({rawData: true});
  Assert.equal(addresses[0].name, undefined);
  Assert.equal(addresses[0]["address-line1"], undefined);
  Assert.equal(addresses[0]["address-line2"], undefined);

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
  Assert.equal(address.name, undefined);
  Assert.equal(address["address-line1"], undefined);
  Assert.equal(address["address-line2"], undefined);

  // Modifying output shouldn't affect the storage.
  address.organization = "test";
  do_check_record_matches(profileStorage.addresses.get(guid), TEST_ADDRESS_1);

  Assert.equal(profileStorage.addresses.get("INVALID_GUID"), null);
});

add_task(async function test_add() {
  let profileStorage = await initProfileStorage(TEST_STORE_FILE_NAME,
                                                [TEST_ADDRESS_1, TEST_ADDRESS_2]);

  let addresses = profileStorage.addresses.getAll();

  Assert.equal(addresses.length, 2);

  do_check_record_matches(addresses[0], TEST_ADDRESS_1);
  do_check_record_matches(addresses[1], TEST_ADDRESS_2);

  Assert.notEqual(addresses[0].guid, undefined);
  Assert.equal(addresses[0].version, 1);
  Assert.notEqual(addresses[0].timeCreated, undefined);
  Assert.equal(addresses[0].timeLastModified, addresses[0].timeCreated);
  Assert.equal(addresses[0].timeLastUsed, 0);
  Assert.equal(addresses[0].timesUsed, 0);

  // Empty string should be deleted before saving.
  profileStorage.addresses.add(TEST_ADDRESS_WITH_EMPTY_FIELD);
  let address = profileStorage.addresses._data[2];
  Assert.equal(address.name, TEST_ADDRESS_WITH_EMPTY_FIELD.name);
  Assert.equal(address["street-address"], undefined);

  // Empty computed fields shouldn't cause any problem.
  profileStorage.addresses.add(TEST_ADDRESS_WITH_EMPTY_COMPUTED_FIELD);
  address = profileStorage.addresses._data[3];
  Assert.equal(address.email, TEST_ADDRESS_WITH_EMPTY_COMPUTED_FIELD.email);

  Assert.throws(() => profileStorage.addresses.add(TEST_ADDRESS_WITH_INVALID_FIELD),
    /"invalidField" is not a valid field\./);

  Assert.throws(() => profileStorage.addresses.add({}),
    /Record contains no valid field\./);

  Assert.throws(() => profileStorage.addresses.add(TEST_ADDRESS_EMPTY_AFTER_NORMALIZE),
    /Record contains no valid field\./);
});

add_task(async function test_update() {
  let profileStorage = await initProfileStorage(TEST_STORE_FILE_NAME,
                                                [TEST_ADDRESS_1, TEST_ADDRESS_2]);

  let addresses = profileStorage.addresses.getAll();
  let guid = addresses[1].guid;
  let timeLastModified = addresses[1].timeLastModified;

  let onChanged = TestUtils.topicObserved(
    "formautofill-storage-changed",
    (subject, data) =>
      data == "update" && subject.QueryInterface(Ci.nsISupportsString).data == guid
  );

  Assert.notEqual(addresses[1].country, undefined);

  profileStorage.addresses.update(guid, TEST_ADDRESS_3);
  await onChanged;
  await profileStorage._saveImmediately();

  profileStorage.addresses.pullSyncChanges(); // force sync metadata, which we check below.

  let address = profileStorage.addresses.get(guid, {rawData: true});

  Assert.equal(address.country, undefined);
  Assert.notEqual(address.timeLastModified, timeLastModified);
  do_check_record_matches(address, TEST_ADDRESS_3);
  Assert.equal(getSyncChangeCounter(profileStorage.addresses, guid), 1);

  // Test preserveOldProperties parameter and field with empty string.
  profileStorage.addresses.update(guid, TEST_ADDRESS_WITH_EMPTY_FIELD, true);
  await onChanged;
  await profileStorage._saveImmediately();

  profileStorage.addresses.pullSyncChanges(); // force sync metadata, which we check below.

  address = profileStorage.addresses.get(guid, {rawData: true});

  Assert.equal(address["given-name"], "Tim");
  Assert.equal(address["family-name"], "Berners");
  Assert.equal(address["street-address"], undefined);
  Assert.equal(address["postal-code"], "12345");
  Assert.notEqual(address.timeLastModified, timeLastModified);
  Assert.equal(getSyncChangeCounter(profileStorage.addresses, guid), 2);

  // Empty string should be deleted while updating.
  profileStorage.addresses.update(profileStorage.addresses._data[0].guid, TEST_ADDRESS_WITH_EMPTY_FIELD);
  address = profileStorage.addresses._data[0];
  Assert.equal(address.name, TEST_ADDRESS_WITH_EMPTY_FIELD.name);
  Assert.equal(address["street-address"], undefined);

  // Empty computed fields shouldn't cause any problem.
  profileStorage.addresses.update(profileStorage.addresses._data[0].guid, TEST_ADDRESS_WITH_EMPTY_COMPUTED_FIELD, false);
  address = profileStorage.addresses._data[0];
  Assert.equal(address.email, TEST_ADDRESS_WITH_EMPTY_COMPUTED_FIELD.email);
  profileStorage.addresses.update(profileStorage.addresses._data[1].guid, TEST_ADDRESS_WITH_EMPTY_COMPUTED_FIELD, true);
  address = profileStorage.addresses._data[1];
  Assert.equal(address.email, TEST_ADDRESS_WITH_EMPTY_COMPUTED_FIELD.email);

  Assert.throws(
    () => profileStorage.addresses.update("INVALID_GUID", TEST_ADDRESS_3),
    /No matching record\./
  );

  Assert.throws(
    () => profileStorage.addresses.update(guid, TEST_ADDRESS_WITH_INVALID_FIELD),
    /"invalidField" is not a valid field\./
  );

  Assert.throws(
    () => profileStorage.addresses.update(guid, {}),
    /Record contains no valid field\./
  );

  Assert.throws(
    () => profileStorage.addresses.update(guid, TEST_ADDRESS_EMPTY_AFTER_NORMALIZE),
    /Record contains no valid field\./
  );

  profileStorage.addresses.update(guid, TEST_ADDRESS_2);
  Assert.throws(
    () => profileStorage.addresses.update(guid, TEST_ADDRESS_EMPTY_AFTER_UPDATE_ADDRESS_2),
    /Record contains no valid field\./
  );
});

add_task(async function test_notifyUsed() {
  let profileStorage = await initProfileStorage(TEST_STORE_FILE_NAME,
                                                [TEST_ADDRESS_1, TEST_ADDRESS_2]);

  let addresses = profileStorage.addresses.getAll();
  let guid = addresses[1].guid;
  let timeLastUsed = addresses[1].timeLastUsed;
  let timesUsed = addresses[1].timesUsed;

  profileStorage.addresses.pullSyncChanges(); // force sync metadata, which we check below.
  let changeCounter = getSyncChangeCounter(profileStorage.addresses, guid);

  let onChanged = TestUtils.topicObserved("formautofill-storage-changed",
                                          (subject, data) => data == "notifyUsed");

  profileStorage.addresses.notifyUsed(guid);
  await onChanged;

  let address = profileStorage.addresses.get(guid);

  Assert.equal(address.timesUsed, timesUsed + 1);
  Assert.notEqual(address.timeLastUsed, timeLastUsed);

  // Using a record should not bump its change counter.
  Assert.equal(getSyncChangeCounter(profileStorage.addresses, guid),
    changeCounter);

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

  Assert.equal(addresses.length, 2);

  profileStorage.addresses.remove(guid);
  await onChanged;

  addresses = profileStorage.addresses.getAll();

  Assert.equal(addresses.length, 1);

  Assert.equal(profileStorage.addresses.get(guid), null);
});

MERGE_TESTCASES.forEach((testcase) => {
  add_task(async function test_merge() {
    do_print("Starting testcase: " + testcase.description);
    let profileStorage = await initProfileStorage(TEST_STORE_FILE_NAME,
                                                  [testcase.addressInStorage]);
    let addresses = profileStorage.addresses.getAll();
    let guid = addresses[0].guid;
    let timeLastModified = addresses[0].timeLastModified;

    // Merge address and verify the guid in notifyObservers subject
    let onMerged = TestUtils.topicObserved(
      "formautofill-storage-changed",
      (subject, data) =>
        data == "update" && subject.QueryInterface(Ci.nsISupportsString).data == guid
    );

    // Force to create sync metadata.
    profileStorage.addresses.pullSyncChanges();
    Assert.equal(getSyncChangeCounter(profileStorage.addresses, guid), 1);

    Assert.ok(profileStorage.addresses.mergeIfPossible(guid,
                                                       testcase.addressToMerge,
                                                       testcase.strict));
    if (!testcase.noNeedToUpdate) {
      await onMerged;
    }

    addresses = profileStorage.addresses.getAll();
    Assert.equal(addresses.length, 1);
    do_check_record_matches(addresses[0], testcase.expectedAddress);
    if (testcase.noNeedToUpdate) {
      Assert.equal(addresses[0].timeLastModified, timeLastModified);

      // No need to bump the change counter if the data is unchanged.
      Assert.equal(getSyncChangeCounter(profileStorage.addresses, guid), 1);
    } else {
      Assert.notEqual(addresses[0].timeLastModified, timeLastModified);

      // Record merging should bump the change counter.
      Assert.equal(getSyncChangeCounter(profileStorage.addresses, guid), 2);
    }
  });
});

add_task(async function test_merge_same_address() {
  let profileStorage = await initProfileStorage(TEST_STORE_FILE_NAME, [TEST_ADDRESS_1]);
  let addresses = profileStorage.addresses.getAll();
  let guid = addresses[0].guid;
  let timeLastModified = addresses[0].timeLastModified;

  // Force to create sync metadata.
  profileStorage.addresses.pullSyncChanges();
  Assert.equal(getSyncChangeCounter(profileStorage.addresses, guid), 1);

  // Merge same address will still return true but it won't update timeLastModified.
  Assert.ok(profileStorage.addresses.mergeIfPossible(guid, TEST_ADDRESS_1));
  Assert.equal(addresses[0].timeLastModified, timeLastModified);

  // ... and won't bump the change counter, either.
  Assert.equal(getSyncChangeCounter(profileStorage.addresses, guid), 1);
});

add_task(async function test_merge_unable_merge() {
  let profileStorage = await initProfileStorage(TEST_STORE_FILE_NAME,
                                                [TEST_ADDRESS_1, TEST_ADDRESS_2]);

  let addresses = profileStorage.addresses.getAll();
  let guid = addresses[1].guid;

  // Force to create sync metadata.
  profileStorage.addresses.pullSyncChanges();
  Assert.equal(getSyncChangeCounter(profileStorage.addresses, guid), 1);

  // Unable to merge because of conflict
  Assert.equal(profileStorage.addresses.mergeIfPossible(guid, TEST_ADDRESS_3), false);

  // Unable to merge because no overlap
  Assert.equal(profileStorage.addresses.mergeIfPossible(guid, TEST_ADDRESS_4), false);

  // Unable to strict merge because subset with empty string
  let subset = Object.assign({}, TEST_ADDRESS_1);
  subset.organization = "";
  Assert.equal(profileStorage.addresses.mergeIfPossible(guid, subset, true), false);

  // Shouldn't bump the change counter
  Assert.equal(getSyncChangeCounter(profileStorage.addresses, guid), 1);
});

add_task(async function test_mergeToStorage() {
  let profileStorage = await initProfileStorage(TEST_STORE_FILE_NAME,
                                                [TEST_ADDRESS_1, TEST_ADDRESS_2]);
  // Merge an address to storage
  let anotherAddress = profileStorage.addresses._clone(TEST_ADDRESS_2);
  profileStorage.addresses.add(anotherAddress);
  anotherAddress.email = "timbl@w3.org";
  Assert.equal(profileStorage.addresses.mergeToStorage(anotherAddress).length, 2);
  Assert.equal(profileStorage.addresses.getAll()[1].email, anotherAddress.email);
  Assert.equal(profileStorage.addresses.getAll()[2].email, anotherAddress.email);

  // Empty computed fields shouldn't cause any problem.
  Assert.equal(profileStorage.addresses.mergeToStorage(TEST_ADDRESS_WITH_EMPTY_COMPUTED_FIELD).length, 3);
});

add_task(async function test_mergeToStorage_strict() {
  let profileStorage = await initProfileStorage(TEST_STORE_FILE_NAME,
                                                [TEST_ADDRESS_1, TEST_ADDRESS_2]);
  // Try to merge a subset with empty string
  let anotherAddress = profileStorage.addresses._clone(TEST_ADDRESS_1);
  anotherAddress.email = "";
  Assert.equal(profileStorage.addresses.mergeToStorage(anotherAddress, true).length, 0);
  Assert.equal(profileStorage.addresses.getAll()[0].email, TEST_ADDRESS_1.email);

  // Empty computed fields shouldn't cause any problem.
  Assert.equal(profileStorage.addresses.mergeToStorage(TEST_ADDRESS_WITH_EMPTY_COMPUTED_FIELD, true).length, 1);
});
