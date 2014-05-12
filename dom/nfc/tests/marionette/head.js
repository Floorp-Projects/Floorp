/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let pendingEmulatorCmdCount = 0;

let Promise =
  SpecialPowers.Cu.import("resource://gre/modules/Promise.jsm").Promise;
let nfc = window.navigator.mozNfc;

SpecialPowers.addPermission("nfc-manager", true, document);

/**
 * Emulator helper.
 */
let emulator = (function() {
  let pendingCmdCount = 0;
  let originalRunEmulatorCmd = runEmulatorCmd;

  // Overwritten it so people could not call this function directly.
  runEmulatorCmd = function() {
    throw "Use emulator.run(cmd, callback) instead of runEmulatorCmd";
  };

  function run(cmd, callback) {
    pendingCmdCount++;
    originalRunEmulatorCmd(cmd, function(result) {
      pendingCmdCount--;
      if (callback && typeof callback === "function") {
        callback(result);
      }
    });
  }

  return {
    run: run
  };
}());

function toggleNFC(enabled, callback) {
  isnot(callback, null);

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

const NDEF = {
  TNF_WELL_KNOWN: 1
};

var NfcUtils = {
  fromUTF8: function(str) {
    let buf = new Uint8Array(str.length);
    for (let i = 0; i < str.length; ++i) {
      buf[i] = str.charCodeAt(i);
    }
    return buf;
  }
};
