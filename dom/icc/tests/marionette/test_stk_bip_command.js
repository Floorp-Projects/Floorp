/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

const TEST_DATA = [
  // Open channel.
  {command: "d04f81030140018202818205074f70656e204944350702030403041f0239020578470a065465737447700272730d08f4557365724c6f670d08f4557365725077643c0301ad9c3e0521010101019e020007",
   expect: {typeOfCommand: MozIccManager.STK_CMD_OPEN_CHANNEL,
            text: "Open ID",
            iconSelfExplanatory: true,
            icons: [COLOR_ICON, COLOR_TRANSPARENCY_ICON]}},
  {command: "d0448103014001820281820500350702030403041f0239020578470a065465737447700272730d08f4557365724c6f670d08f4557365725077643c0301ad9c3e052101010101",
   expect: {typeOfCommand: MozIccManager.STK_CMD_OPEN_CHANNEL,
            text: ""}},
  {command: "d05381030140018202818205094f70656e2049442031350702030403041f0239020578470a065465737447700272730d08f4557365724c6f670d08f4557365725077643c0301ad9c3e052101010101d004000900b4",
   expect: {typeOfCommand: MozIccManager.STK_CMD_OPEN_CHANNEL,
            text: "Open ID 1"}},
  // Close channel.
  {command: "d01b810301410082028121850a436c6f73652049442031d004000a00b4",
   expect: {typeOfCommand: MozIccManager.STK_CMD_CLOSE_CHANNEL,
            text: "Close ID 1"}},
  // Recive data.
  {command: "d022810301420082028121850e5265636569766520446174612031b701c8d004000e00b4",
   expect: {typeOfCommand: MozIccManager.STK_CMD_RECEIVE_DATA,
            text: "Receive Data 1"}},
  // Send data.
  {command: "d026810301430182028121850b53656e6420446174612031b6080001020304050607d004000b00b4",
   expect: {typeOfCommand: MozIccManager.STK_CMD_SEND_DATA,
            text: "Send Data 1"}},
];

function testBipCommand(aCommand, aExpect) {
  is(aCommand.typeOfCommand, aExpect.typeOfCommand, "typeOfCommand");
  is(aCommand.options.text, aExpect.text, "options.text");

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
      log("bip_cmd: " + data.command);

      let promises = [];
      // Wait onstkcommand event.
      promises.push(waitForTargetEvent(icc, "stkcommand")
        .then((aEvent) => testBipCommand(aEvent.command, data.expect)));
      // Send emulator command to generate stk unsolicited event.
      promises.push(sendEmulatorStkPdu(data.command));

      return Promise.all(promises);
    });
  }
  return promise;
});
