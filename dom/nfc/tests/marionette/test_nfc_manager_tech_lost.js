/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;
MARIONETTE_HEAD_JS = 'head.js';

function handleTechnologyLost(msg) {
  log('Received \'nfc-manager-tech-lost\'');
  is(msg.type, 'techLost', 'check for correct message type');

  toggleNFC(false).then(runNextTest)
}

function handleTechnologyDiscoveredRE0(msg) {
  log('Received \'nfc-manager-tech-discovered\'');
  is(msg.type, 'techDiscovered', 'check for correct message type');
  is(msg.techList[0], 'P2P', 'check for correct tech type');

  NCI.deactivate();
}

function testTechLost() {
  log('Running \'testTechLost\'');
  sysMsgHelper.waitForTechDiscovered(handleTechnologyDiscoveredRE0);
  sysMsgHelper.waitForTechLost(handleTechnologyLost);

  toggleNFC(true).then(() => NCI.activateRE(emulator.P2P_RE_INDEX_0));
}

let tests = [
  testTechLost
];

SpecialPowers.pushPermissions(
  [{'type': 'nfc-manager', 'allow': true, context: document}], runTests);
