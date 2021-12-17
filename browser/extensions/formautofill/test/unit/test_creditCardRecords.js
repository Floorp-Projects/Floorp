/**
 * Tests FormAutofillStorage object with creditCards records.
 */

"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "Preferences",
  "resource://gre/modules/Preferences.jsm"
);
const { CreditCard } = ChromeUtils.import(
  "resource://gre/modules/CreditCard.jsm"
);

let FormAutofillStorage;
add_task(async function setup() {
  ({ FormAutofillStorage } = ChromeUtils.import(
    "resource://autofill/FormAutofillStorage.jsm"
  ));
});

const TEST_STORE_FILE_NAME = "test-credit-card.json";
const COLLECTION_NAME = "creditCards";

const TEST_CREDIT_CARD_1 = {
  "cc-name": "John Doe",
  "cc-number": "4929001587121045",
  "cc-exp-month": 4,
  "cc-exp-year": 2017,
  "cc-type": "visa",
};

const TEST_CREDIT_CARD_2 = {
  "cc-name": "Timothy Berners-Lee",
  "cc-number": "5103059495477870",
  "cc-exp-month": 12,
  "cc-exp-year": 2022,
  "cc-type": "mastercard",
};

const TEST_CREDIT_CARD_3 = {
  "cc-number": "3589993783099582",
  "cc-exp-month": 1,
  "cc-exp-year": 2000,
  "cc-type": "amex",
};

const TEST_CREDIT_CARD_4 = {
  "cc-name": "Foo Bar",
  "cc-number": "3589993783099582",
  "cc-type": "amex",
};

const TEST_CREDIT_CARD_WITH_BILLING_ADDRESS = {
  "cc-name": "J. Smith",
  "cc-number": "4111111111111111",
  billingAddressGUID: "9m6hf4gfr6ge",
};

const TEST_CREDIT_CARD_WITH_EMPTY_FIELD = {
  billingAddressGUID: "",
  "cc-name": "",
  "cc-number": "344060747836806",
  "cc-exp-month": 1,
  "cc-type": "",
};

const TEST_CREDIT_CARD_WITH_EMPTY_COMPUTED_FIELD = {
  "cc-given-name": "",
  "cc-additional-name": "",
  "cc-family-name": "",
  "cc-exp": "",
  "cc-number": "5415425865751454",
};

const TEST_CREDIT_CARD_WITH_2_DIGITS_YEAR = {
  "cc-number": "344060747836806",
  "cc-exp-month": 1,
  "cc-exp-year": 12,
};

const TEST_CREDIT_CARD_WITH_INVALID_FIELD = {
  "cc-name": "John Doe",
  "cc-number": "344060747836806",
  invalidField: "INVALID",
};

const TEST_CREDIT_CARD_WITH_INVALID_EXPIRY_DATE = {
  "cc-name": "John Doe",
  "cc-number": "5103059495477870",
  "cc-exp-month": 13,
  "cc-exp-year": -3,
};

const TEST_CREDIT_CARD_WITH_SPACES_BETWEEN_DIGITS = {
  "cc-name": "John Doe",
  "cc-number": "5103 0594 9547 7870",
};

const TEST_CREDIT_CARD_WITH_INVALID_NETWORK = {
  "cc-name": "John Doe",
  "cc-number": "4929001587121045",
  "cc-exp-month": 4,
  "cc-exp-year": 2017,
  "cc-type": "asiv",
};

const TEST_CREDIT_CARD_EMPTY_AFTER_NORMALIZE = {
  "cc-exp-month": 13,
};

const TEST_CREDIT_CARD_EMPTY_AFTER_UPDATE_CREDIT_CARD_1 = {
  "cc-name": "",
  "cc-number": "",
  "cc-exp-month": 13,
  "cc-exp-year": "",
};

