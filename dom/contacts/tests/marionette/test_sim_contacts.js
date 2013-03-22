/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;

SpecialPowers.addPermission("contacts-read", true, document);
// TODO: see bug 804623, We are preventing "read" operations
// even if just "write" has been set to DENY_ACTION.
// Bug 816900: Because PermissionPromptHelper.jsm will expand the
// contacts-read to contacts-read, contacts-write, contacts-create,
// we add the other two permissions here.
SpecialPowers.addPermission("contacts-write", true, document);
SpecialPowers.addPermission("contacts-create", true, document);

let mozContacts = window.navigator.mozContacts;
ok(mozContacts);

function testImportSimContacts() {
  let request = mozContacts.getSimContacts("ADN");
  request.onsuccess = function onsuccess() {
    let simContacts = request.result;

    is(Array.isArray(simContacts), true);

    // These SIM contacts are harded in external/qemu/telephony/sim_card.c
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
  SpecialPowers.removePermission("contacts-write", document);
  SpecialPowers.removePermission("contacts-create", document);
  finish();
}

runNextTest();
