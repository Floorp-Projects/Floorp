/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;

SpecialPowers.addPermission("mobileconnection", true, document);

// Permission changes can't change existing Navigator.prototype
// objects, so grab our objects from a new Navigator
let ifr = document.createElement("iframe");
let connection;
ifr.onload = function() {
  connection = ifr.contentWindow.navigator.mozMobileConnections[0];
  ok(connection instanceof ifr.contentWindow.MozMobileConnection,
     "connection is instanceof " + connection.constructor);

  // The emulator's hard coded iccid value.
  // See it here {B2G_HOME}/external/qemu/telephony/sim_card.c.
  is(connection.iccId, 89014103211118510720);

  runNextTest();
};
document.body.appendChild(ifr);

function waitForIccChange(callback) {
  connection.addEventListener("iccchange", function handler() {
    connection.removeEventListener("iccchange", handler);
    callback();
  });
}

function setRadioEnabled(enabled) {
  let request  = connection.setRadioEnabled(enabled);

  request.onsuccess = function onsuccess() {
    log('setRadioEnabled: ' + enabled);
  };

  request.onerror = function onerror() {
    ok(false, "setRadioEnabled should be ok");
  };
}

function testIccChangeOnRadioPowerOff() {
  // Turn off radio
  setRadioEnabled(false);

  waitForIccChange(function() {
    is(connection.iccId, null);
    runNextTest();
  });
}

function testIccChangeOnRadioPowerOn() {
  // Turn on radio
  setRadioEnabled(true);

  waitForIccChange(function() {
    // The emulator's hard coded iccid value.
    is(connection.iccId, 89014103211118510720);
    runNextTest();
  });
}

let tests = [
  testIccChangeOnRadioPowerOff,
  testIccChangeOnRadioPowerOn
];

function runNextTest() {
  let test = tests.shift();
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
