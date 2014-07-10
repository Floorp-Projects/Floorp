/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;
MARIONETTE_HEAD_JS = "head.js";

let url = "https://www.example.com";

function sendNDEF(techType, sessionToken) {
  let tnf = NDEF.TNF_WELL_KNOWN;
  let type = new Uint8Array(NfcUtils.fromUTF8("U"));
  let id = new Uint8Array(NfcUtils.fromUTF8(""));
  let payload = new Uint8Array(NfcUtils.fromUTF8(url));
  let ndef = [new MozNDEFRecord(tnf, type, id, payload)];

  let peer = window.navigator.mozNfc.getNFCPeer(sessionToken);
  let req = peer.sendNDEF(ndef);
  req.onsuccess = function() {
    log("Successfully sent NDEF message");

    let cmd = "nfc snep put -1 -1"; /* read last SNEP PUT from emulator */
    log("Executing \'" + cmd + "\'");
    emulator.run(cmd, function(result) {
      is(result.pop(), "OK", "check SNEP PUT result");
      NDEF.compare(ndef, NDEF.parseString(result.pop()));
      toggleNFC(false).then(runNextTest);
    });
  };
  req.onerror = function() {
    ok(false, "Failed to send NDEF message, error \'" + this.error + "\'");
    toggleNFC(false).then(runNextTest);
  };
}

function handleTechnologyDiscoveredRE0(msg) {
  log("Received \'nfc-manager-tech-discovered\' " + JSON.stringify(msg));
  is(msg.type, "techDiscovered", "check for correct message type");
  let index = msg.techList.indexOf("P2P");
  isnot(index, -1, "check for \'P2P\' in tech list");
  sendNDEF(msg.techList[index], msg.sessionToken);
}

function testOnPeerReadyRE0() {
  log("Running \'testOnPeerReadyRE0\'");
  window.navigator.mozSetMessageHandler(
    "nfc-manager-tech-discovered", handleTechnologyDiscoveredRE0);
  toggleNFC(true).then(() => NCI.activateRE(emulator.P2P_RE_INDEX_0));
}

let tests = [
  testOnPeerReadyRE0
];

SpecialPowers.pushPermissions(
  [{"type": "nfc", "allow": true,
                   "read": true, 'write': true, context: document},
   {"type": "nfc-manager", 'allow': true, context: document}], runTests);
