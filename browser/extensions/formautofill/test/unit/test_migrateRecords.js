/**
 * Tests the migration algorithm in profileStorage.
 */

"use strict";

let FormAutofillStorage;
add_setup(async () => {
  ({ FormAutofillStorage } = ChromeUtils.importESModule(
    "resource://autofill/FormAutofillStorage.sys.mjs"
  ));
});

const TEST_STORE_FILE_NAME = "test-profile.json";

const { ADDRESS_SCHEMA_VERSION } = ChromeUtils.importESModule(
  "resource://autofill/FormAutofillStorageBase.sys.mjs"
);
const { CREDIT_CARD_SCHEMA_VERSION } = ChromeUtils.importESModule(
  "resource://autofill/FormAutofillStorageBase.sys.mjs"
);

const ADDRESS_TESTCASES = [
  {
    description:
      "The record version is equal to the current version. The migration shouldn't be invoked.",
    record: {
      guid: "test-guid",
      version: ADDRESS_SCHEMA_VERSION,
      // The cached address-line1 field doesn't align "street-address" but it
      // won't be recomputed because the migration isn't invoked.
      "address-line1": "Some Address",
      "street-address": "32 Vassar Street",
    },
    expectedResult: {
      guid: "test-guid",
      version: ADDRESS_SCHEMA_VERSION,
      "address-line1": "Some Address",
      "street-address": "32 Vassar Street",
    },
  },
  {
    description:
      "The record version is greater than the current version. The migration shouldn't be invoked.",
    record: {
      guid: "test-guid",
      version: 99,
      "address-line1": "Some Address",
      "street-address": "32 Vassar Street",
    },
    expectedResult: {
      guid: "test-guid",
      version: 99,
      "address-line1": "Some Address",
      "street-address": "32 Vassar Street",
    },
  },
  {
    description:
      "The record version is less than the current version. The migration should be invoked.",
    record: {
      guid: "test-guid",
      version: 0,
      "address-line1": "Some Address",
      "street-address": "32 Vassar Street",
    },
    expectedResult: {
      guid: "test-guid",
      version: ADDRESS_SCHEMA_VERSION,
      "address-line1": "32 Vassar Street",
      "street-address": "32 Vassar Street",
    },
  },
  {
    description:
      "The record version is omitted. The migration should be invoked.",
    record: {
      guid: "test-guid",
      "address-line1": "Some Address",
      "street-address": "32 Vassar Street",
      "unknown-1": "an unknown field from another client",
    },
    expectedResult: {
      guid: "test-guid",
      version: ADDRESS_SCHEMA_VERSION,
      "address-line1": "32 Vassar Street",
      "street-address": "32 Vassar Street",
      "unknown-1": "an unknown field from another client",
    },
  },
  {
    description:
      "The record version is an invalid value. The migration should be invoked.",
    record: {
      guid: "test-guid",
      version: "ABCDE",
      "address-line1": "Some Address",
      "street-address": "32 Vassar Street",
      "unknown-1": "an unknown field from another client",
    },
    expectedResult: {
      guid: "test-guid",
      version: ADDRESS_SCHEMA_VERSION,
      "address-line1": "32 Vassar Street",
      "street-address": "32 Vassar Street",
      "unknown-1": "an unknown field from another client",
    },
  },
  {
    description:
      "The omitted computed fields should be always recomputed even the record version is up-to-date.",
    record: {
      guid: "test-guid",
      version: ADDRESS_SCHEMA_VERSION,
      "street-address": "32 Vassar Street",
    },
    expectedResult: {
      guid: "test-guid",
      version: ADDRESS_SCHEMA_VERSION,
      "address-line1": "32 Vassar Street",
      "street-address": "32 Vassar Street",
    },
  },
  {
    description: "The migration shouldn't be invoked on tombstones.",
    record: {
      guid: "test-guid",
      timeLastModified: 12345,
      deleted: true,
    },
    expectedResult: {
      guid: "test-guid",
      timeLastModified: 12345,
      deleted: true,

      // Make sure no new fields are appended.
      version: undefined,
      name: undefined,
    },
  },

  // Bug 1836438 - Migrate "*-name" to "name"
  {
    description:
      "Migrate address - `given-name`, `additional-name`, and `family-name` should be migrated to `name`",
    record: {
      guid: "test-guid",
      version: ADDRESS_SCHEMA_VERSION,
      "given-name": "Timothy",
      "additional-name": "John",
      "family-name": "Berners-Lee",
    },
    expectedResult: {
      guid: "test-guid",
      version: ADDRESS_SCHEMA_VERSION,
      name: "Timothy John Berners-Lee",
    },
  },
  {
    description: "Migrate address - `given-name` should be migrated to `name`",
    record: {
      guid: "test-guid",
      version: ADDRESS_SCHEMA_VERSION,
      "given-name": "Timothy",
    },
    expectedResult: {
      guid: "test-guid",
      version: ADDRESS_SCHEMA_VERSION,
      name: "Timothy",
    },
  },
  {
    description:
      "Migrate address - `additional-name` should be migrated to `name`",
    record: {
      guid: "test-guid",
      version: ADDRESS_SCHEMA_VERSION,
      "additional-name": "John",
    },
    expectedResult: {
      guid: "test-guid",
      version: ADDRESS_SCHEMA_VERSION,
      name: "John",
    },
  },
  {
    description: "Migrate address - `family-name` should be migrated to `name`",
    record: {
      guid: "test-guid",
      version: ADDRESS_SCHEMA_VERSION,
      "family-name": "Berners-Lee",
    },
    expectedResult: {
      guid: "test-guid",
      version: ADDRESS_SCHEMA_VERSION,
      name: "Berners-Lee",
    },
  },
  {
    description:
      "Migrate address - `name` should still be empty when there is no *-name fields in the record",
    record: {
      guid: "test-guid",
      version: ADDRESS_SCHEMA_VERSION,
    },
    expectedResult: {
      guid: "test-guid",
      version: ADDRESS_SCHEMA_VERSION,
    },
  },
  {
    description:
      "Migrate address - do not run migration as long as the name field exists",
    record: {
      guid: "test-guid",
      version: ADDRESS_SCHEMA_VERSION,
      // The cached field doesn't align "name" but it
      // won't be recomputed because the migration isn't invoked.
      "given-name": "Timothy",
      "additional-name": "John",
      "family-name": "Berners-Lee",
      name: "Jane",
    },
    expectedResult: {
      guid: "test-guid",
      version: ADDRESS_SCHEMA_VERSION,
      "given-name": "Timothy",
      "additional-name": "John",
      "family-name": "Berners-Lee",
      name: "Jane",
    },
  },
];

