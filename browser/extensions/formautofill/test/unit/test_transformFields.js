/**
 * Tests the transform algorithm in profileStorage.
 */

"use strict";

const {ProfileStorage} = Cu.import("resource://formautofill/ProfileStorage.jsm", {});

const TEST_STORE_FILE_NAME = "test-profile.json";

const ADDRESS_COMPUTE_TESTCASES = [
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
      "address-line3": "line3 line4",
    },
  },
  {
    description: "\"street-address\" with blank lines",
    address: {
      "street-address": "line1\n \nline3\n \nline5",
    },
    expectedResult: {
      "street-address": "line1\n \nline3\n \nline5",
      "address-line1": "line1",
      "address-line2": "",
      "address-line3": "line3 line5",
    },
  },

  // Country
  {
    description: "Has \"country\"",
    address: {
      "country": "US",
    },
    expectedResult: {
      "country": "US",
      "country-name": "United States",
    },
  },

  // Tel
  {
    description: "\"tel\" with US country code",
    address: {
      "tel": "+16172535702",
    },
    expectedResult: {
      "tel": "+16172535702",
      "tel-country-code": "+1",
      "tel-national": "6172535702",
      "tel-area-code": "617",
      "tel-local": "2535702",
      "tel-local-prefix": "253",
      "tel-local-suffix": "5702",
    },
  },
  {
    description: "\"tel\" with TW country code (the components won't be parsed)",
    address: {
      "tel": "+886212345678",
    },
    expectedResult: {
      "tel": "+886212345678",
      "tel-country-code": "+886",
      "tel-national": "0212345678",
      "tel-area-code": "",
      "tel-local": "",
      "tel-local-prefix": "",
      "tel-local-suffix": "",
    },
  },
  {
    description: "\"tel\" without country code so use \"US\" as default resion",
    address: {
      "tel": "6172535702",
    },
    expectedResult: {
      "tel": "+16172535702",
      "tel-country-code": "+1",
      "tel-national": "6172535702",
      "tel-area-code": "617",
      "tel-local": "2535702",
      "tel-local-prefix": "253",
      "tel-local-suffix": "5702",
    },
  },
  {
    description: "\"tel\" without country code but \"country\" is \"TW\"",
    address: {
      "tel": "0212345678",
      "country": "TW",
    },
    expectedResult: {
      "tel": "+886212345678",
      "tel-country-code": "+886",
      "tel-national": "0212345678",
      "tel-area-code": "",
      "tel-local": "",
      "tel-local-prefix": "",
      "tel-local-suffix": "",
    },
  },
  {
    description: "\"tel\" can't be parsed so leave it as-is",
    address: {
      "tel": "12345",
    },
    expectedResult: {
      "tel": "12345",
      "tel-country-code": "",
      "tel-national": "12345",
      "tel-area-code": "",
      "tel-local": "",
      "tel-local-prefix": "",
      "tel-local-suffix": "",
    },
  },
];

const ADDRESS_NORMALIZE_TESTCASES = [
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

  // Country
  {
    description: "Has \"country\" in lowercase",
    address: {
      "country": "us",
    },
    expectedResult: {
      "country": "US",
    },
  },
  {
    description: "Has unknown \"country\"",
    address: {
      "country": "AA",
    },
    expectedResult: {
      "country": undefined,
    },
  },
  {
    description: "Has \"country-name\"",
    address: {
      "country-name": "united states",
    },
    expectedResult: {
      "country": "US",
      "country-name": "United States",
    },
  },
  {
    description: "Has unknown \"country-name\"",
    address: {
      "country-name": "unknown country name",
    },
    expectedResult: {
      "country": undefined,
      "country-name": "",
    },
  },
  {
    description: "Has \"country\" and unknown \"country-name\"",
    address: {
      "country": "us",
      "country-name": "unknown country name",
    },
    expectedResult: {
      "country": "US",
      "country-name": "United States",
    },
  },
  {
    description: "Has \"country-name\" and unknown \"country\"",
    address: {
      "country": "AA",
      "country-name": "united states",
    },
    expectedResult: {
      "country": undefined,
      "country-name": "",
    },
  },
  {
    description: "Has unsupported \"country\"",
    address: {
      "country": "CA",
    },
    expectedResult: {
      "country": undefined,
      "country-name": "",
    },
  },

  // Tel
  {
    description: "Has \"tel\" with country code",
    address: {
      "tel": "+16172535702",
    },
    expectedResult: {
      "tel": "+16172535702",
    },
  },
  {
    description: "Has \"tel\" without country code but \"country\" is set",
    address: {
      "tel": "0212345678",
      "country": "TW",
    },
    expectedResult: {
      "tel": "+886212345678",
    },
  },
  {
    description: "Has \"tel\" without country code and \"country\" so use \"US\" as default region",
    address: {
      "tel": "6172535702",
    },
    expectedResult: {
      "tel": "+16172535702",
    },
  },
  {
    description: "\"tel\" can't be parsed so leave it as-is",
    address: {
      "tel": "12345",
    },
    expectedResult: {
      "tel": "12345",
    },
  },
  {
    description: "Has \"tel-national\" and \"tel-country-code\"",
    address: {
      "tel-national": "0212345678",
      "tel-country-code": "+886",
    },
    expectedResult: {
      "tel": "+886212345678",
    },
  },
  {
    description: "Has \"tel-national\" and \"country\"",
    address: {
      "tel-national": "0212345678",
      "country": "TW",
    },
    expectedResult: {
      "tel": "+886212345678",
    },
  },
  {
    description: "Has \"tel-national\", \"tel-country-code\" and \"country\"",
    address: {
      "tel-national": "0212345678",
      "tel-country-code": "+886",
      "country": "US",
    },
    expectedResult: {
      "tel": "+886212345678",
    },
  },
  {
    description: "Has \"tel-area-code\" and \"tel-local\"",
    address: {
      "tel-area-code": "617",
      "tel-local": "2535702",
    },
    expectedResult: {
      "tel": "+16172535702",
    },
  },
  {
    description: "Has \"tel-area-code\", \"tel-local-prefix\" and \"tel-local-suffix\"",
    address: {
      "tel-area-code": "617",
      "tel-local-prefix": "253",
      "tel-local-suffix": "5702",
    },
    expectedResult: {
      "tel": "+16172535702",
    },
  },
];

