/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

const TEST_DATA = [
  {command: "D018" + // Length
            "8103011500" + // Command details
            "82028182" + // Device identities
            "3100" + // URL
            "050B44656661756C742055524C", // Alpha identifier
   expect: {commandQualifier: 0x00,
            url: "",
            mode: MozIccManager.STK_BROWSER_MODE_LAUNCH_IF_NOT_ALREADY_LAUNCHED,
            confirmMessage: { text: "Default URL" }}},
  {command: "D01F" + // Length
            "8103011500" + // Command details
            "82028182" + // Device identities
            "3112687474703A2F2F7878782E7979792E7A7A7A" + // URL
            "0500", // Alpha identifier
   expect: {commandQualifier: 0x00,
            url: "http://xxx.yyy.zzz",
            mode: MozIccManager.STK_BROWSER_MODE_LAUNCH_IF_NOT_ALREADY_LAUNCHED,
            confirmMessage: { text: "" }}},
  {command: "D023" + // Length
            "8103011500" + // Command details
            "82028182" + // Device identities
            "300100" + // Browser identity
            "3100" + // URL
            "320103" + // Bear
            "0D10046162632E6465662E6768692E6A6B6C", // Text string
   expect: {commandQualifier: 0x00,
            // Browser identity, Bear and Text string are useless.
            url: "",
            mode: MozIccManager.STK_BROWSER_MODE_LAUNCH_IF_NOT_ALREADY_LAUNCHED}},
  {command: "D018" + // Length
            "8103011502" + // Command details
            "82028182" + // Device identities
            "3100" + // URL
            "050B44656661756C742055524C", // Alpha identifier
   expect: {commandQualifier: 0x02,
            url: "",
            mode: MozIccManager.STK_BROWSER_MODE_USING_EXISTING_BROWSER,
            confirmMessage: { text: "Default URL" }}},
  {command: "D018" + // Length
            "8103011503" + // Command details
            "82028182" + // Device identities
            "3100" + // URL
            "050B44656661756C742055524C", // Alpha identifier
   expect: {commandQualifier: 0x03,
            url: "",
            mode: MozIccManager.STK_BROWSER_MODE_USING_NEW_BROWSER,
            confirmMessage: { text: "Default URL"}}},
  {command: "D026" + // Length
            "8103011502" + // Command details
            "82028182" + // Device identities
            "3100" + // URL
            "051980041704140420041004120421042204120423" + // Alpha identifier
            "041904220415",
   expect: {commandQualifier: 0x02,
            url: "",
            mode: MozIccManager.STK_BROWSER_MODE_USING_EXISTING_BROWSER,
            confirmMessage: { text: "ЗДРАВСТВУЙТЕ" }}},
  {command: "D021" + // Length
            "8103011502" + // Command details
            "82028182" + // Device identities
            "3100" + // URL
            "05104E6F742073656C66206578706C616E2E" + // Alpha identifier
            "1E020101", // Icon identifier
   expect: {commandQualifier: 0x02,
            url: "",
            mode: MozIccManager.STK_BROWSER_MODE_USING_EXISTING_BROWSER,
            confirmMessage: { text: "Not self explan.",
                              iconSelfExplanatory: false,
                              icons : [BASIC_ICON] }
            }},
  {command: "D012" + // Length
            "8103011502" + // Command details
            "82028182" + // Device identities
            "3100" + // URL
            "0505804F60597D", // Alpha identifier
   expect: {commandQualifier: 0x02,
            url: "",
            mode: MozIccManager.STK_BROWSER_MODE_USING_EXISTING_BROWSER,
            confirmMessage: { text: "你好" }}},
  {command: "D00F" + // Length
            "8103011500" + // Command details
            "82028182" + // Device identities
            "3100" + // URL
            "1E020001", // Icon identifier
   expect: {commandQualifier: 0x00,
            url: "",
            mode: MozIccManager.STK_BROWSER_MODE_LAUNCH_IF_NOT_ALREADY_LAUNCHED,
            confirmMessage: { iconSelfExplanatory: true,
                              icons: [BASIC_ICON] }}},
  {command: "D00F" + // Length
            "8103011500" + // Command details
            "82028182" + // Device identities
            "3100" + // URL
            "1E020003", // Icon identifier
   expect: {commandQualifier: 0x00,
            url: "",
            mode: MozIccManager.STK_BROWSER_MODE_LAUNCH_IF_NOT_ALREADY_LAUNCHED,
            confirmMessage: { iconSelfExplanatory: true,
                              icons: [COLOR_ICON] }}},
];

function testLaunchBrowser(aCommand, aExpect) {
  is(aCommand.commandNumber, 0x01, "commandNumber");
  is(aCommand.typeOfCommand, MozIccManager.STK_CMD_LAUNCH_BROWSER,
     "typeOfCommand");
  is(aCommand.commandQualifier, aExpect.commandQualifier, "commandQualifier");

  is(aCommand.options.url, aExpect.url, "options.url");
  is(aCommand.options.mode, aExpect.mode, "options.mode");

  // confirmMessage is optional
  if ("confirmMessage" in aExpect) {
    isStkText(aCommand.options.confirmMessage, aExpect.confirmMessage,
              "options.confirmMessage");
  }
}

// Start tests
startTestCommon(function() {
  let icc = getMozIcc();
  let promise = Promise.resolve();
  for (let i = 0; i < TEST_DATA.length; i++) {
    let data = TEST_DATA[i];
    promise = promise.then(() => {
      log("launch_browser_cmd: " + data.command);

      let promises = [];
      // Wait onstkcommand event.
      promises.push(waitForTargetEvent(icc, "stkcommand")
        .then((aEvent) => testLaunchBrowser(aEvent.command, data.expect)));
      // Wait icc-stkcommand system message.
      promises.push(waitForSystemMessage("icc-stkcommand")
        .then((aMessage) => {
          is(aMessage.iccId, icc.iccInfo.iccid, "iccId");
          testLaunchBrowser(aMessage.command, data.expect);
        }));
      // Send emulator command to generate stk unsolicited event.
      promises.push(sendEmulatorStkPdu(data.command));

      return Promise.all(promises);
    });
  }
  return promise;
});
