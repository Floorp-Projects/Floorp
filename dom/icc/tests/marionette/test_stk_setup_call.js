/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

const TEST_DATA = [
  {command: "D01E" + // Length
            "8103011000" + // Command details
            "82028183" + // Device identities
            "85084E6F742062757379" + // Alpha identifier
            "8609911032042143651C2C", // Address
   expect: {commandQualifier: 0x00,
            confirmMessage: { text: "Not busy" },
            address: "+012340123456,1,2"}},
  {command: "D01D" + // Length
            "8103011002" + // Command details
            "82028183" + // Device identities
            "85074F6E20686F6C64" + // Alpha identifier
            "8609911032042143651C2C", // Address
   expect: {commandQualifier: 0x02,
            confirmMessage: { text: "On hold" },
            address: "+012340123456,1,2"}},
  {command: "D020" + // Length
            "8103011004" + // Command details
            "82028183" + // Device identities
            "850A446973636F6E6E656374" + // Alpha identifier
            "8609911032042143651C2C", // Address
   expect: {commandQualifier: 0x04,
            confirmMessage: { text: "Disconnect" },
            address: "+012340123456,1,2"}},
  {command: "D02B" + // Length
            "8103011000" + // Command details
            "82028183" + // Device identities
            "85114361706162696C69747920636F6E666967" + // Alpha identifier
            "8609911032042143651C2C" + // Address
            "870201A0", // Capability configuration parameters
   expect: {commandQualifier: 0x00,
            confirmMessage: { text: "Capability config" },
            address: "+012340123456,1,2"}},
  {command: "D01C" + // Length
            "8103011001" + // Command details
            "82028183" + // Device identities
            "86119110325476981032547698103254769810", // Address
   expect: {commandQualifier: 0x01,
            address: "+01234567890123456789012345678901"}},
  {command: "D081FD" + // Length
            "8103011001" + // Command details
            "82028183" + // Device identities
            "8581ED546872656520747970657320617265206465" + // Alpha identifier
            "66696E65643A202D2073657420757020612063616C" +
            "6C2C20627574206F6E6C79206966206E6F74206375" +
            "7272656E746C792062757379206F6E20616E6F7468" +
            "65722063616C6C3B202D2073657420757020612063" +
            "616C6C2C2070757474696E6720616C6C206F746865" +
            "722063616C6C732028696620616E7929206F6E2068" +
            "6F6C643B202D2073657420757020612063616C6C2C" +
            "20646973636F6E6E656374696E6720616C6C206F74" +
            "6865722063616C6C732028696620616E7929206669" +
            "7273742E20466F722065616368206F662074686573" +
            "652074797065732C20" +
            "86029110", // Address
   expect: {commandQualifier: 0x01,
            confirmMessage: {
              text: "Three types are defined: - set up a call, but only if " +
                    "not currently busy on another call; - set up a call, " +
                    "putting all other calls (if any) on hold; - set up a " +
                    "call, disconnecting all other calls (if any) first. For " +
                    "each of these types, "
            },
            address: "+01"}},
  {command: "D022" + // Length
            "8103011001" + // Command details
            "82028183" + // Device identities
            "85084475726174696F6E" + // Alpha identifier
            "8609911032042143651C2C" + // Address
            "8402010A", // Duration
   expect: {commandQualifier: 0x01,
            confirmMessage: { text: "Duration" },
            address: "+012340123456,1,2",
            duration: {timeUnit: MozIccManager.STK_TIME_UNIT_SECOND,
                       timeInterval: 0x0A}}},
  {command: "D028" + // Length
            "8103011000" + // Command details
            "82028183" + // Device identities
            "850C434F4E4649524D4154494F4E" + // Alpha identifier
            "8609911032042143651C2C" + // Address
            "850443414C4C", // Alpha identifier
   expect: {commandQualifier: 0x00,
            confirmMessage: { text: "CONFIRMATION" },
            callMessage: { text: "CALL" },
            address: "+012340123456,1,2"}},
  {command: "D030" + // Length
            "8103011000" + // Command details
            "82028183" + // Device identities
            "85165365742075702063616C6C2049636F6E20332E312E31" + // Alpha identifier
            "8609911032042143651C2C" + // Address
            "9E020101", // Icon identifier
   expect: {commandQualifier: 0x00,
            confirmMessage: { text: "Set up call Icon 3.1.1",
                              iconSelfExplanatory: false,
                              icons: [BASIC_ICON] },
            address: "+012340123456,1,2"}},
  {command: "D04C" + // Length
            "8103011000" + // Command details
            "82028183" + // Device identities
            "85165365742075702063616C6C2049636F6E20332E342E31" + // Alpha identifier
            "8609911032042143651C2C" + // Address
            "9E020001" + // Icon identifier
            "85165365742075702063616C6C2049636F6E20332E342E32" + // Alpha identifier
            "9E020003", // Icon identifier
   expect: {commandQualifier: 0x00,
            confirmMessage: { text: "Set up call Icon 3.4.1",
                              iconSelfExplanatory: true,
                              icons: [BASIC_ICON] },
            callMessage: { text: "Set up call Icon 3.4.2" ,
                           iconSelfExplanatory: true,
                           icons: [COLOR_ICON] },
            address: "+012340123456,1,2"}},
  {command: "D038" + // Length
            "8103011000" + // Command details
            "82028183" + // Device identities
            "850E434F4E4649524D4154494F4E2031" + // Alpha identifier
            "8609911032042143651C2C" + // Address
            "850643414C4C2031" + // Alpha identifier
            "D004000E00B4" + // Text attribute
            "D004000600B4", // Text attribute
   expect: {commandQualifier: 0x00,
            confirmMessage: { text: "CONFIRMATION 1" },
            callMessage: { text: "CALL 1" },
            address: "+012340123456,1,2"}},
  {command: "D04C" + // Length
            "8103011000" + // Command details
            "82028183" + // Device identities
            "851B800417041404200410041204210422041204230419042204150031" + // Alpha identifier
            "860791103204214365" + // Address
            "851B800417041404200410041204210422041204230419042204150032", // Alpha identifier
   expect: {commandQualifier: 0x00,
            confirmMessage: { text: "ЗДРАВСТВУЙТЕ1" },
            callMessage: { text: "ЗДРАВСТВУЙТЕ2" },
            address: "+012340123456"}},
  {command: "D019" + // Length
            "8103011000" + // Command details
            "82028183" + // Device identities
            "8505804E0D5FD9" + // Alpha identifier
            "860791103204214365", // Address
   expect: {commandQualifier: 0x00,
            confirmMessage: { text: "不忙" },
            address: "+012340123456"}},
  {command: "D022" + // Length
            "8103011000" + // Command details
            "82028183" + // Device identities
            "850580786E5B9A" + // Alpha identifier
            "860791103204214365" + // Address
            "850780625375358BDD", // Alpha identifier
   expect: {commandQualifier: 0x00,
            confirmMessage: { text: "确定" },
            callMessage: { text: "打电话" },
            address: "+012340123456"}},
  {command: "D016" + // Length
            "8103011000" + // Command details
            "82028183" + // Device identities
            "8500" + // Alpha identifier
            "860791103204214365" + // Address
            "8500", // Alpha identifier
   expect: {commandQualifier: 0x00,
            confirmMessage: { text: "" },
            callMessage: { text: "" },
            address: "+012340123456"}},
  {command: "D01C" + // Length
            "8103011000" + // Command details
            "82028183" + // Device identities
            "8609911032042143651C2C" + // Address
            "9E020001" + // Icon identifier
            "9E020103", // Icon identifier
   expect: {commandQualifier: 0x00,
            confirmMessage: { iconSelfExplanatory: true,
                              icons: [BASIC_ICON] },
            callMessage: { iconSelfExplanatory: false,
                           icons: [COLOR_ICON] },
            address: "+012340123456,1,2"}},
];

