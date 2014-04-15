/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let pendingEmulatorCmdCount = 0;

SpecialPowers.addPermission("nfc-manager", true, document);

function toggleNFC(enabled, callback) {
  isnot(callback, null);

  let nfc = window.navigator.mozNfc;
  let req;
  if (enabled) {
    req = nfc.startPoll();
  } else {
    req = nfc.powerOff();
  }

  req.onsuccess = function() {
    callback();
  };

  req.onerror = function() {
    ok(false, 'operation failed, error ' + req.error.name);
    finish();
  };
}

function cleanUp() {
  log('Cleaning up');
  waitFor(function() {
            SpecialPowers.removePermission("nfc-manager", document);
            finish()
          },
          function() {
            return pendingEmulatorCmdCount === 0;
          });
}

function runNextTest() {
  let test = tests.shift();
  if (!test) {
    cleanUp();
    return;
  }
  test();
}

// run this function to start tests
function runTests() {
  if ('mozNfc' in window.navigator) {
    runNextTest();
  } else {
    // succeed immediately on systems without NFC
    log('Skipping test on system without NFC');
    ok(true, 'Skipping test on system without NFC');
    finish();
  }
}
