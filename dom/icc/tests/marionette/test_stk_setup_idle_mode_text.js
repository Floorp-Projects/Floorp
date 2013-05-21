/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;

SpecialPowers.addPermission("mobileconnection", true, document);

let icc = navigator.mozIccManager;
ok(icc instanceof MozIccManager, "icc is instanceof " + icc.constructor);

function testSetupIdleModeText(command, expect) {
  log("STK CMD " + JSON.stringify(command));
  is(command.typeOfCommand, icc.STK_CMD_SET_UP_IDLE_MODE_TEXT, expect.name);
  is(command.commandQualifier, expect.commandQualifier, expect.name);
  is(command.options.text, expect.text, expect.name);

  runNextTest();
}

let tests = [
  {command: "d01a8103012800820281828d0f0449646c65204d6f64652054657874",
   func: testSetupIdleModeText,
   expect: {name: "setup_idle_mode_text_cmd_1",
            commandQualifier: 0x00,
            text: "Idle Mode Text"}},
  {command: "d0188103012800820281828d0d04546f6f6c6b69742054657374",
   func: testSetupIdleModeText,
   expect: {name: "setup_idle_mode_text_cmd_2",
            commandQualifier: 0x00,
            text: "Toolkit Test"}},
  {command: "d081fd8103012800820281828d81f100547419344d3641737498cd06cdeb70383b0f0a83e8653c1d34a7cbd3ee330b7447a7c768d01c1d66b341e232889c9ec3d9e17c990c12e741747419d42c82c27350d80d4a93d96550fb4d2e83e8653c1d943683e8e832a85904a5e7a0b0985d06d1df20f21b94a6bba8e832082e2fcfcb6e7a989e7ebb41737a9e5d06a5e72076d94c0785e7a0b01b946ec3d9e576d94d0fd3d36f37885c1ea7e7e9b71b447f83e8e832a85904b5c3eeba393ca6d7e565b90b444597416932bb0c6abfc96510bd8ca783e6e8309b0d129741e4f41cce0ee7cb6450da0d0a83da61b7bb2c07d1d1613aa8ec9ed7e5e539888e0ed341ee32",
   func: testSetupIdleModeText,
   expect: {name: "setup_idle_mode_text_cmd_3",
            commandQualifier: 0x00,
            text: "The SIM shall supply a text string, which shall be displayed by the ME as an idle mode text if the ME is able to do it.The presentation style is left as an implementation decision to the ME manufacturer. The idle mode text shall be displayed in a manner that ensures that ne"}},
  {command: "d0198103012800820281828d0a0449646c6520746578749e020001",
   func: testSetupIdleModeText,
   expect: {name: "setup_idle_mode_text_cmd_4",
            commandQualifier: 0x00,
            text: "Idle text"}},
  {command: "d0198103012800820281828d0a0449646c6520746578749e020101",
   func: testSetupIdleModeText,
   expect: {name: "setup_idle_mode_text_cmd_5",
            commandQualifier: 0x00,
            text: "Idle text"}},
  {command: "d0198103012800820281828d0a0449646c6520746578749e020002",
   func: testSetupIdleModeText,
   expect: {name: "setup_idle_mode_text_cmd_6",
            commandQualifier: 0x00,
            text: "Idle text"}},
  {command: "d0248103012800820281828d1908041704140420041004120421042204120423041904220415",
   func: testSetupIdleModeText,
   expect: {name: "setup_idle_mode_text_cmd_7",
            commandQualifier: 0x00,
            text: "ЗДРАВСТВУЙТЕ"}},
  {command: "d0228103012800820281828d110449646c65204d6f646520546578742031d004001000b4",
   func: testSetupIdleModeText,
   expect: {name: "setup_idle_mode_text_cmd_8",
            commandQualifier: 0x00,
            text: "Idle Mode Text 1"}},
  {command: "d01c8103012800820281828d110449646c65204d6f646520546578742032",
   func: testSetupIdleModeText,
   expect: {name: "setup_idle_mode_text_cmd_9",
            commandQualifier: 0x00,
            text: "Idle Mode Text 2"}},
  {command: "d0228103012800820281828d110449646c65204d6f646520546578742031d004001001b4",
   func: testSetupIdleModeText,
   expect: {name: "setup_idle_mode_text_cmd_10",
            commandQualifier: 0x00,
            text: "Idle Mode Text 1"}},
  {command: "d01c8103012800820281828d110449646c65204d6f646520546578742032",
   func: testSetupIdleModeText,
   expect: {name: "setup_idle_mode_text_cmd_11",
            commandQualifier: 0x00,
            text: "Idle Mode Text 2"}},
  {command: "d0228103012800820281828d110449646c65204d6f646520546578742031d004001002b4",
   func: testSetupIdleModeText,
   expect: {name: "setup_idle_mode_text_cmd_12",
            commandQualifier: 0x00,
            text: "Idle Mode Text 1"}},
  {command: "d01c8103012800820281828d110449646c65204d6f646520546578742032",
   func: testSetupIdleModeText,
   expect: {name: "setup_idle_mode_text_cmd_13",
            commandQualifier: 0x00,
            text: "Idle Mode Text 2"}},
  {command: "d0228103012800820281828d110449646c65204d6f646520546578742031d004001004b4",
   func: testSetupIdleModeText,
   expect: {name: "setup_idle_mode_text_cmd_14",
            commandQualifier: 0x00,
            text: "Idle Mode Text 1"}},
  {command: "d0228103012800820281828d110449646c65204d6f646520546578742032d004001000b4",
   func: testSetupIdleModeText,
   expect: {name: "setup_idle_mode_text_cmd_15",
            commandQualifier: 0x00,
            text: "Idle Mode Text 2"}},
  {command: "d01c8103012800820281828d110449646c65204d6f646520546578742033",
   func: testSetupIdleModeText,
   expect: {name: "setup_idle_mode_text_cmd_16",
            commandQualifier: 0x00,
            text: "Idle Mode Text 3"}},
  {command: "d0228103012800820281828d110449646c65204d6f646520546578742031d004001008b4",
   func: testSetupIdleModeText,
   expect: {name: "setup_idle_mode_text_cmd_17",
            commandQualifier: 0x00,
            text: "Idle Mode Text 1"}},
  {command: "d0228103012800820281828d110449646c65204d6f646520546578742032d004001000b4",
   func: testSetupIdleModeText,
   expect: {name: "setup_idle_mode_text_cmd_18",
            commandQualifier: 0x00,
            text: "Idle Mode Text 2"}},
  {command: "d01c8103012800820281828d110449646c65204d6f646520546578742033",
   func: testSetupIdleModeText,
   expect: {name: "setup_idle_mode_text_cmd_19",
            commandQualifier: 0x00,
            text: "Idle Mode Text 3"}},
  {command: "d0228103012800820281828d110449646c65204d6f646520546578742031d004001010b4",
   func: testSetupIdleModeText,
   expect: {name: "setup_idle_mode_text_cmd_20",
            commandQualifier: 0x00,
            text: "Idle Mode Text 1"}},
  {command: "d0228103012800820281828d110449646c65204d6f646520546578742032d004001000b4",
   func: testSetupIdleModeText,
   expect: {name: "setup_idle_mode_text_cmd_21",
            commandQualifier: 0x00,
            text: "Idle Mode Text 2"}},
  {command: "d01c8103012800820281828d110449646c65204d6f646520546578742033",
   func: testSetupIdleModeText,
   expect: {name: "setup_idle_mode_text_cmd_22",
            commandQualifier: 0x00,
            text: "Idle Mode Text 3"}},
  {command: "d0228103012800820281828d110449646c65204d6f646520546578742031d004001020b4",
   func: testSetupIdleModeText,
   expect: {name: "setup_idle_mode_text_cmd_23",
            commandQualifier: 0x00,
            text: "Idle Mode Text 1"}},
  {command: "d0228103012800820281828d110449646c65204d6f646520546578742032d004001000b4",
   func: testSetupIdleModeText,
   expect: {name: "setup_idle_mode_text_cmd_24",
            commandQualifier: 0x00,
            text: "Idle Mode Text 2"}},
  {command: "d01c8103012800820281828d110449646c65204d6f646520546578742033",
   func: testSetupIdleModeText,
   expect: {name: "setup_idle_mode_text_cmd_25",
            commandQualifier: 0x00,
            text: "Idle Mode Text 3"}},
  {command: "d0228103012800820281828d110449646c65204d6f646520546578742031d004001040b4",
   func: testSetupIdleModeText,
   expect: {name: "setup_idle_mode_text_cmd_26",
            commandQualifier: 0x00,
            text: "Idle Mode Text 1"}},
  {command: "d0228103012800820281828d110449646c65204d6f646520546578742032d004001000b4",
   func: testSetupIdleModeText,
   expect: {name: "setup_idle_mode_text_cmd_27",
            commandQualifier: 0x00,
            text: "Idle Mode Text 2"}},
  {command: "d01c8103012800820281828d110449646c65204d6f646520546578742033",
   func: testSetupIdleModeText,
   expect: {name: "setup_idle_mode_text_cmd_28",
            commandQualifier: 0x00,
            text: "Idle Mode Text 3"}},
  {command: "d0228103012800820281828d110449646c65204d6f646520546578742031d004001080b4",
   func: testSetupIdleModeText,
   expect: {name: "setup_idle_mode_text_cmd_29",
            commandQualifier: 0x00,
            text: "Idle Mode Text 1"}},
  {command: "d0228103012800820281828d110449646c65204d6f646520546578742032d004001000b4",
   func: testSetupIdleModeText,
   expect: {name: "setup_idle_mode_text_cmd_30",
            commandQualifier: 0x00,
            text: "Idle Mode Text 2"}},
  {command: "d01c8103012800820281828d110449646c65204d6f646520546578742033",
   func: testSetupIdleModeText,
   expect: {name: "setup_idle_mode_text_cmd_31",
            commandQualifier: 0x00,
            text: "Idle Mode Text 3"}},
  {command: "d0228103012800820281828d110449646c65204d6f646520546578742031d004001000b4",
   func: testSetupIdleModeText,
   expect: {name: "setup_idle_mode_text_cmd_32",
            commandQualifier: 0x00,
            text: "Idle Mode Text 1"}},
  {command: "d01c8103012800820281828d110449646c65204d6f646520546578742032",
   func: testSetupIdleModeText,
   expect: {name: "setup_idle_mode_text_cmd_33",
            commandQualifier: 0x00,
            text: "Idle Mode Text 2"}},
  {command: "d0108103012800820281828d05084f60597d",
   func: testSetupIdleModeText,
   expect: {name: "setup_idle_mode_text_cmd_34",
            commandQualifier: 0x00,
            text: "你好"}},
  {command: "d0148103012800820281828d09080038003030eb0030",
   func: testSetupIdleModeText,
   expect: {name: "setup_idle_mode_text_cmd_35",
            commandQualifier: 0x00,
            text: "80ル0"}},
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
