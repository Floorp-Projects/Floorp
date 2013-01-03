/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;

SpecialPowers.addPermission("contacts-read", true, document);

let mozContacts = window.navigator.mozContacts;
ok(mozContacts);

function testImportSimContacts() {
  let request = mozContacts.getSimContacts("ADN");
  request.onsuccess = function onsuccess() {
    let simContacts = request.result;

    // These SIM contacts are harded in external/qemu/telephony/sim_card.c
    is(simContacts.length, 4);

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

let tests = [
  testImportSimContacts,
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
  SpecialPowers.removePermission("contacts-read", document);
  finish();
}

runNextTest();
