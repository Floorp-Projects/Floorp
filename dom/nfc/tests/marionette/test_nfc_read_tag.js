/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;
MARIONETTE_HEAD_JS = "head.js";

let url = "http://www.mozilla.org";

const T1T_RE_INDEX   = 2;
const T2T_RE_INDEX   = 3;
const T3T_RE_INDEX   = 4;
const T4T_RE_INDEX   = 5;

function testUrlTagDiscover(re) {
  log("Running \'testUrlTagDiscover\'");
  // TODO : Make flag value readable.
  let flag = 0xd0;
  let tnf = NDEF.TNF_WELL_KNOWN;
  let type = "U";
  let payload = url;

  window.navigator.mozSetMessageHandler("nfc-manager-tech-discovered", function(msg) {
    log("Received \'nfc-manager-tech-ndiscovered\'");
    is(msg.type, "techDiscovered", "check for correct message type");
    let index = msg.techList.indexOf("NDEF");
    isnot(index, -1, "check for \'NDEF\' in tech list");

    let records = Cu.waiveXrays(msg.records);
    ok(records.length > 0);

    is(tnf, records[0].tnf, "check for TNF field in NDEF");
    is(type, NfcUtils.toUTF8(records[0].type), "check for type field in NDEF");
    is(payload, NfcUtils.toUTF8(records[0].payload), "check for payload field in NDEF");

    toggleNFC(false).then(runNextTest);
  });

  toggleNFC(true)
  .then(() => emulator.setTagData(re, flag, tnf, btoa(type), btoa(payload)))
  .then(() => emulator.activateRE(re));
}

function testEmptyTagDiscover(re) {
  log("Running \'testEmptyTagDiscover\'");

  window.navigator.mozSetMessageHandler("nfc-manager-tech-discovered", function(msg) {
    log("Received \'nfc-manager-tech-ndiscovered\'");
    is(msg.type, "techDiscovered", "check for correct message type");
    let index = msg.techList.indexOf("NDEF");
    isnot(index, -1, "check for \'NDEF\' in tech list");

    let records = msg.records;
    ok(records == null);

    toggleNFC(false).then(runNextTest);
  });

  toggleNFC(true)
  .then(() => emulator.clearTagData(re))
  .then(() => emulator.activateRE(re));
}

function testUrlT1TDiscover() {
  testUrlTagDiscover(T1T_RE_INDEX);
}

function testUrlT2TDiscover() {
  testUrlTagDiscover(T2T_RE_INDEX);
}

function testUrlT3TDiscover() {
  testUrlTagDiscover(T3T_RE_INDEX);
}

function testUrlT4TDiscover() {
  testUrlTagDiscover(T4T_RE_INDEX);
}

function testEmptyT1TDiscover() {
  testEmptyTagDiscover(T1T_RE_INDEX);
}

function testEmptyT2TDiscover() {
  testEmptyTagDiscover(T2T_RE_INDEX);
}

function testEmptyT3TDiscover() {
  testEmptyTagDiscover(T3T_RE_INDEX);
}

function testEmptyT4TDiscover() {
  testEmptyTagDiscover(T4T_RE_INDEX);
}

let tests = [
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
