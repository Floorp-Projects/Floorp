/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;
MARIONETTE_HEAD_JS = "head.js";

let MANIFEST_URL = "app://system.gaiamobile.org/manifest.webapp";

function sendFile(msg) {
  log("sendFile msg="+JSON.stringify(msg));
  let peer = nfc.getNFCPeer(msg.sessionToken);
  ok(peer instanceof MozNFCPeer, "should get a MozNFCPeer");
  ok(msg.blob instanceof Blob, "should get a Blob");

  nfc.peerready = null;
  NCI.deactivate().then(() => toggleNFC(false)).then(runNextTest);
}

function testSendFile() {
  nfc.onpeerready = function(evt) {
    let peer = evt.peer;
    peer.sendFile(new Blob());
    sysMsgHelper.waitForSendFile(sendFile);
  };

  sysMsgHelper.waitForTechDiscovered(function(msg) {
    let request = nfc.checkP2PRegistration(MANIFEST_URL);
    request.onsuccess = function(evt) {
      is(request.result, true, "check for P2P registration result");
      nfc.notifyUserAcceptedP2P(MANIFEST_URL);
    }

    request.onerror = function() {
      ok(false, "checkP2PRegistration failed.");
      toggleNFC(false).then(runNextTest);
    }
  });

  toggleNFC(true).then(() => NCI.activateRE(emulator.P2P_RE_INDEX_0));
}

let tests = [
  testSendFile
];

SpecialPowers.pushPermissions(
  [{"type": "nfc", "allow": true,
                   "read": true, 'write': true, context: document},
   {"type": "nfc-manager", 'allow': true, context: document}], runTests);
