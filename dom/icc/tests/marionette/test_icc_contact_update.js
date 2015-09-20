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
      number: null}
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
      // Get ICC contact for checking expect contact
      return aIcc.readContacts(aType)
        .then((aResult) => {
          let contact = aResult[aContactId - 1];

          is(contact.name[0], aMozContact.name[0]);

          if (aExpect.number == null) {
            is(contact.tel, null);
          } else {
            is(contact.tel[0].value, aExpect.number);
          }

          is(contact.id, aIcc.iccInfo.iccid + aContactId);
        });
      }, (aError) => {
        if (aType === "fdn" && aPin2 === undefined) {
          ok(aError.name === "SimPin2",
             "expected error when pin2 is not provided");
        } else {
          ok(false, "Cannot update " + aType + " contact: " + aError.name);
        }
      });
}

function revertContact(aIcc, aContact, aType, aPin2) {
  log("revertContact: contact:" + JSON.stringify(aContact) +
      ", type=" + aType + ", pin2=" + aPin2);

  return aIcc.updateContact(aType, aContact, aPin2);
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
    .then(() => {
      let promise = Promise.resolve();
      for (let i = 0; i < TEST_UPDATE_DATA.length; i++) {
        let test_data = TEST_UPDATE_DATA[i];
        let adnContact = adnContacts[test_data.id - 1];
        let fdnContact = fdnContacts[test_data.id - 1];

        // Test update adn contacts
        promise = promise.then(() => testUpdateContact(icc, "adn", test_data.id,
                                                       test_data.data, test_data.expect))
          // Test update fdn contacts
          .then(() => testUpdateContact(icc, "fdn", test_data.id, test_data.data,
                                        test_data.expect))
          // Test update fdn contacts without passing pin2
          .then(() => testUpdateContact(icc, "fdn", test_data.id, test_data.data,
                                        test_data.expect, "0000"))
          .then(() => revertContact(icc, adnContact, "adn"))
          .then(() => revertContact(icc, fdnContact, "fdn", "0000"));
      }
      return promise;
    });
});