const MERGE_TESTCASES = [
  {
    description: "Merge a superset",
    creditCardInStorage: {
      "cc-number": "4929001587121045",
      "cc-exp-month": 4,
      "cc-exp-year": 2017,
    },
    creditCardToMerge: {
      "cc-name": "John Doe",
      "cc-number": "4929001587121045",
      "cc-exp-month": 4,
      "cc-exp-year": 2017,
    },
    expectedCreditCard: {
      "cc-name": "John Doe",
      "cc-number": "4929001587121045",
      "cc-exp-month": 4,
      "cc-exp-year": 2017,
    },
  },
  {
    description: "Merge a superset with billingAddressGUID",
    creditCardInStorage: {
      "cc-number": "4929001587121045",
    },
    creditCardToMerge: {
      "cc-number": "4929001587121045",
      billingAddressGUID: "ijsnbhfr",
    },
    expectedCreditCard: {
      "cc-number": "4929001587121045",
      billingAddressGUID: "ijsnbhfr",
    },
  },
  {
    description: "Merge a subset",
    creditCardInStorage: {
      "cc-name": "John Doe",
      "cc-number": "4929001587121045",
      "cc-exp-month": 4,
      "cc-exp-year": 2017,
    },
    creditCardToMerge: {
      "cc-number": "4929001587121045",
      "cc-exp-month": 4,
      "cc-exp-year": 2017,
    },
    expectedCreditCard: {
      "cc-name": "John Doe",
      "cc-number": "4929001587121045",
      "cc-exp-month": 4,
      "cc-exp-year": 2017,
    },
    noNeedToUpdate: true,
  },
  {
    description: "Merge a subset with billingAddressGUID",
    creditCardInStorage: {
      "cc-number": "4929001587121045",
      billingAddressGUID: "8fhdb3ug6",
    },
    creditCardToMerge: {
      "cc-number": "4929001587121045",
    },
    expectedCreditCard: {
      billingAddressGUID: "8fhdb3ug6",
      "cc-number": "4929001587121045",
    },
    noNeedToUpdate: true,
  },
  {
    description: "Merge an creditCard with partial overlaps",
    creditCardInStorage: {
      "cc-name": "John Doe",
      "cc-number": "4929001587121045",
    },
    creditCardToMerge: {
      "cc-number": "4929001587121045",
      "cc-exp-month": 4,
      "cc-exp-year": 2017,
    },
    expectedCreditCard: {
      "cc-name": "John Doe",
      "cc-number": "4929001587121045",
      "cc-exp-month": 4,
      "cc-exp-year": 2017,
    },
  },
];

let prepareTestCreditCards = async function(path) {
  let profileStorage = new FormAutofillStorage(path);
  await profileStorage.initialize();

  let onChanged = TestUtils.topicObserved(
    "formautofill-storage-changed",
    (subject, data) =>
      data == "add" &&
      subject.wrappedJSObject.guid &&
      subject.wrappedJSObject.collectionName == COLLECTION_NAME
  );
  Assert.ok(await profileStorage.creditCards.add(TEST_CREDIT_CARD_1));
  await onChanged;
  Assert.ok(await profileStorage.creditCards.add(TEST_CREDIT_CARD_2));
  await onChanged;
  await profileStorage._saveImmediately();
};

let reCCNumber = /^(\*+)(.{4})$/;

let do_check_credit_card_matches = (creditCardWithMeta, creditCard) => {
  for (let key in creditCard) {
    if (key == "cc-number") {
      let matches = reCCNumber.exec(creditCardWithMeta["cc-number"]);
      Assert.notEqual(matches, null);
      Assert.equal(
        creditCardWithMeta["cc-number"].length,
        creditCard["cc-number"].length
      );
      Assert.equal(creditCard["cc-number"].endsWith(matches[2]), true);
      Assert.notEqual(creditCard["cc-number-encrypted"], "");
    } else {
      Assert.equal(creditCardWithMeta[key], creditCard[key], "Testing " + key);
    }
  }
};

