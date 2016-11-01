/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

const TEST_DATA = [
  {command: "D01A" + // Length
            "8103012101" + // Command details
            "82028102" + // Device identities
            "8D0F04546F6F6C6B697420546573742031", // Text string
   expect: {commandQualifier: 0x01,
            text: "Toolkit Test 1",
            responseNeeded: false}},
  {command: "D01C" + // Length
            "8103012181" + // Command details
            "82028102" + // Device identities
            "8D0F04546F6F6C6B697420546573742032" + // Text string
            "2B00", // Immediate response
   expect: {commandQualifier: 0x81,
            text: "Toolkit Test 2",
            responseNeeded: true}},
  {command: "D081AD" + // Length
            "8103012180" + // Command details
            "82028102" + // Device identities
            "8D81A1045468697320636F6D6D616E6420696E73747275" + // Text string
            "63747320746865204D4520746F20646973706C61792061" +
            "2074657874206D6573736167652E20497420616C6C6F77" +
            "73207468652053494D20746F20646566696E6520746865" +
            "207072696F72697479206F662074686174206D65737361" +
            "67652C20616E6420746865207465787420737472696E67" +
            "20666F726D61742E2054776F207479706573206F662070" +
            "72696F",
   expect: {commandQualifier: 0x80,
            text: "This command instructs the ME to display a text message. " +
                  "It allows the SIM to define the priority of that message," +
                  " and the text string format. Two types of prio",
            responseNeeded: false}},
  {command: "D01A" + // Length
            "8103012180" + // Command details
            "82028102" + // Device identities
            "8D0F043C474F2D4241434B57415244533E", // Text string
   expect: {commandQualifier: 0x80,
            text: "<GO-BACKWARDS>",
            responseNeeded: false}},
  {command: "D024" + // Length
            "8103012180" + // Command details
            "82028102" + // Device identities
            "8D19080417041404200410041204210422041204230419" + // Text string
            "04220415",
   expect: {commandQualifier: 0x80,
            text: "ЗДРАВСТВУЙТЕ",
            responseNeeded: false}},
  {command: "D010" + // Length
            "8103012180" + // Command details
            "82028102" + // Device identities
            "8D05084F60597D", // Text string
   expect: {commandQualifier: 0x80,
            text: "你好",
            responseNeeded: false}},
  {command: "D028" + // Length
            "8103012180" + // Command details
            "82028102" + // Device identities
            "0D1D00D3309BFC06C95C301AA8E80259C3EC34B9AC07C9" + // Text string
            "602F58ED159BB940",
   expect: {commandQualifier: 0x80,
            text: "Saldo 2.04 E. Validez 20/05/13. ",
            responseNeeded: false}},
  {command: "D019" + // Length
            "8103012180" + // Command details
            "82028102" + // Device identities
            "8D0A043130205365636F6E64" + // Text string
            "8402010A", // Duration
   expect: {commandQualifier: 0x80,
            text: "10 Second",
            responseNeeded: false,
            duration: {timeUnit: MozIccManager.STK_TIME_UNIT_SECOND,
                       timeInterval: 0x0A}}},
  {command: "D01A" + // Length
            "8103012180" + // Command details
            "82028102" + // Device identities
            "8D0B0442617369632049636F6E" + // Text string
            "9E020001", // Icon identifier
   expect: {commandQualifier: 0x80,
            text: "Basic Icon",
            responseNeeded: false,
            iconSelfExplanatory: true,
            icons: [BASIC_ICON]}},
  {command: "D026" + // Length
            "8103012100" + // Command details
            "82028102" + // Device identities
            "8D1B" + // Text string
            "00" + // 7BIT
            "D4F79BBD4ED341D4F29C0E3A4A9F55A80E8687C158A09B304905",
   expect: {commandQualifier: 0x00,
            text: "Toolkit Test GROUP:0x00, 7BIT",
            responseNeeded: false}},
  {command: "D029" + // Length
            "8103012100" + // Command details
            "82028102" + // Device identities
            "8D1E" + // Text string
            "04" + // 8BIT
            "546F6F6C6B697420546573742047524F55503A307830302C2038424954",
   expect: {commandQualifier: 0x00,
            text: "Toolkit Test GROUP:0x00, 8BIT",
            responseNeeded: false}},
  {command: "D046" + // Length
            "8103012100" + // Command details
            "82028102" + // Device identities
            "8D3B" + // Text string
            "08" + // UCS2
            "0054006F006F006C006B0069007400200054006500730074002000470052004F" +
            "00550050003A0030007800300030002C00200055004300530032",
   expect: {commandQualifier: 0x00,
            text: "Toolkit Test GROUP:0x00, UCS2",
            responseNeeded: false}},
  {command: "D026" + // Length
            "8103012100" + // Command details
            "82028102" + // Device identities
            "8D1B" + // Text string
            "12" + // 7BIT + Class 2
            "D4F79BBD4ED341D4F29C0E3A4A9F55A80E868FC158A09B304905",
   expect: {commandQualifier: 0x00,
            text: "Toolkit Test GROUP:0x10, 7BIT",
            responseNeeded: false}},
  {command: "D029" + // Length
            "8103012100" + // Command details
            "82028102" + // Device identities
            "8D1E" + // Text string
            "16" + // 8BIT + Class 2
            "546F6F6C6B697420546573742047524F55503A307831302C2038424954",
   expect: {commandQualifier: 0x00,
            text: "Toolkit Test GROUP:0x10, 8BIT",
            responseNeeded: false}},
  {command: "D046" + // Length
            "8103012100" + // Command details
            "82028102" + // Device identities
            "8D3B" + // Text string
            "1A" + // UCS2 + Class 2
            "0054006F006F006C006B0069007400200054006500730074002000470052004F" +
            "00550050003A0030007800310030002C00200055004300530032",
   expect: {commandQualifier: 0x00,
            text: "Toolkit Test GROUP:0x10, UCS2",
            responseNeeded: false}},
  {command: "D026" + // Length
            "8103012100" + // Command details
            "82028102" + // Device identities
            "8D1B" + // Text string
            "F2" + // 7BIT + Class 2
            "D4F79BBD4ED341D4F29C0E3A4A9F55A80E8637C258A09B304905",
   expect: {commandQualifier: 0x00,
            text: "Toolkit Test GROUP:0xF0, 7BIT",
            responseNeeded: false}},
  {command: "D029"  + // Length
            "8103012100" + // Command details
            "82028102" + // Device identities
            "8D1E" + // Text string
            "F6" + // 8BIT + Class 2
            "546F6F6C6B697420546573742047524F55503A307846302C2038424954",
   expect: {commandQualifier: 0x00,
            text: "Toolkit Test GROUP:0xF0, 8BIT",
            responseNeeded: false}},
  // Bug 1088573: this test case is to ensure that we provide |length| argument
  // in |integer| format to GsmPDUHelper.readSeptetsToString().
  {command: "D081FC" + // Length (2-byte length encoded: 252)
            "8103012100" + // Command details
            "82028102" + // Device identities
            "8D" + // Text string
            "81F0" + // 2-byte length encoded: 240
            "00" + // 7BIT
            "C332A85D9ECFC3E732685E068DDF6DF87B5E0691CB20D96D061A87E5E131BD2C" +
            "2FCF416537A8FD269741E3771B2E2FCFE76517685806B5CBF379F85C0695E774" +
            "50D86C4E8FD165D0BC2E07C1D9F579BA5C97CF41E5B13CEC9E83CA7490BB0C22" +
            "BFD374103C3C0795E9F232882E7FBBE3F5B20B24BBCD40E5391DC42E83DCEFB6" +
            "585E06B5C3F874BBDE0691CBA071581E1ED3CBF2F21C14369BD3637458CC2EBB" +
            "40C3329D5E0699DFEE313DFD76BBC3EC34BD0C0A83CAF432280C87CBDF757BB9" +
            "0C8287E5207619346D1E73A0783D0D9A9FCA733A885C96BFEBEC32280C9A6689" +
            "CE621654768382D529551A64268B2E",
   expect: {commandQualifier: 0x00,
            text: "Ce message se compose de 273 caracteres en mode " +
                  "compresse. Ce message est affiche sur plusieurs " +
                  "ecrans et ne doit pas etre tronque. 273 est le " +
                  "nombre maximum de caracteres affichable. Cette " +
                  "fonctionnalite a ete approuvee par le SMG9 qui s'est " +
                  "deroule a SYDNEY en AUSTRALIE.",
            responseNeeded: false}},
];

