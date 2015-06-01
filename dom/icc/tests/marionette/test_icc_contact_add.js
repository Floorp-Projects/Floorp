/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 90000;
MARIONETTE_HEAD_JS = "head.js";

let TEST_ADD_DATA = [{
    // a contact without email and anr.
    name: ["add1"],
    tel: [{value: "0912345678"}],
  }, {
    // a contact over 20 digits.
    name: ["add2"],
    tel: [{value: "012345678901234567890123456789"}],
  }, {
    // a contact with email but without anr.
    name: ["add3"],
    tel: [{value: "01234567890123456789"}],
    email:[{value: "test@mozilla.com"}],
  }, {
    // a contact with anr but without email.
    name: ["add4"],
    tel: [{value: "01234567890123456789"}, {value: "123456"}, {value: "123"}],
  }, {
    // a contact with email and anr.
    name: ["add5"],
    tel: [{value: "01234567890123456789"}, {value: "123456"}, {value: "123"}],
    email:[{value: "test@mozilla.com"}],
  }];

function testAddContact(aIcc, aType, aMozContact, aPin2) {
  log("testAddContact: type=" + aType + ", pin2=" + aPin2);
  let contact = new mozContact(aMozContact);

  return aIcc.updateContact(aType, contact, aPin2)
    .then((aResult) => {
      is(aResult.name[0], aMozContact.name[0]);
      // Maximum digits of the Dialling Number is 20.
      is(aResult.tel[0].value, aMozContact.tel[0].value.substring(0, 20));
      // We only support SIM in emulator, so we don't have anr and email field.
      ok(aResult.tel.length == 1);
      ok(!aResult.email);

      // Get ICC contact for checking new contact
      return aIcc.readContacts(aType)
        .then((aResult) => {
          let contact = aResult[aResult.length - 1];

          is(contact.name[0], aMozContact.name[0]);
          // Maximum digits of the Dialling Number is 20.
          is(contact.tel[0].value, aMozContact.tel[0].value.substring(0, 20));
          is(contact.id, aIcc.iccInfo.iccid + aResult.length);

          return contact.id;
        })
        .then((aContactId) => {
          // Clean up contact
          return removeContact(aIcc, aContactId, aType, aPin2);
        });
    }, (aError) => {
      if (aType === "fdn" && aPin2 === undefined) {
        ok(aError.name === "SimPin2",
           "expected error when pin2 is not provided");
      } else {
        ok(false, "Cannot add " + aType + " contact: " + aError.name);
      }
    })
}

function removeContact(aIcc, aContactId, aType, aPin2) {
  log("removeContact: contactId=" + aContactId +
      ", type=" + aType + ", pin2=" + aPin2);

  let contact = new mozContact({});
  contact.id = aIcc.iccInfo.iccid + aContactId;

  return aIcc.updateContact(aType, contact, aPin2);
}

// Start tests
startTestCommon(function() {
  let icc = getMozIcc();

  for (let i = 0; i < TEST_ADD_DATA.length; i++) {
    let test_data = TEST_ADD_DATA[i];
    // Test add adn contacts
    return testAddContact(icc, "adn", test_data)
      // Test add fdn contacts
      .then(() => testAddContact(icc, "fdn", test_data, "0000"))
      // Test add fdn contacts without passing pin2
      .then(() => testAddContact(icc, "fdn", test_data));
  }
});
