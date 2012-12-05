/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;

SpecialPowers.addPermission("mobileconnection", true, document);

let icc = navigator.mozMobileConnection.icc;
ok(icc instanceof MozIccManager, "icc is instanceof " + icc.constructor);

function testDisplayTextGsm7BitEncoding(cmd) {
  log("STK CMD " + JSON.stringify(cmd));
  is(cmd.typeOfCommand, icc.STK_CMD_DISPLAY_TEXT);
  is(cmd.options.userClear, true);
  is(cmd.options.text, "Saldo 2.04 E. Validez 20/05/13. ");

  runNextTest();
}

let tests = [
  {command: "d0288103012180820281020d1d00d3309bfc06c95c301aa8e80259c3ec34b9ac07c9602f58ed159bb940",
   func: testDisplayTextGsm7BitEncoding},
];

let pendingEmulatorCmdCount = 0;
function sendStkPduToEmulator(cmd, func) {
  ++pendingEmulatorCmdCount;

  runEmulatorCmd(cmd, function (result) {
    --pendingEmulatorCmdCount;
    is(result[0], "OK");
  });

  icc.onstkcommand = function (evt) {
    func(evt.command);
  }
}

function runNextTest() {
  let test = tests.pop();
  if (!test) {
    cleanUp();
    return;
  }

  let cmd = "stk pdu " + test.command;
  sendStkPduToEmulator(cmd, test.func)
}

function cleanUp() {
  if (pendingEmulatorCmdCount) {
    window.setTimeout(cleanUp, 100);
    return;
  }

  SpecialPowers.removePermission("mobileconnection", document);
  finish();
}

runNextTest();
