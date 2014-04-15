/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;
MARIONETTE_HEAD_JS = 'head.js';

function handleTechnologyDiscoveredRE0(msg) {
  log('Received \'nfc-manager-tech-discovered\'');
  is(msg.type, 'techDiscovered', 'check for correct message type');
  is(msg.techList[0], 'P2P', 'check for correct tech type');
  toggleNFC(false, runNextTest);
}

function activateRE0() {
  var cmd = 'nfc ntf rf_intf_activated 0';
  log('Executing \'' + cmd + '\'');
  ++pendingEmulatorCmdCount;
  runEmulatorCmd(cmd, function(result) {
    --pendingEmulatorCmdCount;
    is(result.pop(), 'OK', 'check activation of RE0');
  });
}

function testActivateRE0() {
  log('Running \'testActivateRE0\'');
  window.navigator.mozSetMessageHandler(
    'nfc-manager-tech-discovered', handleTechnologyDiscoveredRE0);
  toggleNFC(true, activateRE0);
}

let tests = [
  testActivateRE0
];

SpecialPowers.pushPermissions(
  [{'type': 'nfc-manager', 'allow': true, context: document}], runTests);
