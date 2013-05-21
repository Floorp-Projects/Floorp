/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;

SpecialPowers.addPermission("mobileconnection", true, document);

let icc = navigator.mozIccManager;
ok(icc instanceof MozIccManager, "icc is instanceof " + icc.constructor);

function testGetInput(command, expect) {
  log("STK CMD " + JSON.stringify(command));
  is(command.typeOfCommand, icc.STK_CMD_GET_INPUT, expect.name);
  is(command.commandQualifier, expect.commandQualifier, expect.name);
  is(command.options.text, expect.text, expect.name);
  is(command.options.minLength, expect.minLength, expect.name);
  is(command.options.maxLength, expect.maxLength, expect.name);
  if (command.options.defaultText) {
    is(command.options.defaultText, expect.defaultText, expect.name);
  }
  if (command.options.isAlphabet) {
    is(command.options.isAlphabet, expect.isAlphabet, expect.name);
  }
  if (command.options.isUCS2) {
    is(command.options.isUCS2, expect.isUCS2, expect.name);
  }
  if (command.options.isPacked) {
    is(command.options.isPacked, expect.isPacked, expect.name);
  }
  if (command.options.hideInput) {
    is(command.options.hideInput, expect.hideInput, expect.name);
  }

  runNextTest();
}

let tests = [
  {command: "d01b8103012300820281828d0c04456e74657220313233343591020505",
   func: testGetInput,
   expect: {name: "get_input_cmd_1",
            commandQualifier: 0x00,
            text: "Enter 12345",
            minLength: 5,
            maxLength: 5}},
  {command: "d01a8103012308820281828d0b004537bd2c07d96eaad10a91020505",
   func: testGetInput,
   expect: {name: "get_input_cmd_2",
            commandQualifier: 0x08,
            text: "Enter 67*#+",
            minLength: 5,
            maxLength: 5,
            isPacked: true}},
  {command: "d01b8103012301820281828d0c04456e74657220416243644591020505",
   func: testGetInput,
   expect: {name: "get_input_cmd_3",
            commandQualifier: 0x01,
            text: "Enter AbCdE",
            minLength: 5,
            maxLength: 5,
            isAlphabet: true}},
  {command: "d0278103012304820281828d180450617373776f726420313c53454e443e3233343536373891020408",
   func: testGetInput,
   expect: {name: "get_input_cmd_4",
            commandQualifier: 0x04,
            text: "Password 1<SEND>2345678",
            minLength: 4,
            maxLength: 8,
            hideInput: true}},
  {command: "d0248103012300820281828d1504456e74657220312e2e392c302e2e392c3028312991020114",
   func: testGetInput,
   expect: {name: "get_input_cmd_5",
            commandQualifier: 0x00,
            text: "Enter 1..9,0..9,0(1)",
            minLength: 1,
            maxLength: 20}},
  {command: "d01e8103012300820281828d0f043c474f2d4241434b57415244533e91020008",
   func: testGetInput,
   expect: {name: "get_input_cmd_6",
            commandQualifier: 0x00,
            text: "<GO-BACKWARDS>",
            minLength: 0,
            maxLength: 8}},
  {command: "d0178103012300820281828d08043c41424f52543e91020008",
   func: testGetInput,
   expect: {name: "get_input_cmd_7",
            commandQualifier: 0x00,
            text: "<ABORT>",
            minLength: 0,
            maxLength: 8}},
  {command: "d081b18103012300820281828d81a1042a2a2a313131313131313131312323232a2a2a323232323232323232322323232a2a2a333333333333333333332323232a2a2a343434343434343434342323232a2a2a353535353535353535352323232a2a2a363636363636363636362323232a2a2a373737373737373737372323232a2a2a383838383838383838382323232a2a2a393939393939393939392323232a2a2a303030303030303030302323239102a0a0",
   func: testGetInput,
   expect: {name: "get_input_cmd_8",
            commandQualifier: 0x00,
            text: "***1111111111###***2222222222###***3333333333###***4444444444###***5555555555###***6666666666###***7777777777###***8888888888###***9999999999###***0000000000###",
            minLength: 160,
            maxLength: 160}},
  {command: "d0168103012300820281828d07043c53454e443e91020001",
   func: testGetInput,
   expect: {name: "get_input_cmd_9",
            commandQualifier: 0x00,
            text: "<SEND>",
            minLength: 0,
            maxLength: 1}},
  {command: "d01a8103012300820281828d0b043c54494d452d4f55543e9102000a",
   func: testGetInput,
   expect: {name: "get_input_cmd_10",
            commandQualifier: 0x00,
            text: "<TIME-OUT>",
            minLength: 0,
            maxLength: 10}},
  {command: "d0288103012301820281828d190804170414042004100412042104220412042304190422041591020505",
   func: testGetInput,
   expect: {name: "get_input_cmd_11",
            commandQualifier: 0x01,
            text: "ЗДРАВСТВУЙТЕ",
            minLength: 5,
            maxLength: 5,
            isAlphabet: true}},
  {command: "d0819d8103012301820281828d818d08041704140420041004120421042204120423041904220415041704140420041004120421042204120423041904220415041704140420041004120421042204120423041904220415041704140420041004120421042204120423041904220415041704140420041004120421042204120423041904220415041704140420041004120421042204120423041991020505",
   func: testGetInput,
   expect: {name: "get_input_cmd_12",
            commandQualifier: 0x01,
            text: "ЗДРАВСТВУЙТЕЗДРАВСТВУЙТЕЗДРАВСТВУЙТЕЗДРАВСТВУЙТЕЗДРАВСТВУЙТЕЗДРАВСТВУЙ",
            minLength: 5,
            maxLength: 5,
            isAlphabet: true}},
  {command: "d01b8103012303820281828d0c04456e7465722048656c6c6f91020c0c",
   func: testGetInput,
   expect: {name: "get_input_cmd_13",
            commandQualifier: 0x03,
            text: "Enter Hello",
            minLength: 12,
            maxLength: 12,
            isAlphabet: true,
            isUCS2: true}},
  {command: "d01b8103012303820281828d0c04456e7465722048656c6c6f910205ff",
   func: testGetInput,
   expect: {name: "get_input_cmd_14",
            commandQualifier: 0x03,
            text: "Enter Hello",
            minLength: 5,
            maxLength: 0xFF,
            isAlphabet: true,
            isUCS2: true}},
  {command: "d0238103012300820281828d0c04456e746572203132333435910205051706043132333435",
   func: testGetInput,
   expect: {name: "get_input_cmd_15",
            commandQualifier: 0x00,
            text: "Enter 12345",
            minLength: 5,
            maxLength: 5,
            defaultText: "12345"}},
  {command: "d081ba8103012300820281828d0704456e7465723a9102a0a01781a1042a2a2a313131313131313131312323232a2a2a323232323232323232322323232a2a2a333333333333333333332323232a2a2a343434343434343434342323232a2a2a353535353535353535352323232a2a2a363636363636363636362323232a2a2a373737373737373737372323232a2a2a383838383838383838382323232a2a2a393939393939393939392323232a2a2a30303030303030303030232323",
   func: testGetInput,
   expect: {name: "get_input_cmd_16",
            commandQualifier: 0x00,
            text: "Enter:",
            minLength: 160,
            maxLength: 160,
            defaultText: "***1111111111###***2222222222###***3333333333###***4444444444###***5555555555###***6666666666###***7777777777###***8888888888###***9999999999###***0000000000###"}},
  {command: "d01d8103012300820281828d0a043c4e4f2d49434f4e3e9102000a1e020001",
   func: testGetInput,
   expect: {name: "get_input_cmd_17",
            commandQualifier: 0x00,
            text: "<NO-ICON>",
            minLength: 0,
            maxLength: 10}},
  {command: "d0208103012300820281828d0d043c42415349432d49434f4e3e9102000a1e020101",
   func: testGetInput,
   expect: {name: "get_input_cmd_18",
            commandQualifier: 0x00,
            text: "<BASIC-ICON>",
            minLength: 0,
            maxLength: 10}},
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
