/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 20000;

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

let emulatorStartLac = 0;
let emulatorStartCid = 0;

function verifyInitialState() {
  log("Verifying initial state.");
  ok(mobileConnection instanceof MozMobileConnection,
      "mobileConnection is instanceof " + mobileConnection.constructor);
  testStartingCellLocation();
}

function testStartingCellLocation() {
  // Get the current emulator data cell location
  log("Getting the starting GSM location from the emulator.");

  runEmulatorCmd("gsm location", function(result) {
    log("Emulator callback.");
    is(result[0].substring(0,3), "lac", "lac output");
    is(result[1].substring(0,2), "ci", "ci output");
    is(result[2], "OK", "emulator ok");

    emulatorStartLac = result[0].substring(5);
    log("Emulator GSM location LAC is '" + emulatorStartLac + "'.");
    emulatorStartCid = result[1].substring(4);
    log("Emulator GSM location CID is '" + emulatorStartCid + "'.");

    log("mobileConnection.data.cell.gsmLocationAreaCode is '"
        + mobileConnection.data.cell.gsmLocationAreaCode + "'.");
    log("mobileConnection.data.cell.gsmCellId is '"
        + mobileConnection.data.cell.gsmCellId + "'.");

    // Verify the mobileConnection.data.cell location matches emulator values
    if (emulatorStartLac == -1) {
      // Emulator initializes LAC to -1, corresponds to these values
      is(mobileConnection.data.cell.gsmLocationAreaCode,
          65535, "starting LAC");
    } else {
      // A previous test changed the LAC, so verify API matches emulator
      is(mobileConnection.data.cell.gsmLocationAreaCode,
          emulatorStartLac, "starting LAC");
    }
    if (emulatorStartCid == -1) {
      // Emulator initializes CID to -1, corresponds to these values
      is(mobileConnection.data.cell.gsmCellId, 268435455, "starting CID");
    } else {
      // A previous test changed the CID, so verify API matches emulator
      is(mobileConnection.data.cell.gsmCellId,
          emulatorStartCid, "starting CID");
    }

    // Now test changing the GSM location
    testChangeCellLocation(emulatorStartLac, emulatorStartCid);
  });
}

function testChangeCellLocation() {
  // Change emulator GSM location and verify mobileConnection.data.cell values
  let newLac = 1000;
  let newCid = 2000;
  let gotCallback = false;

  // Ensure values will actually be changed
  if (newLac == emulatorStartLac) { newLac++; };
  if (newCid == emulatorStartCid) { newCid++; };

  // Setup 'ondatachange' event listener
  mobileConnection.addEventListener("datachange", function ondatachange() {
    mobileConnection.removeEventListener("datachange", ondatachange);
    log("Received 'ondatachange' event.");
    log("mobileConnection.data.cell.gsmLocationAreaCode is now '"
        + mobileConnection.data.cell.gsmLocationAreaCode + "'.");
    log("mobileConnection.data.cell.gsmCellId is now '"
        + mobileConnection.data.cell.gsmCellId + "'.");
    is(mobileConnection.data.cell.gsmLocationAreaCode, newLac,
        "data.cell.gsmLocationAreaCode");
    is(mobileConnection.data.cell.gsmCellId, newCid, "data.cell.gsmCellId");
    waitFor(restoreLocation, function() {
      return(gotCallback);
    });
  });

  // Use emulator command to change GSM location
  log("Changing emulator GSM location to '" + newLac + ", " + newCid
      + "' and waiting for 'ondatachange' event.");
  gotCallback = false;
  runEmulatorCmd("gsm location " + newLac + " " + newCid, function(result) {
    is(result[0], "OK");
    log("Emulator callback on location change.");
    gotCallback = true;
  });
}

function restoreLocation() {
  // Restore the emulator GSM location back to what it was originally
  log("Restoring emulator GSM location back to '" + emulatorStartLac + ", "
      + emulatorStartCid + "'.");
  runEmulatorCmd("gsm location " + emulatorStartLac + " " + emulatorStartCid,
      function(result) {
    log("Emulator callback on restore.");
    is(result[0], "OK");
    cleanUp();
  });
}

function cleanUp() {
  mobileConnection.ondatachange = null;
  SpecialPowers.removePermission("mobileconnection", document);
  finish();
}