add_task(async function test_initialize() {
  let path = getTempFile(TEST_STORE_FILE_NAME).path;
  let profileStorage = new FormAutofillStorage(path);
  await profileStorage.initialize();

  Assert.equal(profileStorage._store.data.version, 1);
  Assert.equal(profileStorage._store.data.creditCards.length, 0);

  let data = profileStorage._store.data;
  Assert.deepEqual(data.creditCards, []);

  await profileStorage._saveImmediately();

  profileStorage = new FormAutofillStorage(path);
  await profileStorage.initialize();

  Assert.deepEqual(profileStorage._store.data, data);
});

add_task(async function test_getAll() {
  let path = getTempFile(TEST_STORE_FILE_NAME).path;
  await prepareTestCreditCards(path);

  let profileStorage = new FormAutofillStorage(path);
  await profileStorage.initialize();

  let creditCards = await profileStorage.creditCards.getAll();

  Assert.equal(creditCards.length, 2);
  do_check_credit_card_matches(creditCards[0], TEST_CREDIT_CARD_1);
  do_check_credit_card_matches(creditCards[1], TEST_CREDIT_CARD_2);

  // Check computed fields.
  Assert.equal(creditCards[0]["cc-given-name"], "John");
  Assert.equal(creditCards[0]["cc-family-name"], "Doe");
  Assert.equal(creditCards[0]["cc-exp"], "2017-04");

  // Test with rawData set.
  creditCards = await profileStorage.creditCards.getAll({ rawData: true });
  Assert.equal(creditCards[0]["cc-given-name"], undefined);
  Assert.equal(creditCards[0]["cc-family-name"], undefined);
  Assert.equal(creditCards[0]["cc-exp"], undefined);

  // Modifying output shouldn't affect the storage.
  creditCards[0]["cc-name"] = "test";
  do_check_credit_card_matches(
    (await profileStorage.creditCards.getAll())[0],
    TEST_CREDIT_CARD_1
  );
});

add_task(async function test_get() {
  let path = getTempFile(TEST_STORE_FILE_NAME).path;
  await prepareTestCreditCards(path);

  let profileStorage = new FormAutofillStorage(path);
  await profileStorage.initialize();

  let creditCards = await profileStorage.creditCards.getAll();
  let guid = creditCards[0].guid;

  let creditCard = await profileStorage.creditCards.get(guid);
  do_check_credit_card_matches(creditCard, TEST_CREDIT_CARD_1);

  // Modifying output shouldn't affect the storage.
  creditCards[0]["cc-name"] = "test";
  do_check_credit_card_matches(
    await profileStorage.creditCards.get(guid),
    TEST_CREDIT_CARD_1
  );

  Assert.equal(await profileStorage.creditCards.get("INVALID_GUID"), null);
});

add_task(async function test_add() {
  let path = getTempFile(TEST_STORE_FILE_NAME).path;
  await prepareTestCreditCards(path);

  let profileStorage = new FormAutofillStorage(path);
  await profileStorage.initialize();

  let creditCards = await profileStorage.creditCards.getAll();

  Assert.equal(creditCards.length, 2);

  do_check_credit_card_matches(creditCards[0], TEST_CREDIT_CARD_1);
  do_check_credit_card_matches(creditCards[1], TEST_CREDIT_CARD_2);

  Assert.notEqual(creditCards[0].guid, undefined);
  Assert.equal(creditCards[0].version, 3);
  Assert.notEqual(creditCards[0].timeCreated, undefined);
  Assert.equal(creditCards[0].timeLastModified, creditCards[0].timeCreated);
  Assert.equal(creditCards[0].timeLastUsed, 0);
  Assert.equal(creditCards[0].timesUsed, 0);

  // Empty string should be deleted before saving.
  await profileStorage.creditCards.add(TEST_CREDIT_CARD_WITH_EMPTY_FIELD);
  let creditCard = profileStorage.creditCards._data[2];
  Assert.equal(
    creditCard["cc-exp-month"],
    TEST_CREDIT_CARD_WITH_EMPTY_FIELD["cc-exp-month"]
  );
  Assert.equal(creditCard["cc-name"], undefined);
  Assert.equal(creditCard.billingAddressGUID, undefined);

  // Empty computed fields shouldn't cause any problem.
  await profileStorage.creditCards.add(
    TEST_CREDIT_CARD_WITH_EMPTY_COMPUTED_FIELD
  );
  creditCard = profileStorage.creditCards._data[3];
  Assert.equal(
    creditCard["cc-number"],
    CreditCard.getLongMaskedNumber(
      TEST_CREDIT_CARD_WITH_EMPTY_COMPUTED_FIELD["cc-number"]
    )
  );

  await Assert.rejects(
    profileStorage.creditCards.add(TEST_CREDIT_CARD_WITH_INVALID_FIELD),
    /"invalidField" is not a valid field\./
  );

  await Assert.rejects(
    profileStorage.creditCards.add({}),
    /Record contains no valid field\./
  );

  await Assert.rejects(
    profileStorage.creditCards.add(TEST_CREDIT_CARD_EMPTY_AFTER_NORMALIZE),
    /Record contains no valid field\./
  );
});

