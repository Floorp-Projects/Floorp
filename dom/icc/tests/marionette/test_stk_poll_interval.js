/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

const TEST_DATA = [
  {command: "D00D" + // Length
            "8103010300" + // Command details
            "82028182" + // Device identities
            "8402001A", // Duration
   expect: {commandQualifier: 0x00,
            timeUnit: MozIccManager.STK_TIME_UNIT_MINUTE,
            timeInterval: 0x1A}},
  {command: "D00D" + // Length
            "8103010300" + // Command details
            "82028182" + // Device identities
            "8402010A", // Duration
   expect: {commandQualifier: 0x00,
            timeUnit: MozIccManager.STK_TIME_UNIT_SECOND,
            timeInterval: 0x0A}},
  {command: "D00D" + // Length
            "8103010300" + // Command details
            "82028182" + // Device identities
            "84020205", // Duration
   expect: {commandQualifier: 0x00,
            timeUnit: MozIccManager.STK_TIME_UNIT_TENTH_SECOND,
            timeInterval: 0x05}},
];

function testPollOff(aCommand, aExpect) {
  is(aCommand.commandNumber, 0x01, "commandNumber");
  is(aCommand.typeOfCommand, MozIccManager.STK_CMD_POLL_INTERVAL,
     "typeOfCommand");
  is(aCommand.commandQualifier, aExpect.commandQualifier, "commandQualifier");
  is(aCommand.options.timeUnit, aExpect.timeUnit,
     "options.timeUnit");
  is(aCommand.options.timeInterval, aExpect.timeInterval,
     "options.timeInterval");
}

// Start tests
startTestCommon(function() {
  let icc = getMozIcc();
  let promise = Promise.resolve();
  for (let i = 0; i < TEST_DATA.length; i++) {
    let data = TEST_DATA[i];
    promise = promise.then(() => {
      log("poll_interval_cmd: " + data.command);

      let promises = [];
      // Wait onstkcommand event.
      promises.push(waitForTargetEvent(icc, "stkcommand")
        .then((aEvent) => testPollOff(aEvent.command, data.expect)));
      // Wait icc-stkcommand system message.
      promises.push(waitForSystemMessage("icc-stkcommand")
        .then((aMessage) => {
          is(aMessage.iccId, icc.iccInfo.iccid, "iccId");
          testPollOff(aMessage.command, data.expect);
        }));
      // Send emulator command to generate stk unsolicited event.
      promises.push(sendEmulatorStkPdu(data.command));

      return Promise.all(promises);
    });
  }
  return promise;
});
