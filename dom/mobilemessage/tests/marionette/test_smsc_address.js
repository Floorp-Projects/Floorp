/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

// Expected SMSC addresses of emulator
const SMSC = "+123456789";
const TON = "international";
const NPI = "isdn";

function verifySmscAddress(smsc, expectedAddr, expectedTon, expectedNpi) {
  is(smsc.address, expectedAddr);
  is(smsc.typeOfAddress.typeOfNumber, expectedTon);
  is(smsc.typeOfAddress.numberPlanIdentification, expectedNpi);
}

startTestCommon(function testCaseMain() {
  return Promise.resolve()
    .then(() => manager.getSmscAddress())
    .then((result) => verifySmscAddress(result, SMSC, TON, NPI));
});
