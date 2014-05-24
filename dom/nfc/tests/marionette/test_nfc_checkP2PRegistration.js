/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

/* globals log, is, ok, runTests, toggleNFC, runNextTest,
   SpecialPowers, nfc, enableRE0 */

const MARIONETTE_TIMEOUT = 30000;
const MARIONETTE_HEAD_JS = 'head.js';

const MANIFEST_URL = 'app://system.gaiamobile.org/manifest.webapp';
const FAKE_MANIFEST_URL = 'app://fake.gaiamobile.org/manifest.webapp';

/**
 * Failure scenarion without onpeerread handler registration
 * Nfc not enabled -> no session token.
 */
function testNoTargetNoSessionToken() {
  log('testNoTargetNoSessionToken');
  fireCheckP2PReg(MANIFEST_URL)
  .then((result) => {
    is(result, false, 'No target, no sesionToken, result should be false');
    runNextTest();
  })
  .catch(handleRejectedPromise);
}

/**
 * Failure scenario onpeerready handler registered but Nfc not enabled
 * -> no session token.
 */
function testWithTargetNoSessionToken() {
  log('testWithTargetNoSessionToken');
  registerOnpeerready()
  .then(() => fireCheckP2PReg(MANIFEST_URL))
  .then((result) => {
    is(result, false,
      'session token is available and it shouldnt be');
    nfc.onpeerready = null;
    runNextTest();
  })
  .catch(handleRejectedPromise);
}

/**
 * Success scenario, nfc enabled, activated RE0 (p2p ndef is received,
 * creates session token) opeerreadyhandler registered.
 */
function testWithSessionTokenWithTarget() {
  log('testWithSessionTokenWithTarget');
  toggleNFC(true)
  .then(enableRE0)
  .then(registerOnpeerready)
  .then(() => fireCheckP2PReg(MANIFEST_URL))
  .then((result) => {
    is(result, true, 'should be true, onpeerready reg, sessionToken set');
    nfc.onpeerready = null;
    return toggleNFC(false);
  })
  .then(runNextTest)
  .catch(handleRejectedPromiseWithNfcOn);
}

/**
 * Failure scenario, nfc enabled, activated RE0 (p2p ndef is received,
 * creates session token) opeerready handler not registered.
 */
function testWithSessionTokenNoTarget() {
  log('testWithSessionTokenNoTarget');
  toggleNFC(true)
  .then(enableRE0)
  .then(() => fireCheckP2PReg(MANIFEST_URL))
  .then((result) => {
    is(result, false,
      'session token  avilable but onpeerready not registered');
    return toggleNFC(false);
  })
  .then(runNextTest)
  .catch(handleRejectedPromiseWithNfcOn);
}

/**
 * Failure scenario, nfc enabled, re0 activated, onpeerready registered,
 * checking wrong manifest url.
 */
function testWithSessionTokenWrongTarget() {
  log('testWithSessionTokenWrongTarget');
  toggleNFC(true)
  .then(enableRE0)
  .then(registerOnpeerready)
  .then(() => fireCheckP2PReg(FAKE_MANIFEST_URL))
  .then((result) => {
    is(result, false, 'should be false, fake manifest, sessionToken set');
    nfc.onpeerready = null;
    return toggleNFC(false);
  })
  .then(runNextTest)
  .catch(handleRejectedPromiseWithNfcOn);
}

function registerOnpeerready() {
  nfc.onpeerready = function() {
    ok(false, 'onpeerready callback cannot be fired');
  };
  let d = Promise.defer();
  d.resolve();
  return d.promise;
}

function fireCheckP2PReg(manifestUrl) {
  let deferred = Promise.defer();

  let request = nfc.checkP2PRegistration(manifestUrl);
  request.onsuccess = function() {
    ok(true, 'checkP2PRegistration allways results in success');
    deferred.resolve(request.result);
  };

  request.onerror = function() {
    ok(false, 'see NfcContentHelper.handleCheckP2PRegistrationResponse');
    deferred.reject();
  };

  return deferred.promise;
}

function handleRejectedPromise() {
   ok(false, 'Promise rejected. This should not happen');
   nfc.onpeerready = null;
   runNextTest();
}

function handleRejectedPromiseWithNfcOn() {
  ok(false, 'Promise rejected. This should not happen. Turning off nfc');
  nfc.onpeerready = null;
  toggleNFC(false).then(runNextTest);
}

let tests = [
  testNoTargetNoSessionToken,
  testWithTargetNoSessionToken,
  testWithSessionTokenWithTarget,
  testWithSessionTokenNoTarget,
  testWithSessionTokenWrongTarget
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
