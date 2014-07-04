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

  emulator.deactivate();
}

function testTechLost() {
  log('Running \'testTechLost\'');
  window.navigator.mozSetMessageHandler(
    'nfc-manager-tech-discovered', handleTechnologyDiscoveredRE0);
  window.navigator.mozSetMessageHandler(
    'nfc-manager-tech-lost', handleTechnologyLost);

  toggleNFC(true).then(() => emulator.activateRE(0));
}

let tests = [
  testTechLost
];

SpecialPowers.pushPermissions(
  [{'type': 'nfc-manager', 'allow': true, context: document}], runTests);
