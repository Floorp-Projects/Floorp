/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;
MARIONETTE_HEAD_JS = 'head.js';

let tnf = NDEF.TNF_WELL_KNOWN;
let type = "U";
let payload = "https://www.example.com";
let id = "";

let ndef = null;

// See nfc-nci.h.
const NCI_LAST_NOTIFICATION = 0;
const NCI_LIMIT_NOTIFICATION = 1;
const NCI_MORE_NOTIFICATIONS = 2;

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
    .then(() => emulator.activateRE(0))
    .then(() => emulator.snepPutNdef(4, 4, 0, tnf, btoa(type),
                                     btoa(payload), btoa(id)));
}

let tests = [
  testReceiveNDEF
];

SpecialPowers.pushPermissions(
  [{'type': 'nfc', 'allow': true,
                   'read': true, 'write': true, context: document},
   {'type': 'nfc-manager', 'allow': true, context: document}], runTests);