const CREDIT_CARD_TESTCASES = [
  {
    description:
      "The record version is equal to the current version. The migration shouldn't be invoked.",
    record: {
      guid: "test-guid",
      version: CREDIT_CARD_SCHEMA_VERSION,
      "cc-name": "Timothy",
      "cc-given-name": "John", // The cached "cc-given-name" field doesn't align
      // "cc-name" but it won't be recomputed because
      // the migration isn't invoked.
    },
    expectedResult: {
      guid: "test-guid",
      version: CREDIT_CARD_SCHEMA_VERSION,
      "cc-name": "Timothy",
      "cc-given-name": "John",
    },
  },
  {
    description:
      "The record version is greater than the current version. The migration shouldn't be invoked.",
    record: {
      guid: "test-guid",
      version: 99,
      "cc-name": "Timothy",
      "cc-given-name": "John",
    },
    expectedResult: {
      guid: "test-guid",
      version: 99,
      "cc-name": "Timothy",
      "cc-given-name": "John",
    },
  },
  {
    description:
      "The record version is less than the current version. The migration should be invoked.",
    record: {
      guid: "test-guid",
      version: 0,
      "cc-name": "Timothy",
      "cc-given-name": "John",
    },
    expectedResult: {
      guid: "test-guid",
      version: CREDIT_CARD_SCHEMA_VERSION,
      "cc-name": "Timothy",
      "cc-given-name": "Timothy",
    },
  },
  {
    description:
      "The record version is omitted. The migration should be invoked.",
    record: {
      guid: "test-guid",
      "cc-name": "Timothy",
      "cc-given-name": "John",
      "unknown-1": "an unknown field from another client",
    },
    expectedResult: {
      guid: "test-guid",
      version: CREDIT_CARD_SCHEMA_VERSION,
      "cc-name": "Timothy",
      "cc-given-name": "Timothy",
      "unknown-1": "an unknown field from another client",
    },
  },
  {
    description:
      "The record version is an invalid value. The migration should be invoked.",
    record: {
      guid: "test-guid",
      version: "ABCDE",
      "cc-name": "Timothy",
      "cc-given-name": "John",
      "unknown-1": "an unknown field from another client",
    },
    expectedResult: {
      guid: "test-guid",
      version: CREDIT_CARD_SCHEMA_VERSION,
      "cc-name": "Timothy",
      "cc-given-name": "Timothy",
      "unknown-1": "an unknown field from another client",
    },
  },
  {
    description:
      "The omitted computed fields should be always recomputed even the record version is up-to-date.",
    record: {
      guid: "test-guid",
      version: CREDIT_CARD_SCHEMA_VERSION,
      "cc-name": "Timothy",
    },
    expectedResult: {
      guid: "test-guid",
      version: CREDIT_CARD_SCHEMA_VERSION,
      "cc-name": "Timothy",
      "cc-given-name": "Timothy",
    },
  },
  {
    description: "The migration shouldn't be invoked on tombstones.",
    record: {
      guid: "test-guid",
      timeLastModified: 12345,
      deleted: true,
    },
    expectedResult: {
      guid: "test-guid",
      timeLastModified: 12345,
      deleted: true,

      // Make sure no new fields are appended.
      version: undefined,
      "cc-given-name": undefined,
    },
  },
];

