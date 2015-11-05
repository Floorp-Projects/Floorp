/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 120000;
MARIONETTE_HEAD_JS = "head.js";

var TEST_ADD_DATA = [{
    // a contact without email and anr.
    name: ["add1"],
    tel: [{value: "0912345678"}],
  }, {
    // a contact over 20 digits.
    name: ["add2"],
    tel: [{value: "012345678901234567890123456789"}],
  }, {
    // a contact over 40 digits.
    name: ["add3"],
    tel: [{value: "01234567890123456789012345678901234567890123456789"}],
  }, {
    // a contact with email but without anr.
    name: ["add4"],
    tel: [{value: "01234567890123456789"}],
    email:[{value: "test@mozilla.com"}],
  }, {
    // a contact with anr but without email.
    name: ["add5"],
    tel: [{value: "01234567890123456789"}, {value: "123456"}, {value: "123"}],
  }, {
    // a contact with email and anr.
    name: ["add6"],
    tel: [{value: "01234567890123456789"}, {value: "123456"}, {value: "123"}],
    email:[{value: "test@mozilla.com"}],
  }, {
    // a contact without number.
    name: ["add7"],
    tel: [{value: ""}],
  }, {
    // a contact without name.
    name: [""],
    tel: [{value: "0987654321"}],
  }];

function testAddContact(aIcc, aType, aMozContact, aPin2) {
  log("testAddContact: type=" + aType + ", pin2=" + aPin2);
  let contact = new mozContact(aMozContact);

  return aIcc.updateContact(aType, contact, aPin2)
    .then((aResult) => {
      is(aResult.name[0], aMozContact.name[0]);
      // Maximum digits of the Dialling Number is 20, and maximum digits of Extension is 20.
      is(aResult.tel[0].value, aMozContact.tel[0].value.substring(0, 40));
      // We only support SIM in emulator, so we don't have anr and email field.
      ok(aResult.tel.length == 1);
      ok(!aResult.email);
    }, (aError) => {
      if (aType === "fdn" && aPin2 === undefined) {
        ok(aError.name === "SimPin2",
           "expected error when pin2 is not provided");
      } else {
        ok(false, "Cannot add " + aType + " contact: " + aError.name);
      }
    });
}

function removeContacts(aIcc, aContacts, aType, aPin2) {
  log("removeContacts: type=" + aType + ", pin2=" + aPin2);
  let promise = Promise.resolve();

  // Clean up contacts
  for (let i = 0; i < aContacts.length ; i++) {
    let contact = new mozContact({});
    contact.id = aContacts[i].id;
    promise = promise.then(() => aIcc.updateContact(aType, contact, aPin2));
  }
  return promise;
}

function testAddContacts(aIcc, aType, aPin2) {
  let promise = Promise.resolve();

  for (let i = 0; i < TEST_ADD_DATA.length; i++) {
    let test_data = TEST_ADD_DATA[i];

    promise = promise.then(() => testAddContact(aIcc, aType, test_data, aPin2));
  }

  // Get ICC contact for checking new contacts
  promise = promise.then(() => aIcc.readContacts(aType))
  .then((aResult) => {
    aResult = aResult.slice(aResult.length - TEST_ADD_DATA.length);

    for (let i = 0; i < aResult.length ; i++) {
      let contact = aResult[i];
      let expectedResult = TEST_ADD_DATA[i];

      is(contact.name[0], expectedResult.name[0]);
      // Maximum digits of the Dialling Number is 20, and maximum digits of Extension is 20.
      is(contact.tel[0].value, expectedResult.tel[0].value.substring(0, 40));
      is(contact.id.substring(0, aIcc.iccInfo.iccid.length), aIcc.iccInfo.iccid);
    }
    return removeContacts(aIcc, aResult, aType, aPin2);
  });


  return promise;
}

// Start tests
startTestCommon(function() {
  let icc = getMozIcc();

  return Promise.resolve()
    // Test add adn contacts
    .then(() => testAddContacts(icc, "adn"))
    // Test add fdn contacts
    .then(() => testAddContacts(icc, "fdn", "0000"))
    // Test one fdn contact without passing pin2
    .then(() => testAddContact(icc, "fdn", TEST_ADD_DATA[0]));
});