function testDisplayText(aCommand, aExpect) {
  is(aCommand.commandNumber, 0x01, "commandNumber");
  is(aCommand.typeOfCommand, MozIccManager.STK_CMD_DISPLAY_TEXT,
     "typeOfCommand");
  is(aCommand.commandQualifier, aExpect.commandQualifier, "commandQualifier");

  is(aCommand.options.userClear, !!(aExpect.commandQualifier & 0x80),
     "options.userClear");
  is(aCommand.options.isHighPriority, !!(aExpect.commandQualifier & 0x01),
     "options.isHighPriority");
  is(aCommand.options.text, aExpect.text, "options.text");
  is(aCommand.options.responseNeeded, aExpect.responseNeeded,
     "options.responseNeeded");

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
      log("display_text_cmd: " + data.command);

      let promises = [];
      // Wait onstkcommand event.
      promises.push(waitForTargetEvent(icc, "stkcommand")
        .then((aEvent) => testDisplayText(aEvent.command, data.expect)));
      // Wait icc-stkcommand system message.
      promises.push(waitForSystemMessage("icc-stkcommand")
        .then((aMessage) => {
          is(aMessage.iccId, icc.iccInfo.iccid, "iccId");
          testDisplayText(aMessage.command, data.expect);
        }));
      // Send emulator command to generate stk unsolicited event.
      promises.push(sendEmulatorStkPdu(data.command));

      return Promise.all(promises);
    });
  }
  return promise;
});
