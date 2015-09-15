/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

var url = "http://www.mozilla.org";

function testUrlTagDiscover(re) {
  log("Running \'testUrlTagDiscover\'");
  // TODO : Make flag value readable.
  let flag = 0xd0;
  let tnf = NDEF.TNF_WELL_KNOWN;
  let type = "U";
  let payload = url;

  sysMsgHelper.waitForTechDiscovered(function(msg) {
    log("Received \'nfc-manager-tech-ndiscovered\'");
    ok(msg.peer === undefined, "peer object should be undefined");

    let records = msg.records;
    ok(records.length > 0);

    is(tnf, records[0].tnf, "check for TNF field in NDEF");
    is(type, NfcUtils.toUTF8(Cu.waiveXrays(records[0].type)), "check for type field in NDEF");
    is(payload, NfcUtils.toUTF8(Cu.waiveXrays(records[0].payload)), "check for payload field in NDEF");

    deactivateAndWaitForTechLost().then(() => toggleNFC(false)).then(runNextTest);
  });

  toggleNFC(true)
  .then(() => TAG.setData(re, flag, tnf, btoa(type), btoa(payload)))
  .then(() => NCI.activateRE(re));
}

function testEmptyTagDiscover(re) {
  log("Running \'testEmptyTagDiscover\'");

  sysMsgHelper.waitForTechDiscovered(function(msg) {
    log("Received \'nfc-manager-tech-ndiscovered\'");
    ok(msg.peer === undefined, "peer object should be undefined");

    let records = msg.records;
    ok(records == null);

    deactivateAndWaitForTechLost().then(() => toggleNFC(false)).then(runNextTest);
  });

  toggleNFC(true)
  .then(() => TAG.clearData(re))
  .then(() => NCI.activateRE(re));
}

function testUrlT1TDiscover() {
  testUrlTagDiscover(emulator.T1T_RE_INDEX);
}

function testUrlT2TDiscover() {
  testUrlTagDiscover(emulator.T2T_RE_INDEX);
}

function testUrlT3TDiscover() {
  testUrlTagDiscover(emulator.T3T_RE_INDEX);
}

function testUrlT4TDiscover() {
  testUrlTagDiscover(emulator.T4T_RE_INDEX);
}

function testEmptyT1TDiscover() {
  testEmptyTagDiscover(emulator.T1T_RE_INDEX);
}

function testEmptyT2TDiscover() {
  testEmptyTagDiscover(emulator.T2T_RE_INDEX);
}

function testEmptyT3TDiscover() {
  testEmptyTagDiscover(emulator.T3T_RE_INDEX);
}

function testEmptyT4TDiscover() {
  testEmptyTagDiscover(emulator.T4T_RE_INDEX);
}

var tests = [
    testUrlT1TDiscover,
    testUrlT2TDiscover,
    testUrlT3TDiscover,
    testUrlT4TDiscover,
    testEmptyT1TDiscover,
    testEmptyT2TDiscover,
    testEmptyT3TDiscover,
    testEmptyT4TDiscover
];

SpecialPowers.pushPermissions(
  [{'type': 'nfc-manager', 'allow': true, context: document}], runTests);
