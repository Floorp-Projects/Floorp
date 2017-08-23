/*
 * Test for make sure getRecords can retrieve right collection from storag.
 */

"use strict";

Cu.import("resource://formautofill/FormAutofillParent.jsm");
Cu.import("resource://formautofill/ProfileStorage.jsm");

add_task(async function test_getRecords() {
  let formAutofillParent = new FormAutofillParent();

  await formAutofillParent.init();
  await formAutofillParent.profileStorage.initialize();

  let fakeResult = {
    addresses: [{
      "given-name": "Timothy",
      "additional-name": "John",
      "family-name": "Berners-Lee",
      "organization": "World Wide Web Consortium",
    }],
    creditCards: [{
      "cc-name": "John Doe",
      "cc-number": "1234567812345678",
      "cc-exp-month": 4,
      "cc-exp-year": 2017,
    }],
  };

  ["addresses", "creditCards", "nonExisting"].forEach(collectionName => {
    let collection = profileStorage[collectionName];
    let expectedResult = fakeResult[collectionName] || [];
    let target = {
      sendAsyncMessage: function sendAsyncMessage(msg, payload) {},
    };
    let mock = sinon.mock(target);
    mock.expects("sendAsyncMessage").once().withExactArgs("FormAutofill:Records", expectedResult);

    if (collection) {
      sinon.stub(collection, "getAll");
      collection.getAll.returns(expectedResult);
    }
    formAutofillParent._getRecords({collectionName}, target);
    mock.verify();
    if (collection) {
      do_check_eq(collection.getAll.called, true);
    }
  });
});
