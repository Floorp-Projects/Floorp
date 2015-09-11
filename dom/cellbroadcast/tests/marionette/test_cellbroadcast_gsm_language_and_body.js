/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 90000;
MARIONETTE_HEAD_JS = 'head.js';

function testReceiving_GSM_Language_and_Body() {
  log("Test receiving GSM Cell Broadcast - Language & Body");

  let promise = Promise.resolve();

  let testDcs = [];
  let dcs = 0;
  while (dcs <= 0xFF) {
    try {
      let dcsInfo = { dcs: dcs };
      [ dcsInfo.encoding, dcsInfo.language,
        dcsInfo.indicator, dcsInfo.messageClass ] = decodeGsmDataCodingScheme(dcs);
      testDcs.push(dcsInfo);
    } catch (e) {
      // Unsupported coding group, skip.
      dcs = (dcs & PDU_DCS_CODING_GROUP_BITS) + 0x10;
    }
    dcs++;
  }

  let verifyCBMessage = (aMessage, aDcsInfo) => {
    if (aDcsInfo.language) {
      is(aMessage.language, aDcsInfo.language, "aMessage.language");
    } else if (aDcsInfo.indicator) {
      is(aMessage.language, "@@", "aMessage.language");
    } else {
      ok(aMessage.language == null, "aMessage.language");
    }

    switch (aDcsInfo.encoding) {
      case PDU_DCS_MSG_CODING_7BITS_ALPHABET:
        is(aMessage.body, aDcsInfo.indicator ? DUMMY_BODY_7BITS_IND : DUMMY_BODY_7BITS, "aMessage.body");
        break;
      case PDU_DCS_MSG_CODING_8BITS_ALPHABET:
        ok(aMessage.body == null, "aMessage.body");
        break;
      case PDU_DCS_MSG_CODING_16BITS_ALPHABET:
        is(aMessage.body, aDcsInfo.indicator ? DUMMY_BODY_UCS2_IND : DUMMY_BODY_UCS2, "aMessage.body");
        break;
    }

    is(aMessage.messageClass, aDcsInfo.messageClass, "aMessage.messageClass");
  };

  ok(testDcs.length, "testDcs.length");
  testDcs.forEach(function(aDcsInfo) {
    let pdu = buildHexStr(0, 8)
            + buildHexStr(aDcsInfo.dcs, 2)
            + buildHexStr(0, (CB_MESSAGE_SIZE_GSM - 5) * 2);
    promise = promise
      .then(() => sendMultipleRawCbsToEmulatorAndWait([pdu]))
      .then((aMessage) => verifyCBMessage(aMessage, aDcsInfo));
  });

  return promise;
}

startTestCommon(() => testReceiving_GSM_Language_and_Body());
