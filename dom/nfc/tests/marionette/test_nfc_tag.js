/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;
MARIONETTE_HEAD_JS = "head.js";

let url = "http://www.mozilla.org";

// TODO : Get this from emulator console command.
const T2T_RE_INDEX = 2;

function activateRE(re) {
  let deferred = Promise.defer();
  let cmd = "nfc nci rf_intf_activated_ntf " + re;

  emulator.run(cmd, function(result) {
    is(result.pop(), "OK", "check activation of RE" + re);
    deferred.resolve();
  });

  return deferred.promise;
}

function setTagData(re, flag, tnf, type, payload) {
  let deferred = Promise.defer();
  let cmd = "nfc tag set " + re +
            " [" + flag + "," + tnf + "," + type + "," + payload + ",]";

  log("Executing \'" + cmd + "\'");
  emulator.run(cmd, function(result) {
    is(result.pop(), "OK", "set NDEF data of tag" + re);
    deferred.resolve();
  });

  return deferred.promise;
}

function testUrlTagDiscover() {
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

    let records = msg.records;
    ok(records.length > 0);

    is(tnf, records[0].tnf, "check for TNF field in NDEF");
    is(type, NfcUtils.toUTF8(records[0].type), "check for type field in NDEF");
    is(payload, NfcUtils.toUTF8(records[0].payload), "check for payload field in NDEF");

    toggleNFC(false).then(runNextTest);
  });

  toggleNFC(true)
  .then(() => setTagData(T2T_RE_INDEX, flag, tnf, btoa(type), btoa(payload)))
  .then(() => activateRE(T2T_RE_INDEX));
}

let tests = [
  testUrlTagDiscover
];

SpecialPowers.pushPermissions(
  [{'type': 'nfc-manager', 'allow': true, context: document}], runTests);