add_task(async function test_addWithBillingAddress() {
  let path = getTempFile(TEST_STORE_FILE_NAME).path;
  let profileStorage = new FormAutofillStorage(path);
  await profileStorage.initialize();

  let creditCards = await profileStorage.creditCards.getAll();

  Assert.equal(creditCards.length, 0);

  await profileStorage.creditCards.add(TEST_CREDIT_CARD_WITH_BILLING_ADDRESS);

  creditCards = await profileStorage.creditCards.getAll();
  Assert.equal(creditCards.length, 1);
  do_check_credit_card_matches(
    creditCards[0],
    TEST_CREDIT_CARD_WITH_BILLING_ADDRESS
  );
});

add_task(async function test_update() {
  // Test assumes that when an entry is saved a second time, it's last modified date will
  // be different from the first. With high values of precision reduction, we execute too
  // fast for that to be true.
  let timerPrecision = Preferences.get("privacy.reduceTimerPrecision");
  Preferences.set("privacy.reduceTimerPrecision", false);

  registerCleanupFunction(function() {
    Preferences.set("privacy.reduceTimerPrecision", timerPrecision);
  });

  let path = getTempFile(TEST_STORE_FILE_NAME).path;
  await prepareTestCreditCards(path);

  let profileStorage = new FormAutofillStorage(path);
  await profileStorage.initialize();

  let creditCards = await profileStorage.creditCards.getAll();
  let guid = creditCards[1].guid;
  let timeLastModified = creditCards[1].timeLastModified;

  let onChanged = TestUtils.topicObserved(
    "formautofill-storage-changed",
    (subject, data) =>
      data == "update" &&
      subject.wrappedJSObject.guid == guid &&
      subject.wrappedJSObject.collectionName == COLLECTION_NAME
  );

  Assert.notEqual(creditCards[1]["cc-name"], undefined);
  await profileStorage.creditCards.update(guid, TEST_CREDIT_CARD_3);
  await onChanged;
  await profileStorage._saveImmediately();

  profileStorage = new FormAutofillStorage(path);
  await profileStorage.initialize();

  let creditCard = await profileStorage.creditCards.get(guid);

  Assert.equal(creditCard["cc-name"], undefined);
  Assert.notEqual(creditCard.timeLastModified, timeLastModified);
  do_check_credit_card_matches(creditCard, TEST_CREDIT_CARD_3);

  // Empty string should be deleted while updating.
  await profileStorage.creditCards.update(
    profileStorage.creditCards._data[0].guid,
    TEST_CREDIT_CARD_WITH_EMPTY_FIELD
  );
  creditCard = profileStorage.creditCards._data[0];
  Assert.equal(
    creditCard["cc-exp-month"],
    TEST_CREDIT_CARD_WITH_EMPTY_FIELD["cc-exp-month"]
  );
  Assert.equal(creditCard["cc-name"], undefined);
  Assert.equal(creditCard["cc-type"], "amex");
  Assert.equal(creditCard.billingAddressGUID, undefined);

  // Empty computed fields shouldn't cause any problem.
  await profileStorage.creditCards.update(
    profileStorage.creditCards._data[0].guid,
    TEST_CREDIT_CARD_WITH_EMPTY_COMPUTED_FIELD,
    false
  );
  creditCard = profileStorage.creditCards._data[0];
  Assert.equal(
    creditCard["cc-number"],
    CreditCard.getLongMaskedNumber(
      TEST_CREDIT_CARD_WITH_EMPTY_COMPUTED_FIELD["cc-number"]
    )
  );
  await profileStorage.creditCards.update(
    profileStorage.creditCards._data[1].guid,
    TEST_CREDIT_CARD_WITH_EMPTY_COMPUTED_FIELD,
    true
  );
  creditCard = profileStorage.creditCards._data[1];
  Assert.equal(
    creditCard["cc-number"],
    CreditCard.getLongMaskedNumber(
      TEST_CREDIT_CARD_WITH_EMPTY_COMPUTED_FIELD["cc-number"]
    )
  );

  // Decryption failure of existing record should not prevent it from being updated.
  creditCard = profileStorage.creditCards._data[0];
  creditCard["cc-number-encrypted"] = "INVALID";
  await profileStorage.creditCards.update(
    profileStorage.creditCards._data[0].guid,
    TEST_CREDIT_CARD_WITH_EMPTY_COMPUTED_FIELD,
    false
  );
  creditCard = profileStorage.creditCards._data[0];
  Assert.equal(
    creditCard["cc-number"],
    CreditCard.getLongMaskedNumber(
      TEST_CREDIT_CARD_WITH_EMPTY_COMPUTED_FIELD["cc-number"]
    )
  );

  await Assert.rejects(
    profileStorage.creditCards.update("INVALID_GUID", TEST_CREDIT_CARD_3),
    /No matching record\./
  );

  await Assert.rejects(
    profileStorage.creditCards.update(
      guid,
      TEST_CREDIT_CARD_WITH_INVALID_FIELD
    ),
    /"invalidField" is not a valid field\./
  );

  await Assert.rejects(
    profileStorage.creditCards.update(guid, {}),
    /Record contains no valid field\./
  );

  await Assert.rejects(
    profileStorage.creditCards.update(
      guid,
      TEST_CREDIT_CARD_EMPTY_AFTER_NORMALIZE
    ),
    /Record contains no valid field\./
  );

  await profileStorage.creditCards.update(guid, TEST_CREDIT_CARD_1);
  await Assert.rejects(
    profileStorage.creditCards.update(
      guid,
      TEST_CREDIT_CARD_EMPTY_AFTER_UPDATE_CREDIT_CARD_1
    ),
    /Record contains no valid field\./
  );
});

