/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_HEAD_JS = "stk_helper.js";

function testGetInKey(command, expect) {
  log("STK CMD " + JSON.stringify(command));
  is(command.typeOfCommand, iccManager.STK_CMD_GET_INKEY, expect.name);
  is(command.commandQualifier, expect.commandQualifier, expect.name);
  is(command.options.text, expect.text, expect.name);
  is(command.options.isAlphabet, expect.isAlphabet, expect.name);
  is(command.options.isUCS2, expect.isUCS2, expect.name);
  is(command.options.isYesNoRequested, expect.isYesNoRequested, expect.name);

  let duration = command.options.duration;
  if (duration) {
    is(duration.timeUnit, expect.duration.timeUnit, expect.name);
    is(duration.timeInterval, expect.duration.timeInterval, expect.name);
  }

  let icons = command.options.icons;
  if (icons) {
    isIcons(icons, expect.icons, expect.name);

    let iconSelfExplanatory = command.options.iconSelfExplanatory;
    is(iconSelfExplanatory, expect.iconSelfExplanatory, expect.name);
  }

  runNextTest();
}

let tests = [
  {command: "d0158103012200820281828d0a04456e74657220222b22",
   func: testGetInKey,
   expect: {name: "get_inkey_cmd_1",
            commandQualifier: 0x00,
            text: "Enter \"+\""}},
  {command: "d0148103012200820281828d09004537bd2c07896022",
   func: testGetInKey,
   expect: {name: "get_inkey_cmd_2",
            commandQualifier: 0x00,
            text: "Enter \"0\""}},
  {command: "d01a8103012200820281828d0f043c474f2d4241434b57415244533e",
   func: testGetInKey,
   expect: {name: "get_inkey_cmd_3",
            commandQualifier: 0x00,
            text: "<GO-BACKWARDS>"}},
  {command: "d0138103012200820281828d08043c41424f52543e",
   func: testGetInKey,
   expect: {name: "get_inkey_cmd_4",
            commandQualifier: 0x00,
            text: "<ABORT>"}},
  {command: "d0158103012201820281828d0a04456e74657220227122",
   func: testGetInKey,
   expect: {name: "get_inkey_cmd_5",
            commandQualifier: 0x01,
            text: "Enter \"q\"",
            isAlphabet: true}},
  {command: "d081ad8103012201820281828d81a104456e746572202278222e205468697320636f6d6d616e6420696e7374727563747320746865204d4520746f20646973706c617920746578742c20616e6420746f2065787065637420746865207573657220746f20656e74657220612073696e676c65206368617261637465722e20416e7920726573706f6e736520656e7465726564206279207468652075736572207368616c6c206265207061737365642074",
   func: testGetInKey,
   expect: {name: "get_inkey_cmd_6",
            commandQualifier: 0x01,
            text: "Enter \"x\". This command instructs the ME to display text, and to expect the user to enter a single character. Any response entered by the user shall be passed t",
            isAlphabet: true}},
  {command: "d0168103012200820281828d0b043c54494d452d4f55543e",
   func: testGetInKey,
   expect: {name: "get_inkey_cmd_7",
            commandQualifier: 0x00,
            text: "<TIME-OUT>"}},
  {command: "d0248103012200820281828d1908041704140420041004120421042204120423041904220415",
   func: testGetInKey,
   expect: {name: "get_inkey_cmd_8",
            commandQualifier: 0x00,
            text: "ЗДРАВСТВУЙТЕ"}},
  {command: "d081998103012200820281828d818d080417041404200410041204210422041204230419042204150417041404200410041204210422041204230419042204150417041404200410041204210422041204230419042204150417041404200410041204210422041204230419042204150417041404200410041204210422041204230419042204150417041404200410041204210422041204230419",
   func: testGetInKey,
   expect: {name: "get_inkey_cmd_9",
            commandQualifier: 0x00,
            text: "ЗДРАВСТВУЙТЕЗДРАВСТВУЙТЕЗДРАВСТВУЙТЕЗДРАВСТВУЙТЕЗДРАВСТВУЙТЕЗДРАВСТВУЙ"}},
  {command: "d0118103012203820281828d0604456e746572",
   func: testGetInKey,
   expect: {name: "get_inkey_cmd_10",
            commandQualifier: 0x03,
            text: "Enter",
            isAlphabet: true,
            isUCS2: true}},
  {command: "d0158103012204820281828d0a04456e74657220594553",
   func: testGetInKey,
   expect: {name: "get_inkey_cmd_11",
            commandQualifier: 0x04,
            text: "Enter YES",
            isYesNoRequested: true}},
  {command: "d0148103012204820281828d0904456e746572204e4f",
   func: testGetInKey,
   expect: {name: "get_inkey_cmd_12",
            commandQualifier: 0x04,
            text: "Enter NO",
            isYesNoRequested: true}},
  {command: "d0198103012200820281828d0a043c4e4f2d49434f4e3e1e020002",
   func: testGetInKey,
   expect: {name: "get_inkey_cmd_13",
            commandQualifier: 0x00,
            // The record number 02 in EFimg is not defined, so no icon will be
            // shown, but the text string should still be displayed.
            text: "<NO-ICON>"}},
  {command: "D0198103012200820281828D0A04456E74657220222B228402010A",
   func: testGetInKey,
   expect: {name: "get_inkey_cmd_14",
            commandQualifier: 0x00,
            text: "Enter \"+\"",
            duration: {timeUnit: iccManager.STK_TIME_UNIT_SECOND,
                       timeInterval: 0x0A}}},
];

runNextTest();
