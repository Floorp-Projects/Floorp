/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;
MARIONETTE_HEAD_JS = 'head.js';

let tnf = NDEF.TNF_WELL_KNOWN;
let type = "U";
let id = "";
let payload = "https://www.example.com";

let ndef = null;

function handleSnep(msg) {
  ok(msg.records != null, "msg.records should have values");
  // validate received NDEF message against reference
  let ndef = [new MozNDEFRecord({tnf: tnf,
                                 type: NfcUtils.fromUTF8(type),
                                 payload: NfcUtils.fromUTF8(payload)})];
  NDEF.compare(ndef, msg.records);
  toggleNFC(false).then(runNextTest);
}

function handleTechnologyDiscoveredRE0(msg) {
  log("Received 'nfc-manager-tech-discovered'");
  is(msg.type, "techDiscovered", "check for correct message type");
  is(msg.isP2P, "P2P", "check for correct tech type");

  sysMsgHelper.waitForTechDiscovered(handleSnep);
  SNEP.put(SNEP.SAP_NDEF, SNEP.SAP_NDEF, 0, tnf, btoa(type), btoa(id), btoa(payload));
}

function testReceiveNDEF() {
  log("Running 'testReceiveNDEF'");
  sysMsgHelper.waitForTechDiscovered(handleTechnologyDiscoveredRE0);
  toggleNFC(true).then(() => NCI.activateRE(emulator.P2P_RE_INDEX_0));
}

let tests = [
  testReceiveNDEF
];

SpecialPowers.pushPermissions(
  [{'type': 'nfc', 'allow': true,
                   'read': true, 'write': true, context: document},
   {'type': 'nfc-manager', 'allow': true, context: document}], runTests);
