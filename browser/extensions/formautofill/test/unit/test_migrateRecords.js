/**
 * Tests the migration algorithm in profileStorage.
 */

"use strict";

let FormAutofillStorage;
add_task(async function setup() {
  ({ FormAutofillStorage } = ChromeUtils.import(
    "resource://autofill/FormAutofillStorage.jsm"
  ));
});

const TEST_STORE_FILE_NAME = "test-profile.json";

const ADDRESS_SCHEMA_VERSION = 1;
const CREDIT_CARD_SCHEMA_VERSION = 3;

const ADDRESS_TESTCASES = [
  {
    description:
      "The record version is equal to the current version. The migration shouldn't be invoked.",
    record: {
      guid: "test-guid",
      version: ADDRESS_SCHEMA_VERSION,
      "given-name": "Timothy",
      name: "John", // The cached name field doesn't align "given-name" but it
      // won't be recomputed because the migration isn't invoked.
    },
    expectedResult: {
      guid: "test-guid",
      version: ADDRESS_SCHEMA_VERSION,
      "given-name": "Timothy",
      name: "John",
    },
  },
  {
    description:
      "The record version is greater than the current version. The migration shouldn't be invoked.",
    record: {
      guid: "test-guid",
      version: 99,
      "given-name": "Timothy",
      name: "John",
    },
    expectedResult: {
      guid: "test-guid",
      version: 99,
      "given-name": "Timothy",
      name: "John",
    },
  },
  {
    description:
      "The record version is less than the current version. The migration should be invoked.",
    record: {
      guid: "test-guid",
      version: 0,
      "given-name": "Timothy",
      name: "John",
    },
    expectedResult: {
      guid: "test-guid",
      version: ADDRESS_SCHEMA_VERSION,
      "given-name": "Timothy",
      name: "Timothy",
    },
  },
  {
    description:
      "The record version is omitted. The migration should be invoked.",
    record: {
      guid: "test-guid",
      "given-name": "Timothy",
      name: "John",
    },
    expectedResult: {
      guid: "test-guid",
      version: ADDRESS_SCHEMA_VERSION,
      "given-name": "Timothy",
      name: "Timothy",
    },
  },
  {
    description:
      "The record version is an invalid value. The migration should be invoked.",
    record: {
      guid: "test-guid",
      version: "ABCDE",
      "given-name": "Timothy",
      name: "John",
    },
    expectedResult: {
      guid: "test-guid",
      version: ADDRESS_SCHEMA_VERSION,
      "given-name": "Timothy",
      name: "Timothy",
    },
  },
  {
    description:
      "The omitted computed fields should be always recomputed even the record version is up-to-date.",
    record: {
      guid: "test-guid",
      version: ADDRESS_SCHEMA_VERSION,
      "given-name": "Timothy",
    },
    expectedResult: {
      guid: "test-guid",
      version: ADDRESS_SCHEMA_VERSION,
      "given-name": "Timothy",
      name: "Timothy",
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
      "The record version is an invalid value. The migration should be invoked.",
    record: {
      guid: "test-guid",
      version: "ABCDE",
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
