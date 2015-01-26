/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

const TEST_DATA = [
  // Start
  {command: "d011810301270082028182a40101a503102030",
   expect: {commandNumber: 0x01,
            commandQualifier: MozIccManager.STK_TIMER_START,
            timerAction: MozIccManager.STK_TIMER_START,
            timerId: 0x01,
            timerValue: (0x01 * 60 * 60) + (0x02 * 60) + 0x03}},
  // Deactivate
  {command: "d00c810301270182028182a40104",
   expect: {commandNumber: 0x01,
            commandQualifier: MozIccManager.STK_TIMER_DEACTIVATE,
            timerAction: MozIccManager.STK_TIMER_DEACTIVATE,
            timerId: 0x04}},
  // Get current value
  {command: "d00c810301270282028182a40108",
   expect: {commandNumber: 0x01,
            commandQualifier: MozIccManager.STK_TIMER_GET_CURRENT_VALUE,
            timerAction: MozIccManager.STK_TIMER_GET_CURRENT_VALUE,
            timerId: 0x08}},
];

function testTimerManagement(aCommand, aExpect) {
  is(aCommand.typeOfCommand, MozIccManager.STK_CMD_TIMER_MANAGEMENT,
     "typeOfCommand");
  is(aCommand.commandQualifier, aExpect.commandQualifier, "commandQualifier");
  is(aCommand.options.timerAction, aExpect.timerAction, "options.timerAction");
  is(aCommand.options.timerId, aExpect.timerId, "options.timerId");

  if (aExpect.timerValue) {
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
      // Send emulator command to generate stk unsolicited event.
      promises.push(sendEmulatorStkPdu(data.command));

      return Promise.all(promises);
    });
  }
  return promise;
});
