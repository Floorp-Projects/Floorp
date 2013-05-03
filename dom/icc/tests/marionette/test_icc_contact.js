/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;

SpecialPowers.addPermission("mobileconnection", true, document);

let icc = navigator.mozMobileConnection.icc;
ok(icc instanceof MozIccManager, "icc is instanceof " + icc.constructor);

function testImportSimContacts() {
  let request = icc.readContacts("adn");
  request.onsuccess = function onsuccess() {
    let simContacts = request.result;

    is(Array.isArray(simContacts), true);

    is(simContacts[0].name, "Mozilla");
    is(simContacts[0].tel[0].value, "15555218201");

    is(simContacts[1].name, "Saßê黃");
    is(simContacts[1].tel[0].value, "15555218202");

    is(simContacts[2].name, "Fire 火");
    is(simContacts[2].tel[0].value, "15555218203");

    is(simContacts[3].name, "Huang 黃");
    is(simContacts[3].tel[0].value, "15555218204");

    runNextTest();
  };

  request.onerror = function onerror() {
    ok(false, "Cannot get Sim Contacts");
    runNextTest();
  };
};

function testAddIccContact() {
  let contact = new mozContact();

  contact.init({
    name: "add",
    tel: [{value: "0912345678"}]
  });

  let updateRequest = icc.updateContact("adn", contact);

  updateRequest.onsuccess = function onsuccess() {
    // Get ICC contact for checking new contact

    let getRequest = icc.readContacts("adn");

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
  testImportSimContacts,
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
  finish();
}

runNextTest();
