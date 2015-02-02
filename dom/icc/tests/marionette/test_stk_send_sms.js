/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

const TEST_DATA = [
  {command: "d02e81030113008202818386099111223344556677f88b180100099110325476f840f40c54657374204d657373616765",
   expect: {commandQualifier: 0x00}},
  {command: "d03d810301130082028183850d53686f7274204d65737361676586099111223344556677f88b180100099110325476f840f00d53f45b4e0735cbf379f85c06",
   expect: {commandQualifier: 0x00,
            text: "Short Message"}},
  {command: "d081fd810301130182028183853854686520616464726573732064617461206f626a65637420686f6c6473207468652052501144657374696e6174696f6e114164647265737386099111223344556677f88b81ac0100099110325476f840f4a054776f2074797065732061726520646566696e65643a202d20412073686f7274206d65737361676520746f2062652073656e7420746f20746865206e6574776f726b20696e20616e20534d532d5355424d4954206d6573736167652c206f7220616e20534d532d434f4d4d414e44206d6573736167652c20776865726520746865207573657220646174612063616e20626520706173736564207472616e7370",
   expect: {commandQualifier: 0x01,
            text: "The address data object holds the RP_Destination_Address"}},
  {command: "d081c381030113018202818386099111223344556677f88b81ac0100099110325476f840f4a054776f2074797065732061726520646566696e65643a202d20412073686f7274206d65737361676520746f2062652073656e7420746f20746865206e6574776f726b20696e20616e20534d532d5355424d4954206d6573736167652c206f7220616e20534d532d434f4d4d414e44206d6573736167652c20776865726520746865207573657220646174612063616e20626520706173736564207472616e7370",
   expect: {commandQualifier: 0x01}},
  {command: "d081fd8103011300820281838581e654776f2074797065732061726520646566696e65643a202d20412073686f7274206d65737361676520746f2062652073656e7420746f20746865206e6574776f726b20696e20616e20534d532d5355424d4954206d6573736167652c206f7220616e20534d532d434f4d4d414e44206d6573736167652c20776865726520746865207573657220646174612063616e20626520706173736564207472616e73706172656e746c793b202d20412073686f7274206d65737361676520746f2062652073656e7420746f20746865206e6574776f726b20696e20616e20534d532d5355424d4954208b09010002911040f00120",
   expect: {commandQualifier: 0x00,
            text: "Two types are defined: - A short message to be sent to the network in an SMS-SUBMIT message, or an SMS-COMMAND message, where the user data can be passed transparently; - A short message to be sent to the network in an SMS-SUBMIT "}},
  {command: "d030810301130082028183850086099111223344556677f88b180100099110325476f840f40c54657374204d657373616765",
   func: testSendSMS,
   expect: {commandQualifier: 0x00,
            text: ""}},
  {command: "d05581030113008202818385198004170414042004100412042104220412042304190422041586099111223344556677f88b240100099110325476f8400818041704140420041004120421042204120423041904220415",
   expect: {commandQualifier: 0x00,
            text: "ЗДРАВСТВУЙТЕ"}},
  {command: "d04b810301130082028183850f810c089794a09092a1a292a399a29586099111223344556677f88b240100099110325476f8400818041704140420041004120421042204120423041904220415",
   expect: {commandQualifier: 0x00,
            text: "ЗДРАВСТВУЙТЕ"}},
  {command: "d03b81030113008202818385074e4f2049434f4e86099111223344556677f88b180100099110325476f840f40c54657374204d6573736167659e020002",
   expect: {commandQualifier: 0x00,
            // The record number 02 in EFimg is not defined, so no icon will be
            // shown, but the text string should still be displayed.
            text: "NO ICON"}},
  {command: "d03281030113008202818386099111223344556677f88b180100099110325476f840f40c54657374204d6573736167659e020001",
   expect: {commandQualifier: 0x00,
            iconSelfExplanatory: true,
            icons: [BASIC_ICON]}},
  {command: "d03b810301130082028183850753656e6420534d86099111223344556677f88b180100099110325476f840f40c54657374204d6573736167651e020101",
   expect: {commandQualifier: 0x00,
            text: "Send SM",
            iconSelfExplanatory: false,
            icons: [BASIC_ICON]}},
  {command: "d02c8103011300820281838510546578742041747472696275746520318b09010002911040f00120d004001000b4",
   expect: {commandQualifier: 0x00,
            text: "Text Attribute 1"}},
  {command: "d0358103011300820281838509800038003030eb003086099111223344556677f88b140100099110325476f84008080038003030eb0031",
   expect: {commandQualifier: 0x00,
            text: "80ル0"}},
  {command: "d03381030113008202818385078104613831eb3186099111223344556677f88b140100099110325476f84008080038003030eb0032",
   expect: {commandQualifier: 0x00,
            text: "81ル1"}},
  {command: "d0348103011300820281838508820430a03832cb3286099111223344556677f88b140100099110325476f84008080038003030eb0033",
   expect: {commandQualifier: 0x00,
            text: "82ル2"}},
];

function testSendSMS(aCommand, aExpect) {
  is(aCommand.typeOfCommand, MozIccManager.STK_CMD_SEND_SMS, "typeOfCommand");
  is(aCommand.commandQualifier, aExpect.commandQualifier, "commandQualifier");

  if (aExpect.text) {
    is(aCommand.options.text, aExpect.text, "options.text");
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
      log("send_sms_cmd: " + data.command);

      let promises = [];
      // Wait onstkcommand event.
      promises.push(waitForTargetEvent(icc, "stkcommand")
        .then((aEvent) => testSendSMS(aEvent.command, data.expect)));
      // Send emulator command to generate stk unsolicited event.
      promises.push(sendEmulatorStkPdu(data.command));

      return Promise.all(promises);
    });
  }
  return promise;
});
