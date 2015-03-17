/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

const PDU_SMSC_NONE = "00"; // no SMSC Address

// |  TP-RP|TP-UDHI| TP-SRI|     Unused    | TP-MMS|     TP-MTI    |
// |   0   |   1   |   0   |   0   |   0   |   0   |   0   |   0   | => 0x40
const PDU_FIRST_OCTET = "40";

// |   | <= TON => | <=== NOI ===> |
// | 1 | 0 | 1 | 0 | 1 | 0 | 0 | 0 | => 0xa8
const PDU_OA = "0AA89021436587";  // 0912345678

const PDU_PID_NORMAL = "00";
const PDU_DCS_GSM_7BIT = "00";
const PDU_TIMESTAMP = "51302151740020"; // 2015/3/12 15:47:00 UTC+8

// ==> | <========== G ==========> |
// | 1 | 1 | 0 | 0 | 0 | 1 | 1 | 1 |  => 0xc7
// ======> | <========== S =========
// | 0 | 1 | 1 | 0 | 1 | 0 | 0 | 1 |  => 0x69
// |<=padding=>| <========== M =====
// | 0 | 0 | 0 | 1 | 0 | 0 | 1 | 1 |  => 0x13
const PDU_UD_GSM = "C76913";

const IE_USE_SPANISH_LOCKING_SHIFT_TABLE = "250102";
const IE_USE_SPANISH_SINGLE_SHIFT_TABLE = "240102";
const PDU_UDHL = "06";

const PDU_UDL = "0B"; // UDH occupies 7 octets = 8 septets, plus 3 septets data.

const PDU = PDU_SMSC_NONE + PDU_FIRST_OCTET + PDU_OA + PDU_PID_NORMAL
  + PDU_DCS_GSM_7BIT + PDU_TIMESTAMP + PDU_UDL + PDU_UDHL
  + IE_USE_SPANISH_LOCKING_SHIFT_TABLE + IE_USE_SPANISH_SINGLE_SHIFT_TABLE
  + PDU_UD_GSM;

function verifyMessage(aMessage) {
  is(aMessage.body, "GSM", "SmsMessage body");
}

/**
 * Test and verify that user data encoded in GSM default alphabet can be
 * correctly decoded with Spanish locking shift table. See bug 1138841.
 */
startTestCommon(function testCaseMain() {
  return Promise.resolve()
    .then(() => sendMultipleRawSmsToEmulatorAndWait([PDU]))
    .then(results => verifyMessage(results[0].message));
});
