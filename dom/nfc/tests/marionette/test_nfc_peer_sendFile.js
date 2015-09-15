/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

var MANIFEST_URL = "app://system.gaiamobile.org/manifest.webapp";

function sendFile(msg) {
  log("sendFile msg="+JSON.stringify(msg));
  ok(msg.peer instanceof MozNFCPeer, "should get a MozNFCPeer");
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
    nfc.checkP2PRegistration(MANIFEST_URL).then(result => {
      if (result) {
        nfc.notifyUserAcceptedP2P(MANIFEST_URL);
      } else {
        ok(false, "checkP2PRegistration failed.");
        deactivateAndWaitForTechLost().then(() => toggleNFC(false)).then(runNextTest);
      }
    });
  });

  toggleNFC(true).then(() => NCI.activateRE(emulator.P2P_RE_INDEX_0));
}

var tests = [
  testSendFile
];

SpecialPowers.pushPermissions(
  [{"type": "nfc-share", "allow": true, context: document},
   {"type": "nfc-manager", 'allow': true, context: document}], runTests);
