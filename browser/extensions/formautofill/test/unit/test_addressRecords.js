/**
 * Tests FormAutofillStorage object with addresses records.
 */

"use strict";

const TEST_STORE_FILE_NAME = "test-profile.json";
const COLLECTION_NAME = "addresses";

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
  "unknown-1": "an unknown field from another client",
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
  email: { email: "invalidemail" },
};

const TEST_ADDRESS_EMPTY_AFTER_NORMALIZE = {
  country: "XXXXXX",
};

ChromeUtils.defineESModuleGetters(this, {
  Preferences: "resource://gre/modules/Preferences.sys.mjs",
});

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
  for (let { _sync } of profileStorage._store.data.addresses) {
    Assert.ok(_sync);
    Assert.equal(_sync.changeCounter, 1);
  }
});

add_task(async function test_getAll() {
  let profileStorage = await initProfileStorage(TEST_STORE_FILE_NAME, [
    TEST_ADDRESS_1,
    TEST_ADDRESS_2,
  ]);

  let addresses = await profileStorage.addresses.getAll();

  Assert.equal(addresses.length, 2);
  do_check_record_matches(addresses[0], TEST_ADDRESS_1);
  do_check_record_matches(addresses[1], TEST_ADDRESS_2);

  // Check computed fields.
  Assert.equal(addresses[0].name, "Timothy John Berners-Lee");
  Assert.equal(addresses[0]["address-line1"], "32 Vassar Street");
  Assert.equal(addresses[0]["address-line2"], "MIT Room 32-G524");

  // Test with rawData set.
  addresses = await profileStorage.addresses.getAll({ rawData: true });
  Assert.equal(addresses[0].name, undefined);
  Assert.equal(addresses[0]["address-line1"], undefined);
  Assert.equal(addresses[0]["address-line2"], undefined);

  // Modifying output shouldn't affect the storage.
  addresses[0].organization = "test";
  do_check_record_matches(
    (await profileStorage.addresses.getAll())[0],
    TEST_ADDRESS_1
  );
});

add_task(async function test_get() {
  let profileStorage = await initProfileStorage(TEST_STORE_FILE_NAME, [
    TEST_ADDRESS_1,
    TEST_ADDRESS_2,
  ]);

  let addresses = await profileStorage.addresses.getAll();
  let guid = addresses[0].guid;

  let address = await profileStorage.addresses.get(guid);
  do_check_record_matches(address, TEST_ADDRESS_1);

  // Test with rawData set.
  address = await profileStorage.addresses.get(guid, { rawData: true });
  Assert.equal(address.name, undefined);
  Assert.equal(address["address-line1"], undefined);
  Assert.equal(address["address-line2"], undefined);

  // Modifying output shouldn't affect the storage.
  address.organization = "test";
  do_check_record_matches(
    await profileStorage.addresses.get(guid),
    TEST_ADDRESS_1
  );

  Assert.equal(await profileStorage.addresses.get("INVALID_GUID"), null);
});

add_task(async function test_add() {
  let profileStorage = await initProfileStorage(TEST_STORE_FILE_NAME, [
    TEST_ADDRESS_1,
    TEST_ADDRESS_2,
  ]);

  let addresses = await profileStorage.addresses.getAll();

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
  await profileStorage.addresses.add(TEST_ADDRESS_WITH_EMPTY_FIELD);
  let address = profileStorage.addresses._data[2];
  Assert.equal(address.name, TEST_ADDRESS_WITH_EMPTY_FIELD.name);
  Assert.equal(address["street-address"], undefined);

  // Empty computed fields shouldn't cause any problem.
  await profileStorage.addresses.add(TEST_ADDRESS_WITH_EMPTY_COMPUTED_FIELD);
  address = profileStorage.addresses._data[3];
  Assert.equal(address.email, TEST_ADDRESS_WITH_EMPTY_COMPUTED_FIELD.email);

  await Assert.rejects(
    profileStorage.addresses.add(TEST_ADDRESS_WITH_INVALID_FIELD),
    /"email" contains invalid data type: object/
  );

  await Assert.rejects(
    profileStorage.addresses.add({}),
    /Record contains no valid field\./
  );

  await Assert.rejects(
    profileStorage.addresses.add(TEST_ADDRESS_EMPTY_AFTER_NORMALIZE),
    /Record contains no valid field\./
  );
});

