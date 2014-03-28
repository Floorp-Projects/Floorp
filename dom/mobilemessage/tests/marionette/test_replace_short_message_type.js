/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 120000;
MARIONETTE_HEAD_JS = 'head.js';

const PDU_SMSC_NONE = "00"; // no SMSC Address

const PDU_FIRST_OCTET = "00"; // RP:no, UDHI:no, SRI:no, MMS:no, MTI:SMS-DELIVER

const PDU_SENDER_0 = "0A912143658709"; // +1234567890
const PDU_SENDER_1 = "0A912143658719"; // +1234567891
const SENDER_0 = "+1234567890";
const SENDER_1 = "+1234567891";

const PDU_PID_NORMAL = "00";
const PDU_PIDS = ["00", "41", "42", "43", "44", "45", "46", "47", "5F"];

const PDU_DCS_NORMAL = "00";

const PDU_TIMESTAMP = "00101000000000"; // 2000/01/01

const PDU_UDL = "01";
const PDU_UD_A = "41"; // "A"
const PDU_UD_B = "42"; // "B"
const BODY_A = "A";
const BODY_B = "B";

function buildPdu(aSender, aPid, aBody) {
  return PDU_SMSC_NONE + PDU_FIRST_OCTET + aSender + aPid + PDU_DCS_NORMAL +
         PDU_TIMESTAMP + PDU_UDL + aBody;
}

function verifyReplacing(aVictim, aSender, aPid, aCompare) {
  let readableSender = aSender === PDU_SENDER_0 ? SENDER_0 : SENDER_1;
  log("  Checking ('" + readableSender + "', '" + aPid + "', '" + BODY_B + "')");

  let pdu = buildPdu(aSender, aPid, PDU_UD_B);
  ok(true, "Sending " + pdu);

  return sendMultipleRawSmsToEmulatorAndWait([pdu])
    .then(function(results) {
      let receivedMsg = results[0].message;
      is(receivedMsg.sender, readableSender, "SmsMessage sender");
      is(receivedMsg.body, BODY_B, "SmsMessage body");

      aCompare(receivedMsg.id, aVictim.id, "SmsMessage id");
    });
}

function verifyNotReplaced(aVictim, aSender, aPid) {
  return verifyReplacing(aVictim, aSender, aPid, isnot);
}

function verifyReplaced(aVictim, aSender, aPid) {
  return verifyReplacing(aVictim, aSender, aPid, is);
}

function testPid(aPid) {
  log("Test message PID '" + aPid + "'");

  return sendMultipleRawSmsToEmulatorAndWait([buildPdu(PDU_SENDER_0, aPid, PDU_UD_A)])
    .then(function(results) {
      let receivedMsg = results[0].message;
      let promise = Promise.resolve();

      for (let pid of PDU_PIDS) {
        let verify = (aPid !== PDU_PID_NORMAL && pid === aPid)
                   ? verifyReplaced : verifyNotReplaced;
        promise =
          promise.then(verify.bind(null, receivedMsg, PDU_SENDER_0, pid))
                 .then(verifyNotReplaced.bind(null, receivedMsg,
                                              PDU_SENDER_1, pid));
      }

      return promise;
    });
}

startTestCommon(function testCaseMain() {
  let promise = Promise.resolve();
  for (let pid of PDU_PIDS) {
    promise = promise.then(testPid.bind(null, pid))
                     .then(deleteAllMessages);
  }

  return promise;
});
