/**
 * Tests the migration algorithm in profileStorage.
 */

"use strict";

ChromeUtils.import("resource://testing-common/LoginTestUtils.jsm", this);

let FormAutofillStorage;
let OSKeyStore;
add_task(async function setup() {
  ({ FormAutofillStorage } = ChromeUtils.import(
    "resource://formautofill/FormAutofillStorage.jsm",
    null
  ));
  ({ OSKeyStore } = ChromeUtils.import(
    "resource://gre/modules/OSKeyStore.jsm"
  ));
});

const TEST_STORE_FILE_NAME = "test-profile.json";

const ADDRESS_SCHEMA_VERSION = 1;
const CREDIT_CARD_SCHEMA_VERSION = 2;

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

  const ccNumber = "4111111111111111";
  let cryptoSDR = Cc["@mozilla.org/login-manager/crypto/SDR;1"].createInstance(
    Ci.nsILoginManagerCrypto
  );

  info("Encrypted credit card should be migrated from v1 to v2");

  let record = {
    guid: "test-guid",
    version: 1,
    "cc-name": "Timothy",
    "cc-number-encrypted": cryptoSDR.encrypt(ccNumber),
  };

  let expectedRecord = {
    guid: "test-guid",
    version: CREDIT_CARD_SCHEMA_VERSION,
    "cc-name": "Timothy",
    "cc-given-name": "Timothy",
  };
  profileStorage._store.data.creditCards = [record];
  await profileStorage.creditCards._migrateRecord(record, 0);
  record = profileStorage.creditCards._data[0];

  Assert.equal(expectedRecord.guid, record.guid);
  Assert.equal(expectedRecord.version, record.version);
  Assert.equal(expectedRecord["cc-name"], record["cc-name"]);
  Assert.equal(expectedRecord["cc-given-name"], record["cc-given-name"]);

  // Ciphertext of OS Key Store is not stable, must compare decrypted text here.
  Assert.equal(
    ccNumber,
    await OSKeyStore.decrypt(record["cc-number-encrypted"])
  );
});

add_task(async function test_migrateEncryptedCreditCardNumberWithMP() {
  LoginTestUtils.masterPassword.enable();

  let path = getTempFile(TEST_STORE_FILE_NAME).path;

  let profileStorage = new FormAutofillStorage(path);
  await profileStorage.initialize();

  info("Encrypted credit card should be migrated a tombstone if MP is enabled");

  let record = {
    guid: "test-guid",
    version: 1,
    "cc-name": "Timothy",
    "cc-number-encrypted": "(encrypted to be discarded)",
  };

  profileStorage._store.data.creditCards = [record];
  await profileStorage.creditCards._migrateRecord(record, 0);
  record = profileStorage.creditCards._data[0];

  Assert.equal(record.guid, "test-guid");
  Assert.equal(record.deleted, true);
  Assert.equal(typeof record.version, "undefined");
  Assert.equal(typeof record["cc-name"], "undefined");
  Assert.equal(typeof record["cc-number-encrypted"], "undefined");

  LoginTestUtils.masterPassword.disable();
});
