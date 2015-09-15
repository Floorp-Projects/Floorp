/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

function handleTechnologyDiscoveredRE0(msg) {
  log('Received \'nfc-manager-tech-discovered\'');
  ok(msg.peer, 'check for correct tech type');
  deactivateAndWaitForTechLost().then(() => toggleNFC(false)).then(runNextTest);
}

function testActivateRE0() {
  log('Running \'testActivateRE0\'');
  sysMsgHelper.waitForTechDiscovered(handleTechnologyDiscoveredRE0);

  toggleNFC(true).then(() => NCI.activateRE(emulator.P2P_RE_INDEX_0));
}

// Check NCI Spec 5.2, this will change NCI state from
// DISCOVERY -> W4_ALL_DISCOVERIES -> W4_HOST_SELECT -> POLL_ACTIVE
function testRfDiscover() {
  log('Running \'testRfDiscover\'');
  sysMsgHelper.waitForTechDiscovered(handleTechnologyDiscoveredRE0);

  toggleNFC(true)
  .then(() => NCI.notifyDiscoverRE(emulator.P2P_RE_INDEX_0, NCI.MORE_NOTIFICATIONS))
  .then(() => NCI.notifyDiscoverRE(emulator.P2P_RE_INDEX_1, NCI.LAST_NOTIFICATION))
  .then(() => NCI.activateRE(emulator.P2P_RE_INDEX_0));
}

var tests = [
  testActivateRE0,
  testRfDiscover
];

SpecialPowers.pushPermissions(
  [{'type': 'nfc-manager', 'allow': true, context: document}], runTests);
