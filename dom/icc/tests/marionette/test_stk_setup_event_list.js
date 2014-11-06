/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

const TEST_DATA = [
  {command: "d00c810301050082028182990104",
   expect: {commandQualifier: 0x00,
            eventList: [4]}},
  {command: "d00d81030105008202818299020507",
   expect: {commandQualifier: 0x00,
            eventList: [5, 7]}},
  {command: "d00c810301050082028182990107",
   expect: {commandQualifier: 0x00,
            eventList: [7]}},
  {command: "d00c810301050082028182990107",
   expect: {commandQualifier: 0x00,
            eventList: [7]}},
  {command: "d00b8103010500820281829900",
   expect: {commandQualifier: 0x00,
            eventList: null}},
  {command: "d00c810301050082028182990107",
   expect: {commandQualifier: 0x00,
            eventList: [7]}}
];

function testSetupEventList(aCommand, aExpect) {
  is(aCommand.typeOfCommand, MozIccManager.STK_CMD_SET_UP_EVENT_LIST,
     "typeOfCommand");
  is(aCommand.commandQualifier, aExpect.commandQualifier, "commandQualifier");

  for (let index in aExpect.eventList) {
    is(aCommand.options.eventList[index], aExpect.eventList[index],
       "options.eventList[" + index + "]");
  }
}

// Start tests
startTestCommon(function() {
  let icc = getMozIcc();
  let promise = Promise.resolve();
  for (let i = 0; i < TEST_DATA.length; i++) {
    let data = TEST_DATA[i];
    promise = promise.then(() => {
      log("setup_event_list_cmd: " + data.command);

      let promises = [];
      // Wait onstkcommand event.
      promises.push(waitForTargetEvent(icc, "stkcommand")
        .then((aEvent) => testSetupEventList(aEvent.command, data.expect)));
      // Send emulator command to generate stk unsolicited event.
      promises.push(sendEmulatorStkPdu(data.command));

      return Promise.all(promises);
    });
  }
  return promise;
});
