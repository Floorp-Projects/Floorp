/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;
MARIONETTE_HEAD_JS = 'head.js';

let MANIFEST_URL = "app://system.gaiamobile.org/manifest.webapp";
let INCORRECT_MANIFEST_URL = "app://xyz.gaiamobile.org/manifest.webapp";

function peerReadyCb(evt) {
  log("peerReadyCb called");
  let peer = nfc.getNFCPeer(evt.detail);
  let peer1 = nfc.getNFCPeer(evt.detail);
  ok(peer == peer1, "Should get the same NFCPeer object.");
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
    toggleNFC(false).then(runNextTest);
  }

  request.onerror = function () {
    ok(false, "checkP2PRegistration failed.");

    nfc.onpeerready = null;
    toggleNFC(false).then(runNextTest);
  }
}

function testPeerReady() {
  window.navigator.mozSetMessageHandler(
    "nfc-manager-tech-discovered", handleTechnologyDiscoveredRE0);

  toggleNFC(true).then(() => NCI.activateRE(emulator.P2P_RE_INDEX_0));
}

function testCheckP2PRegFailure() {
  window.navigator.mozSetMessageHandler(
    "nfc-manager-tech-discovered", handleTechnologyDiscoveredRE0ForP2PRegFailure);

  toggleNFC(true).then(() => NCI.activateRE(emulator.P2P_RE_INDEX_0));
}

function testCheckNfcPeerObjForInvalidToken() {
  try {
    // Use a'fakeSessionToken'
    let peer = nfc.getNFCPeer("fakeSessionToken");
    ok(false, "Should not get a NFCPeer object.");
  } catch (ex) {
    ok(true, "Exception expected");
  }

  toggleNFC(false).then(runNextTest);
}

function testPeerLostShouldNotBeCalled() {
  nfc.onpeerlost = function () {
    ok(false, "onpeerlost shouldn't be called");
  };

  toggleNFC(true)
    .then(() => NCI.activateRE(emulator.P2P_RE_INDEX_0))
    .then(NCI.deactivate)
    .then(() => toggleNFC(false));

  nfc.onpeerlost = null;
  runNextTest();
}

function testPeerShouldThrow() {
  let peer;
  let tnf = NDEF.TNF_WELL_KNOWN;
  let type = new Uint8Array(NfcUtils.fromUTF8("U"));
  let id = new Uint8Array(NfcUtils.fromUTF8(""));
  let payload = new Uint8Array(NfcUtils.fromUTF8(url));
  let ndef = [new MozNDEFRecord(tnf, type, id, payload)];

  nfc.onpeerready = function (evt) {
    peer = nfc.getNFCPeer(evt.detail);
  };

  let request = nfc.checkP2PRegistration(MANIFEST_URL);
  request.onsuccess = function (evt) {
    is(request.result, true, "check for P2P registration result");
    nfc.notifyUserAcceptedP2P(MANIFEST_URL);
  }

  toggleNFC(true)
    .then(() => NCI.activateRE(emulator.P2P_RE_INDEX_0))
    .then(NCI.deactivate);

  try {
    peer.sendNDEF(ndef);
    ok(false, "sendNDEF should throw error");
  } catch (e) {
    ok(true, "Exception expected");
  }

  try {
    peer.sendFile(new Blob());
    ok(false, "sendfile should throw error");
  } catch (e) {
    ok(true, "Exception expected");
  }

  nfc.onpeerready = null;
  toggleNFC(false).then(runNextTest);
}

let tests = [
  testPeerReady,
  testCheckP2PRegFailure,
  testCheckNfcPeerObjForInvalidToken,
  testPeerLostShouldNotBeCalled,
  testPeerShouldThrow
];

SpecialPowers.pushPermissions(
  [{"type": "nfc-manager", "allow": true, context: document},
   {"type": "nfc-write", "allow": true, context: document}], runTests);
