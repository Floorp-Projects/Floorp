/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;
MARIONETTE_HEAD_JS = 'head.js';

function testEnableNFC() {
  log('Running \'testEnableNFC\'');
  let req = nfc.startPoll();
  req.onsuccess = function () {
    ok(true);
    runNextTest();
  };
  req.onerror = function () {
    ok(false, "startPoll failed");
    runNextTest();
  };
}

function testDisableNFC() {
  log('Running \'testDisableNFC\'');
  let req = nfc.powerOff();
  req.onsuccess = function () {
    ok(true);
    runNextTest();
  };
  req.onerror = function () {
    ok(false, "powerOff failed");
    runNextTest();
  };
}

function testStopPollNFC() {
  log('Running \'testStopPollNFC\'');
  let req = nfc.stopPoll();
  req.onsuccess = function () {
    ok(true);
    runNextTest();
  };
  req.onerror = function () {
    ok(false, "stopPoll failed");
    runNextTest();
  };
}

let tests = [
  testEnableNFC,
  testStopPollNFC,
  testDisableNFC
];

SpecialPowers.pushPermissions(
  [{'type': 'nfc-manager', 'allow': true, 'context': document}],
  runTests);
