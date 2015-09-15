/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;
MARIONETTE_HEAD_JS = 'head.js';

var MANIFEST_URL = "app://system.gaiamobile.org/manifest.webapp";
var INCORRECT_MANIFEST_URL = "app://xyz.gaiamobile.org/manifest.webapp";

function peerReadyCb(evt) {
  log("peerReadyCb called");
  let peer = evt.peer;
  ok(peer instanceof MozNFCPeer, "Should get a NFCPeer object.");

  NCI.deactivate();
}

function peerLostCb(evt) {
  log("peerLostCb called");
  ok(evt.detail === undefined, "evt.detail should be undefined");
  ok(true);

  // reset callback.
  nfc.onpeerready = null;
  nfc.onpeerlost = null;
  toggleNFC(false).then(runNextTest);
}

function handleTechnologyDiscoveredRE0(msg) {
  log("Received \'nfc-manager-tech-discovered\'");
  ok(msg.peer, "check for correct tech type");

  nfc.onpeerready = peerReadyCb;
  nfc.onpeerlost = peerLostCb;

  nfc.notifyUserAcceptedP2P(MANIFEST_URL);
}

function handleTechnologyDiscoveredRE0ForP2PRegFailure(msg) {
  log("Received \'nfc-manager-tech-discovered\'");
  ok(msg.peer, "check for correct tech type");

  nfc.onpeerready = peerReadyCb;

  nfc.checkP2PRegistration(INCORRECT_MANIFEST_URL)
  .then((result) => {
    is(result, false, "check for P2P registration result");

    nfc.onpeerready = null;
    NCI.deactivate().then(() => toggleNFC(false)).then(runNextTest);
  });
}

function testPeerReady() {
  sysMsgHelper.waitForTechDiscovered(handleTechnologyDiscoveredRE0);

  toggleNFC(true).then(() => NCI.activateRE(emulator.P2P_RE_INDEX_0));
}

function testGetNFCPeer() {
  sysMsgHelper.waitForTechDiscovered(function (msg) {
    ok(msg.peer instanceof MozNFCPeer, "Should get a NFCPeer object.");

    NCI.deactivate().then(() => toggleNFC(false)).then(runNextTest);
  });

  toggleNFC(true).then(() => NCI.activateRE(emulator.P2P_RE_INDEX_0));
}

function testCheckP2PRegFailure() {
  sysMsgHelper.waitForTechDiscovered(handleTechnologyDiscoveredRE0ForP2PRegFailure);

  toggleNFC(true).then(() => NCI.activateRE(emulator.P2P_RE_INDEX_0));
}

/**
 * test for onpeerlost still should be called even onpeerready resets itself.
 */
function testPeerLostShouldBeCalled() {
  nfc.onpeerready = function() {
    NCI.deactivate();
    nfc.onpeerready = null;
  };

  nfc.onpeerlost = function() {
    ok(true, "peerlost should be called even peerready is reset");
    toggleNFC(false).then(runNextTest);
  };

  sysMsgHelper.waitForTechDiscovered(function() {
    log("testPeerLostShouldBeCalled techDiscoverd");
    nfc.notifyUserAcceptedP2P(MANIFEST_URL);
  });

  toggleNFC(true)
    .then(() => NCI.activateRE(emulator.P2P_RE_INDEX_0));
}

/**
 * test case for
 * 1. tech-discovered
 * 2. tech-lost -> onpeerlost shouldn't be called.
 */
function testPeerLostShouldNotBeCalled() {
  log("testPeerLostShouldNotBeCalled");
  nfc.onpeerlost = function () {
    ok(false, "onpeerlost shouldn't be called");
  };

  let isDiscovered = false;
  sysMsgHelper.waitForTechDiscovered(function() {
    log("testPeerLostShouldNotBeCalled techDiscoverd");
    isDiscovered = true;
    NCI.deactivate();
  });

  sysMsgHelper.waitForTechLost(function() {
    log("testPeerLostShouldNotBeCalled techLost");
    // if isDiscovered is still false, means this techLost is fired from
    // previous test.
    ok(isDiscovered, "tech-discovered should be fired first");
    nfc.onpeerlost = null;
    toggleNFC(false).then(runNextTest);
  });

  toggleNFC(true)
    .then(() => NCI.activateRE(emulator.P2P_RE_INDEX_0));
}

function testPeerShouldThrow() {
  log("testPeerShouldThrow");
  let peer;
  let tnf = NDEF.TNF_WELL_KNOWN;
  let type = new Uint8Array(NfcUtils.fromUTF8("U"));
  let payload = new Uint8Array(NfcUtils.fromUTF8("http://www.hi.com"));
  let ndef = [new MozNDEFRecord({tnf: tnf, type: type, payload: payload})];

  nfc.onpeerready = function (evt) {
    log("testPeerShouldThrow peerready");
    peer = evt.peer;
    NCI.deactivate();
  };

  nfc.onpeerlost = function () {
    log("testPeerShouldThrow peerlost");
    peer.sendNDEF(ndef)
    .then(() => {
      ok(false, "sendNDEF should throw error");
    }).catch((e) => {
      ok(true, "Exception expected " + e);
    });

    peer.sendFile(new Blob())
    .then(() => {
      ok(false, "sendfile should throw error");
    }).catch((e) => {
      ok(true, "Exception expected" + e);
    });

    nfc.onpeerready = null;
    nfc.onpeerlost = null;
    toggleNFC(false).then(runNextTest);
  };

  sysMsgHelper.waitForTechDiscovered(function() {
    log("testPeerShouldThrow techDiscovered");
    nfc.notifyUserAcceptedP2P(MANIFEST_URL);
  });

  toggleNFC(true)
    .then(() => NCI.activateRE(emulator.P2P_RE_INDEX_0));
}

var tests = [
  testPeerReady,
  testGetNFCPeer,
  testCheckP2PRegFailure,
  testPeerLostShouldBeCalled,
  testPeerLostShouldNotBeCalled,
  testPeerShouldThrow
];

SpecialPowers.pushPermissions(
  [{"type": "nfc-manager", "allow": true, context: document},
   {"type": "nfc", "allow": true, context: document},
   {"type": "nfc-share", "allow": true, context: document}], runTests);

