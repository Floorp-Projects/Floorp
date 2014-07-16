/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;
MARIONETTE_HEAD_JS = 'head.js';

let tnf = NDEF.TNF_WELL_KNOWN;
let type = "U";
let id = "";
let payload = "https://www.example.com";

let ndef = null;

function handleTechnologyDiscoveredRE0(msg) {
  log("Received 'nfc-manager-tech-discovered'");
  is(msg.type, "techDiscovered", "check for correct message type");
  is(msg.techList[0], "P2P", "check for correct tech type");

  if (msg.records) {
    isnot(msg.techList.indexOf("NDEF"), -1, "check for correct tech type");
    // validate received NDEF message against reference
    let ndef = [new MozNDEFRecord(tnf,
                                  new Uint8Array(NfcUtils.fromUTF8(type)),
                                  new Uint8Array(NfcUtils.fromUTF8(id)),
                                  new Uint8Array(NfcUtils.fromUTF8(payload)))];
    NDEF.compare(ndef, msg.records);
    toggleNFC(false).then(runNextTest);
  }
}

function testReceiveNDEF() {
  log("Running 'testReceiveNDEF'");
  window.navigator.mozSetMessageHandler(
    "nfc-manager-tech-discovered", handleTechnologyDiscoveredRE0);
  toggleNFC(true)
    .then(() => NCI.activateRE(emulator.P2P_RE_INDEX_0))
    .then(() => SNEP.put(SNEP.SAP_NDEF, SNEP.SAP_NDEF, 0, tnf, btoa(type),
                         btoa(id), btoa(payload)));
}

let tests = [
  testReceiveNDEF
];

SpecialPowers.pushPermissions(
  [{'type': 'nfc', 'allow': true,
                   'read': true, 'write': true, context: document},
   {'type': 'nfc-manager', 'allow': true, context: document}], runTests);
