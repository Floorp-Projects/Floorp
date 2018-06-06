/**
 * Tests the transform algorithm in profileStorage.
 */

"use strict";

const {FormAutofillStorage} = ChromeUtils.import("resource://formautofill/FormAutofillStorage.jsm", {});

const TEST_STORE_FILE_NAME = "test-profile.json";

const ADDRESS_COMPUTE_TESTCASES = [
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
  {
    description: "Has split CJK names",
    address: {
      "given-name": "德明",
      "family-name": "孫",
    },
    expectedResult: {
      "given-name": "德明",
      "family-name": "孫",
      "name": "孫德明",
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
      "address-line2": undefined,
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
      "address-line2": undefined,
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
      "tel-area-code": undefined,
      "tel-local": undefined,
      "tel-local-prefix": undefined,
      "tel-local-suffix": undefined,
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
      "tel-area-code": undefined,
      "tel-local": undefined,
      "tel-local-prefix": undefined,
      "tel-local-suffix": undefined,
    },
  },
  {
    description: "\"tel\" can't be parsed so leave it as-is",
    address: {
      "tel": "12345",
    },
    expectedResult: {
      "tel": "12345",
      "tel-country-code": undefined,
      "tel-national": "12345",
      "tel-area-code": undefined,
      "tel-local": undefined,
      "tel-local-prefix": undefined,
      "tel-local-suffix": undefined,
    },
  },
];

