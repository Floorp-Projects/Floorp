/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;

SpecialPowers.addPermission("mobileconnection", true, document);

let connection = navigator.mozMobileConnection;
ok(connection instanceof MozMobileConnection,
   "connection is instanceof " + connection.constructor);

let caseId = 0;
let options = [
  buildOption(5, true, '0000', 0),  // invalid program.

  // test null.
  buildOption(null, true, '0000', 0),
  buildOption(0, null, '0000', 0),
  buildOption(0, true, null, 0),
  buildOption(0, true, '0000', null),

  // test undefined.
  {'enabled': true, 'password': '0000', 'serviceClass': 0},
  {'program': 0, 'password': '0000', 'serviceClass': 0},
  {'program': 0, 'enabled': true, 'serviceClass': 0},
  {'program': 0, 'enabled': true, 'password': '0000'},
];

function buildOption(program, enabled, password, serviceClass) {
  return {
    'program': program,
    'enabled': enabled,
    'password': password,
    'serviceClass': serviceClass
  };
}

function testSetCallBarringOptionError(option) {
  let request = connection.setCallBarringOption(option);
  request.onsuccess = function() {
    ok(false,
       'should not fire onsuccess for invaild call barring option: '
       + JSON.stringify(option));
  };
  request.onerror = function() {
    nextTest();
  };
}

function nextTest() {
  if (caseId >= options.length) {
    cleanUp();
  } else {
    let option = options[caseId++];
    log('test for ' + JSON.stringify(option));
    testSetCallBarringOptionError(option);
  }
}

function cleanUp() {
  SpecialPowers.removePermission("mobileconnection", document);
  finish();
}

nextTest();
