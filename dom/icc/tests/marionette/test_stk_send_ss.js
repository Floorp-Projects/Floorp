/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

const TEST_DATA = [
  {command: "D029" + // Length
            "8103011100" + // Command details
            "82028183" + // Device identities
            "850C43616C6C20466F7277617264" + // Alpha identifier
            "891091AA120A214365870921436587A901FB", // SS string
   expect: {commandQualifier: 0x00,
            text: "Call Forward"}},
  {command: "D01B" + // Length
            "8103011100" + // Command details
            "82028183" + // Device identities
            "891091AA120A214365870921436587A901FB", // SS string
   expect: {commandQualifier: 0x00}},
  {command: "D081FD" + // Length
            "8103011100" + // Command details
            "82028183" + // Device identities
            "8581EB4576656E2069662074686520466978656420" + // Alpha identifier
            "4469616C6C696E67204E756D626572207365727669" +
            "636520697320656E61626C65642C20746865207375" +
            "70706C656D656E7461727920736572766963652063" +
            "6F6E74726F6C20737472696E6720696E636C756465" +
            "6420696E207468652053454E442053532070726F61" +
            "637469766520636F6D6D616E64207368616C6C206E" +
            "6F7420626520636865636B656420616761696E7374" +
            "2074686F7365206F66207468652046444E206C6973" +
            "742E2055706F6E20726563656976696E6720746869" +
            "7320636F6D6D616E642C20746865204D4520736861" +
            "6C6C2064656369" +
            "8904FFBA13FB", // SS string
   expect: {commandQualifier: 0x00,
            text: "Even if the Fixed Dialling Number service is enabled, " +
                  "the supplementary service control string included in the " +
                  "SEND SS proactive command shall not be checked against " +
                  "those of the FDN list. Upon receiving this command, the " +
                  "ME shall deci"}},
  {command: "D01B" + // Length
            "8103011100" + // Command details
            "82028183" + // Device identities
            "891091AA120A214365870921436587A901FB", // SS string
   expect: {commandQualifier: 0x00}},
  {command: "D02B" + // Length
            "8103011100" + // Command details
            "82028183" + // Device identities
            "850A42617369632049636F6E" + // Alpha identifier
            "891091AA120A214365870921436587A901FB" + // SS string
            "9E020001", // Icon identifier
   expect: {commandQualifier: 0x00,
            text: "Basic Icon",
            iconSelfExplanatory: true,
            icons: [BASIC_ICON]}},
  {command: "D01F" + // Length
            "8103011100" + // Command details
            "82028183" + // Device identities
            "891091AA120A214365870921436587A901FB" + // SS string
            "9E020103", // Icon identifier
   expect: {commandQualifier: 0x00,
            iconSelfExplanatory: false,
            icons: [COLOR_ICON]}},
  {command: "D036" + // Length
            "8103011100" + // Command details
            "82028183" + // Device identities
            "851980041704140420041004120421042204120423041904220415" + // Alpha identifier
            "891091AA120A214365870921436587A901FB", // SS string
   expect: {commandQualifier: 0x00,
            text: "ЗДРАВСТВУЙТЕ"}},
  {command: "D033" + // Length
            "8103011100" + // Command details
            "82028183" + // Device identities
            "851054657874204174747269627574652031" + // Alpha identifier
            "891091AA120A214365870921436587A901FB" + // SS string
            "D004001000B4", // Text attribute
   expect: {commandQualifier: 0x00,
            text: "Text Attribute 1"}},
  {command: "D022" + // Length
            "8103011100" + // Command details
            "82028183" + // Device identities
            "8505804F60597D" + // Alpha identifier
            "891091AA120A214365870921436587A901FB", // SS string
   expect: {commandQualifier: 0x00,
            text: "你好"}},
];

const TEST_CMD_NULL_ALPHA_ID =
        "D01D" + // Length
        "8103011100" + // Command details
        "82028183" + // Device identities
        "8500" + // Alpha identifier
        "891091AA120A214365870921436587A901FB"; // SS string

function verifySendSS(aCommand, aExpect) {
  is(aCommand.commandNumber, 0x01, "commandNumber");
  is(aCommand.typeOfCommand, MozIccManager.STK_CMD_SEND_SS, "typeOfCommand");
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

function testSendSS() {
  let icc = getMozIcc();
  let promise = Promise.resolve();
  for (let i = 0; i < TEST_DATA.length; i++) {
    let data = TEST_DATA[i];
    promise = promise.then(() => {
      log("send_ss_cmd: " + data.command);

      let promises = [];
      // Wait onstkcommand event.
      promises.push(waitForTargetEvent(icc, "stkcommand")
        .then((aEvent) => verifySendSS(aEvent.command, data.expect)));
      // Wait icc-stkcommand system message.
      promises.push(waitForSystemMessage("icc-stkcommand")
        .then((aMessage) => {
          is(aMessage.iccId, icc.iccInfo.iccid, "iccId");
          verifySendSS(aMessage.command, data.expect);
        }));
      // Send emulator command to generate stk unsolicited event.
      promises.push(sendEmulatorStkPdu(data.command));

      return Promise.all(promises);
    });
  }
  return promise;
}

function testSendSSNullAlphaId() {
  let icc = getMozIcc();

  // No "stkcommand" event should occur.
  icc.addEventListener("stkcommand",
    (event) => ok(false, event + " should not occur."));

  // No "icc-stkcommand" system message should be sent.
  workingFrame.contentWindow.navigator.mozSetMessageHandler("icc-stkcommand",
    (msg) => ok(false, msg + " should not be sent."));

  // If nothing happens within 3 seconds after the emulator command sent,
  // treat as success.
  log("send_ss_cmd: " + TEST_CMD_NULL_ALPHA_ID);
  return sendEmulatorStkPdu(TEST_CMD_NULL_ALPHA_ID)
    .then(() => new Promise(function(resolve, reject) {
      setTimeout(() => resolve(), 3000);
    }));
}

// Start tests
startTestCommon(function() {
  return Promise.resolve()
    .then(() => testSendSS())
    .then(() => testSendSSNullAlphaId());
});