add_task(async function test_validate() {
  let path = getTempFile(TEST_STORE_FILE_NAME).path;

  let profileStorage = new FormAutofillStorage(path);
  await profileStorage.initialize();

  await profileStorage.creditCards.add(
    TEST_CREDIT_CARD_WITH_INVALID_EXPIRY_DATE
  );
  await profileStorage.creditCards.add(TEST_CREDIT_CARD_WITH_2_DIGITS_YEAR);
  await profileStorage.creditCards.add(
    TEST_CREDIT_CARD_WITH_SPACES_BETWEEN_DIGITS
  );
  await profileStorage.creditCards.add(TEST_CREDIT_CARD_WITH_INVALID_NETWORK);

  let creditCards = await profileStorage.creditCards.getAll();

  Assert.equal(creditCards[0]["cc-exp-month"], undefined);
  Assert.equal(creditCards[0]["cc-exp-year"], undefined);
  Assert.equal(creditCards[0]["cc-exp"], undefined);

  let month = TEST_CREDIT_CARD_WITH_2_DIGITS_YEAR["cc-exp-month"];
  let year =
    parseInt(TEST_CREDIT_CARD_WITH_2_DIGITS_YEAR["cc-exp-year"], 10) + 2000;
  Assert.equal(creditCards[1]["cc-exp-month"], month);
  Assert.equal(creditCards[1]["cc-exp-year"], year);
  Assert.equal(
    creditCards[1]["cc-exp"],
    year + "-" + month.toString().padStart(2, "0")
  );

  Assert.equal(creditCards[2]["cc-number"].length, 16);

  // dont enforce validity on the card network when storing a record,
  // to avoid data loss when syncing records between different clients with different rules
  Assert.equal(creditCards[3]["cc-type"], "asiv");
});

