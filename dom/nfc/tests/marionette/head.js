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

function toggleNFC(enabled) {
  let deferred = Promise.defer();

  let req;
  if (enabled) {
    req = nfc.startPoll();
  } else {
    req = nfc.powerOff();
  }

  req.onsuccess = function() {
    deferred.resolve();
  };

  req.onerror = function() {
    ok(false, 'operation failed, error ' + req.error.name);
    deferred.reject();
    finish();
  };

  return deferred.promise;
}

function enableRE0() {
  let deferred = Promise.defer();
  let cmd = 'nfc nci rf_intf_activated_ntf 0';

  emulator.run(cmd, function(result) {
    is(result.pop(), 'OK', 'check activation of RE0');
    deferred.resolve();
  });

  return deferred.promise;
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
  TNF_WELL_KNOWN: 1,

  // compares two NDEF messages
  compare: function(ndef1, ndef2) {
    isnot(ndef1, null, "LHS message is not null");
    isnot(ndef2, null, "RHS message is not null");
    is(ndef1.length, ndef2.length,
       "NDEF messages have the same number of records");
    ndef1.forEach(function(record1, index) {
      let record2 = this[index];
      is(record1.tnf, record2.tnf, "test for equal TNF fields");
      let fields = ["type", "id", "payload"];
      fields.forEach(function(value) {
        let field1 = record1[value];
        let field2 = record2[value];
        is(field1.length, field2.length,
           value + " fields have the same length");
        let eq = true;
        for (let i = 0; eq && i < field1.length; ++i) {
          eq = (field1[i] === field2[i]);
        }
        ok(eq, value + " fields contain the same data");
      });
    }, ndef2);
  },

  // parses an emulator output string into an NDEF message
  parseString: function(str) {
    // make it an object
    let arr = null;
    try {
      arr = JSON.parse(str);
    } catch (e) {
      ok(false, "Parser error: " + e.message);
      return null;
    }
    // and build NDEF array
    let ndef = arr.map(function(value) {
        let type = new Uint8Array(NfcUtils.fromUTF8(this.atob(value.type)));
        let id = new Uint8Array(NfcUtils.fromUTF8(this.atob(value.id)));
        let payload =
          new Uint8Array(NfcUtils.fromUTF8(this.atob(value.payload)));
        return new MozNDEFRecord(value.tnf, type, id, payload);
      }, window);
    return ndef;
  }
};

var NfcUtils = {
  fromUTF8: function(str) {
    let buf = new Uint8Array(str.length);
    for (let i = 0; i < str.length; ++i) {
      buf[i] = str.charCodeAt(i);
    }
    return buf;
  },
  toUTF8: function(array) {
    if (!array) {
      return null;
    }
    let str = "";
    for (var i = 0; i < array.length; i++) {
      str += String.fromCharCode(array[i]);
    }
    return str;
  }
};
