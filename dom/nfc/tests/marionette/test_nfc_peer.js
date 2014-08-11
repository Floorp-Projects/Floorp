/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;
MARIONETTE_HEAD_JS = 'head.js';

let MANIFEST_URL = "app://system.gaiamobile.org/manifest.webapp";
let INCORRECT_MANIFEST_URL = "app://xyz.gaiamobile.org/manifest.webapp";

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
  is(msg.type, "techDiscovered", "check for correct message type");
  is(msg.techList[0], "P2P", "check for correct tech type");

  nfc.onpeerready = peerReadyCb;
  nfc.onpeerlost = peerLostCb;

  let request = nfc.checkP2PRegistration(MANIFEST_URL);
  request.onsuccess = function (evt) {
    is(request.result, true, "check for P2P registration result");
    nfc.notifyUserAcceptedP2P(MANIFEST_URL);
  }

  request.onerror = function () {
    ok(false, "checkP2PRegistration failed.");
    toggleNFC(false).then(runNextTest);
  }
}

function handleTechnologyDiscoveredRE0ForP2PRegFailure(msg) {
  log("Received \'nfc-manager-tech-discovered\'");
  is(msg.type, "techDiscovered", "check for correct message type");
  is(msg.techList[0], "P2P", "check for correct tech type");

  nfc.onpeerready = peerReadyCb;

  let request = nfc.checkP2PRegistration(INCORRECT_MANIFEST_URL);
  request.onsuccess = function (evt) {
    is(request.result, false, "check for P2P registration result");

    nfc.onpeerready = null;
    NCI.deactivate().then(() => toggleNFC(false)).then(runNextTest);
  }

  request.onerror = function () {
    ok(false, "checkP2PRegistration failed.");

    nfc.onpeerready = null;
    NCI.deactivate().then(() => toggleNFC(false)).then(runNextTest);
  }
}

function testPeerReady() {
  sysMsgHelper.waitForTechDiscovered(handleTechnologyDiscoveredRE0);

  toggleNFC(true).then(() => NCI.activateRE(emulator.P2P_RE_INDEX_0));
}

function testGetNFCPeer() {
  sysMsgHelper.waitForTechDiscovered(function (msg) {
    let peer = nfc.getNFCPeer(msg.sessionToken);
    ok(peer instanceof MozNFCPeer, "Should get a NFCPeer object.");
    let peer1 = nfc.getNFCPeer(msg.sessionToken);
    ok(peer == peer1, "Should get the same MozNFCPeer object");

    NCI.deactivate().then(() => toggleNFC(false)).then(runNextTest);
  });

  toggleNFC(true).then(() => NCI.activateRE(emulator.P2P_RE_INDEX_0));
}

function testCheckP2PRegFailure() {
  sysMsgHelper.waitForTechDiscovered(handleTechnologyDiscoveredRE0ForP2PRegFailure);

  toggleNFC(true).then(() => NCI.activateRE(emulator.P2P_RE_INDEX_0));
}

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
  let id = new Uint8Array(NfcUtils.fromUTF8(""));
  let payload = new Uint8Array(NfcUtils.fromUTF8("http://www.hi.com"));
  let ndef = [new MozNDEFRecord(tnf, type, id, payload)];

  nfc.onpeerready = function (evt) {
    log("testPeerShouldThrow peerready");
    peer = evt.peer;
    NCI.deactivate();
  };

  nfc.onpeerlost = function () {
    log("testPeerShouldThrow peerlost");
    try {
      peer.sendNDEF(ndef);
      ok(false, "sendNDEF should throw error");
    } catch (e) {
      ok(true, "Exception expected " + e);
    }

    try {
      peer.sendFile(new Blob());
      ok(false, "sendfile should throw error");
    } catch (e) {
      ok(true, "Exception expected" + e);
    }

    nfc.onpeerready = null;
    nfc.onpeerlost = null;
    toggleNFC(false).then(runNextTest);
  };

  sysMsgHelper.waitForTechDiscovered(function() {
    log("testPeerShouldThrow techDiscovered");
    let request = nfc.checkP2PRegistration(MANIFEST_URL);
    request.onsuccess = function (evt) {
      nfc.notifyUserAcceptedP2P(MANIFEST_URL);
    }

    request.onerror = function () {
      ok(false, "checkP2PRegistration failed.");
      toggleNFC(false).then(runNextTest);
    }
  });

  toggleNFC(true)
    .then(() => NCI.activateRE(emulator.P2P_RE_INDEX_0));
}

function testPeerInvalidToken() {
  log("testPeerInvalidToken");
  let peer = nfc.getNFCPeer("fakeSessionToken");
  is(peer, null, "NFCPeer should be null on wrong session token");

  runNextTest();
}

/**
 * Added for completeness in Bug 1042651,
 * TODO: remove once Bug 963531 lands
 */
function testTagInvalidToken() {
  log("testTagInvalidToken");
  let tag = nfc.getNFCTag("fakeSessionToken");
  is(tag, null, "NFCTag should be null on wrong session token");

  runNextTest();
}

let tests = [
  testPeerReady,
  testGetNFCPeer,
  testCheckP2PRegFailure,
  testPeerLostShouldNotBeCalled,
  testPeerShouldThrow,
  testPeerInvalidToken,
  testTagInvalidToken
];

SpecialPowers.pushPermissions(
  [{"type": "nfc-manager", "allow": true, context: document},
   {"type": "nfc-write", "allow": true, context: document}], runTests);
