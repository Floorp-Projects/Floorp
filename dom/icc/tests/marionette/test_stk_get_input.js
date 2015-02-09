/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

const TEST_DATA = [
  {command: "D01B" + // Length
            "8103012300" + // Command details
            "82028182" + // Device identities
            "8D0C04456E746572203132333435" + // Text string
            "91020505", // Response length
   expect: {commandQualifier: 0x00,
            text: "Enter 12345",
            minLength: 5,
            maxLength: 5}},
  {command: "D01A" + // Length
            "8103012308" + // Command details
            "82028182" + // Device identities
            "8D0B004537BD2C07D96EAAD10A" + // Text string
            "91020505", // Response length
   expect: {commandQualifier: 0x08,
            text: "Enter 67*#+",
            minLength: 5,
            maxLength: 5}},
  {command: "D01B" + // Length
            "8103012301" + // Command details
            "82028182" + // Device identities
            "8D0C04456E746572204162436445" + // Text string
            "91020505", // Response length
   expect: {commandQualifier: 0x01,
            text: "Enter AbCdE",
            minLength: 5,
            maxLength: 5}},
  {command: "D027" + // Length
            "8103012304" + // Command details
            "82028182" + // Device identities
            "8D180450617373776F726420313C53454E443E323334" + // Text string
            "35363738" +
            "91020408", // Response length
   expect: {commandQualifier: 0x04,
            text: "Password 1<SEND>2345678",
            minLength: 4,
            maxLength: 8}},
  {command: "D01E" + // Length
            "8103012300" + // Command details
            "82028182" + // Device identities
            "8D0F043C474F2D4241434B57415244533E" + // Text string
            "91020008", // Response length
   expect: {commandQualifier: 0x00,
            text: "<GO-BACKWARDS>",
            minLength: 0,
            maxLength: 8}},
  {command: "D081B1" + // Length
            "8103012300" + // Command details
            "82028182" + // Device identities
            "8D81A1042A2A2A313131313131313131312323232A2A2A" + // Text string
            "323232323232323232322323232A2A2A33333333333333" +
            "3333332323232A2A2A343434343434343434342323232A" +
            "2A2A353535353535353535352323232A2A2A3636363636" +
            "36363636362323232A2A2A373737373737373737372323" +
            "232A2A2A383838383838383838382323232A2A2A393939" +
            "393939393939392323232A2A2A30303030303030303030" +
            "232323" +
            "9102A0A0", // Response length
   expect: {commandQualifier: 0x00,
            text: "***1111111111###***2222222222###***3333333333###***444444" +
                  "4444###***5555555555###***6666666666###***7777777777###**" +
                  "*8888888888###***9999999999###***0000000000###",
            minLength: 160,
            maxLength: 160}},
  {command: "D0819D" + // Length
            "8103012301" + // Command details
            "82028182" + // Device identities
            "8D818D0804170414042004100412042104220412042304" + // Text string
            "1904220415041704140420041004120421042204120423" +
            "0419042204150417041404200410041204210422041204" +
            "2304190422041504170414042004100412042104220412" +
            "0423041904220415041704140420041004120421042204" +
            "1204230419042204150417041404200410041204210422" +
            "041204230419" +
            "91020505", // Response length
   expect: {commandQualifier: 0x01,
            text: "ЗДРАВСТВУЙТЕЗДРАВСТВУЙТЕЗДРАВСТВУЙТЕЗДРАВСТВУЙТЕЗДРАВСТВУ" +
                  "ЙТЕЗДРАВСТВУЙ",
            minLength: 5,
            maxLength: 5}},
  {command: "D01B" + // Length
            "8103012303" + // Command details
            "82028182" + // Device identities
            "8D0C04456E7465722048656C6C6F" + // Text string
            "910205FF", // Response length
   expect: {commandQualifier: 0x03,
            text: "Enter Hello",
            minLength: 5,
            maxLength: 0xFF}},
  {command: "D081BA" + // Length
            "8103012300" + // Command details
            "82028182" + // Device identities
            "8D0704456E7465723A" + // Text string
            "9102A0A0" + // Response length
            "1781A1042A2A2A313131313131313131312323232A2A2A" + // Default text
            "323232323232323232322323232A2A2A33333333333333" +
            "3333332323232A2A2A343434343434343434342323232A" +
            "2A2A353535353535353535352323232A2A2A3636363636" +
            "36363636362323232A2A2A373737373737373737372323" +
            "232A2A2A383838383838383838382323232A2A2A393939" +
            "393939393939392323232A2A2A30303030303030303030" +
            "232323",
   expect: {commandQualifier: 0x00,
            text: "Enter:",
            minLength: 160,
            maxLength: 160,
            defaultText: "***1111111111###***2222222222###***3333333333###**" +
                         "*4444444444###***5555555555###***6666666666###***7" +
                         "777777777###***8888888888###***9999999999###***000" +
                         "0000000###"}},
  {command: "D01D" + // Length
            "8103012300" + // Command details
            "82028182" + // Device identities
            "8D0A043C4E4F2D49434F4E3E" + // Text string
            "9102000A" + // Response length
            "1E020002", // Icon identifier
   expect: {commandQualifier: 0x00,
            // The record number 02 in EFimg is not defined, so no icon will be
            // shown, but the text string should still be displayed.
            text: "<NO-ICON>",
            minLength: 0,
            maxLength: 10}},
  {command: "D020" + // Length
            "8103012380" + // Command details
            "82028182" + // Device identities
            "8D0D043C42415349432D49434F4E3E" + // Text string
            "9102000A" + // Response length
            "1E020101", // Icon identifier
   expect: {commandQualifier: 0x80,
            text: "<BASIC-ICON>",
            minLength: 0,
            maxLength: 10,
            iconSelfExplanatory: false,
            icons: [BASIC_ICON]}},
];

function testGetInput(aCommand, aExpect) {
  is(aCommand.commandNumber, 0x01, "commandNumber");
  is(aCommand.typeOfCommand, MozIccManager.STK_CMD_GET_INPUT, "typeOfCommand");
  is(aCommand.commandQualifier, aExpect.commandQualifier, "commandQualifier");

  is(aCommand.options.isAlphabet, !!(aExpect.commandQualifier & 0x01),
     "options.isAlphabet");
  is(aCommand.options.isUCS2, !!(aExpect.commandQualifier & 0x02),
     "options.isUCS2");
  is(aCommand.options.hideInput, !!(aExpect.commandQualifier & 0x04),
     "options.hideInput");
  is(aCommand.options.isPacked, !!(aExpect.commandQualifier & 0x08),
     "options.isPacked");
  is(aCommand.options.isHelpAvailable, !!(aExpect.commandQualifier & 0x80),
     "options.isHelpAvailable");
  is(aCommand.options.text, aExpect.text, "options.text");
  is(aCommand.options.minLength, aExpect.minLength, "options.minLength");
  is(aCommand.options.maxLength, aExpect.maxLength, "options.maxLength");

  // defaultText is optional.
  if ("defaultText" in aExpect) {
    is(aCommand.options.defaultText, aExpect.defaultText, "options.defaultText");
  }

  // icons is optional.
  if ("icons" in aExpect) {
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
      // Wait icc-stkcommand system message.
      promises.push(waitForSystemMessage("icc-stkcommand")
        .then((aMessage) => {
          is(aMessage.iccId, icc.iccInfo.iccid, "iccId");
          testGetInput(aMessage.command, data.expect);
        }));
      // Send emulator command to generate stk unsolicited event.
      promises.push(sendEmulatorStkPdu(data.command));

      return Promise.all(promises);
    });
  }
  return promise;
});