add_task(async function test_notifyUsed() {
  let path = getTempFile(TEST_STORE_FILE_NAME).path;
  await prepareTestCreditCards(path);

  let profileStorage = new FormAutofillStorage(path);
  await profileStorage.initialize();

  let creditCards = await profileStorage.creditCards.getAll();
  let guid = creditCards[1].guid;
  let timeLastUsed = creditCards[1].timeLastUsed;
  let timesUsed = creditCards[1].timesUsed;

  let onChanged = TestUtils.topicObserved(
    "formautofill-storage-changed",
    (subject, data) =>
      data == "notifyUsed" &&
      subject.wrappedJSObject.collectionName == COLLECTION_NAME &&
      subject.wrappedJSObject.guid == guid
  );

  profileStorage.creditCards.notifyUsed(guid);
  await onChanged;
  await profileStorage._saveImmediately();

  profileStorage = new FormAutofillStorage(path);
  await profileStorage.initialize();

  let creditCard = await profileStorage.creditCards.get(guid);

  Assert.equal(creditCard.timesUsed, timesUsed + 1);
  Assert.notEqual(creditCard.timeLastUsed, timeLastUsed);

  Assert.throws(
    () => profileStorage.creditCards.notifyUsed("INVALID_GUID"),
    /No matching record\./
  );
});

add_task(async function test_remove() {
  let path = getTempFile(TEST_STORE_FILE_NAME).path;
  await prepareTestCreditCards(path);

  let profileStorage = new FormAutofillStorage(path);
  await profileStorage.initialize();

  let creditCards = await profileStorage.creditCards.getAll();
  let guid = creditCards[1].guid;

  let onChanged = TestUtils.topicObserved(
    "formautofill-storage-changed",
    (subject, data) =>
      data == "remove" &&
      subject.wrappedJSObject.guid == guid &&
      subject.wrappedJSObject.collectionName == COLLECTION_NAME
  );

  Assert.equal(creditCards.length, 2);

  profileStorage.creditCards.remove(guid);
  await onChanged;
  await profileStorage._saveImmediately();

  profileStorage = new FormAutofillStorage(path);
  await profileStorage.initialize();

  creditCards = await profileStorage.creditCards.getAll();

  Assert.equal(creditCards.length, 1);

  Assert.equal(await profileStorage.creditCards.get(guid), null);
});

MERGE_TESTCASES.forEach(testcase => {
  add_task(async function test_merge() {
    info("Starting testcase: " + testcase.description);
    let profileStorage = await initProfileStorage(
      TEST_STORE_FILE_NAME,
      [testcase.creditCardInStorage],
      "creditCards"
    );
    let creditCards = await profileStorage.creditCards.getAll();
    let guid = creditCards[0].guid;
    let timeLastModified = creditCards[0].timeLastModified;
    // Merge creditCard and verify the guid in notifyObservers subject
    let onMerged = TestUtils.topicObserved(
      "formautofill-storage-changed",
      (subject, data) =>
        data == "update" &&
        subject.wrappedJSObject.guid == guid &&
        subject.wrappedJSObject.collectionName == COLLECTION_NAME
    );
    // Force to create sync metadata.
    profileStorage.creditCards.pullSyncChanges();
    Assert.equal(getSyncChangeCounter(profileStorage.creditCards, guid), 1);
    Assert.ok(
      await profileStorage.creditCards.mergeIfPossible(
        guid,
        testcase.creditCardToMerge
      )
    );
    if (!testcase.noNeedToUpdate) {
      await onMerged;
    }
    creditCards = await profileStorage.creditCards.getAll();
    Assert.equal(creditCards.length, 1);
    do_check_credit_card_matches(creditCards[0], testcase.expectedCreditCard);
    if (!testcase.noNeedToUpdate) {
      // Record merging should update timeLastModified and bump the change counter.
      Assert.notEqual(creditCards[0].timeLastModified, timeLastModified);
      Assert.equal(getSyncChangeCounter(profileStorage.creditCards, guid), 2);
    } else {
      // Subset record merging should not update timeLastModified and the change
      // counter is still the same.
      Assert.equal(creditCards[0].timeLastModified, timeLastModified);
      Assert.equal(getSyncChangeCounter(profileStorage.creditCards, guid), 1);
    }
  });
});

