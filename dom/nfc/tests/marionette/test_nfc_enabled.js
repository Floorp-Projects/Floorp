/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;
MARIONETTE_HEAD_JS = 'head.js';

function testEnableNFC() {
  log('Running \'testEnableNFC\'');
  let promise = nfc.startPoll();
  promise.then(() => {
    ok(true);
    runNextTest();
  }).catch(() => {
    ok(false, "startPoll failed");
    runNextTest();
  });
}

function testDisableNFC() {
  log('Running \'testDisableNFC\'');
  let promise = nfc.powerOff();
  promise.then(() => {
    ok(true);
    runNextTest();
  }).catch(() => {
    ok(false, "powerOff failed");
    runNextTest();
  });
}

function testStopPollNFC() {
  log('Running \'testStopPollNFC\'');
  let promise = nfc.stopPoll();
  promise.then(() => {
    ok(true);
    runNextTest();
  }).catch(() => {
    ok(false, "stopPoll failed");
    runNextTest();
  });
}

var tests = [
  testEnableNFC,
  testStopPollNFC,
  testDisableNFC
];

SpecialPowers.pushPermissions(
  [{'type': 'nfc-manager', 'allow': true, 'context': document}],
  runTests);
