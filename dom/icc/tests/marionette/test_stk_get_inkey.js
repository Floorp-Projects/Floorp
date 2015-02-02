/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

const TEST_DATA = [
  {command: "d0148103012200820281828d09004537bd2c07896022",
   expect: {commandQualifier: 0x00,
            text: "Enter \"0\""}},
  {command: "d081ad8103012201820281828d81a104456e746572202278222e205468697320636f6d6d616e6420696e7374727563747320746865204d4520746f20646973706c617920746578742c20616e6420746f2065787065637420746865207573657220746f20656e74657220612073696e676c65206368617261637465722e20416e7920726573706f6e736520656e7465726564206279207468652075736572207368616c6c206265207061737365642074",
   expect: {commandQualifier: 0x01,
            text: "Enter \"x\". This command instructs the ME to display text, and to expect the user to enter a single character. Any response entered by the user shall be passed t",
            isAlphabet: true}},
  {command: "d0168103012200820281828d0b043c54494d452d4f55543e",
   expect: {commandQualifier: 0x00,
            text: "<TIME-OUT>"}},
  {command: "d081998103012200820281828d818d080417041404200410041204210422041204230419042204150417041404200410041204210422041204230419042204150417041404200410041204210422041204230419042204150417041404200410041204210422041204230419042204150417041404200410041204210422041204230419042204150417041404200410041204210422041204230419",
   expect: {commandQualifier: 0x00,
            text: "ЗДРАВСТВУЙТЕЗДРАВСТВУЙТЕЗДРАВСТВУЙТЕЗДРАВСТВУЙТЕЗДРАВСТВУЙТЕЗДРАВСТВУЙ"}},
  {command: "d0118103012203820281828d0604456e746572",
   expect: {commandQualifier: 0x03,
            text: "Enter",
            isAlphabet: true,
            isUCS2: true}},
  {command: "d0158103012204820281828d0a04456e74657220594553",
   expect: {commandQualifier: 0x04,
            text: "Enter YES",
            isYesNoRequested: true}},
  {command: "d0198103012200820281828d0a043c4e4f2d49434f4e3e1e020002",
   expect: {commandQualifier: 0x00,
            // The record number 02 in EFimg is not defined, so no icon will be
            // shown, but the text string should still be displayed.
            text: "<NO-ICON>"}},
  {command: "D0198103012200820281828D0A04456E74657220222B228402010A",
   expect: {commandQualifier: 0x00,
            text: "Enter \"+\"",
            duration: {timeUnit: MozIccManager.STK_TIME_UNIT_SECOND,
                       timeInterval: 0x0A}}},
];

function testGetInKey(aCommand, aExpect) {
  is(aCommand.typeOfCommand, MozIccManager.STK_CMD_GET_INKEY, "typeOfCommand");
  is(aCommand.commandQualifier, aExpect.commandQualifier, "commandQualifier");
  is(aCommand.options.text, aExpect.text, "options.text");
  is(aCommand.options.isAlphabet, aExpect.isAlphabet, "options.isAlphabet");
  is(aCommand.options.isUCS2, aExpect.isUCS2, "options.isUCS2");
  is(aCommand.options.isYesNoRequested, aExpect.isYesNoRequested,
     "options.isYesNoRequested");

  if (aExpect.duration) {
    let duration = aCommand.options.duration;
    is(duration.timeUnit, aExpect.duration.timeUnit,
       "options.duration.timeUnit");
    is(duration.timeInterval, aExpect.duration.timeInterval,
       "options.duration.timeInterval");
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
      log("get_inkey_cmd: " + data.command);

      let promises = [];
      // Wait onstkcommand event.
      promises.push(waitForTargetEvent(icc, "stkcommand")
        .then((aEvent) => testGetInKey(aEvent.command, data.expect)));
      // Send emulator command to generate stk unsolicited event.
      promises.push(sendEmulatorStkPdu(data.command));

      return Promise.all(promises);
    });
  }
  return promise;
});