const CREDIT_CARD_COMPUTE_TESTCASES = [
  // Empty
  {
    description: "Empty credit card",
    creditCard: {
    },
    expectedResult: {
    },
  },

  // Name
  {
    description: "Has \"cc-name\"",
    creditCard: {
      "cc-name": "Timothy John Berners-Lee",
    },
    expectedResult: {
      "cc-name": "Timothy John Berners-Lee",
      "cc-given-name": "Timothy",
      "cc-additional-name": "John",
      "cc-family-name": "Berners-Lee",
    },
  },
];

const CREDIT_CARD_NORMALIZE_TESTCASES = [
  // Empty
  {
    description: "Empty credit card",
    creditCard: {
    },
    expectedResult: {
    },
  },

  // Name
  {
    description: "Has both \"cc-name\" and the split name fields",
    creditCard: {
      "cc-name": "Timothy John Berners-Lee",
      "cc-given-name": "John",
      "cc-family-name": "Doe",
    },
    expectedResult: {
      "cc-name": "Timothy John Berners-Lee",
    },
  },
  {
    description: "Has only the split name fields",
    creditCard: {
      "cc-given-name": "John",
      "cc-family-name": "Doe",
    },
    expectedResult: {
      "cc-name": "John Doe",
    },
  },
];

let do_check_record_matches = (expectedRecord, record) => {
  for (let key in expectedRecord) {
    do_check_eq(expectedRecord[key], record[key]);
  }
};

add_task(async function test_computeAddressFields() {
  let path = getTempFile(TEST_STORE_FILE_NAME).path;

  let profileStorage = new ProfileStorage(path);
  await profileStorage.initialize();

  ADDRESS_COMPUTE_TESTCASES.forEach(testcase => profileStorage.addresses.add(testcase.address));
  await profileStorage._saveImmediately();

  profileStorage = new ProfileStorage(path);
  await profileStorage.initialize();

  let addresses = profileStorage.addresses.getAll();

  for (let i in addresses) {
    do_print("Verify testcase: " + ADDRESS_COMPUTE_TESTCASES[i].description);
    do_check_record_matches(ADDRESS_COMPUTE_TESTCASES[i].expectedResult, addresses[i]);
  }
});

add_task(async function test_normalizeAddressFields() {
  let path = getTempFile(TEST_STORE_FILE_NAME).path;

  let profileStorage = new ProfileStorage(path);
  await profileStorage.initialize();

  ADDRESS_NORMALIZE_TESTCASES.forEach(testcase => profileStorage.addresses.add(testcase.address));
  await profileStorage._saveImmediately();

  profileStorage = new ProfileStorage(path);
  await profileStorage.initialize();

  let addresses = profileStorage.addresses.getAll();

  for (let i in addresses) {
    do_print("Verify testcase: " + ADDRESS_NORMALIZE_TESTCASES[i].description);
    do_check_record_matches(ADDRESS_NORMALIZE_TESTCASES[i].expectedResult, addresses[i]);
  }
});

add_task(async function test_computeCreditCardFields() {
  let path = getTempFile(TEST_STORE_FILE_NAME).path;

  let profileStorage = new ProfileStorage(path);
  await profileStorage.initialize();

  CREDIT_CARD_COMPUTE_TESTCASES.forEach(testcase => profileStorage.creditCards.add(testcase.creditCard));
  await profileStorage._saveImmediately();

  profileStorage = new ProfileStorage(path);
  await profileStorage.initialize();

  let creditCards = profileStorage.creditCards.getAll();

  for (let i in creditCards) {
    do_print("Verify testcase: " + CREDIT_CARD_COMPUTE_TESTCASES[i].description);
    do_check_record_matches(CREDIT_CARD_COMPUTE_TESTCASES[i].expectedResult, creditCards[i]);
  }
});

add_task(async function test_normalizeCreditCardFields() {
  let path = getTempFile(TEST_STORE_FILE_NAME).path;

  let profileStorage = new ProfileStorage(path);
  await profileStorage.initialize();

  CREDIT_CARD_NORMALIZE_TESTCASES.forEach(testcase => profileStorage.creditCards.add(testcase.creditCard));
  await profileStorage._saveImmediately();

  profileStorage = new ProfileStorage(path);
  await profileStorage.initialize();

  let creditCards = profileStorage.creditCards.getAll();

  for (let i in creditCards) {
    do_print("Verify testcase: " + CREDIT_CARD_NORMALIZE_TESTCASES[i].description);
    do_check_record_matches(CREDIT_CARD_NORMALIZE_TESTCASES[i].expectedResult, creditCards[i]);
  }
});
