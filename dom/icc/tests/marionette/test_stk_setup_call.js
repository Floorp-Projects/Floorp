/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

const TEST_DATA = [
  {command: "d01e81030110008202818385084e6f7420627573798609911032042143651c2c",
   expect: {commandQualifier: 0x00,
            confirmMessage: { text: "Not busy" },
            address: "+012340123456,1,2"}},
  {command: "d01d81030110028202818385074f6e20686f6c648609911032042143651c2c",
   expect: {commandQualifier: 0x02,
            confirmMessage: { text: "On hold" },
            address: "+012340123456,1,2"}},
  {command: "d020810301100482028183850a446973636f6e6e6563748609911032042143651c2c",
   expect: {commandQualifier: 0x04,
            confirmMessage: { text: "Disconnect" },
            address: "+012340123456,1,2"}},
  {command: "d02b81030110008202818385114361706162696c69747920636f6e6669678609911032042143651c2c870201a0",
   expect: {commandQualifier: 0x00,
            confirmMessage: { text: "Capability config" },
            address: "+012340123456,1,2"}},
  {command: "d01c81030110018202818386119110325476981032547698103254769810",
   expect: {commandQualifier: 0x01,
            address: "+01234567890123456789012345678901"}},
  {command: "d081fd8103011001820281838581ed54687265652074797065732061726520646566696e65643a202d2073657420757020612063616c6c2c20627574206f6e6c79206966206e6f742063757272656e746c792062757379206f6e20616e6f746865722063616c6c3b202d2073657420757020612063616c6c2c2070757474696e6720616c6c206f746865722063616c6c732028696620616e7929206f6e20686f6c643b202d2073657420757020612063616c6c2c20646973636f6e6e656374696e6720616c6c206f746865722063616c6c732028696620616e79292066697273742e20466f722065616368206f662074686573652074797065732c2086029110",
   expect: {commandQualifier: 0x01,
            confirmMessage: { text: "Three types are defined: - set up a call, but only if not currently busy on another call; - set up a call, putting all other calls (if any) on hold; - set up a call, disconnecting all other calls (if any) first. For each of these types, " },
            address: "+01"}},
  {command: "d02281030110018202818385084475726174696f6e8609911032042143651c2c8402010a",
   expect: {commandQualifier: 0x01,
            confirmMessage: { text: "Duration" },
            address: "+012340123456,1,2",
            duration: {timeUnit: MozIccManager.STK_TIME_UNIT_SECOND,
                       timeInterval: 0x0A}}},
  {command: "d028810301100082028183850c434f4e4649524d4154494f4e8609911032042143651c2c850443414c4c",
   expect: {commandQualifier: 0x00,
            confirmMessage: { text: "CONFIRMATION" },
            callMessage: { text: "CALL" },
            address: "+012340123456,1,2"}},
  {command: "d03081030110008202818385165365742075702063616c6c2049636f6e20332e312e318609911032042143651c2c9e020101",
   expect: {commandQualifier: 0x00,
            confirmMessage: { text: "Set up call Icon 3.1.1",
                              iconSelfExplanatory: false,
                              icons: [BASIC_ICON] },
            address: "+012340123456,1,2"}},
  {command: "d04c81030110008202818385165365742075702063616c6c2049636f6e20332e342e318609911032042143651c2c9e02000185165365742075702063616c6c2049636f6e20332e342e329e020001",
   expect: {commandQualifier: 0x00,
            confirmMessage: { text: "Set up call Icon 3.4.1",
                              iconSelfExplanatory: true,
                              icons: [BASIC_ICON] },
            callMessage: { text: "Set up call Icon 3.4.2" },
            address: "+012340123456,1,2"}},
  {command: "d038810301100082028183850e434f4e4649524d4154494f4e20318609911032042143651c2c850643414c4c2031d004000e00b4d004000600b4",
   expect: {commandQualifier: 0x00,
            confirmMessage: { text: "CONFIRMATION 1" },
            callMessage: { text: "CALL 1" },
            address: "+012340123456,1,2"}},
  {command: "d04c810301100082028183851b800417041404200410041204210422041204230419042204150031860791103204214365851b800417041404200410041204210422041204230419042204150032",
   expect: {commandQualifier: 0x00,
            confirmMessage: { text: "ЗДРАВСТВУЙТЕ1" },
            callMessage: { text: "ЗДРАВСТВУЙТЕ2" },
            address: "+012340123456"}},
  {command: "d0198103011000820281838505804e0d5fd9860791103204214365",
   expect: {commandQualifier: 0x00,
            confirmMessage: { text: "不忙" },
            address: "+012340123456"}},
  {command: "d022810301100082028183850580786e5b9a860791103204214365850780625375358bdd",
   expect: {commandQualifier: 0x00,
            confirmMessage: { text: "确定" },
            callMessage: { text: "打电话" },
            address: "+012340123456"}},
  {command: "d01c8103011000820281838609911032042143651c2c9e0200019e020103",
   expect: {commandQualifier: 0x00,
            confirmMessage: { iconSelfExplanatory: true,
                              icons: [BASIC_ICON] },
            callMessage: { iconSelfExplanatory: false,
                           icons: [COLOR_ICON] },
            address: "+012340123456,1,2"}},
];

function testSetupCall(aCommand, aExpect) {
  is(aCommand.typeOfCommand, MozIccManager.STK_CMD_SET_UP_CALL,
     "typeOfCommand");
  is(aCommand.commandQualifier, aExpect.commandQualifier,
     "commandQualifier");
  is(aCommand.options.address, aExpect.address,
     "options.address");

  if (aExpect.confirmMessage) {
    isStkText(aCommand.options.confirmMessage, aExpect.confirmMessage);
  }

  if (aExpect.callMessage) {
    isStkText(aCommand.options.callMessage, aExpect.callMessage);
  }

  let duration = aCommand.options.duration;
  if (duration) {
    is(duration.timeUnit, aExpect.duration.timeUnit, "duration.timeUnit");
    is(duration.timeInterval, aExpect.duration.timeInterval,
       "duration.timeInterval");
  }
}

// Start tests
startTestCommon(function() {
  let icc = getMozIcc();
  let promise = Promise.resolve();
  for (let i = 0; i < TEST_DATA.length; i++) {
    let data = TEST_DATA[i];
    promise = promise.then(() => {
      log("setup_call_cmd: " + data.command);

      let promises = [];
      // Wait onstkcommand event.
      promises.push(waitForTargetEvent(icc, "stkcommand")
        .then((aEvent) => testSetupCall(aEvent.command, data.expect)));
      // Send emulator command to generate stk unsolicited event.
      promises.push(sendEmulatorStkPdu(data.command));

      return Promise.all(promises);
    });
  }
  return promise;
});
