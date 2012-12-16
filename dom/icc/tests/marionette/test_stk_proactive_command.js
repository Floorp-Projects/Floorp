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

function testLocalInfoLocation(cmd) {
  log("STK CMD " + JSON.stringify(cmd));
  is(cmd.typeOfCommand, icc.STK_CMD_PROVIDE_LOCAL_INFO);
  is(cmd.commandNumber, 0x01);
  is(cmd.commandQualifier, icc.STK_LOCAL_INFO_LOCATION_INFO);
  is(cmd.options.localInfoType, icc.STK_LOCAL_INFO_LOCATION_INFO);

  runNextTest();
}

function testLocalInfoImei(cmd) {
  log("STK CMD " + JSON.stringify(cmd));
  is(cmd.typeOfCommand, icc.STK_CMD_PROVIDE_LOCAL_INFO);
  is(cmd.commandNumber, 0x01);
  is(cmd.commandQualifier, icc.STK_LOCAL_INFO_IMEI);
  is(cmd.options.localInfoType, icc.STK_LOCAL_INFO_IMEI);

  runNextTest();
}

function testLocalInfoDate(cmd) {
  log("STK CMD " + JSON.stringify(cmd));
  is(cmd.typeOfCommand, icc.STK_CMD_PROVIDE_LOCAL_INFO);
  is(cmd.commandNumber, 0x01);
  is(cmd.commandQualifier, icc.STK_LOCAL_INFO_DATE_TIME_ZONE);
  is(cmd.options.localInfoType, icc.STK_LOCAL_INFO_DATE_TIME_ZONE);

  runNextTest();
}

function testLocalInfoLanguage(cmd) {
  log("STK CMD " + JSON.stringify(cmd));
  is(cmd.typeOfCommand, icc.STK_CMD_PROVIDE_LOCAL_INFO);
  is(cmd.commandNumber, 0x01);
  is(cmd.commandQualifier, icc.STK_LOCAL_INFO_LANGUAGE);
  is(cmd.options.localInfoType, icc.STK_LOCAL_INFO_LANGUAGE);

  runNextTest();
}

function isFirstMenuItemNull(cmd) {
  return (cmd.options.items.length == 1 && !(cmd.options.items[0]));
}

function testInitialSetupMenu(cmd) {
  log("STK CMD " + JSON.stringify(cmd));
  is(cmd.typeOfCommand, icc.STK_CMD_SET_UP_MENU);
  is(isFirstMenuItemNull(cmd), false);

  runNextTest();
}
function testRemoveSetupMenu(cmd) {
  log("STK CMD " + JSON.stringify(cmd));
  is(cmd.typeOfCommand, icc.STK_CMD_SET_UP_MENU);
  is(isFirstMenuItemNull(cmd), true);

  runNextTest();
}

function testPollingOff(cmd) {
  log("STK CMD " + JSON.stringify(cmd));
  is(cmd.typeOfCommand, icc.STK_CMD_POLL_OFF);
  is(cmd.commandNumber, 0x01);
  is(cmd.commandQualifier, 0x00);
  is(cmd.options, null);

  runNextTest();
}

function testRefresh(cmd) {
  log("STK CMD " + JSON.stringify(cmd));
  is(cmd.typeOfCommand, icc.STK_CMD_REFRESH);
  is(cmd.commandNumber, 0x01);
  is(cmd.commandQualifier, 0x01);
  is(cmd.options, null);

  runNextTest();
}

let tests = [
  {command: "d0288103012180820281020d1d00d3309bfc06c95c301aa8e80259c3ec34b9ac07c9602f58ed159bb940",
   func: testDisplayTextGsm7BitEncoding},
  {command: "d009810301260082028182",
   func: testLocalInfoLocation},
  {command: "d009810301260182028182",
   func: testLocalInfoImei},
  {command: "d009810301260382028182",
   func: testLocalInfoDate},
  {command: "d009810301260482028182",
   func: testLocalInfoLanguage},
  {command: "D00D81030125008202818285008F00",
   func: testRemoveSetupMenu},
  {command:"D03B810301250082028182850C546F6F6C6B6974204D656E758F07014974656D20318F07024974656D20328F07034974656D20338F07044974656D2034",
   func: testInitialSetupMenu},
  {command: "d009810301040082028182",
   func: testPollingOff},
  {command: "d0108103010101820281829205013f002fe2",
   func: testRefresh},
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
