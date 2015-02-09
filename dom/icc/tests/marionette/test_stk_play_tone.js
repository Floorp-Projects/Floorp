/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

const TEST_DATA = [
  {command: "D009" + // Length
            "8103012000" + // Command details
            "82028182", // Device identities
   expect: {commandQualifier: 0x00}},
  {command: "D014" + // Length
            "8103012001" + // Command details
            "82028182" + // Device identities
            "8509506C617920546F6E65", // Alpha identifier
   expect: {commandQualifier: 0x01,
            text: "Play Tone"}},
  {command: "D00B" + // Length
            "8103012001" + // Command details
            "82028182" + // Device identities
            "8500", // Alpha identifier
   expect: {commandQualifier: 0x01,
            text: ""}},
  {command: "D00C" + // Length
            "8103012000" + // Command details
            "82028182" + // Device identities
            "8E0101", // Tone
   expect: {commandQualifier: 0x00,
            tone: 0x01}},
  {command: "D00D" + // Length
            "8103012000" + // Command details
            "82028182" + // Device identities
            "84020205", // Duration
   expect: {commandQualifier: 0x00,
            duration: {timeUnit: MozIccManager.STK_TIME_UNIT_TENTH_SECOND,
                       timeInterval: 0x05}}},
  {command: "D00D" + // Length
            "8103012000" + // Command details
            "82028182" + // Device identities
            "1E020101", // Icon identifier
   expect: {commandQualifier: 0x00,
            iconSelfExplanatory: false,
            icons: [BASIC_ICON]}},
  {command: "D01F" + // Length
            "8103012001" + // Command details
            "82028182" + // Device identities
            "8509506C617920546F6E65" + // Alpha identifier
            "8E0101" + // Tone
            "84020202" + // Duration
            "1E020003", // Icon identifier
   expect: {commandQualifier: 0x01,
            text: "Play Tone",
            duration: {timeUnit: MozIccManager.STK_TIME_UNIT_TENTH_SECOND,
                       timeInterval: 0x02},
            iconSelfExplanatory: true,
            icons: [COLOR_ICON]}},
];

function testPlayTone(aCommand, aExpect) {
  is(aCommand.commandNumber, 0x01, "commandNumber");
  is(aCommand.typeOfCommand, MozIccManager.STK_CMD_PLAY_TONE, "typeOfCommand");
  is(aCommand.commandQualifier, aExpect.commandQualifier, "commandQualifier");

  is(aCommand.options.isVibrate, !!(aExpect.commandQualifier & 0x01),
     "options.isVibrate");

  // text is optional.
  if ("text" in aExpect) {
    is(aCommand.options.text, aExpect.text, "options.text");
  }

  // tone is optional.
  if ("tone" in aExpect) {
    is(aCommand.options.tone, aExpect.tone, "options.tone");
  }

  // duration is optional.
  if ("duration" in aExpect) {
    let duration = aCommand.options.duration;
    is(duration.timeUnit, aExpect.duration.timeUnit,
       "options.duration.timeUnit");
    is(duration.timeInterval, aExpect.duration.timeInterval,
       "options.duration.timeInterval");
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
      log("play_tone_cmd: " + data.command);

      let promises = [];
      // Wait onstkcommand event.
      promises.push(waitForTargetEvent(icc, "stkcommand")
        .then((aEvent) => testPlayTone(aEvent.command, data.expect)));
      // Wait icc-stkcommand system message.
      promises.push(waitForSystemMessage("icc-stkcommand")
        .then((aMessage) => {
          is(aMessage.iccId, icc.iccInfo.iccid, "iccId");
          testPlayTone(aMessage.command, data.expect);
        }));
      // Send emulator command to generate stk unsolicited event.
      promises.push(sendEmulatorStkPdu(data.command));

      return Promise.all(promises);
    });
  }
  return promise;
});
