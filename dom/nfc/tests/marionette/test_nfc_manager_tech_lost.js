/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;
MARIONETTE_HEAD_JS = 'head.js';

function handleTechnologyLost(msg) {
  log('Received \'nfc-manager-tech-lost\'');
  ok(true);

  toggleNFC(false).then(runNextTest)
}

function handleTechnologyDiscoveredRE0(msg) {
  log('Received \'nfc-manager-tech-discovered\'');
  ok(msg.peer, 'check for correct tech type');

  NCI.deactivate();
}

function testTechLost() {
  log('Running \'testTechLost\'');
  sysMsgHelper.waitForTechDiscovered(handleTechnologyDiscoveredRE0);
  sysMsgHelper.waitForTechLost(handleTechnologyLost);

  toggleNFC(true).then(() => NCI.activateRE(emulator.P2P_RE_INDEX_0));
}

var tests = [
  testTechLost
];

SpecialPowers.pushPermissions(
  [{'type': 'nfc-manager', 'allow': true, context: document}], runTests);
