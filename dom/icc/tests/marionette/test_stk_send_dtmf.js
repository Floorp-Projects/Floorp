/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

const TEST_DATA = [
  {command: "D01B" + // Length
            "8103011400" + // Command details
            "82028183" + // Device identities
            "850953656E642044544D46" + // Alpha identifier
            "AC052143658709", // DTMF string
   expect: {commandQualifier: 0x00,
            text: "Send DTMF"}},
  {command: "D010" + // Length
            "8103011400" + // Command details
            "82028183" + // Device identities
            "AC052143658709", // DTMF string
   expect: {commandQualifier: 0x00}},
  {command: "D011" + // Length
            "8103011400" + // Command details
            "82028183" + // Device identities
            "AC06C1CCCCCCCC2C", // DTMF string
   expect: {commandQualifier: 0x00}},
  {command: "D01D" + // Length
            "8103011400" + // Command details
            "82028183" + // Device identities
            "850A42617369632049636F6E" + // Alpha identifier
            "AC02C1F2" + // DTMF string
            "9E020001", // Icon identifier
   expect: {commandQualifier: 0x00,
            text: "Basic Icon",
            iconSelfExplanatory: true,
            icons: [BASIC_ICON]}},
  {command: "D011" + // Length
            "8103011400" + // Command details
            "82028183" + // Device identities
            "AC02C1F2" + // DTMF string
            "9E020005", // Icon identifier
   expect: {commandQualifier: 0x00,
            iconSelfExplanatory: true,
            icons: [COLOR_TRANSPARENCY_ICON]}},
  {command: "D028" + // Length
            "8103011400" + // Command details
            "82028183" + // Device identities
            "851980041704140420041004120421042204120423041904220415" + // Alpha identifier
            "AC02C1F2", // DTMF string
   expect: {commandQualifier: 0x00,
            text: "ЗДРАВСТВУЙТЕ"}},
  {command: "D023" + // Length
            "8103011400" + // Command details
            "82028183" + // Device identities
            "850B53656E642044544D462031" + // Alpha identifier
            "AC052143658709" + // DTMF string
            "D004000B00B4", // Text attribute
   expect: {commandQualifier: 0x00,
            text: "Send DTMF 1"}},
  {command: "D014" + // Length
            "8103011400" + // Command details
            "82028183" + // Device identities
            "8505804F60597D" + // Alpha identifier
            "AC02C1F2", // DTMF string
   expect: {commandQualifier: 0x00,
            text: "你好"}},
];

const TEST_CMD_NULL_ALPHA_ID =
        "D013" + // Length
        "8103011400" + // Command details
        "82028183" + // Device identities
        "8500" + // Alpha identifier
        "AC06C1CCCCCCCC2C";

function verifySendDTMF(aCommand, aExpect) {
  is(aCommand.commandNumber, 0x01, "commandNumber");
  is(aCommand.typeOfCommand, MozIccManager.STK_CMD_SEND_DTMF, "typeOfCommand");
  is(aCommand.commandQualifier, aExpect.commandQualifier, "commandQualifier");

  // text is optional.
  if ("text" in aExpect) {
    is(aCommand.options.text, aExpect.text, "options.text");
  }

  // icons is optional.
  if ("icons" in aExpect) {
    isIcons(aCommand.options.icons, aExpect.icons);
    is(aCommand.options.iconSelfExplanatory, aExpect.iconSelfExplanatory,
       "options.iconSelfExplanatory");
  }
}

function testSendDTMF() {
  let icc = getMozIcc();
  let promise = Promise.resolve();
  for (let i = 0; i < TEST_DATA.length; i++) {
    let data = TEST_DATA[i];
    promise = promise.then(() => {
      log("send_dtmf_cmd: " + data.command);

      let promises = [];
      // Wait onstkcommand event.
      promises.push(waitForTargetEvent(icc, "stkcommand")
        .then((aEvent) => verifySendDTMF(aEvent.command, data.expect)));
      // Wait icc-stkcommand system message.
      promises.push(waitForSystemMessage("icc-stkcommand")
        .then((aMessage) => {
          is(aMessage.iccId, icc.iccInfo.iccid, "iccId");
          verifySendDTMF(aMessage.command, data.expect);
        }));
      // Send emulator command to generate stk unsolicited event.
      promises.push(sendEmulatorStkPdu(data.command));

      return Promise.all(promises);
    });
  }
  return promise;
}

function testSendDTMFNullAlphaId() {
  let icc = getMozIcc();

  // No "stkcommand" event should occur.
  icc.addEventListener("stkcommand",
    (event) => ok(false, event + " should not occur."));

  // No "icc-stkcommand" system message should be sent.
  workingFrame.contentWindow.navigator.mozSetMessageHandler("icc-stkcommand",
    (msg) => ok(false, msg + " should not be sent."));

  // If nothing happens within 3 seconds after the emulator command sent,
  // treat as success.
  log("send_dtmf_cmd: " + TEST_CMD_NULL_ALPHA_ID);
  return sendEmulatorStkPdu(TEST_CMD_NULL_ALPHA_ID)
    .then(() => new Promise(function(resolve, reject) {
      setTimeout(() => resolve(), 3000);
    }));
}


// Start tests
startTestCommon(function() {
  return Promise.resolve()
    .then(() => testSendDTMF())
    .then(() => testSendDTMFNullAlphaId());
});
