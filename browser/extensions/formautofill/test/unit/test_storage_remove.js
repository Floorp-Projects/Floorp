/**
 * Tests removing all address/creditcard records.
 */

"use strict";

let FormAutofillStorage;
add_task(async function setup() {
  ({ FormAutofillStorage } = ChromeUtils.import(
    "resource://autofill/FormAutofillStorage.jsm"
  ));
});

const TEST_STORE_FILE_NAME = "test-tombstones.json";

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
  tel: "+1 617 253 5702",
  email: "timbl@w3.org",
};

const TEST_ADDRESS_2 = {
  "street-address": "Some Address",
  country: "US",
};

const TEST_CREDIT_CARD_1 = {
  "cc-name": "John Doe",
  "cc-number": "4111111111111111",
  "cc-exp-month": 4,
  "cc-exp-year": 2017,
};

const TEST_CREDIT_CARD_2 = {
  "cc-name": "Timothy Berners-Lee",
  "cc-number": "4929001587121045",
  "cc-exp-month": 12,
  "cc-exp-year": 2022,
};

// Like add_task, but actually adds 2 - one for addresses and one for cards.
function add_storage_task(test_function) {
  add_task(async function() {
    let path = getTempFile(TEST_STORE_FILE_NAME).path;
    let profileStorage = new FormAutofillStorage(path);
    await profileStorage.initialize();
    let address_records = [TEST_ADDRESS_1, TEST_ADDRESS_2];
    let cc_records = [TEST_CREDIT_CARD_1, TEST_CREDIT_CARD_2];

    for (let [storage, record] of [
      [profileStorage.addresses, address_records],
      [profileStorage.creditCards, cc_records],
    ]) {
      await test_function(storage, record);
    }
  });
}

add_storage_task(async function test_remove_everything(storage, records) {
  info("check simple tombstone semantics");

  let guid = await storage.add(records[0]);
  Assert.equal((await storage.getAll()).length, 1);

  storage.pullSyncChanges(); // force sync metadata, which triggers tombstone behaviour.

  storage.remove(guid);

  await storage.add(records[1]);
  // getAll() is still 1 as we deleted the first.
  Assert.equal((await storage.getAll()).length, 1);

  // check we have the tombstone.
  Assert.equal((await storage.getAll({ includeDeleted: true })).length, 2);

  storage.removeAll();

  // should have deleted both the existing and deleted records.
  Assert.equal((await storage.getAll({ includeDeleted: true })).length, 0);
});
