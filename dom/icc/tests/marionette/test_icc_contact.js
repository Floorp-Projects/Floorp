/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;

SpecialPowers.addPermission("mobileconnection", true, document);

let icc = navigator.mozIccManager;
ok(icc instanceof MozIccManager, "icc is instanceof " + icc.constructor);

function testReadContacts(type) {
  let request = icc.readContacts(type);
  request.onsuccess = function onsuccess() {
    let contacts = request.result;

    is(Array.isArray(contacts), true);

    is(contacts[0].name, "Mozilla");
    is(contacts[0].tel[0].value, "15555218201");
    is(contacts[0].id, "890141032111185107201");

    is(contacts[1].name, "Saßê黃");
    is(contacts[1].tel[0].value, "15555218202");
    is(contacts[1].id, "890141032111185107202");

    is(contacts[2].name, "Fire 火");
    is(contacts[2].tel[0].value, "15555218203");
    is(contacts[2].id, "890141032111185107203");

    is(contacts[3].name, "Huang 黃");
    is(contacts[3].tel[0].value, "15555218204");
    is(contacts[3].id, "890141032111185107204");

    runNextTest();
  };

  request.onerror = function onerror() {
    ok(false, "Cannot get " + type + " contacts");
    runNextTest();
  };
};

function testAddContact(type, pin2) {
  let contact = new mozContact();

  contact.init({
    name: "add",
    tel: [{value: "0912345678"}],
    email:[]
  });

  let updateRequest = icc.updateContact(type, contact, pin2);

  updateRequest.onsuccess = function onsuccess() {
    // Get ICC contact for checking new contact

    let getRequest = icc.readContacts(type);

    getRequest.onsuccess = function onsuccess() {
      let contacts = getRequest.result;

      // There are 4 SIM contacts which are harded in emulator
      is(contacts.length, 5);

      is(contacts[4].name, "add");
      is(contacts[4].tel[0].value, "0912345678");

      runNextTest();
    };

    getRequest.onerror = function onerror() {
      ok(false, "Cannot get " + type + " contacts: " + getRequest.error.name);
      runNextTest();
    };
  };

  updateRequest.onerror = function onerror() {
    if (type === "fdn" && pin2 === undefined) {
      ok(updateRequest.error.name === "pin2 is empty",
         "expected error when pin2 is not provided");
    } else {
      ok(false, "Cannot add " + type + " contact: " + updateRequest.error.name);
    }
    runNextTest();
  };
};

function testReadAdnContacts() {
  testReadContacts("adn");
}

function testAddAdnContact() {
  testAddContact("adn");
}

function testReadFdnContacts() {
  testReadContacts("fdn");
}

function testAddFdnContact() {
  testAddContact("fdn", "0000");
  testAddContact("fdn");
}

let tests = [
  testReadAdnContacts,
  testAddAdnContact,
  testReadFdnContacts,
  testAddFdnContact
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
