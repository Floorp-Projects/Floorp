/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

const PDU_SMSC_NONE = "00"; // no SMSC Address

const PDU_FIRST_OCTET = "40"; // RP:no, UDHI:yes, SRI:no, MMS:no, MTI:SMS-DELIVER

const PDU_SENDER = "0A912143658709"; // +1234567890
const SENDER = "+1234567890";

const PDU_PID_NORMAL = "00";

const PDU_DCS_NORMAL_UCS2 = "08";
const PDU_DCS_CLASS0_UCS2 = "18";
const PDU_DCS_NORMAL_8BIT = "04";
const PDU_DCS_CLASS0_8BIT = "14";

const PDU_TIMESTAMP = "00101000000000"; // 2000/01/01

function byteValueToHexString(aValue) {
  let str = Number(aValue).toString(16).toUpperCase();
  return str.length == 1 ? "0" + str : str;
}

let ref_num = 0;
function buildTextPdus(aDcs) {
  ref_num++;

  let IEI_CONCATE_1 = "0003" + byteValueToHexString(ref_num) + "0301";
  let IEI_CONCATE_2 = "0003" + byteValueToHexString(ref_num) + "0302";
  let IEI_CONCATE_3 = "0003" + byteValueToHexString(ref_num) + "0303";
  let PDU_UDL = "08"; // UDHL(1) + UDH(5) + UCS2 Char (2)
  let PDU_UDHL = "05";

  let PDU_UD_A = "0041"; // "A"
  let PDU_UD_B = "0042"; // "B"
  let PDU_UD_C = "0043"; // "C"

  let PDU_COMMON = PDU_SMSC_NONE + PDU_FIRST_OCTET + PDU_SENDER +
    PDU_PID_NORMAL + aDcs + PDU_TIMESTAMP + PDU_UDL + PDU_UDHL;

  return [
    PDU_COMMON + IEI_CONCATE_1 + PDU_UD_A,
    PDU_COMMON + IEI_CONCATE_2 + PDU_UD_B,
    PDU_COMMON + IEI_CONCATE_3 + PDU_UD_C
  ];
}

function buildBinaryPdus(aDcs) {
  ref_num++;
  let IEI_PORT = "05040B8423F0";

  let PDU_DATA1 = "C106316170706C69636174696F6E2F76" +
                  "6E642E7761702E6D6D732D6D65737361" +
                  "676500B131302E382E3133302E313800" +
                  "AF84B4818C82986B4430595538595347" +
                  "77464E446741416B4876736C58303141" +
                  "41414141414141008D90890380310096" +
                  "05EA4D4D53008A808E02024188058103" +
                  "015F9083687474703A2F2F6D6D732E65";

  let PDU_DATA2 = "6D6F6D652E6E65743A383030322F6B44" +
                  "3059553859534777464E446741416B48" +
                  "76736C583031414141414141414100";

  let PDU_COMMON = PDU_SMSC_NONE + PDU_FIRST_OCTET + PDU_SENDER +
    PDU_PID_NORMAL + aDcs + PDU_TIMESTAMP;

  function construstBinaryUserData(aBinaryData, aSeqNum) {
    let ieiConcat = "0003" + byteValueToHexString(ref_num) + "02" +
                    byteValueToHexString(aSeqNum);

    let udh = IEI_PORT + ieiConcat;
    let udhl = byteValueToHexString(udh.length / 2);
    let ud = udhl + udh + aBinaryData;
    let udl = byteValueToHexString(ud.length / 2);

    return udl + ud;
  }

  return [
    PDU_COMMON + construstBinaryUserData(PDU_DATA1, 1),
    PDU_COMMON + construstBinaryUserData(PDU_DATA2, 2)
  ];
}

function verifyTextMessage(aMessage, aMessageClass) {
  is(aMessage.messageClass, aMessageClass, "SmsMessage class");
  is(aMessage.sender, SENDER, "SmsMessage sender");
  is(aMessage.body, "ABC", "SmsMessage body");
}

function verifyBinaryMessage(aMessage) {
  is(aMessage.type, "mms", "MmsMessage type");
  is(aMessage.delivery, "not-downloaded", "MmsMessage delivery");

  // remove duplicated M-Notification.ind for next test.
  return deleteMessagesById([aMessage.id]);
}

function testText(aDcs, aClass) {
  log("testText(): aDcs = " + aDcs + ", aClass = " + aClass);
  return sendMultipleRawSmsToEmulatorAndWait(buildTextPdus(aDcs))
    .then((results) => verifyTextMessage(results[0].message, aClass));
}

function testBinary(aDcs) {
  log("testBinary(): aDcs = " + aDcs);
  return sendMultipleRawSmsToEmulatorAndWait(buildBinaryPdus(aDcs))
    .then((results) => verifyBinaryMessage(results[0].message));
}

SpecialPowers.pushPrefEnv(
  {"set": [["dom.mms.retrieval_mode", "manual"]]},
  function startTest() {
    startTestCommon(function testCaseMain() {
      return Promise.resolve()
        .then(() => testText(PDU_DCS_NORMAL_UCS2, "normal"))
        .then(() => testText(PDU_DCS_CLASS0_UCS2, "class-0"))
        .then(() => testBinary(PDU_DCS_NORMAL_8BIT))
        .then(() => testBinary(PDU_DCS_CLASS0_8BIT));
    });
  }
);
