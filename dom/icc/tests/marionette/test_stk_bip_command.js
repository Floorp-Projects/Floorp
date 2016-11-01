/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

const TEST_DATA = [
  // Open channel.
  {command: "D02E" + // Length
            "8103014001" + // Command details
            "82028182" + // Device identities
            "05074F70656E204944" + // Alpha identifier
            "9E020007" + // Icon identifier
            "86099111223344556677F8" + // Address
            "350702030403041F02" + // Bear description
            "39020578", // Buffer size
   expect: {typeOfCommand: MozIccManager.STK_CMD_OPEN_CHANNEL,
            commandQualifier: 0x01,
            text: "Open ID",
            iconSelfExplanatory: true,
            icons: [COLOR_ICON, COLOR_TRANSPARENCY_ICON]}},
  {command: "D023" + // Length
            "8103014001" + // Command details
            "82028182" + // Device identities
            "0500" + // Alpha identifier
            "86099111223344556677F8" + // Address
            "350702030403041F02" + // Bear description
            "39020578", // Buffer size
   expect: {typeOfCommand: MozIccManager.STK_CMD_OPEN_CHANNEL,
            commandQualifier: 0x01,
            text: ""}},
  {command: "D02C" + // Length
            "8103014001" + // Command details
            "82028182" + // Device identities
            "05094F70656E2049442031" + // Alpha identifier
            "86099111223344556677F8" + // Address
            "350702030403041F02" + // Bear description
            "39020578", // Buffer size
   expect: {typeOfCommand: MozIccManager.STK_CMD_OPEN_CHANNEL,
            commandQualifier: 0x01,
            text: "Open ID 1"}},
  // Close channel.
  {command: "D00D" + // Length
            "8103014100" + // Command details
            "82028182" + // Device identities
            "9E020007", // Icon identifier
   expect: {typeOfCommand: MozIccManager.STK_CMD_CLOSE_CHANNEL,
            commandQualifier: 0x00,
            iconSelfExplanatory: true,
            icons: [COLOR_ICON, COLOR_TRANSPARENCY_ICON]}},
  {command: "D015" + // Length
            "8103014100" + // Command details
            "82028121" + // Device identities
            "850A436C6F73652049442031", // Alpha identifier
   expect: {typeOfCommand: MozIccManager.STK_CMD_CLOSE_CHANNEL,
            commandQualifier: 0x00,
            text: "Close ID 1"}},
  // Receive data.
  {command: "D00C" + // Length
            "8103014200" + // Command details
            "82028121" + // Device identities
            "B701C8", // Channel data length
   expect: {typeOfCommand: MozIccManager.STK_CMD_RECEIVE_DATA,
            commandQualifier: 0x00}},
  {command: "D01C" + // Length
            "8103014200" + // Command details
            "82028121" + // Device identities
            "850E5265636569766520446174612031" + // Alpha identifier
            "B701C8", // Channel data length
   expect: {typeOfCommand: MozIccManager.STK_CMD_RECEIVE_DATA,
            commandQualifier: 0x00,
            text: "Receive Data 1"}},
  // Send data.
  {command: "D017" + // Length
            "8103014301" + // Command details
            "82028121" + // Device identities
            "9E020007" + // Icon identifier
            "B6080001020304050607", // Channel data
   expect: {typeOfCommand: MozIccManager.STK_CMD_SEND_DATA,
            commandQualifier: 0x01,
            iconSelfExplanatory: true,
            icons: [COLOR_ICON, COLOR_TRANSPARENCY_ICON]}},
  {command: "D020" + // Length
            "8103014301" + // Command details
            "82028121" + // Device identities
            "850B53656E6420446174612031" + // Alpha identifier
            "B6080001020304050607", // Channel data
   expect: {typeOfCommand: MozIccManager.STK_CMD_SEND_DATA,
            commandQualifier: 0x01,
            text: "Send Data 1"}},
];

function testBipCommand(aCommand, aExpect) {
  is(aCommand.commandNumber, 0x01, "commandNumber");
  is(aCommand.typeOfCommand, aExpect.typeOfCommand, "typeOfCommand");
  is(aCommand.commandQualifier, aExpect.commandQualifier, "commandQualifier");

  // text is optional.
  if ("text" in aExpect) {
    is(aCommand.options.text, aExpect.text, "options.text");
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
      log("bip_cmd: " + data.command);

      let promises = [];
      // Wait onstkcommand event.
      promises.push(waitForTargetEvent(icc, "stkcommand")
        .then((aEvent) => testBipCommand(aEvent.command, data.expect)));
      // Wait icc-stkcommand system message.
      promises.push(waitForSystemMessage("icc-stkcommand")
        .then((aMessage) => {
          is(aMessage.iccId, icc.iccInfo.iccid, "iccId");
          testBipCommand(aMessage.command, data.expect);
        }));
      // Send emulator command to generate stk unsolicited event.
      promises.push(sendEmulatorStkPdu(data.command));

      return Promise.all(promises);
    });
  }
  return promise;
});
