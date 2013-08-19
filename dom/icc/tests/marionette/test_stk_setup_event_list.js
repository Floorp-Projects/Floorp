/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;

SpecialPowers.addPermission("mobileconnection", true, document);

let icc = navigator.mozIccManager;
ok(icc instanceof MozIccManager, "icc is instanceof " + icc.constructor);

function testSetupEventList(command, expect) {
  log("STK CMD " + JSON.stringify(command));
  is(command.typeOfCommand, icc.STK_CMD_SET_UP_EVENT_LIST, expect.name);
  is(command.commandQualifier, expect.commandQualifier, expect.name);
  for (let index in command.options.eventList) {
    is(command.options.eventList[index], expect.eventList[index], expect.name);
  }

  runNextTest();
}

let tests = [
  {command: "d00c810301050082028182990104",
   func: testSetupEventList,
   expect: {name: "setup_event_list_cmd_1",
            commandQualifier: 0x00,
            eventList: [4]}},
  {command: "d00d81030105008202818299020507",
   func: testSetupEventList,
   expect: {name: "setup_event_list_cmd_2",
            commandQualifier: 0x00,
            eventList: [5, 7]}},
  {command: "d00c810301050082028182990107",
   func: testSetupEventList,
   expect: {name: "setup_event_list_cmd_3",
            commandQualifier: 0x00,
            eventList: [7]}},
  {command: "d00c810301050082028182990107",
   func: testSetupEventList,
   expect: {name: "setup_event_list_cmd_4",
            commandQualifier: 0x00,
            eventList: [7]}},
  {command: "d00b8103010500820281829900",
   func: testSetupEventList,
   expect: {name: "setup_event_list_cmd_5",
            commandQualifier: 0x00,
            eventList: null}},
  {command: "d00c810301050082028182990107",
   func: testSetupEventList,
   expect: {name: "setup_event_list_cmd_6",
            commandQualifier: 0x00,
            eventList: [7]}}
];

// TODO - Bug 843455: Import scripts for marionette tests.
let pendingEmulatorCmdCount = 0;
function sendStkPduToEmulator(command, func, expect) {
  ++pendingEmulatorCmdCount;

  runEmulatorCmd(command, function (result) {
    --pendingEmulatorCmdCount;
    is(result[0], "OK");
  });

  icc.onstkcommand = function (evt) {
    func(evt.command, expect);
  }
}

function runNextTest() {
  let test = tests.pop();
  if (!test) {
    cleanUp();
    return;
  }

  let command = "stk pdu " + test.command;
  sendStkPduToEmulator(command, test.func, test.expect)
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
