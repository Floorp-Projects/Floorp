/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;

SpecialPowers.addPermission("mobileconnection", true, document);

// Permission changes can't change existing Navigator.prototype
// objects, so grab our objects from a new Navigator
let ifr = document.createElement("iframe");
let mobileConnection;
ifr.onload = function() {
  mobileConnection = ifr.contentWindow.navigator.mozMobileConnections[0];

  // Start the test
  verifyInitialState();
};
document.body.appendChild(ifr);

function verifyInitialState() {
  log("Verifying initial state.");
  ok(mobileConnection instanceof MozMobileConnection,
      "mobileConnection is instanceof " + mobileConnection.constructor);
  // Want to start test with mobileConnection.data.state 'registered'
  // This is the default state; if it is not currently this value then set it
  log("Starting mobileConnection.data.state is: '"
      + mobileConnection.data.state + "'.");
  if (mobileConnection.data.state != "registered") {
    changeDataStateAndVerify("home", "registered", testUnregistered);
  } else {
    testUnregistered();
  }
}

function changeDataStateAndVerify(dataState, expected, nextFunction) {
  let gotCallback = false;

  // Change the mobileConnection.data.state via 'gsm data' command
  log("Changing emulator data state to '" + dataState
      + "' and waiting for 'ondatachange' event.");

  // Setup 'ondatachange' event handler
  mobileConnection.addEventListener("datachange", function ondatachange() {
    mobileConnection.removeEventListener("datachange", ondatachange);
    log("Received 'ondatachange' event.");
    log("mobileConnection.data.state is now '"
        + mobileConnection.data.state + "'.");
    is(mobileConnection.data.state, expected, "data.state");
    waitFor(nextFunction, function() {
      return(gotCallback);
    });
  });

  // Change the emulator data state
  gotCallback = false;
  runEmulatorCmd("gsm data " + dataState, function(result) {
    is(result[0], "OK");
    log("Emulator callback complete.");
    gotCallback = true;
  });
}

function testUnregistered() {
  log("Test 1: Unregistered.");
  // Set emulator data state to 'unregistered' and verify
  // Expect mobileConnection.data.state to be 'notsearching'
  changeDataStateAndVerify("unregistered", "notSearching", testRoaming);
}

function testRoaming() {
  log("Test 2: Roaming.");
  // Set emulator data state to 'roaming' and verify
  // Expect mobileConnection.data.state to be 'registered'
  changeDataStateAndVerify("roaming", "registered", testOff);
}

function testOff() {
  log("Test 3: Off.");
  // Set emulator data state to 'off' and verify
  // Expect mobileConnection.data.state to be 'notsearching'
  changeDataStateAndVerify("off", "notSearching", testSearching);
}

function testSearching() {
  log("Test 4: Searching.");
  // Set emulator data state to 'searching' and verify
  // Expect mobileConnection.data.state to be 'searching'
  changeDataStateAndVerify("searching", "searching", testDenied);
}

function testDenied() {
  log("Test 5: Denied.");
  // Set emulator data state to 'denied' and verify
  // Expect mobileConnection.data.state to be 'denied'
  changeDataStateAndVerify("denied", "denied", testOn);
}

function testOn() {
  log("Test 6: On.");
  // Set emulator data state to 'on' and verify
  // Expect mobileConnection.data.state to be 'registered'
  changeDataStateAndVerify("on", "registered", testOffAgain);
}

function testOffAgain() {
  log("Test 7: Off again.");
  // Set emulator data state to 'off' and verify
  // Expect mobileConnection.data.state to be 'notsearching'
  changeDataStateAndVerify("off", "notSearching", testHome);
}

function testHome() {
  log("Test 8: Home.");
  // Set emulator data state to 'home' and verify
  // Expect mobileConnection.data.state to be 'registered'
  changeDataStateAndVerify("home", "registered", cleanUp);
}

function cleanUp() {
  mobileConnection.ondatachange = null;
  SpecialPowers.removePermission("mobileconnection", document);
  finish();
}
