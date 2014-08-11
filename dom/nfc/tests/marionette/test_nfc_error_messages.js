/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

/* globals log, is, ok, runTests, toggleNFC, runNextTest,
   SpecialPowers, nfc, MozNDEFRecord, emulator */

const MARIONETTE_TIMEOUT = 60000;
const MARIONETTE_HEAD_JS = 'head.js';

const MANIFEST_URL = 'app://system.gaiamobile.org/manifest.webapp';
const NDEF_MESSAGE = [new MozNDEFRecord(0x01,
                                        new Uint8Array(0x84),
                                        new Uint8Array(0),
                                        new Uint8Array(0x20))];

let nfcPeers = [];
let sessionTokens = [];

/**
 * Enables nfc and RE0 then registers onpeerready callback and once
 * it's fired it creates mozNFCPeer and stores it for later.
 * After disabling nfc tries to do mozNFCPeer.sendNDEF which should
 * fail with NfcNotEnabledError.
 */
function testNfcNotEnabledError() {
  log('testNfcNotEnabledError');
  toggleNFC(true)
  .then(() => NCI.activateRE(emulator.P2P_RE_INDEX_0))
  .then(registerAndFireOnpeerready)
  .then(() => toggleNFC(false))
  .then(() => sendNDEFExpectError(nfcPeers[0], 'NfcNotEnabledError'))
  .then(endTest)
  .catch(handleRejectedPromise);
}

/**
 * Enables nfc and RE0, register onpeerready callback, once it's fired
 * it creates and stores mozNFCPeer. Disables nfc, enables nfc and
 * once again registers and fires new onpeerready callback and stores
 * mozNfcPeer. Than fires sendNDEF on the first stored peer which
 * should have invalid session token and we should get NfcBadSessionIdError
 */
function testNfcBadSessionIdError() {
  log('testNfcBadSessionIdError');
  toggleNFC(true)
  .then(() => NCI.activateRE(emulator.P2P_RE_INDEX_0))
  .then(registerAndFireOnpeerready)
  .then(() => NCI.deactivate())
  .then(() => NCI.activateRE(emulator.P2P_RE_INDEX_0))
  .then(registerAndFireOnpeerready)
  // we have 2 peers in nfcPeers array, peer0 has old/invalid session token
  .then(() => sendNDEFExpectError(nfcPeers[0], 'NfcBadSessionIdError'))
  .then(() => toggleNFC(false))
  .then(endTest)
  .catch(handleRejectedPromise);
}

/**
 * Eables nfc and RE0, register onpeerready callback, once it's fired
 * it stores sessionToken. Using sessionToken cretes mozNFCTag and fires
 * mozNFCTag.connect('NDEF') which should result in NfcConnectError.
 */
function testNfcConnectError() {
  log('testNfcConnectError');
  toggleNFC(true)
  .then(() => NCI.activateRE(emulator.P2P_RE_INDEX_0))
  .then(registerAndFireOnpeerready)
  .then(() => connectToNFCTagExpectError(sessionTokens[0],
                                         'NDEF',
                                         'NfcConnectError'))
  .then(() => toggleNFC(false))
  .then(endTest)
  .catch(handleRejectedPromise);
}

/**
 * Enables nfc and RE0, registers tech-discovered msg handler, once it's
 * fired set tech-lost handler and disables nfc. In both handlers checks
 * if error message is not present.
 */
function testNoErrorInTechMsg() {
  log('testNoErrorInTechMsg');

  let techDiscoveredHandler = function(msg) {
    ok('Message handler for nfc-manager-tech-discovered');
    is(msg.type, 'techDiscovered');
    is(msg.errorMsg, undefined, 'Should not get error msg in tech discovered');

    setAndFireTechLostHandler()
    .then(() => toggleNFC(false))
    .then(endTest)
    .catch(handleRejectedPromise);
  };

  sysMsgHelper.waitForTechDiscovered(techDiscoveredHandler);

  toggleNFC(true)
  .then(() => NCI.activateRE(emulator.P2P_RE_INDEX_0))
  .catch(handleRejectedPromise);
}

function endTest() {
  nfcPeers = [];
  sessionTokens = [];
  runNextTest();
}

function handleRejectedPromise() {
  ok(false, 'Handling rejected promise');
  toggleNFC(false).then(endTest);
}

function registerAndFireOnpeerready() {
  let deferred = Promise.defer();

  nfc.onpeerready = function(event) {
    sessionTokens.push(event.detail);
    nfcPeers.push(nfc.getNFCPeer(event.detail));
    nfc.onpeerready = null;
    deferred.resolve();
  };

  let req = nfc.checkP2PRegistration(MANIFEST_URL);
  req.onsuccess = function() {
    is(req.result, true, 'P2P registration result');
    if(req.result) {
      nfc.notifyUserAcceptedP2P(MANIFEST_URL);
    } else {
      ok(false, 'this should not happen');
      nfc.onpeerready = null;
      deferred.reject();
    }
  };

  req.onerror = function() {
    ok(false, 'not possible');
    nfc.onpeerready = null;
    deferred.reject();
  };

  return deferred.promise;
}

function sendNDEFExpectError(peer, errorMsg) {
  let deferred = Promise.defer();

  let req = peer.sendNDEF(NDEF_MESSAGE);
  req.onsuccess = function() {
    ok(false, 'success on sending ndef not possible shoudl get: ' + errorMsg);
    deferred.reject();
  };

  req.onerror = function() {
    ok(true, 'this should happen');
    is(req.error.name, errorMsg, 'Should have proper error name');
    deferred.resolve();
  };

  return deferred.promise;
}

function connectToNFCTagExpectError(sessionToken, tech, errorMsg) {
  let deferred = Promise.defer();

  let nfcTag = nfc.getNFCTag(sessionTokens[0]);
  let req = nfcTag.connect(tech);
  req.onsuccess = function() {
    ok(false, 'we should not be able to connect to the tag');
    deferred.reject();
  };

  req.onerror = function() {
    ok(true, 'we should get an error');
    is(req.error.name, errorMsg, 'Should have proper error name');
    deferred.resolve();
  };

  return deferred.promise;
}

function setAndFireTechLostHandler() {
  let deferred = Promise.defer();

  let techLostHandler = function(msg) {
    ok('Message handler for nfc-manager-tech-lost');
    is(msg.type, 'techLost');
    is(msg.errorMsg, undefined, 'Should not get error msg in tech lost');

    deferred.resolve();
  };

  sysMsgHelper.waitForTechLost(techLostHandler);

  // triggers tech-lost
  NCI.deactivate();
  return deferred.promise;
}

let tests = [
  testNfcNotEnabledError,
  testNfcBadSessionIdError,
  testNfcConnectError,
  testNoErrorInTechMsg
];

/**
 * nfc-manager for mozNfc.checkP2PRegistration(manifestUrl)
 *  -> "NFC:CheckP2PRegistration" IPC
 * nfc-write to set/unset onpeerready
 *  -> "NFC:RegisterPeerTarget", "NFC:UnregisterPeerTarget" IPC
 */
SpecialPowers.pushPermissions(
  [
    {'type': 'nfc-manager', 'allow': true, context: document},
    {'type': 'nfc-write', 'allow': true, context: document}
  ], runTests);
