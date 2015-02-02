/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

const TEST_DATA = [
  {command: "d050810301120082028183850a372d62697420555353448a39f041e19058341e9149e592d9743ea151e9945ab55eb1596d2b2c1e93cbe6333aad5eb3dbee373c2e9fd3ebf63b3eaf6fc564335acd76c3e560",
   expect: {commandQualifier: 0x00,
            text: "7-bit USSD"}},
  {command: "d0448103011200820281838a39f041e19058341e9149e592d9743ea151e9945ab55eb1596d2b2c1e93cbe6333aad5eb3dbee373c2e9fd3ebf63b3eaf6fc564335acd76c3e560",
   expect: {commandQualifier: 0x00}},
  {command: "d058810301120082028183850a382d62697420555353448a41444142434445464748494a4b4c4d4e4f505152535455565758595a2d6162636465666768696a6b6c6d6e6f707172737475767778797a2d31323334353637383930",
   expect: {commandQualifier: 0x00,
            text: "8-bit USSD"}},
  {command: "d02f81030112008202818385095543533220555353448a1948041704140420041004120421042204120423041904220415",
   expect: {commandQualifier: 0x00,
            text: "UCS2 USSD"}},
  {command: "d081fd8103011200820281838581b66f6e636520612052454c4541534520434f4d504c455445206d65737361676520636f6e7461696e696e672074686520555353442052657475726e20526573756c74206d657373616765206e6f7420636f6e7461696e696e6720616e206572726f7220686173206265656e2072656365697665642066726f6d20746865206e6574776f726b2c20746865204d45207368616c6c20696e666f726d207468652053494d20746861742074686520636f6d6d616e64206861738a39f041e19058341e9149e592d9743ea151e9945ab55eb1596d2b2c1e93cbe6333aad5eb3dbee373c2e9fd3ebf63b3eaf6fc564335acd76c3e560",
   expect: {commandQualifier: 0x00,
            text: "once a RELEASE COMPLETE message containing the USSD Return Result message not containing an error has been received from the network, the ME shall inform the SIM that the command has"}},
  {command: "d04681030112008202818385008a39f041e19058341e9149e592d9743ea151e9945ab55eb1596d2b2c1e93cbe6333aad5eb3dbee373c2e9fd3ebf63b3eaf6fc564335acd76c3e560",
   expect: {commandQualifier: 0x00,
            text: ""}},
  {command: "d054810301120082028183850a42617369632049636f6e8a39f041e19058341e9149e592d9743ea151e9945ab55eb1596d2b2c1e93cbe6333aad5eb3dbee373c2e9fd3ebf63b3eaf6fc564335acd76c3e5609e020001",
   expect: {commandQualifier: 0x00,
            text: "Basic Icon",
            iconSelfExplanatory: true,
            icons: [BASIC_ICON]}},
  {command: "d0488103011200820281838a39f041e19058341e9149e592d9743ea151e9945ab55eb1596d2b2c1e93cbe6333aad5eb3dbee373c2e9fd3ebf63b3eaf6fc564335acd76c3e5609e020003",
   expect: {commandQualifier: 0x00,
            iconSelfExplanatory: true,
            icons: [COLOR_ICON]}},
  {command: "d05f8103011200820281838519800417041404200410041204210422041204230419042204158a39f041e19058341e9149e592d9743ea151e9945ab55eb1596d2b2c1e93cbe6333aad5eb3dbee373c2e9fd3ebf63b3eaf6fc564335acd76c3e560",
   expect: {commandQualifier: 0x00,
            text: "ЗДРАВСТВУЙТЕ"}},
  {command: "d05c8103011200820281838510546578742041747472696275746520318a39f041e19058341e9149e592d9743ea151e9945ab55eb1596d2b2c1e93cbe6333aad5eb3dbee373c2e9fd3ebf63b3eaf6fc564335acd76c3e560d004001000b4",
   expect: {commandQualifier: 0x00,
            text: "Text Attribute 1"}},
  {command: "d04a8103011200820281838a39f041e19058341e9149e592d9743ea151e9945ab55eb1596d2b2c1e93cbe6333aad5eb3dbee373c2e9fd3ebf63b3eaf6fc564335acd76c3e560d004001000b4",
   expect: {commandQualifier: 0x00}},
  {command: "d04b8103011200820281838505804f60597d8a39f041e19058341e9149e592d9743ea151e9945ab55eb1596d2b2c1e93cbe6333aad5eb3dbee373c2e9fd3ebf63b3eaf6fc564335acd76c3e560",
   expect: {commandQualifier: 0x00,
            text: "你好"}},
];

function testSendUSSD(aCommand, aExpect) {
  is(aCommand.typeOfCommand, MozIccManager.STK_CMD_SEND_USSD, "typeOfCommand");
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
      log("send_ussd_cmd: " + data.command);

      let promises = [];
      // Wait onstkcommand event.
      promises.push(waitForTargetEvent(icc, "stkcommand")
        .then((aEvent) => testSendUSSD(aEvent.command, data.expect)));
      // Send emulator command to generate stk unsolicited event.
      promises.push(sendEmulatorStkPdu(data.command));

      return Promise.all(promises);
    });
  }
  return promise;
});