const ADDRESS_NORMALIZE_TESTCASES = [
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
  {
    description: "Has only \"address-line1~2\"",
    address: {
      "address-line1": "line1",
      "address-line2": "line2",
    },
    expectedResult: {
      "street-address": "line1\nline2",
    },
  },
  {
    description: "Has only \"address-line1\"",
    address: {
      "address-line1": "line1",
    },
    expectedResult: {
      "street-address": "line1",
    },
  },
  {
    description: "Has only \"address-line2~3\"",
    address: {
      "address-line2": "line2",
      "address-line3": "line3",
    },
    expectedResult: {
      "street-address": "\nline2\nline3",
    },
  },
  {
    description: "Has only \"address-line2\"",
    address: {
      "address-line2": "line2",
    },
    expectedResult: {
      "street-address": "\nline2",
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
      "given-name": "John", // Make sure it won't be an empty record.
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
    description: "Has alternative \"country-name\"",
    address: {
      "country-name": "america",
    },
    expectedResult: {
      "country": "US",
      "country-name": "United States",
    },
  },
  {
    description: "Has \"country-name\" as a substring",
    address: {
      "country-name": "test america test",
    },
    expectedResult: {
      "country": "US",
      "country-name": "United States",
    },
  },
  {
    description: "Has \"country-name\" as part of a word",
    address: {
      "given-name": "John", // Make sure it won't be an empty record.
      "country-name": "TRUST",
    },
    expectedResult: {
      "country": undefined,
      "country-name": undefined,
    },
  },
  {
    description: "Has unknown \"country-name\"",
    address: {
      "given-name": "John", // Make sure it won't be an empty record.
      "country-name": "unknown country name",
    },
    expectedResult: {
      "country": undefined,
      "country-name": undefined,
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
      "given-name": "John", // Make sure it won't be an empty record.
      "country": "AA",
      "country-name": "united states",
    },
    expectedResult: {
      "country": undefined,
      "country-name": undefined,
    },
  },
  {
    description: "Has unsupported \"country\"",
    address: {
      "given-name": "John", // Make sure it won't be an empty record.
      "country": "TV",
    },
    expectedResult: {
      "country": undefined,
      "country-name": undefined,
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
    description: "Has a valid tel-local format \"tel\"",
    address: {
      "tel": "1234567",
    },
    expectedResult: {
      "tel": "1234567",
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

  // Card Number
  {
    description: "Number should be encrypted and masked",
    creditCard: {
      "cc-number": "4929001587121045",
    },
    expectedResult: {
      "cc-number": "************1045",
    },
  },

  // Expiration Date
  {
    description: "Has \"cc-exp-year\" and \"cc-exp-month\"",
    creditCard: {
      "cc-exp-month": 12,
      "cc-exp-year": 2022,
    },
    expectedResult: {
      "cc-exp-month": 12,
      "cc-exp-year": 2022,
      "cc-exp": "2022-12",
    },
  },
  {
    description: "Has only \"cc-exp-month\"",
    creditCard: {
      "cc-exp-month": 12,
    },
    expectedResult: {
      "cc-exp-month": 12,
      "cc-exp": undefined,
    },
  },
  {
    description: "Has only \"cc-exp-year\"",
    creditCard: {
      "cc-exp-year": 2022,
    },
    expectedResult: {
      "cc-exp-year": 2022,
      "cc-exp": undefined,
    },
  },
];

const CREDIT_CARD_NORMALIZE_TESTCASES = [
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

  // Card Number
  {
    description: "Regular number",
    creditCard: {
      "cc-number": "4929001587121045",
    },
    expectedResult: {
      "cc-number": "4929001587121045",
    },
  },
  {
    description: "Number with spaces",
    creditCard: {
      "cc-number": "4111 1111  1111 1111",
    },
    expectedResult: {
      "cc-number": "4111111111111111",
    },
  },
  {
    description: "Number with hyphens",
    creditCard: {
      "cc-number": "4111-1111-1111-1111",
    },
    expectedResult: {
      "cc-number": "4111111111111111",
    },
  },

  // Expiration Date
  {
    description: "Has \"cc-exp\" formatted \"yyyy-mm\"",
    creditCard: {
      "cc-exp": "2022-12",
    },
    expectedResult: {
      "cc-exp-month": 12,
      "cc-exp-year": 2022,
    },
  },
  {
    description: "Has \"cc-exp\" formatted \"yyyy/mm\"",
    creditCard: {
      "cc-exp": "2022/12",
    },
    expectedResult: {
      "cc-exp-month": 12,
      "cc-exp-year": 2022,
    },
  },
  {
    description: "Has \"cc-exp\" formatted \"yyyy-m\"",
    creditCard: {
      "cc-exp": "2022-3",
    },
    expectedResult: {
      "cc-exp-month": 3,
      "cc-exp-year": 2022,
    },
  },
  {
    description: "Has \"cc-exp\" formatted \"yyyy/m\"",
    creditCard: {
      "cc-exp": "2022/3",
    },
    expectedResult: {
      "cc-exp-month": 3,
      "cc-exp-year": 2022,
    },
  },
  {
    description: "Has \"cc-exp\" formatted \"mm-yyyy\"",
    creditCard: {
      "cc-exp": "12-2022",
    },
    expectedResult: {
      "cc-exp-month": 12,
      "cc-exp-year": 2022,
    },
  },
  {
    description: "Has \"cc-exp\" formatted \"mm/yyyy\"",
    creditCard: {
      "cc-exp": "12/2022",
    },
    expectedResult: {
      "cc-exp-month": 12,
      "cc-exp-year": 2022,
    },
  },
  {
    description: "Has \"cc-exp\" formatted \"m-yyyy\"",
    creditCard: {
      "cc-exp": "3-2022",
    },
    expectedResult: {
      "cc-exp-month": 3,
      "cc-exp-year": 2022,
    },
  },
  {
    description: "Has \"cc-exp\" formatted \"m/yyyy\"",
    creditCard: {
      "cc-exp": "3/2022",
    },
    expectedResult: {
      "cc-exp-month": 3,
      "cc-exp-year": 2022,
    },
  },
  {
    description: "Has \"cc-exp\" formatted \"mm-yy\"",
    creditCard: {
      "cc-exp": "12-22",
    },
    expectedResult: {
      "cc-exp-month": 12,
      "cc-exp-year": 2022,
    },
  },
  {
    description: "Has \"cc-exp\" formatted \"mm/yy\"",
    creditCard: {
      "cc-exp": "12/22",
    },
    expectedResult: {
      "cc-exp-month": 12,
      "cc-exp-year": 2022,
    },
  },
  {
    description: "Has \"cc-exp\" formatted \"yy-mm\"",
    creditCard: {
      "cc-exp": "22-12",
    },
    expectedResult: {
      "cc-exp-month": 12,
      "cc-exp-year": 2022,
    },
  },
  {
    description: "Has \"cc-exp\" formatted \"yy/mm\"",
    creditCard: {
      "cc-exp": "22/12",
    },
    expectedResult: {
      "cc-exp-month": 12,
      "cc-exp-year": 2022,
    },
  },
  {
    description: "Has \"cc-exp\" formatted \"mmyy\"",
    creditCard: {
      "cc-exp": "1222",
    },
    expectedResult: {
      "cc-exp-month": 12,
      "cc-exp-year": 2022,
    },
  },
  {
    description: "Has \"cc-exp\" formatted \"yymm\"",
    creditCard: {
      "cc-exp": "2212",
    },
    expectedResult: {
      "cc-exp-month": 12,
      "cc-exp-year": 2022,
    },
  },
  {
    description: "Has \"cc-exp\" with spaces",
    creditCard: {
      "cc-exp": "  2033-11  ",
    },
    expectedResult: {
      "cc-exp-month": 11,
      "cc-exp-year": 2033,
    },
  },
  {
    description: "Has invalid \"cc-exp\"",
    creditCard: {
      "cc-number": "4111111111111111", // Make sure it won't be an empty record.
      "cc-exp": "99-9999",
    },
    expectedResult: {
      "cc-exp-month": undefined,
      "cc-exp-year": undefined,
    },
  },
  {
    description: "Has both \"cc-exp-*\" and \"cc-exp\"",
    creditCard: {
      "cc-exp": "2022-12",
      "cc-exp-month": 3,
      "cc-exp-year": 2030,
    },
    expectedResult: {
      "cc-exp-month": 3,
      "cc-exp-year": 2030,
    },
  },
  {
    description: "Has only \"cc-exp-year\" and \"cc-exp\"",
    creditCard: {
      "cc-exp": "2022-12",
      "cc-exp-year": 2030,
    },
    expectedResult: {
      "cc-exp-month": 12,
      "cc-exp-year": 2022,
    },
  },
  {
    description: "Has only \"cc-exp-month\" and \"cc-exp\"",
    creditCard: {
      "cc-exp": "2022-12",
      "cc-exp-month": 3,
    },
    expectedResult: {
      "cc-exp-month": 12,
      "cc-exp-year": 2022,
    },
  },
];

let do_check_record_matches = (expectedRecord, record) => {
  for (let key in expectedRecord) {
    Assert.equal(expectedRecord[key], record[key]);
  }
};

add_task(async function test_computeAddressFields() {
  let path = getTempFile(TEST_STORE_FILE_NAME).path;

  let profileStorage = new FormAutofillStorage(path);
  await profileStorage.initialize();

  ADDRESS_COMPUTE_TESTCASES.forEach(testcase => {
    info("Verify testcase: " + testcase.description);

    let guid = profileStorage.addresses.add(testcase.address);
    let address = profileStorage.addresses.get(guid);
    do_check_record_matches(testcase.expectedResult, address);

    profileStorage.addresses.remove(guid);
  });
});

add_task(async function test_normalizeAddressFields() {
  let path = getTempFile(TEST_STORE_FILE_NAME).path;

  let profileStorage = new FormAutofillStorage(path);
  await profileStorage.initialize();

  ADDRESS_NORMALIZE_TESTCASES.forEach(testcase => {
    info("Verify testcase: " + testcase.description);

    let guid = profileStorage.addresses.add(testcase.address);
    let address = profileStorage.addresses.get(guid);
    do_check_record_matches(testcase.expectedResult, address);

    profileStorage.addresses.remove(guid);
  });
});

add_task(async function test_computeCreditCardFields() {
  let path = getTempFile(TEST_STORE_FILE_NAME).path;

  let profileStorage = new FormAutofillStorage(path);
  await profileStorage.initialize();

  CREDIT_CARD_COMPUTE_TESTCASES.forEach(testcase => {
    info("Verify testcase: " + testcase.description);

    let guid = profileStorage.creditCards.add(testcase.creditCard);
    let creditCard = profileStorage.creditCards.get(guid);
    do_check_record_matches(testcase.expectedResult, creditCard);

    profileStorage.creditCards.remove(guid);
  });
});

add_task(async function test_normalizeCreditCardFields() {
  let path = getTempFile(TEST_STORE_FILE_NAME).path;

  let profileStorage = new FormAutofillStorage(path);
  await profileStorage.initialize();

  CREDIT_CARD_NORMALIZE_TESTCASES.forEach(testcase => {
    info("Verify testcase: " + testcase.description);

    let guid = profileStorage.creditCards.add(testcase.creditCard);
    let creditCard = profileStorage.creditCards.get(guid, {rawData: true});
    do_check_record_matches(testcase.expectedResult, creditCard);

    profileStorage.creditCards.remove(guid);
  });
});