add_task(async function test_merge_unable_merge() {
  let profileStorage = await initProfileStorage(
    TEST_STORE_FILE_NAME,
    [TEST_CREDIT_CARD_1],
    "creditCards"
  );

  let creditCards = await profileStorage.creditCards.getAll();
  let guid = creditCards[0].guid;
  // Force to create sync metadata.
  profileStorage.creditCards.pullSyncChanges();
  Assert.equal(getSyncChangeCounter(profileStorage.creditCards, guid), 1);

  // Unable to merge because of conflict
  let anotherCreditCard = profileStorage.creditCards._clone(TEST_CREDIT_CARD_1);
  anotherCreditCard["cc-name"] = "Foo Bar";
  Assert.equal(
    await profileStorage.creditCards.mergeIfPossible(guid, anotherCreditCard),
    false
  );
  // The change counter is unchanged.
  Assert.equal(getSyncChangeCounter(profileStorage.creditCards, guid), 1);

  // Unable to merge because no credit card number
  anotherCreditCard = profileStorage.creditCards._clone(TEST_CREDIT_CARD_1);
  anotherCreditCard["cc-number"] = "";
  Assert.equal(
    await profileStorage.creditCards.mergeIfPossible(guid, anotherCreditCard),
    false
  );
  // The change counter is still unchanged.
  Assert.equal(getSyncChangeCounter(profileStorage.creditCards, guid), 1);
});

add_task(async function test_mergeToStorage() {
  let profileStorage = await initProfileStorage(
    TEST_STORE_FILE_NAME,
    [TEST_CREDIT_CARD_3, TEST_CREDIT_CARD_4],
    "creditCards"
  );
  // Merge a creditCard to storage
  let anotherCreditCard = profileStorage.creditCards._clone(TEST_CREDIT_CARD_3);
  anotherCreditCard["cc-name"] = "Foo Bar";
  Assert.equal(
    (await profileStorage.creditCards.mergeToStorage(anotherCreditCard)).length,
    2
  );
  Assert.equal(
    (await profileStorage.creditCards.getAll())[0]["cc-name"],
    "Foo Bar"
  );
  Assert.equal(
    (await profileStorage.creditCards.getAll())[0]["cc-exp"],
    "2000-01"
  );
  Assert.equal(
    (await profileStorage.creditCards.getAll())[1]["cc-name"],
    "Foo Bar"
  );
  Assert.equal(
    (await profileStorage.creditCards.getAll())[1]["cc-exp"],
    "2000-01"
  );

  // Empty computed fields shouldn't cause any problem.
  Assert.equal(
    (
      await profileStorage.creditCards.mergeToStorage(
        TEST_CREDIT_CARD_WITH_EMPTY_COMPUTED_FIELD
      )
    ).length,
    0
  );
});

