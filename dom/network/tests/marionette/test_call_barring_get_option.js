/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;

SpecialPowers.addPermission("mobileconnection", true, document);

// Permission changes can't change existing Navigator.prototype
// objects, so grab our objects from a new Navigator
let ifr = document.createElement("iframe");
let connection;
ifr.onload = function() {
  connection = ifr.contentWindow.navigator.mozMobileConnections[0];

  ok(connection instanceof ifr.contentWindow.MozMobileConnection,
     "connection is instanceof " + connection.constructor);

  testGetCallBarringOption();
};
document.body.appendChild(ifr);

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
