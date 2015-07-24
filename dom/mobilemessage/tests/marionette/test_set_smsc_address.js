/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

const SMSC_ATT = '+13123149810';
const SMSC_ATT_TYPO = '+++1312@@@314$$$9,8,1,0';
const SMSC_ATT_TEXT = '"+13123149810",145';
const SMSC_O2 = '+447802000332';
const SMSC_O2_TEXT = '"+447802000332",145';
const SMSC_DEF = '+123456789';
const SMSC_DEF_TEXT = '"+123456789",145';
const SMSC_TON_UNKNOWN = '0407485455'
const SMSC_TON_UNKNOWN_TEXT = '"0407485455",129';

function verifySmscAddress(smsc, expectedAddr, expectedTon, expectedNpi) {
  is(smsc.address, expectedAddr);
  is(smsc.typeOfAddress.typeOfNumber, expectedTon);
  is(smsc.typeOfAddress.numberPlanIdentification, expectedNpi);
}

startTestCommon(function testCaseMain() {
  return Promise.resolve()

    // Verify setting AT&T SMSC address.
    .then(() => manager.setSmscAddress({ address:SMSC_ATT }))
    .then(() => manager.getSmscAddress())
    .then((result) =>
      verifySmscAddress(result, SMSC_ATT, "international", "isdn"))

    // Verify setting O2 SMSC address.
    .then(() => manager.setSmscAddress({ address:SMSC_O2 }))
    .then(() => manager.getSmscAddress())
    .then((result) =>
      verifySmscAddress(result, SMSC_O2, "international", "isdn"))

    // Verify setting AT&T SMSC address with extra illegal characters.
    .then(() => manager.setSmscAddress({ address:SMSC_ATT_TYPO }))
    .then(() => manager.getSmscAddress())
    .then((result) =>
      verifySmscAddress(result, SMSC_ATT, "international", "isdn"))

    // Verify setting a SMSC address with TON=unknown.
    .then(() => manager.setSmscAddress({ address:SMSC_TON_UNKNOWN }))
    .then(() => manager.getSmscAddress())
    .then((result) =>
      verifySmscAddress(result, SMSC_TON_UNKNOWN, "unknown", "isdn"))

    // Verify setting invalid SMSC address.
    .then(() => manager.setSmscAddress({}))
    .then(() => Promise.reject("Expect for an error."),
      (err) => log("Got expected error: " + err))
    .then(() => manager.setSmscAddress({ address:"" }))
    .then(() => Promise.reject("Expect for an error."),
      (err) => log("Got expected error: " + err))
    .then(() => manager.setSmscAddress({ address:"???" }))
    .then(() => Promise.reject("Expect for an error."),
      (err) => log("Got expected error: " + err))

    // Restore to default emulator SMSC address.
    .then(() => manager.setSmscAddress({ address:SMSC_DEF }))
    .then(() => manager.getSmscAddress())
    .then((result) =>
      verifySmscAddress(result, SMSC_DEF, "international", "isdn"));
});
