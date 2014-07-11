/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;
MARIONETTE_HEAD_JS = 'head.js';

function handleTechnologyDiscoveredRE0(msg) {
  log('Received \'nfc-manager-tech-discovered\'');
  is(msg.type, 'techDiscovered', 'check for correct message type');
  is(msg.techList[0], 'P2P', 'check for correct tech type');
  toggleNFC(false).then(runNextTest);
}

function testActivateRE0() {
  log('Running \'testActivateRE0\'');
  window.navigator.mozSetMessageHandler(
    'nfc-manager-tech-discovered', handleTechnologyDiscoveredRE0);

  toggleNFC(true).then(() => NCI.activateRE(emulator.P2P_RE_INDEX_0));
}

// Check NCI Spec 5.2, this will change NCI state from
// DISCOVERY -> W4_ALL_DISCOVERIES -> W4_HOST_SELECT -> POLL_ACTIVE
function testRfDiscover() {
  log('Running \'testRfDiscover\'');
  window.navigator.mozSetMessageHandler(
    'nfc-manager-tech-discovered', handleTechnologyDiscoveredRE0);

  toggleNFC(true)
  .then(() => NCI.notifyDiscoverRE(emulator.P2P_RE_INDEX_0, NCI.MORE_NOTIFICATIONS))
  .then(() => NCI.notifyDiscoverRE(emulator.P2P_RE_INDEX_1, NCI.LAST_NOTIFICATION))
  .then(() => NCI.activateRE(emulator.P2P_RE_INDEX_0));
}

let tests = [
  testActivateRE0,
  testRfDiscover
];

SpecialPowers.pushPermissions(
  [{'type': 'nfc-manager', 'allow': true, context: document}], runTests);