add_task(async function test_update() {
  // Test assumes that when an entry is saved a second time, it's last modified date will
  // be different from the first. With high values of precision reduction, we execute too
  // fast for that to be true.
  let timerPrecision = Preferences.get("privacy.reduceTimerPrecision");
  Preferences.set("privacy.reduceTimerPrecision", false);

  registerCleanupFunction(function () {
    Preferences.set("privacy.reduceTimerPrecision", timerPrecision);
  });

  let profileStorage = await initProfileStorage(TEST_STORE_FILE_NAME, [
    TEST_ADDRESS_1,
    TEST_ADDRESS_2,
  ]);

  let addresses = await profileStorage.addresses.getAll();
  let guid = addresses[1].guid;
  // We need to cheat a little due to race conditions of Date.now() when
  // we're running these tests, so we subtract one and test accordingly
  // in the times Date.now() returns the same timestamp
  let timeLastModified = addresses[1].timeLastModified - 1;

  let onChanged = TestUtils.topicObserved(
    "formautofill-storage-changed",
    (subject, data) =>
      data == "update" &&
      subject.wrappedJSObject.guid == guid &&
      subject.wrappedJSObject.collectionName == COLLECTION_NAME
  );

  Assert.notEqual(addresses[1].country, undefined);

  await profileStorage.addresses.update(guid, TEST_ADDRESS_3);
  await onChanged;
  await profileStorage._saveImmediately();

  profileStorage.addresses.pullSyncChanges(); // force sync metadata, which we check below.

  let address = await profileStorage.addresses.get(guid, { rawData: true });

  Assert.equal(address.country, "US");
  Assert.ok(address.timeLastModified > timeLastModified);
  do_check_record_matches(address, TEST_ADDRESS_3);
  Assert.equal(getSyncChangeCounter(profileStorage.addresses, guid), 1);

  // Test preserveOldProperties parameter and field with empty string.
  await profileStorage.addresses.update(
    guid,
    TEST_ADDRESS_WITH_EMPTY_FIELD,
    true
  );
  await onChanged;
  await profileStorage._saveImmediately();

  profileStorage.addresses.pullSyncChanges(); // force sync metadata, which we check below.

  address = await profileStorage.addresses.get(guid, { rawData: true });

  Assert.equal(address["given-name"], "Tim");
  Assert.equal(address["family-name"], "Berners");
  Assert.equal(address["street-address"], undefined);
  Assert.equal(address["postal-code"], "12345");
  Assert.notEqual(address.timeLastModified, timeLastModified);
  Assert.equal(getSyncChangeCounter(profileStorage.addresses, guid), 2);

  // Empty string should be deleted while updating.
  await profileStorage.addresses.update(
    profileStorage.addresses._data[0].guid,
    TEST_ADDRESS_WITH_EMPTY_FIELD
  );
  address = profileStorage.addresses._data[0];
  Assert.equal(address.name, TEST_ADDRESS_WITH_EMPTY_FIELD.name);
  Assert.equal(address["street-address"], undefined);
  Assert.equal(address[("unknown-1", "an unknown field from another client")]);

  // Empty computed fields shouldn't cause any problem.
  await profileStorage.addresses.update(
    profileStorage.addresses._data[0].guid,
    TEST_ADDRESS_WITH_EMPTY_COMPUTED_FIELD,
    false
  );
  address = profileStorage.addresses._data[0];
  Assert.equal(address.email, TEST_ADDRESS_WITH_EMPTY_COMPUTED_FIELD.email);
  await profileStorage.addresses.update(
    profileStorage.addresses._data[1].guid,
    TEST_ADDRESS_WITH_EMPTY_COMPUTED_FIELD,
    true
  );
  address = profileStorage.addresses._data[1];
  Assert.equal(address.email, TEST_ADDRESS_WITH_EMPTY_COMPUTED_FIELD.email);

  await Assert.rejects(
    profileStorage.addresses.update("INVALID_GUID", TEST_ADDRESS_3),
    /No matching record\./
  );

  await Assert.rejects(
    profileStorage.addresses.update(guid, TEST_ADDRESS_WITH_INVALID_FIELD),
    /"email" contains invalid data type: object/
  );

  await Assert.rejects(
    profileStorage.addresses.update(guid, {}),
    /Record contains no valid field\./
  );

  await Assert.rejects(
    profileStorage.addresses.update(guid, TEST_ADDRESS_EMPTY_AFTER_NORMALIZE),
    /Record contains no valid field\./
  );

  profileStorage.addresses.update(guid, TEST_ADDRESS_2);
});

add_task(async function test_notifyUsed() {
  let profileStorage = await initProfileStorage(TEST_STORE_FILE_NAME, [
    TEST_ADDRESS_1,
    TEST_ADDRESS_2,
  ]);

  let addresses = await profileStorage.addresses.getAll();
  let guid = addresses[1].guid;
  let timeLastUsed = addresses[1].timeLastUsed;
  let timesUsed = addresses[1].timesUsed;

  profileStorage.addresses.pullSyncChanges(); // force sync metadata, which we check below.
  let changeCounter = getSyncChangeCounter(profileStorage.addresses, guid);

  let onChanged = TestUtils.topicObserved(
    "formautofill-storage-changed",
    (subject, data) =>
      data == "notifyUsed" &&
      subject.wrappedJSObject.guid == guid &&
      subject.wrappedJSObject.collectionName == COLLECTION_NAME
  );

  profileStorage.addresses.notifyUsed(guid);
  await onChanged;

  let address = await profileStorage.addresses.get(guid);

  Assert.equal(address.timesUsed, timesUsed + 1);
  Assert.notEqual(address.timeLastUsed, timeLastUsed);

  // Using a record should not bump its change counter.
  Assert.equal(
    getSyncChangeCounter(profileStorage.addresses, guid),
    changeCounter
  );

  Assert.throws(
    () => profileStorage.addresses.notifyUsed("INVALID_GUID"),
    /No matching record\./
  );
});

add_task(async function test_remove() {
  let profileStorage = await initProfileStorage(TEST_STORE_FILE_NAME, [
    TEST_ADDRESS_1,
    TEST_ADDRESS_2,
  ]);

  let addresses = await profileStorage.addresses.getAll();
  let guid = addresses[1].guid;

  let onChanged = TestUtils.topicObserved(
    "formautofill-storage-changed",
    (subject, data) =>
      data == "remove" &&
      subject.wrappedJSObject.guid == guid &&
      subject.wrappedJSObject.collectionName == COLLECTION_NAME
  );

  Assert.equal(addresses.length, 2);

  profileStorage.addresses.remove(guid);
  await onChanged;

  addresses = await profileStorage.addresses.getAll();

  Assert.equal(addresses.length, 1);

  Assert.equal(await profileStorage.addresses.get(guid), null);
});
