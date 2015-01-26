/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

const TEST_DATA = [
  {command: "d01b810301140082028183850953656e642044544d46ac052143658709",
   expect: {commandQualifier: 0x00,
            text: "Send DTMF"}},
  {command: "d010810301140082028183ac052143658709",
   expect: {commandQualifier: 0x00}},
  {command: "d0138103011400820281838500ac06c1cccccccc2c",
   expect: {commandQualifier: 0x00,
            text: ""}},
  {command: "d011810301140082028183ac06c1cccccccc2c",
   expect: {commandQualifier: 0x00}},
  {command: "d01d810301140082028183850a42617369632049636f6eac02c1f29e020001",
   expect: {commandQualifier: 0x00,
            text: "Basic Icon",
            iconSelfExplanatory: true,
            icons: [BASIC_ICON]}},
  {command: "d011810301140082028183ac02c1f29e020005",
   expect: {commandQualifier: 0x00,
            iconSelfExplanatory: true,
            icons: [COLOR_TRANSPARENCY_ICON]}},
  {command: "d01c810301140082028183850953656e642044544d46ac02c1f29e020101",
   expect: {commandQualifier: 0x00,
            text: "Send DTMF",
            iconSelfExplanatory: false,
            icons: [BASIC_ICON]}},
  {command: "d011810301140082028183ac02c1f29e020105",
   expect: {commandQualifier: 0x00,
            iconSelfExplanatory: false,
            icons: [COLOR_TRANSPARENCY_ICON]}},
  {command: "d028810301140082028183851980041704140420041004120421042204120423041904220415ac02c1f2",
   expect: {commandQualifier: 0x00,
            text: "ЗДРАВСТВУЙТЕ"}},
  {command: "d00d810301140082028183ac02c1f2",
   expect: {commandQualifier: 0x00}},
  {command: "d023810301140082028183850b53656e642044544d462031ac052143658709d004000b00b4",
   expect: {commandQualifier: 0x00,
            text: "Send DTMF 1"}},
  {command: "d016810301140082028183ac052143658709d004000b00b4",
   expect: {commandQualifier: 0x00}},
  {command: "d01d810301140082028183850b53656e642044544d462032ac052143658709",
   expect: {commandQualifier: 0x00,
            text: "Send DTMF 2"}},
  {command: "d023810301140082028183850b53656e642044544d462031ac052143658709d004000b01b4",
   expect: {commandQualifier: 0x00,
            text: "Send DTMF 1"}},
  {command: "d016810301140082028183ac052143658709d004000b01b4",
   expect: {commandQualifier: 0x00}},
  {command: "d023810301140082028183850b53656e642044544d462031ac052143658709d00400b002b4",
   expect: {commandQualifier: 0x00,
            text: "Send DTMF 1"}},
  {command: "d016810301140082028183ac052143658709d00400b002b4",
   expect: {commandQualifier: 0x00}},
  {command: "d023810301140082028183850b53656e642044544d462031ac052143658709d004000b04b4",
   expect: {commandQualifier: 0x00,
            text: "Send DTMF 1"}},
  {command: "d016810301140082028183ac052143658709d004000b04b4",
   expect: {commandQualifier: 0x00}},
  {command: "d023810301140082028183850b53656e642044544d462032ac052143658709d004000b00b4",
   expect: {commandQualifier: 0x00,
            text: "Send DTMF 2"}},
  {command: "d01d810301140082028183850b53656e642044544d462033ac052143658709",
   expect: {commandQualifier: 0x00,
            text: "Send DTMF 3"}},
  {command: "d023810301140082028183850b53656e642044544d462031ac052143658709d004000b08b4",
   expect: {commandQualifier: 0x00,
            text: "Send DTMF 1"}},
  {command: "d016810301140082028183ac052143658709d004000b08b4",
   expect: {commandQualifier: 0x00}},
  {command: "d023810301140082028183850b53656e642044544d462031ac052143658709d004000b10b4",
   expect: {commandQualifier: 0x00,
            text: "Send DTMF 1"}},
  {command: "d016810301140082028183ac052143658709d004000b10b4",
   expect: {commandQualifier: 0x00}},
  {command: "d023810301140082028183850b53656e642044544d462031ac052143658709d004000b20b4",
   expect: {commandQualifier: 0x00,
            text: "Send DTMF 1"}},
  {command: "d016810301140082028183ac052143658709d004000b20b4",
   expect: {commandQualifier: 0x00}},
  {command: "d023810301140082028183850b53656e642044544d462031ac052143658709d004000b40b4",
   expect: {commandQualifier: 0x00,
            text: "Send DTMF 1"}},
  {command: "d016810301140082028183ac052143658709d004000b40b4",
   expect: {commandQualifier: 0x00}},
  {command: "d023810301140082028183850b53656e642044544d462031ac052143658709d004000b80b4",
   expect: {commandQualifier: 0x00,
            text: "Send DTMF 1"}},
  {command: "d016810301140082028183ac052143658709d004000b80b4",
   expect: {commandQualifier: 0x00}},
  {command: "d0148103011400820281838505804f60597dac02c1f2",
   expect: {commandQualifier: 0x00,
            text: "你好"}},
  {command: "d01281030114008202818385038030ebac02c1f2",
   expect: {commandQualifier: 0x00,
            text: "ル"}}
];

function testSendDTMF(aCommand, aExpect) {
  is(aCommand.typeOfCommand, MozIccManager.STK_CMD_SEND_DTMF, "typeOfCommand");
  is(aCommand.commandQualifier, aExpect.commandQualifier, "commandQualifier");

  if (aExpect.text) {
    is(aCommand.options.text, aExpect.text, "options.text");
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
      log("send_dtmf_cmd: " + data.command);

      let promises = [];
      // Wait onstkcommand event.
      promises.push(waitForTargetEvent(icc, "stkcommand")
        .then((aEvent) => testSendDTMF(aEvent.command, data.expect)));
      // Send emulator command to generate stk unsolicited event.
      promises.push(sendEmulatorStkPdu(data.command));

      return Promise.all(promises);
    });
  }
  return promise;
});
