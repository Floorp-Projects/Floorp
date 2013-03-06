/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;

SpecialPowers.addPermission("mobileconnection", true, document);

// TODO: bug 847741 move getSimContacts to IccManager
SpecialPowers.addPermission("contacts-read", true, document);
SpecialPowers.addPermission("contacts-write", true, document);
SpecialPowers.addPermission("contacts-create", true, document);

let icc = navigator.mozMobileConnection.icc;
ok(icc instanceof MozIccManager, "icc is instanceof " + icc.constructor);

// TODO: bug 847741 move getSimContacts to IccManager
let mozContacts = window.navigator.mozContacts;
ok(mozContacts);

function testAddIccContact() {
  let contact = new mozContact();

  contact.init({
    name: "add",
    tel: [{value: "0912345678"}]
  });

  // TODO: 'ADN' should change to use lower case
  let updateRequest = icc.updateContact("ADN", contact);

  updateRequest.onsuccess = function onsuccess() {
    // Get ICC contact for checking new contact

    // TODO: 1. bug 847741 move getSimContacts to IccManager
    //       2. 'ADN' should change to use lower case
    let getRequest = mozContacts.getSimContacts("ADN");

    getRequest.onsuccess = function onsuccess() {
      let simContacts = getRequest.result;

      // There are 4 SIM contacts which are harded in emulator
      is(simContacts.length, 5);

      is(simContacts[4].name, "add");
      is(simContacts[4].tel[0].value, "0912345678");

      runNextTest();
    };

    getRequest.onerror = function onerror() {
      ok(false, "Cannot get ICC contacts: " + getRequest.error.name);
      runNextTest();
    };
  };

  updateRequest.onerror = function onerror() {
    ok(false, "Cannot add ICC contact: " + updateRequest.error.name);
    runNextTest();
  };
};

let tests = [
  testAddIccContact,
];

function runNextTest() {
  let test = tests.pop();
  if (!test) {
    cleanUp();
    return;
  }

  test();
}

function cleanUp() {
  SpecialPowers.removePermission("mobileconnection", document);

  // TODO: bug 847741 move getSimContacts to IccManager
  SpecialPowers.removePermission("contacts-read", document);
  SpecialPowers.removePermission("contacts-write", document);
  SpecialPowers.removePermission("contacts-create", document);

  finish();
}

runNextTest();
