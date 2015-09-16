/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

var url = "https://www.example.com";

function sendNDEF(peer) {
  let tnf = NDEF.TNF_WELL_KNOWN;
  let type = new Uint8Array(NfcUtils.fromUTF8("U"));
  let payload = new Uint8Array(NfcUtils.fromUTF8(url));
  let ndef = [new MozNDEFRecord({tnf: tnf, type: type, payload: payload})];

  let promise = peer.sendNDEF(ndef);
  promise.then(() => {
    log("Successfully sent NDEF message");

    let cmd = "nfc snep put -1 -1"; /* read last SNEP PUT from emulator */
    log("Executing \'" + cmd + "\'");
    emulator.run(cmd, function(result) {
      is(result.pop(), "OK", "check SNEP PUT result");
      NDEF.compare(ndef, NDEF.parseString(result.pop()));
      deactivateAndWaitForTechLost().then(() => toggleNFC(false)).then(runNextTest);
    });
  }).catch(() => {
    ok(false, "Failed to send NDEF message, error \'" + this.error + "\'");
    deactivateAndWaitForTechLost().then(() => toggleNFC(false)).then(runNextTest);
  });
}

function handleTechnologyDiscoveredRE0(msg) {
  log("Received \'nfc-manager-tech-discovered\' " + JSON.stringify(msg));
  ok(msg.peer, "techDiscovered", "check for correct message type");
  sendNDEF(msg.peer);
}

function testOnPeerReadyRE0() {
  log("Running \'testOnPeerReadyRE0\'");
  sysMsgHelper.waitForTechDiscovered(handleTechnologyDiscoveredRE0);
  toggleNFC(true).then(() => NCI.activateRE(emulator.P2P_RE_INDEX_0));
}

var tests = [
  testOnPeerReadyRE0
];

SpecialPowers.pushPermissions(
  [{"type": "nfc", "allow": true,
                   "read": true, 'write': true, context: document},
   {"type": "nfc-manager", 'allow': true, context: document}], runTests);
