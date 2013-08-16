/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;

SpecialPowers.addPermission("mobileconnection", true, document);

let icc = navigator.mozIccManager;
ok(icc instanceof MozIccManager, "icc is instanceof " + icc.constructor);

function testSendDTMF(command, expect) {
  log("STK CMD " + JSON.stringify(command));
  is(command.typeOfCommand, icc.STK_CMD_SEND_DTMF, expect.name);
  is(command.commandQualifier, expect.commandQualifier, expect.name);
  if (command.options.text) {
    is(command.options.text, expect.text, expect.name);
  }

  runNextTest();
}

let tests = [
  {command: "d01b810301140082028183850953656e642044544d46ac052143658709",
   func: testSendDTMF,
   expect: {name: "send_dtmf_cmd_1",
            commandQualifier: 0x00,
            text: "Send DTMF"}},
  {command: "d0138103011400820281838500ac06c1cccccccc2c",
   func: testSendDTMF,
   expect: {name: "send_dtmf_cmd_2",
            commandQualifier: 0x00,
            text: ""}},
  {command: "d01d810301140082028183850a42617369632049636f6eac02c1f29e020001",
   func: testSendDTMF,
   expect: {name: "send_dtmf_cmd_3",
            commandQualifier: 0x00,
            text: "Basic Icon"}},
  {command: "d01b810301140082028183850953656e642044544d46ac052143658709",
   func: testSendDTMF,
   expect: {name: "send_dtmf_cmd_4",
            commandQualifier: 0x00,
            text: "Send DTMF"}},
  {command: "d01c810301140082028183850953656e642044544d46ac02c1f29e020101",
   func: testSendDTMF,
   expect: {name: "send_dtmf_cmd_5",
            commandQualifier: 0x00,
            text: "Send DTMF"}},
  {command: "d028810301140082028183851980041704140420041004120421042204120423041904220415ac02c1f2",
   func: testSendDTMF,
   expect: {name: "send_dtmf_cmd_6",
            commandQualifier: 0x00,
            text: "ЗДРАВСТВУЙТЕ"}},
  {command: "d023810301140082028183850b53656e642044544d462031ac052143658709d004000b00b4",
   func: testSendDTMF,
   expect: {name: "send_dtmf_cmd_7",
            commandQualifier: 0x00,
            text: "Send DTMF 1"}},
  {command: "d01d810301140082028183850b53656e642044544d462032ac052143658709",
   func: testSendDTMF,
   expect: {name: "send_dtmf_cmd_8",
            commandQualifier: 0x00,
            text: "Send DTMF 2"}},
  {command: "d023810301140082028183850b53656e642044544d462031ac052143658709d004000b01b4",
   func: testSendDTMF,
   expect: {name: "send_dtmf_cmd_9",
            commandQualifier: 0x00,
            text: "Send DTMF 1"}},
  {command: "d01d810301140082028183850b53656e642044544d462032ac052143658709",
   func: testSendDTMF,
   expect: {name: "send_dtmf_cmd_10",
            commandQualifier: 0x00,
            text: "Send DTMF 2"}},
  {command: "d023810301140082028183850b53656e642044544d462031ac052143658709d00400b002b4",
   func: testSendDTMF,
   expect: {name: "send_dtmf_cmd_11",
            commandQualifier: 0x00,
            text: "Send DTMF 1"}},
  {command: "d01d810301140082028183850b53656e642044544d462032ac052143658709",
   func: testSendDTMF,
   expect: {name: "send_dtmf_cmd_12",
            commandQualifier: 0x00,
            text: "Send DTMF 2"}},
  {command: "d023810301140082028183850b53656e642044544d462031ac052143658709d004000b04b4",
   func: testSendDTMF,
   expect: {name: "send_dtmf_cmd_13",
            commandQualifier: 0x00,
            text: "Send DTMF 1"}},
  {command: "d023810301140082028183850b53656e642044544d462032ac052143658709d004000b00b4",
   func: testSendDTMF,
   expect: {name: "send_dtmf_cmd_14",
            commandQualifier: 0x00,
            text: "Send DTMF 2"}},
  {command: "d01d810301140082028183850b53656e642044544d462033ac052143658709",
   func: testSendDTMF,
   expect: {name: "send_dtmf_cmd_15",
            commandQualifier: 0x00,
            text: "Send DTMF 3"}},
  {command: "d023810301140082028183850b53656e642044544d462031ac052143658709d004000b08b4",
   func: testSendDTMF,
   expect: {name: "send_dtmf_cmd_16",
            commandQualifier: 0x00,
            text: "Send DTMF 1"}},
  {command: "d023810301140082028183850b53656e642044544d462032ac052143658709d004000b00b4",
   func: testSendDTMF,
   expect: {name: "send_dtmf_cmd_17",
            commandQualifier: 0x00,
            text: "Send DTMF 2"}},
  {command: "d01d810301140082028183850b53656e642044544d462033ac052143658709",
   func: testSendDTMF,
   expect: {name: "send_dtmf_cmd_18",
            commandQualifier: 0x00,
            text: "Send DTMF 3"}},
  {command: "d023810301140082028183850b53656e642044544d462031ac052143658709d004000b10b4",
   func: testSendDTMF,
   expect: {name: "send_dtmf_cmd_19",
            commandQualifier: 0x00,
            text: "Send DTMF 1"}},
  {command: "d023810301140082028183850b53656e642044544d462032ac052143658709d004000b00b4",
   func: testSendDTMF,
   expect: {name: "send_dtmf_cmd_20",
            commandQualifier: 0x00,
            text: "Send DTMF 2"}},
  {command: "d01d810301140082028183850b53656e642044544d462033ac052143658709",
   func: testSendDTMF,
   expect: {name: "send_dtmf_cmd_21",
            commandQualifier: 0x00,
            text: "Send DTMF 3"}},
  {command: "d023810301140082028183850b53656e642044544d462031ac052143658709d004000b20b4",
   func: testSendDTMF,
   expect: {name: "send_dtmf_cmd_22",
            commandQualifier: 0x00,
            text: "Send DTMF 1"}},
  {command: "d023810301140082028183850b53656e642044544d462032ac052143658709d004000b00b4",
   func: testSendDTMF,
   expect: {name: "send_dtmf_cmd_23",
            commandQualifier: 0x00,
            text: "Send DTMF 2"}},
  {command: "d01d810301140082028183850b53656e642044544d462033ac052143658709",
   func: testSendDTMF,
   expect: {name: "send_dtmf_cmd_24",
            commandQualifier: 0x00,
            text: "Send DTMF 3"}},
  {command: "d023810301140082028183850b53656e642044544d462031ac052143658709d004000b40b4",
   func: testSendDTMF,
   expect: {name: "send_dtmf_cmd_25",
            commandQualifier: 0x00,
            text: "Send DTMF 1"}},
  {command: "d023810301140082028183850b53656e642044544d462032ac052143658709d004000b00b4",
   func: testSendDTMF,
   expect: {name: "send_dtmf_cmd_26",
            commandQualifier: 0x00,
            text: "Send DTMF 2"}},
  {command: "d01d810301140082028183850b53656e642044544d462033ac052143658709",
   func: testSendDTMF,
   expect: {name: "send_dtmf_cmd_27",
            commandQualifier: 0x00,
            text: "Send DTMF 3"}},
  {command: "d023810301140082028183850b53656e642044544d462031ac052143658709d004000b80b4",
   func: testSendDTMF,
   expect: {name: "send_dtmf_cmd_28",
            commandQualifier: 0x00,
            text: "Send DTMF 1"}},
  {command: "d023810301140082028183850b53656e642044544d462032ac052143658709d004000b00b4",
   func: testSendDTMF,
   expect: {name: "send_dtmf_cmd_29",
            commandQualifier: 0x00,
            text: "Send DTMF 2"}},
  {command: "d01d810301140082028183850b53656e642044544d462033ac052143658709",
   func: testSendDTMF,
   expect: {name: "send_dtmf_cmd_30",
            commandQualifier: 0x00,
            text: "Send DTMF 3"}},
  {command: "d023810301140082028183850b53656e642044544d462031ac052143658709d004000b00b4",
   func: testSendDTMF,
   expect: {name: "send_dtmf_cmd_31",
            commandQualifier: 0x00,
            text: "Send DTMF 1"}},
  {command: "d01d810301140082028183850b53656e642044544d462032ac052143658709",
   func: testSendDTMF,
   expect: {name: "send_dtmf_cmd_32",
            commandQualifier: 0x00,
            text: "Send DTMF 2"}},
  {command: "d0148103011400820281838505804f60597dac02c1f2",
   func: testSendDTMF,
   expect: {name: "send_dtmf_cmd_33",
            commandQualifier: 0x00,
            text: "你好"}},
  {command: "d01281030114008202818385038030ebac02c1f2",
   func: testSendDTMF,
   expect: {name: "send_dtmf_cmd_34",
            commandQualifier: 0x00,
            text: "ル"}}
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