add_task(async function test_getDuplicateGuid() {
  let profileStorage = await initProfileStorage(
    TEST_STORE_FILE_NAME,
    [TEST_CREDIT_CARD_3],
    "creditCards"
  );
  let guid = profileStorage.creditCards._data[0].guid;

  // Absolutely a duplicate.
  Assert.equal(
    await profileStorage.creditCards.getDuplicateGuid(TEST_CREDIT_CARD_3),
    guid
  );

  // Absolutely not a duplicate.
  Assert.equal(
    await profileStorage.creditCards.getDuplicateGuid(TEST_CREDIT_CARD_1),
    null
  );

  // Subset with the same number is a duplicate.
  let record = Object.assign({}, TEST_CREDIT_CARD_3);
  delete record["cc-exp-month"];
  Assert.equal(await profileStorage.creditCards.getDuplicateGuid(record), guid);

  // Superset with the same number is a duplicate.
  record = Object.assign({}, TEST_CREDIT_CARD_3);
  record["cc-name"] = "John Doe";
  Assert.equal(await profileStorage.creditCards.getDuplicateGuid(record), guid);

  // Numbers with the same last 4 digits shouldn't be treated as a duplicate.
  record = Object.assign({}, TEST_CREDIT_CARD_3);
  let last4Digits = record["cc-number"].substr(-4);
  // This number differs from TEST_CREDIT_CARD_3 by swapping the order of the
  // 09 and 90 adjacent digits, which is still a valid credit card number.
  record["cc-number"] = "358999378390" + last4Digits;

  // We don't treat numbers with the same last 4 digits as a duplicate.
  Assert.equal(await profileStorage.creditCards.getDuplicateGuid(record), null);
});

add_task(async function test_getDuplicateGuidMatch() {
  let profileStorage = await initProfileStorage(
    TEST_STORE_FILE_NAME,
    [TEST_CREDIT_CARD_2],
    "creditCards"
  );
  let guid = profileStorage.creditCards._data[0].guid;

  // Absolutely a duplicate.
  Assert.equal(
    await profileStorage.creditCards.getDuplicateGuid(TEST_CREDIT_CARD_2),
    guid
  );

  // Absolutely not a duplicate.
  Assert.equal(
    await profileStorage.creditCards.getDuplicateGuid(TEST_CREDIT_CARD_1),
    null
  );

  // Numbers with the same last 4 digits shouldn't be treated as a duplicate.
  record = Object.assign({}, TEST_CREDIT_CARD_2);

  // We change month from `1` to `2`
  record["cc-exp-month"] = 2;
  Assert.equal(await profileStorage.creditCards.getDuplicateGuid(record), guid);

  // We change year from `2000` to `2001`
  record["cc-exp-year"] = 2001;
  Assert.equal(await profileStorage.creditCards.getDuplicateGuid(record), guid);

  // New name, same card
  record["cc-name"] = "John Doe";
  Assert.equal(await profileStorage.creditCards.getDuplicateGuid(record), guid);
});

add_task(async function test_creditCardFillDisabled() {
  Services.prefs.setBoolPref(
    "extensions.formautofill.creditCards.enabled",
    false
  );

  let path = getTempFile(TEST_STORE_FILE_NAME).path;
  let profileStorage = new FormAutofillStorage(path);
  await profileStorage.initialize();

  Assert.equal(
    !!profileStorage.creditCards,
    true,
    "credit card records initialized and available."
  );

  Services.prefs.setBoolPref(
    "extensions.formautofill.creditCards.enabled",
    true
  );
});

add_task(async function test_creditCardFillUnavailable() {
  Services.prefs.setBoolPref(
    "extensions.formautofill.creditCards.available",
    false
  );

  let path = getTempFile(TEST_STORE_FILE_NAME).path;
  let profileStorage = new FormAutofillStorage(path);
  await profileStorage.initialize();

  try {
    profileStorage.creditCards; // eslint-disable-line no-unused-expressions
    throw new Error("Access credit card didn't throw.");
  } catch (err) {
    Assert.equal(
      err.message,
      "CreditCards is not initialized. " +
        "Please restart if you flip the pref manually."
    );
  }

  Services.prefs.setBoolPref(
    "extensions.formautofill.creditCards.available",
    true
  );
});
