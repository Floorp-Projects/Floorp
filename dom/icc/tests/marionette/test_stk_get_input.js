/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

const TEST_DATA = [
  {command: "d01b8103012300820281828d0c04456e74657220313233343591020505",
   expect: {commandQualifier: 0x00,
            text: "Enter 12345",
            minLength: 5,
            maxLength: 5}},
  {command: "d01a8103012308820281828d0b004537bd2c07d96eaad10a91020505",
   expect: {commandQualifier: 0x08,
            text: "Enter 67*#+",
            minLength: 5,
            maxLength: 5,
            isPacked: true}},
  {command: "d01b8103012301820281828d0c04456e74657220416243644591020505",
   expect: {commandQualifier: 0x01,
            text: "Enter AbCdE",
            minLength: 5,
            maxLength: 5,
            isAlphabet: true}},
  {command: "d0278103012304820281828d180450617373776f726420313c53454e443e3233343536373891020408",
   expect: {commandQualifier: 0x04,
            text: "Password 1<SEND>2345678",
            minLength: 4,
            maxLength: 8,
            hideInput: true}},
  {command: "d01e8103012300820281828d0f043c474f2d4241434b57415244533e91020008",
   expect: {commandQualifier: 0x00,
            text: "<GO-BACKWARDS>",
            minLength: 0,
            maxLength: 8}},
  {command: "d081b18103012300820281828d81a1042a2a2a313131313131313131312323232a2a2a323232323232323232322323232a2a2a333333333333333333332323232a2a2a343434343434343434342323232a2a2a353535353535353535352323232a2a2a363636363636363636362323232a2a2a373737373737373737372323232a2a2a383838383838383838382323232a2a2a393939393939393939392323232a2a2a303030303030303030302323239102a0a0",
   expect: {commandQualifier: 0x00,
            text: "***1111111111###***2222222222###***3333333333###***4444444444###***5555555555###***6666666666###***7777777777###***8888888888###***9999999999###***0000000000###",
            minLength: 160,
            maxLength: 160}},
  {command: "d0819d8103012301820281828d818d08041704140420041004120421042204120423041904220415041704140420041004120421042204120423041904220415041704140420041004120421042204120423041904220415041704140420041004120421042204120423041904220415041704140420041004120421042204120423041904220415041704140420041004120421042204120423041991020505",
   expect: {commandQualifier: 0x01,
            text: "ЗДРАВСТВУЙТЕЗДРАВСТВУЙТЕЗДРАВСТВУЙТЕЗДРАВСТВУЙТЕЗДРАВСТВУЙТЕЗДРАВСТВУЙ",
            minLength: 5,
            maxLength: 5,
            isAlphabet: true}},
  {command: "d01b8103012303820281828d0c04456e7465722048656c6c6f910205ff",
   expect: {commandQualifier: 0x03,
            text: "Enter Hello",
            minLength: 5,
            maxLength: 0xFF,
            isAlphabet: true,
            isUCS2: true}},
  {command: "d081ba8103012300820281828d0704456e7465723a9102a0a01781a1042a2a2a313131313131313131312323232a2a2a323232323232323232322323232a2a2a333333333333333333332323232a2a2a343434343434343434342323232a2a2a353535353535353535352323232a2a2a363636363636363636362323232a2a2a373737373737373737372323232a2a2a383838383838383838382323232a2a2a393939393939393939392323232a2a2a30303030303030303030232323",
   expect: {commandQualifier: 0x00,
            text: "Enter:",
            minLength: 160,
            maxLength: 160,
            defaultText: "***1111111111###***2222222222###***3333333333###***4444444444###***5555555555###***6666666666###***7777777777###***8888888888###***9999999999###***0000000000###"}},
  {command: "d01d8103012300820281828d0a043c4e4f2d49434f4e3e9102000a1e020002",
   expect: {commandQualifier: 0x00,
            // The record number 02 in EFimg is not defined, so no icon will be
            // shown, but the text string should still be displayed.
            text: "<NO-ICON>",
            minLength: 0,
            maxLength: 10}},
  {command: "d0208103012300820281828d0d043c42415349432d49434f4e3e9102000a1e020101",
   expect: {commandQualifier: 0x00,
            text: "<BASIC-ICON>",
            minLength: 0,
            maxLength: 10,
            iconSelfExplanatory: false,
            icons: [BASIC_ICON]}},
];

function testGetInput(aCommand, aExpect) {
  is(aCommand.typeOfCommand, MozIccManager.STK_CMD_GET_INPUT, "typeOfCommand");
  is(aCommand.commandQualifier, aExpect.commandQualifier, "commandQualifier");
  is(aCommand.options.text, aExpect.text, "options.text");
  is(aCommand.options.minLength, aExpect.minLength, "options.minLength");
  is(aCommand.options.maxLength, aExpect.maxLength, "options.maxLength");

  if (aExpect.defaultText) {
    is(aCommand.options.defaultText, aExpect.defaultText, "options.defaultText");
  }

  if (aExpect.isAlphabet) {
    is(aCommand.options.isAlphabet, aExpect.isAlphabet, "options.isAlphabet");
  }

  if (aExpect.isUCS2) {
    is(aCommand.options.isUCS2, aExpect.isUCS2, "options.isUCS2");
  }

  if (aExpect.isPacked) {
    is(aCommand.options.isPacked, aExpect.isPacked, "options.isPacked");
  }

  if (aExpect.hideInput) {
    is(aCommand.options.hideInput, aExpect.hideInput, "options.hideInput");
  }

  if (aExpect.icons) {
    isIcons(aCommand.options.icons, aExpect.icons);
    is(aCommand.options.iconSelfExplanatory, aExpect.iconSelfExplanatory,
       "options.iconSelfExplanatory");
  }
}

// Start tests
startTestCommon(function() {
  let icc = getMozIcc();
  let promise = Promise.resolve();
  for (let i = 0; i < TEST_DATA.length; i++) {
    let data = TEST_DATA[i];
    promise = promise.then(() => {
      log("get_input_cmd: " + data.command);

      let promises = [];
      // Wait onstkcommand event.
      promises.push(waitForTargetEvent(icc, "stkcommand")
        .then((aEvent) => testGetInput(aEvent.command, data.expect)));
      // Send emulator command to generate stk unsolicited event.
      promises.push(sendEmulatorStkPdu(data.command));

      return Promise.all(promises);
    });
  }
  return promise;
});
