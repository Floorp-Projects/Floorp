/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;

SpecialPowers.addPermission("mobileconnection", true, document);

let iccManager = navigator.mozIccManager;
ok(iccManager instanceof MozIccManager,
   "iccManager is instanceof " + iccManager.constructor);

// TODO: Bug 932650 - B2G RIL: WebIccManager API - add marionette tests for
//                    multi-sim
// In single sim scenario, there is only one sim card, we can use below way to
// check iccId and get icc object. But in multi-sim, the index of iccIds may
// not map to sim slot directly, we should have a better way to handle this.
let iccIds = iccManager.iccIds;
ok(Array.isArray(iccIds), "iccIds is an array");
is(iccIds.length, 1, "iccIds.length is " + iccIds.length);

let iccId = iccIds[0];
is(iccId, "89014103211118510720", "iccId is " + iccId);

let icc = iccManager.getIccById(iccId);
ok(icc instanceof MozIcc, "icc is instanceof " + icc.constructor);

let pendingEmulatorCmdCount = 0;
function sendStkPduToEmulator(command, func, expect) {
  ++pendingEmulatorCmdCount;

  runEmulatorCmd(command, function(result) {
    --pendingEmulatorCmdCount;
    is(result[0], "OK");
  });

  icc.onstkcommand = function(evt) {
    if (expect) {
      func(evt.command, expect);
    } else {
      func(evt.command);
    }
  }
}

function runNextTest() {
  let test = tests.pop();
  if (!test) {
    cleanUp();
    return;
  }

  let command = "stk pdu " + test.command;
  sendStkPduToEmulator(command, test.func, test.expect);
}

function cleanUp() {
  if (pendingEmulatorCmdCount) {
    window.setTimeout(cleanUp, 100);
    return;
  }

  SpecialPowers.removePermission("mobileconnection", document);
  finish();
}
