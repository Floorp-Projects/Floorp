/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

const TEST_DATA = [
  // Start
  {command: "D011" + // Length
            "8103012700" + // Command details
            "82028182" + // Device identities
            "A40101" + // Timer identifier
            "A503102030", // Timer value
   expect: {commandQualifier: MozIccManager.STK_TIMER_START,
            timerAction: MozIccManager.STK_TIMER_START,
            timerId: 0x01,
            timerValue: (0x01 * 60 * 60) + (0x02 * 60) + 0x03}},
  // Deactivate
  {command: "D00C" + // Length
            "8103012701" + // Command details
            "82028182" + // Device identities
            "A40104", // Timer identifier
   expect: {commandQualifier: MozIccManager.STK_TIMER_DEACTIVATE,
            timerAction: MozIccManager.STK_TIMER_DEACTIVATE,
            timerId: 0x04}},
  // Get current value
  {command: "D00C" + // Length
            "8103012702" + // Command details
            "82028182" + // Device identities
            "A40108", // Timer identifier
   expect: {commandQualifier: MozIccManager.STK_TIMER_GET_CURRENT_VALUE,
            timerAction: MozIccManager.STK_TIMER_GET_CURRENT_VALUE,
            timerId: 0x08}},
];

function testTimerManagement(aCommand, aExpect) {
  is(aCommand.commandNumber, 0x01, "commandNumber");
  is(aCommand.typeOfCommand, MozIccManager.STK_CMD_TIMER_MANAGEMENT,
     "typeOfCommand");
  is(aCommand.commandQualifier, aExpect.commandQualifier, "commandQualifier");

  is(aCommand.options.timerAction, aExpect.timerAction, "options.timerAction");
  is(aCommand.options.timerId, aExpect.timerId, "options.timerId");

  if ("timerValue" in aExpect) {
    is(aCommand.options.timerValue, aExpect.timerValue, "options.timerValue");
  }
}

// Start tests
startTestCommon(function() {
  let icc = getMozIcc();
  let promise = Promise.resolve();
  for (let i = 0; i < TEST_DATA.length; i++) {
    let data = TEST_DATA[i];
    promise = promise.then(() => {
      log("timer_management_cmd: " + data.command);

      let promises = [];
      // Wait onstkcommand event.
      promises.push(waitForTargetEvent(icc, "stkcommand")
        .then((aEvent) => testTimerManagement(aEvent.command, data.expect)));
      // Wait icc-stkcommand system message.
      promises.push(waitForSystemMessage("icc-stkcommand")
        .then((aMessage) => {
          is(aMessage.iccId, icc.iccInfo.iccid, "iccId");
          testTimerManagement(aMessage.command, data.expect);
        }));
      // Send emulator command to generate stk unsolicited event.
      promises.push(sendEmulatorStkPdu(data.command));

      return Promise.all(promises);
    });
  }
  return promise;
});
