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

function getSmscAddress() {
  return new Promise((resolve, reject) => {
    let req = manager.getSmscAddress();
    if (!req) {
      reject("manager.getSmscAddress() returns null.");
    }

    req.onsuccess = function() {
      resolve(this.result);
    };

    req.onerror = function() {
      reject(this.error);
    };
  });
};

startTestBase(function testCaseMain() {
  return ensureMobileMessage()

  // Verify setting AT&T SMSC address.
  .then(() => manager.setSmscAddress({ address:SMSC_ATT }))
  .then(() => getSmscAddress())
  .then((result) => is(result, SMSC_ATT_TEXT))

  // Verify setting O2 SMSC address.
  .then(() => manager.setSmscAddress({ address:SMSC_O2 }))
  .then(() => getSmscAddress())
  .then((result) => is(result, SMSC_O2_TEXT))

  // Verify setting AT&T SMSC address with extra illegal characters.
  .then(() => manager.setSmscAddress({ address:SMSC_ATT_TYPO }))
  .then(() => getSmscAddress())
  .then((result) => is(result, SMSC_ATT_TEXT))

  // Verify setting a SMSC address with TON=unknown.
  .then(() => manager.setSmscAddress({ address:SMSC_TON_UNKNOWN }))
  .then(() => getSmscAddress())
  .then((result) => is(result, SMSC_TON_UNKNOWN_TEXT))

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
  .then(() => getSmscAddress())
  .then((result) => is(result, SMSC_DEF_TEXT));
});
