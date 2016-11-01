/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 120000;
MARIONETTE_HEAD_JS = "head.js";

const TEST_UPDATE_DATA = [{
    id: 1,
    data: {
      name: ["Mozilla"],
      tel: [{value: "9876543210987654321001234"}]},
    expect: {
      number: "9876543210987654321001234"}
  }, {
    id:2,
    data: {
      name: ["Saßê黃"],
      tel: [{value: "98765432109876543210998877665544332211001234"}]},
    expect: {
      // We don't support extension chain now.
      number: "9876543210987654321099887766554433221100"}
  }, {
    id: 3,
    data: {
      name: ["Fire 火"],
      tel: [{value: ""}]},
    expect: {
      number: ""}
  }, {
    id: 5,
    data: {
      name: ["Contact001"],
      tel: [{value: "9988776655443322110098765432109876543210"}]},
    expect: {
      number: "9988776655443322110098765432109876543210"}
  }, {
    id: 6,
    data: {
      name: ["Contact002"],
      tel: [{value: "+99887766554433221100"}]},
    expect: {
      number: "+99887766554433221100"}
  }];

function testUpdateContact(aIcc, aType, aContactId, aMozContact, aExpect, aPin2) {
  log("testUpdateContact: type=" + aType +
      ", mozContact=" + JSON.stringify(aMozContact) +
      ", expect=" + aExpect.number + ", pin2=" + aPin2);

  let contact = new mozContact(aMozContact);
  contact.id = aIcc.iccInfo.iccid + aContactId;

  return aIcc.updateContact(aType, contact, aPin2)
    .then((aResult) => {
      is(aResult.tel[0].value, aExpect.number);
      ok(aResult.tel.length == 1);
      // We only support SIM in emulator, so we don't have anr and email field.
      ok(!aResult.email);
      is(contact.id, aIcc.iccInfo.iccid + aContactId);
    }, (aError) => {
      if (aType === "fdn" && aPin2 === undefined) {
        ok(aError.name === "SimPin2",
           "expected error when pin2 is not provided");
      } else {
        ok(false, "Cannot update " + aType + " contact: " + aError.name);
      }
    });
}

function revertContacts(aIcc, aContacts, aType, aPin2) {
  log("revertContacts type=" + aType + ", pin2=" + aPin2);
  let promise = Promise.resolve();

  for (let i = 0; i < aContacts.length; i++) {
    let aContact = aContacts[i];
    promise = promise.then(() => aIcc.updateContact(aType, aContact, aPin2));
  }
  return promise;
}

function testUpdateContacts(aIcc, aType, aCacheContacts, aPin2) {
  let promise = Promise.resolve();

  for (let i = 0; i < TEST_UPDATE_DATA.length; i++) {
    let test_data = TEST_UPDATE_DATA[i];
    promise = promise.then(() => testUpdateContact(aIcc, aType, test_data.id,
                                                   test_data.data, test_data.expect,
                                                   aPin2));
  }

  // Get ICC contact for checking expect contacts
  promise = promise.then(() => aIcc.readContacts(aType))
  .then((aResult) => {
    for (let i = 0; i < TEST_UPDATE_DATA.length; i++) {
      let expectedResult = TEST_UPDATE_DATA[i];
      let contact = aResult[expectedResult.id - 1];

      is(contact.name[0], expectedResult.data.name[0]);
      is(contact.id, aIcc.iccInfo.iccid + expectedResult.id);
      is(contact.tel[0].value, expectedResult.expect.number);
    }
    return revertContacts(aIcc, aCacheContacts, aType, aPin2);
  });

  return promise;
}

// Start tests
startTestCommon(function() {
  let icc = getMozIcc();
  let adnContacts;
  let fdnContacts;

  return icc.readContacts("adn")
    .then((aResult) => {
      adnContacts = aResult;
    })
    .then(() => icc.readContacts("fdn"))
    .then((aResult) => {
      fdnContacts = aResult;
    })
    // Test update adn contacts
    .then(() => testUpdateContacts(icc, "adn", adnContacts))
    // Test update fdn contacts
    .then(() => testUpdateContacts(icc, "fdn", fdnContacts, "0000"))
    // Test one fdn contact without passing pin2
    .then(() => testUpdateContact(icc, "fdn", TEST_UPDATE_DATA[0].id,
                                  TEST_UPDATE_DATA[0].data,
                                  TEST_UPDATE_DATA[0].expect));
});
