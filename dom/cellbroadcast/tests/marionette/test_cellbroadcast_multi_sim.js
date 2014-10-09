/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

function testReceiving_MultiSIM() {
  log("Test receiving GSM Cell Broadcast - Multi-SIM");

  let pdu = buildHexStr(0, CB_MESSAGE_SIZE_GSM * 2);

  let verifyCBMessage = (aMessage, aServiceId) => {
    log("Verify CB message received from serviceId: " + aServiceId);
    is(aMessage.body, DUMMY_BODY_7BITS, "Checking message body.");
    is(aMessage.serviceId, aServiceId, "Checking serviceId.");
  };

  return selectModem(1)
    .then(() => sendMultipleRawCbsToEmulatorAndWait([pdu]))
    .then((aMessage) => verifyCBMessage(aMessage, 1))
    .then(() => selectModem(0))
    .then(() => sendMultipleRawCbsToEmulatorAndWait([pdu]))
    .then((aMessage) => verifyCBMessage(aMessage, 0));
}

startTestCommon(function testCaseMain() {
  return runIfMultiSIM(testReceiving_MultiSIM);
});
