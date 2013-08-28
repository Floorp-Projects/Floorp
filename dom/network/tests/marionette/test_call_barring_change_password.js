/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;

SpecialPowers.addPermission("mobileconnection", true, document);

// Permission changes can't change existing Navigator.prototype
// objects, so grab our objects from a new Navigator
let ifr = document.createElement("iframe");
let connection;
ifr.onload = function() {
  connection = ifr.contentWindow.navigator.mozMobileConnection;

  ok(connection instanceof ifr.contentWindow.MozMobileConnection,
     "connection is instanceof " + connection.constructor);

  setTimeout(testChangeCallBarringPasswordWithFailure, 0);
};
document.body.appendChild(ifr);

function testChangeCallBarringPasswordWithFailure() {
  // Incorrect parameters, expect onerror callback.
  let options = [
    {pin: null, newPin: '0000'},
    {pin: '0000', newPin: null},
    {pin: null, newPin: null},
    {pin: '000', newPin: '0000'},
    {pin: '00000', newPin: '1111'},
    {pin: 'abcd', newPin: 'efgh'},
  ];

  function do_test() {
    for (let i = 0; i < options.length; i++) {
      let request = connection.changeCallBarringPassword(options[i]);

      request.onsuccess = function() {
        ok(false, 'Unexpected result.');
        setTimeout(cleanUp , 0);
      };

      request.onerror = function() {
        ok(request.error.name === 'InvalidPassword', 'InvalidPassword');
        if (i >= options.length) {
          setTimeout(testChangeCallBarringPasswordWithSuccess, 0);
        }
      };
    }
  }

  do_test();
}

function testChangeCallBarringPasswordWithSuccess() {
  // TODO: Bug 906603 - B2G RIL: Support Change Call Barring Password on
  // Emulator.
  setTimeout(cleanUp , 0);
}

function cleanUp() {
  SpecialPowers.removePermission("mobileconnection", document);
  finish();
}
