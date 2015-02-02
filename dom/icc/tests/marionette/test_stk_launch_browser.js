/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

const TEST_DATA = [
  {command: "d0188103011500820281823100050b44656661756c742055524c",
   expect: {commandQualifier: 0x00,
            url: "",
            confirmMessage: { text: "Default URL" }}},
  {command: "d01f8103011500820281823112687474703a2f2f7878782e7979792e7a7a7a0500",
   expect: {commandQualifier: 0x00,
            url: "http://xxx.yyy.zzz",
            confirmMessage: { text: "" }}},
  {command: "d0208103011500820281823100320103" +
            "0d10046162632e6465662e6768692e6a6b6c", // "0D" String TLV is useless for Launch Browser.
   expect: {commandQualifier: 0x00,
            url: ""}},
  {command: "d0188103011502820281823100050b44656661756c742055524c",
   expect: {commandQualifier: 0x02,
            url: "",
            confirmMessage: { text: "Default URL" }}},
  {command: "d0188103011503820281823100050b44656661756c742055524c",
   expect: {commandQualifier: 0x03,
            url: "",
            confirmMessage: { text: "Default URL"}}},
  {command: "d0268103011502820281823100051980041704140420041004120421042204120423041904220415",
   expect: {commandQualifier: 0x02,
            url: "",
            confirmMessage: { text: "ЗДРАВСТВУЙТЕ" }}},
  {command: "d021810301150282028182310005104e6f742073656c66206578706c616e2e1e020101",
   expect: {commandQualifier: 0x02,
            url: "",
            confirmMessage: { text: "Not self explan.",
                              iconSelfExplanatory: false,
                              icons : [BASIC_ICON] }
            }},
  {command: "d01281030115028202818231000505804f60597d",
   expect: {commandQualifier: 0x02,
            url: "",
            confirmMessage: { text: "你好" }}},
  {command: "d01281030115008202818230010031001e020001",
   expect: {commandQualifier: 0x00,
            url: "",
            confirmMessage: { iconSelfExplanatory: true,
                              icons: [BASIC_ICON] }}},
  {command: "d01281030115008202818230010031001e020003",
   expect: {commandQualifier: 0x00,
            url: "",
            confirmMessage: { iconSelfExplanatory: true,
                              icons: [COLOR_ICON] }}},
];

function testLaunchBrowser(aCommand, aExpect) {
  is(aCommand.typeOfCommand, MozIccManager.STK_CMD_LAUNCH_BROWSER,
     "typeOfCommand");
  is(aCommand.commandQualifier, aExpect.commandQualifier, "commandQualifier");
  is(aCommand.options.url, aExpect.url, "options.url");

  if (aExpect.confirmMessage) {
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
      // Send emulator command to generate stk unsolicited event.
      promises.push(sendEmulatorStkPdu(data.command));

      return Promise.all(promises);
    });
  }
  return promise;
});
