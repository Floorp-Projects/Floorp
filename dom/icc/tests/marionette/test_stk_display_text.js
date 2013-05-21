/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;

SpecialPowers.addPermission("mobileconnection", true, document);

let icc = navigator.mozIccManager;
ok(icc instanceof MozIccManager, "icc is instanceof " + icc.constructor);

function testDisplayText(command, expect) {
  log("STK CMD " + JSON.stringify(command));
  is(command.typeOfCommand, icc.STK_CMD_DISPLAY_TEXT, expect.name);
  is(command.options.text, expect.text, expect.name);
  is(command.commandQualifier, expect.commandQualifier, expect.name);
  if (expect.userClear) {
    is(command.options.userClear, expect.userClear, expect.name);
  }
  if (expect.isHighPriority) {
    is(command.options.isHighPriority, expect.isHighPriority, expect.name);
  }

  runNextTest();
}

let tests = [
  {command: "d01a8103012180820281028d0f04546f6f6c6b697420546573742031",
   func: testDisplayText,
   expect: {name: "display_text_cmd_1",
            commandQualifier: 0x80,
            text: "Toolkit Test 1",
            userClear: true}},
  {command: "d01a8103012181820281028d0f04546f6f6c6b697420546573742032",
   func: testDisplayText,
   expect: {name: "display_text_cmd_2",
            commandQualifier: 0x81,
            text: "Toolkit Test 2",
            isHighPriority: true,
            userClear: true}},
  {command: "d0198103012180820281028d0e00d4f79bbd4ed341d4f29c0e9a01",
   func: testDisplayText,
   expect: {name: "display_text_cmd_3",
            commandQualifier: 0x80,
            text: "Toolkit Test 3",
            userClear: true}},
  {command: "d01a8103012100820281028d0f04546f6f6c6b697420546573742034",
   func: testDisplayText,
   expect: {name: "display_text_cmd_4",
            commandQualifier: 0x00,
            text: "Toolkit Test 4"}},
  {command: "d081ad8103012180820281028d81a1045468697320636f6d6d616e6420696e7374727563747320746865204d4520746f20646973706c617920612074657874206d6573736167652e20497420616c6c6f7773207468652053494d20746f20646566696e6520746865207072696f72697479206f662074686174206d6573736167652c20616e6420746865207465787420737472696e6720666f726d61742e2054776f207479706573206f66207072696f",
   func: testDisplayText,
   expect: {name: "display_text_cmd_5",
            commandQualifier: 0x80,
            text: "This command instructs the ME to display a text message. It allows the SIM to define the priority of that message, and the text string format. Two types of prio",
            userClear: true}},
  {command: "d01a8103012180820281028d0f043c474f2d4241434b57415244533e",
   func: testDisplayText,
   expect: {name: "display_text_cmd_6",
            commandQualifier: 0x80,
            text: "<GO-BACKWARDS>",
            userClear: true}},
   {command: "d0248103012180820281028d1908041704140420041004120421042204120423041904220415",
   func: testDisplayText,
   expect: {name: "display_text_cmd_7",
            commandQualifier: 0x80,
            text: "ЗДРАВСТВУЙТЕ",
            userClear: true}},
   {command: "d0108103012180820281028d05084f60597d",
   func: testDisplayText,
   expect: {name: "display_text_cmd_8",
            commandQualifier: 0x80,
            text: "你好",
            userClear: true}},
   {command: "d0128103012180820281028d07080038003030eb",
   func: testDisplayText,
   expect: {name: "display_text_cmd_9",
            commandQualifier: 0x80,
            text: "80ル",
            userClear: true}},
];

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
