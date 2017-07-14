/**
 * Tests the migration algorithm in profileStorage.
 */

"use strict";

const {ProfileStorage} = Cu.import("resource://formautofill/ProfileStorage.jsm", {});

const TEST_STORE_FILE_NAME = "test-profile.json";

const ADDRESS_SCHEMA_VERSION = 1;
const CREDIT_CARD_SCHEMA_VERSION = 1;

const ADDRESS_TESTCASES = [
  {
    description: "The record version is equal to the current version. The migration shouldn't be invoked.",
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
    description: "The record version is greater than the current version. The migration shouldn't be invoked.",
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
    description: "The record version is less than the current version. The migration should be invoked.",
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
    description: "The record version is omitted. The migration should be invoked.",
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
    description: "The record version is an invalid value. The migration should be invoked.",
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
    description: "The omitted computed fields should be always recomputed even the record version is up-to-date.",
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
];

const CREDIT_CARD_TESTCASES = [
  {
    description: "The record version is equal to the current version. The migration shouldn't be invoked.",
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
    description: "The record version is greater than the current version. The migration shouldn't be invoked.",
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
    description: "The record version is less than the current version. The migration should be invoked.",
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
    description: "The record version is omitted. The migration should be invoked.",
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
    description: "The record version is an invalid value. The migration should be invoked.",
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
    description: "The omitted computed fields should be always recomputed even the record version is up-to-date.",
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
];

let do_check_record_matches = (expectedRecord, record) => {
  for (let key in expectedRecord) {
    do_check_eq(expectedRecord[key], record[key]);
  }
};

add_task(async function test_migrateAddressRecords() {
  let path = getTempFile(TEST_STORE_FILE_NAME).path;

  let profileStorage = new ProfileStorage(path);
  await profileStorage.initialize();

  ADDRESS_TESTCASES.forEach(testcase => {
    do_print(testcase.description);
    profileStorage.addresses._migrateRecord(testcase.record);
    do_check_record_matches(testcase.expectedResult, testcase.record);
  });
});

add_task(async function test_migrateCreditCardRecords() {
  let path = getTempFile(TEST_STORE_FILE_NAME).path;

  let profileStorage = new ProfileStorage(path);
  await profileStorage.initialize();

  CREDIT_CARD_TESTCASES.forEach(testcase => {
    do_print(testcase.description);
    profileStorage.creditCards._migrateRecord(testcase.record);
    do_check_record_matches(testcase.expectedResult, testcase.record);
  });
});
