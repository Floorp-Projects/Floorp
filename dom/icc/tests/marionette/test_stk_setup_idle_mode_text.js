/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

const TEST_DATA = [
  {command: "D01A" + // Length
            "8103012800" + // Command details
            "82028182" + // Device identities
            "8D0F0449646C65204D6F64652054657874", // Text string
   expect: {commandQualifier: 0x00,
            text: "Idle Mode Text"}},
  {command: "D081FD" + // Length
            "8103012800" + // Command details
            "82028182" + // Device identities
            "8D81F100547419344D3641737498CD06CDEB70383B0F0A" + // Text string
            "83E8653C1D34A7CBD3EE330B7447A7C768D01C1D66B341" +
            "E232889C9EC3D9E17C990C12E741747419D42C82C27350" +
            "D80D4A93D96550FB4D2E83E8653C1D943683E8E832A859" +
            "04A5E7A0B0985D06D1DF20F21B94A6BBA8E832082E2FCF" +
            "CB6E7A989E7EBB41737A9E5D06A5E72076D94C0785E7A0" +
            "B01B946EC3D9E576D94D0FD3D36F37885C1EA7E7E9B71B" +
            "447F83E8E832A85904B5C3EEBA393CA6D7E565B90B4445" +
            "97416932BB0C6ABFC96510BD8CA783E6E8309B0D129741" +
            "E4F41CCE0EE7CB6450DA0D0A83DA61B7BB2C07D1D1613A" +
            "A8EC9ED7E5E539888E0ED341EE32",
   expect: {commandQualifier: 0x00,
            text: "The SIM shall supply a text string, which shall be " +
                  "displayed by the ME as an idle mode text if the ME is " +
                  "able to do it.The presentation style is left as an " +
                  "implementation decision to the ME manufacturer. The idle " +
                  "mode text shall be displayed in a manner that ensures " +
                  "that ne"}},
  {command: "D019" + // Length
            "8103012800" + // Command details
            "82028182" + // Device identities
            "8D0A0449646C652074657874" + // Text string
            "9E020101", // Icon identifier
   expect: {commandQualifier: 0x00,
            text: "Idle text",
            iconSelfExplanatory: false,
            icons: [BASIC_ICON]}},
  {command: "D019" + // Length
            "8103012800" + // Command details
            "82028182" + // Device identities
            "8D0A0449646C652074657874" + // Text string
            "9E020007", // Icon identifier
   expect: {commandQualifier: 0x00,
            text: "Idle text",
            iconSelfExplanatory: true,
            icons: [COLOR_ICON, COLOR_TRANSPARENCY_ICON]}},
  {command: "D024" + // Length
            "8103012800" + // Command details
            "82028182" + // Device identities
            "8D1908041704140420041004120421042204120423041904220415", // Text string
   expect: {commandQualifier: 0x00,
            text: "ЗДРАВСТВУЙТЕ"}},
  {command: "D022" + // Length
            "8103012800" + // Command details
            "82028182" + // Device identities
            "8D110449646C65204D6F646520546578742031" + // Text string
            "D004001000B4", // Text attribute
   expect: {commandQualifier: 0x00,
            text: "Idle Mode Text 1"}},
  {command: "D010" + // Length
            "8103012800" + // Command details
            "82028182" + // Device identities
            "8D05084F60597D", // Text string
   expect: {commandQualifier: 0x00,
            text: "你好"}},
];

function testSetupIdleModeText(aCommand, aExpect) {
  is(aCommand.commandNumber, 0x01, "commandNumber");
  is(aCommand.typeOfCommand, MozIccManager.STK_CMD_SET_UP_IDLE_MODE_TEXT,
     "typeOfCommand");
  is(aCommand.commandQualifier, aExpect.commandQualifier, "commandQualifier");
  is(aCommand.options.text, aExpect.text, "options.text");

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
      log("setup_idle_mode_text_cmd: " + data.command);

      let promises = [];
      // Wait onstkcommand event.
      promises.push(waitForTargetEvent(icc, "stkcommand")
        .then((aEvent) => testSetupIdleModeText(aEvent.command, data.expect)));
      // Wait icc-stkcommand system message.
      promises.push(waitForSystemMessage("icc-stkcommand")
        .then((aMessage) => {
          is(aMessage.iccId, icc.iccInfo.iccid, "iccId");
          testSetupIdleModeText(aMessage.command, data.expect);
        }));
      // Send emulator command to generate stk unsolicited event.
      promises.push(sendEmulatorStkPdu(data.command));

      return Promise.all(promises);
    });
  }
  return promise;
});