function testSetupCall(aCommand, aExpect) {
  is(aCommand.commandNumber, 0x01, "commandNumber");
  is(aCommand.typeOfCommand, MozIccManager.STK_CMD_SET_UP_CALL,
     "typeOfCommand");
  is(aCommand.commandQualifier, aExpect.commandQualifier, "commandQualifier");
  is(aCommand.options.address, aExpect.address, "options.address");

  // confirmMessage is optional.
  if ("confirmMessage" in aExpect) {
    isStkText(aCommand.options.confirmMessage, aExpect.confirmMessage);
  }

  // callMessage is optional.
  if ("callMessage" in aExpect) {
    isStkText(aCommand.options.callMessage, aExpect.callMessage);
  }

  // duration is optional.
  if ("duration" in aExpect) {
    let duration = aCommand.options.duration;
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
      // Wait icc-stkcommand system message.
      promises.push(waitForSystemMessage("icc-stkcommand")
        .then((aMessage) => {
          is(aMessage.iccId, icc.iccInfo.iccid, "iccId");
          testSetupCall(aMessage.command, data.expect);
        }));
      // Send emulator command to generate stk unsolicited event.
      promises.push(sendEmulatorStkPdu(data.command));

      return Promise.all(promises);
    });
  }
  return promise;
});