let do_check_record_matches = (expectedRecord, record) => {
  for (let key in expectedRecord) {
    Assert.equal(expectedRecord[key], record[key]);
  }
};

add_task(async function test_migrateAddressRecords() {
  let path = getTempFile(TEST_STORE_FILE_NAME).path;

  let profileStorage = new FormAutofillStorage(path);
  await profileStorage.initialize();

  for (let testcase of ADDRESS_TESTCASES) {
    info(testcase.description);
    profileStorage._store.data.addresses = [testcase.record];
    await profileStorage.addresses._migrateRecord(testcase.record, 0);
    do_check_record_matches(
      testcase.expectedResult,
      profileStorage.addresses._data[0]
    );
  }
});

add_task(async function test_migrateCreditCardRecords() {
  let path = getTempFile(TEST_STORE_FILE_NAME).path;

  let profileStorage = new FormAutofillStorage(path);
  await profileStorage.initialize();

  for (let testcase of CREDIT_CARD_TESTCASES) {
    info(testcase.description);
    profileStorage._store.data.creditCards = [testcase.record];
    await profileStorage.creditCards._migrateRecord(testcase.record, 0);
    do_check_record_matches(
      testcase.expectedResult,
      profileStorage.creditCards._data[0]
    );
  }
});

add_task(async function test_migrateEncryptedCreditCardNumber() {
  let path = getTempFile(TEST_STORE_FILE_NAME).path;

  let profileStorage = new FormAutofillStorage(path);
  await profileStorage.initialize();

  info("v1 and v2 schema cards should be abandoned.");

  let v1record = {
    guid: "test-guid1",
    version: 1,
    "cc-name": "Timothy",
    "cc-number-encrypted": "aaaa",
  };

  let v2record = {
    guid: "test-guid2",
    version: 2,
    "cc-name": "Bob",
    "cc-number-encrypted": "bbbb",
  };

  profileStorage._store.data.creditCards = [v1record, v2record];
  await profileStorage.creditCards._migrateRecord(v1record, 0);
  await profileStorage.creditCards._migrateRecord(v2record, 1);
  v1record = profileStorage.creditCards._data[0];
  v2record = profileStorage.creditCards._data[1];

  Assert.ok(v1record.deleted);
  Assert.ok(v2record.deleted);
});

add_task(async function test_migrateDeprecatedCreditCardV4() {
  let path = getTempFile(TEST_STORE_FILE_NAME).path;

  let profileStorage = new FormAutofillStorage(path);
  await profileStorage.initialize();

  let records = [
    {
      guid: "test-guid1",
      version: CREDIT_CARD_SCHEMA_VERSION,
      "cc-name": "Alice",
      _sync: {
        changeCounter: 0,
        lastSyncedFields: {},
      },
    },
    {
      guid: "test-guid2",
      version: 4,
      "cc-name": "Timothy",
      _sync: {
        changeCounter: 0,
        lastSyncedFields: {},
      },
    },
    {
      guid: "test-guid3",
      version: 4,
      "cc-name": "Bob",
    },
  ];

  profileStorage._store.data.creditCards = records;
  for (let idx = 0; idx < records.length; idx++) {
    await profileStorage.creditCards._migrateRecord(records[idx], idx);
  }

  profileStorage.creditCards.pullSyncChanges();

  // Record that has already synced before, do not sync again
  equal(getSyncChangeCounter(profileStorage.creditCards, records[0].guid), 0);

  // alaways force sync v4 record
  equal(records[1].version, CREDIT_CARD_SCHEMA_VERSION);
  equal(getSyncChangeCounter(profileStorage.creditCards, records[1].guid), 1);

  equal(records[2].version, CREDIT_CARD_SCHEMA_VERSION);
  equal(getSyncChangeCounter(profileStorage.creditCards, records[2].guid), 1);
});
