/*
 * Test for make sure getRecords can retrieve right collection from storage.
 */

"use strict";

const { CreditCard } = ChromeUtils.import(
  "resource://gre/modules/CreditCard.jsm"
);

let FormAutofillParent, FormAutofillStatus;
let OSKeyStore;
add_setup(async () => {
  ({ FormAutofillParent, FormAutofillStatus } = ChromeUtils.import(
    "resource://autofill/FormAutofillParent.jsm"
  ));
  ({ OSKeyStore } = ChromeUtils.import(
    "resource://gre/modules/OSKeyStore.jsm"
  ));
});

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

let TEST_CREDIT_CARD_1 = {
  "cc-name": "John Doe",
  "cc-number": "4111111111111111",
  "cc-exp-month": 4,
  "cc-exp-year": 2017,
  "cc-type": "visa",
};

let TEST_CREDIT_CARD_2 = {
  "cc-name": "John Dai",
  "cc-number": "4929001587121045",
  "cc-exp-month": 2,
  "cc-exp-year": 2017,
  "cc-type": "visa",
};

add_task(async function test_getRecords() {
  FormAutofillStatus.init();

  await FormAutofillStatus.formAutofillStorage.initialize();
  let fakeResult = {
    addresses: [
      {
        "given-name": "Timothy",
        "additional-name": "John",
        "family-name": "Berners-Lee",
        organization: "World Wide Web Consortium",
      },
    ],
    creditCards: [
      {
        "cc-name": "John Doe",
        "cc-number": "4111111111111111",
        "cc-exp-month": 4,
        "cc-exp-year": 2017,
      },
    ],
  };

  for (let collectionName of ["addresses", "creditCards", "nonExisting"]) {
    let collection = FormAutofillStatus.formAutofillStorage[collectionName];
    let expectedResult = fakeResult[collectionName] || [];

    if (collection) {
      sinon.stub(collection, "getAll");
      collection.getAll.returns(Promise.resolve(expectedResult));
    }
    await FormAutofillParent._getRecords({ collectionName });
    if (collection) {
      Assert.equal(collection.getAll.called, true);
      collection.getAll.restore();
    }
  }
});

add_task(async function test_getRecords_addresses() {
  await FormAutofillStatus.formAutofillStorage.initialize();
  let mockAddresses = [TEST_ADDRESS_1, TEST_ADDRESS_2];
  let collection = FormAutofillStatus.formAutofillStorage.addresses;
  sinon.stub(collection, "getAll");
  collection.getAll.returns(Promise.resolve(mockAddresses));

  let testCases = [
    {
      description: "If the search string could match 1 address",
      filter: {
        collectionName: "addresses",
        info: { fieldName: "street-address" },
        searchString: "Some",
      },
      expectedResult: [TEST_ADDRESS_2],
    },
    {
      description: "If the search string could match multiple addresses",
      filter: {
        collectionName: "addresses",
        info: { fieldName: "country" },
        searchString: "u",
      },
      expectedResult: [TEST_ADDRESS_1, TEST_ADDRESS_2],
    },
    {
      description: "If the search string could not match any address",
      filter: {
        collectionName: "addresses",
        info: { fieldName: "street-address" },
        searchString: "test",
      },
      expectedResult: [],
    },
    {
      description: "If the search string is empty",
      filter: {
        collectionName: "addresses",
        info: { fieldName: "street-address" },
        searchString: "",
      },
      expectedResult: [TEST_ADDRESS_1, TEST_ADDRESS_2],
    },
    {
      description:
        "Check if the filtering logic is free from searching special chars",
      filter: {
        collectionName: "addresses",
        info: { fieldName: "street-address" },
        searchString: ".*",
      },
      expectedResult: [],
    },
    {
      description:
        "Prevent broken while searching the property that does not exist",
      filter: {
        collectionName: "addresses",
        info: { fieldName: "tel" },
        searchString: "1",
      },
      expectedResult: [],
    },
  ];

  for (let testCase of testCases) {
    info("Starting testcase: " + testCase.description);
    let result = await FormAutofillParent._getRecords(testCase.filter);
    Assert.deepEqual(result, testCase.expectedResult);
  }
});

add_task(async function test_getRecords_creditCards() {
  await FormAutofillStatus.formAutofillStorage.initialize();
  let collection = FormAutofillStatus.formAutofillStorage.creditCards;
  let encryptedCCRecords = await Promise.all(
    [TEST_CREDIT_CARD_1, TEST_CREDIT_CARD_2].map(async record => {
      let clonedRecord = Object.assign({}, record);
      clonedRecord["cc-number"] = CreditCard.getLongMaskedNumber(
        record["cc-number"]
      );
      clonedRecord["cc-number-encrypted"] = await OSKeyStore.encrypt(
        record["cc-number"]
      );
      return clonedRecord;
    })
  );
  sinon
    .stub(collection, "getAll")
    .callsFake(() =>
      Promise.resolve([
        Object.assign({}, encryptedCCRecords[0]),
        Object.assign({}, encryptedCCRecords[1]),
      ])
    );

  let testCases = [
    {
      description: "If the search string could match multiple creditCards",
      filter: {
        collectionName: "creditCards",
        info: { fieldName: "cc-name" },
        searchString: "John",
      },
      expectedResult: encryptedCCRecords,
    },
    {
      description: "If the search string could not match any creditCard",
      filter: {
        collectionName: "creditCards",
        info: { fieldName: "cc-name" },
        searchString: "T",
      },
      expectedResult: [],
    },
    {
      description:
        "Return all creditCards if focused field is cc number; " +
        "if the search string could match multiple creditCards",
      filter: {
        collectionName: "creditCards",
        info: { fieldName: "cc-number" },
        searchString: "4",
      },
      expectedResult: encryptedCCRecords,
    },
    {
      description: "If the search string could match 1 creditCard",
      filter: {
        collectionName: "creditCards",
        info: { fieldName: "cc-name" },
        searchString: "John Doe",
      },
      mpEnabled: true,
      expectedResult: encryptedCCRecords.slice(0, 1),
    },
    {
      description: "Return all creditCards if focused field is cc number",
      filter: {
        collectionName: "creditCards",
        info: { fieldName: "cc-number" },
        searchString: "411",
      },
      mpEnabled: true,
      expectedResult: encryptedCCRecords,
    },
  ];

  for (let testCase of testCases) {
    info("Starting testcase: " + testCase.description);
    if (testCase.mpEnabled) {
      let tokendb = Cc["@mozilla.org/security/pk11tokendb;1"].createInstance(
        Ci.nsIPK11TokenDB
      );
      let token = tokendb.getInternalKeyToken();
      token.reset();
      token.initPassword("password");
    }
    let result = await FormAutofillParent._getRecords(testCase.filter);
    Assert.deepEqual(result, testCase.expectedResult);
  }
});
