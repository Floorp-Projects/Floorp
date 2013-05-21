/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;

SpecialPowers.addPermission("mobileconnection", true, document);

let connection = navigator.mozMobileConnection;
ok(connection instanceof MozMobileConnection,
   "connection is instanceof " + connection.constructor);

function testGetCallBarringOption() {
  let option = {'program': 0, 'password': '', 'serviceClass': 0};
  let request = connection.getCallBarringOption(option);
  request.onsuccess = function() {
    ok(request.result);
    ok('enabled' in request.result, 'should have "enabled" field');
    cleanUp();
  };
  request.onerror = function() {
    // Call barring is not supported by current emulator.
    cleanUp();
  };
}

function cleanUp() {
  SpecialPowers.removePermission("mobileconnection", document);
  finish();
}

testGetCallBarringOption();
