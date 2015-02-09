/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

const TEST_DATA = [
  {command: "D00C" + // Length
            "8103010500" + // Command details
            "82028182" + // Device identities
            "990104", // Event list
   expect: {commandQualifier: 0x00,
            eventList: [4]}},
  {command: "D00D" + // Length
            "8103010500" + // Command details
            "82028182" + // Device identities
            "99020507", // Event list
   expect: {commandQualifier: 0x00,
            eventList: [5, 7]}},
  {command: "D00C" + // Length
            "8103010500" + // Command details
            "82028182" + // Device identities
            "990107", // Event list
   expect: {commandQualifier: 0x00,
            eventList: [7]}},
  {command: "D00C" + // Length
            "8103010500" + // Command details
            "82028182" + // Device identities
            "990107", // Event list
   expect: {commandQualifier: 0x00,
            eventList: [7]}},
  {command: "D00B" + // Length
            "8103010500" + // Command details
            "82028182" + // Device identities
            "9900", // Event list
   expect: {commandQualifier: 0x00,
            eventList: null}},
  {command: "D00C" + // Length
            "8103010500" + // Command details
            "82028182" + // Device identities
            "990107", // Event list
   expect: {commandQualifier: 0x00,
            eventList: [7]}}
];

function testSetupEventList(aCommand, aExpect) {
  is(aCommand.commandNumber, 0x01, "commandNumber");
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
      // Wait icc-stkcommand system message.
      promises.push(waitForSystemMessage("icc-stkcommand")
        .then((aMessage) => {
          is(aMessage.iccId, icc.iccInfo.iccid, "iccId");
          testSetupEventList(aMessage.command, data.expect);
        }));
      // Send emulator command to generate stk unsolicited event.
      promises.push(sendEmulatorStkPdu(data.command));

      return Promise.all(promises);
    });
  }
  return promise;
});
