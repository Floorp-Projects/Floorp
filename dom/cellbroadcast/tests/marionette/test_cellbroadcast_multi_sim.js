/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

const BODY_7BITS = "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
                 + "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
                 + "@@@@@@@@@@@@@"; // 93 ascii chars.
const CB_PDU_SIZE = 88;

function testReceivingMultiSIM() {
  let CB_PDU = "";
  while (CB_PDU.length < CB_PDU_SIZE * 2) {
    CB_PDU += "00";
  }

  let verifyCBMessage = (aMessage, aServiceId) => {
    log("Verify CB message received from serviceId: " + aServiceId);
    is(aMessage.body, BODY_7BITS, "Checking message body.");
    is(aMessage.serviceId, aServiceId, "Checking serviceId.");
  };

  return selectModem(1)
    .then(() => sendMultipleRawCbsToEmulatorAndWait([CB_PDU]))
    .then((results) => verifyCBMessage(results[0].message, 1))
    .then(() => selectModem(0))
    .then(() => sendMultipleRawCbsToEmulatorAndWait([CB_PDU]))
    .then((results) => verifyCBMessage(results[0].message, 0));
}

startTestCommon(function testCaseMain() {
  return runIfMultiSIM(testReceivingMultiSIM);
});
