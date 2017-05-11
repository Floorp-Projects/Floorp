/**
 * Tests the transform algorithm in profileStorage.
 */

"use strict";

Cu.import("resource://gre/modules/Task.jsm");
const {ProfileStorage} = Cu.import("resource://formautofill/ProfileStorage.jsm", {});

const TEST_STORE_FILE_NAME = "test-profile.json";

const COMPUTE_TESTCASES = [
  // Empty
  {
    description: "Empty address",
    address: {
    },
    expectedResult: {
    },
  },

  // Name
  {
    description: "Has split names",
    address: {
      "given-name": "Timothy",
      "additional-name": "John",
      "family-name": "Berners-Lee",
    },
    expectedResult: {
      "given-name": "Timothy",
      "additional-name": "John",
      "family-name": "Berners-Lee",
      "name": "Timothy John Berners-Lee",
    },
  },

  // Address
  {
    description: "\"street-address\" with single line",
    address: {
      "street-address": "single line",
    },
    expectedResult: {
      "street-address": "single line",
      "address-line1": "single line",
    },
  },
  {
    description: "\"street-address\" with multiple lines",
    address: {
      "street-address": "line1\nline2\nline3",
    },
    expectedResult: {
      "street-address": "line1\nline2\nline3",
      "address-line1": "line1",
      "address-line2": "line2",
      "address-line3": "line3",
    },
  },
  {
    description: "\"street-address\" with multiple lines but line2 is omitted",
    address: {
      "street-address": "line1\n\nline3",
    },
    expectedResult: {
      "street-address": "line1\n\nline3",
      "address-line1": "line1",
      "address-line2": "",
      "address-line3": "line3",
    },
  },
  {
    description: "\"street-address\" with 4 lines",
    address: {
      "street-address": "line1\nline2\nline3\nline4",
    },
    expectedResult: {
      "street-address": "line1\nline2\nline3\nline4",
      "address-line1": "line1",
      "address-line2": "line2",
      "address-line3": "line3",
    },
  },
];

const NORMALIZE_TESTCASES = [
  // Empty
  {
    description: "Empty address",
    address: {
    },
    expectedResult: {
    },
  },

  // Name
  {
    description: "Has \"name\", and the split names are omitted",
    address: {
      "name": "Timothy John Berners-Lee",
    },
    expectedResult: {
      "given-name": "Timothy",
      "additional-name": "John",
      "family-name": "Berners-Lee",
    },
  },
  {
    description: "Has both \"name\" and split names",
    address: {
      "name": "John Doe",
      "given-name": "Timothy",
      "additional-name": "John",
      "family-name": "Berners-Lee",
    },
    expectedResult: {
      "given-name": "Timothy",
      "additional-name": "John",
      "family-name": "Berners-Lee",
    },
  },
  {
    description: "Has \"name\", and some of split names are omitted",
    address: {
      "name": "John Doe",
      "given-name": "Timothy",
    },
    expectedResult: {
      "given-name": "Timothy",
      "family-name": "Doe",
    },
  },


  // Address
  {
    description: "Has \"address-line1~3\" and \"street-address\" is omitted",
    address: {
      "address-line1": "line1",
      "address-line2": "line2",
      "address-line3": "line3",
    },
    expectedResult: {
      "street-address": "line1\nline2\nline3",
    },
  },
  {
    description: "Has both \"address-line1~3\" and \"street-address\"",
    address: {
      "street-address": "street address",
      "address-line1": "line1",
      "address-line2": "line2",
      "address-line3": "line3",
    },
    expectedResult: {
      "street-address": "street address",
    },
  },
  {
    description: "Has \"address-line2~3\" and single-line \"street-address\"",
    address: {
      "street-address": "street address",
      "address-line2": "line2",
      "address-line3": "line3",
    },
    expectedResult: {
      "street-address": "street address\nline2\nline3",
    },
  },
  {
    description: "Has \"address-line2~3\" and multiple-line \"street-address\"",
    address: {
      "street-address": "street address\nstreet address line 2",
      "address-line2": "line2",
      "address-line3": "line3",
    },
    expectedResult: {
      "street-address": "street address\nstreet address line 2",
    },
  },
];

let do_check_record_matches = (expectedRecord, record) => {
  for (let key in expectedRecord) {
    do_check_eq(expectedRecord[key], record[key] || "");
  }
};

add_task(function* test_computeFields() {
  let path = getTempFile(TEST_STORE_FILE_NAME).path;

  let profileStorage = new ProfileStorage(path);
  yield profileStorage.initialize();

  COMPUTE_TESTCASES.forEach(testcase => profileStorage.add(testcase.address));
  yield profileStorage._saveImmediately();

  profileStorage = new ProfileStorage(path);
  yield profileStorage.initialize();

  let addresses = profileStorage.getAll();

  for (let i in addresses) {
    do_print("Verify testcase: " + COMPUTE_TESTCASES[i].description);
    do_check_record_matches(COMPUTE_TESTCASES[i].expectedResult, addresses[i]);
  }
});

add_task(function* test_normalizeFields() {
  let path = getTempFile(TEST_STORE_FILE_NAME).path;

  let profileStorage = new ProfileStorage(path);
  yield profileStorage.initialize();

  NORMALIZE_TESTCASES.forEach(testcase => profileStorage.add(testcase.address));
  yield profileStorage._saveImmediately();

  profileStorage = new ProfileStorage(path);
  yield profileStorage.initialize();

  let addresses = profileStorage.getAll();

  for (let i in addresses) {
    do_print("Verify testcase: " + NORMALIZE_TESTCASES[i].description);
    do_check_record_matches(NORMALIZE_TESTCASES[i].expectedResult, addresses[i]);
  }
});
