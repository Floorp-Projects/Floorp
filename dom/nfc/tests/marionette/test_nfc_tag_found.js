/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;
MARIONETTE_HEAD_JS = 'head.js';

function handleTechnologyDiscoveredRE0(msg) {
  log('Received \'nfc-manager-tech-discovered\'');
  ok(msg.peer, 'check for correct tech type');

  deactivateAndWaitForTechLost().then(() => toggleNFC(false)).then(runNextTest);
}

function tagFoundCb(evt) {
  log("tagFoundCb called ");
  ok(evt.tag instanceof MozNFCTag, "Should get a NFCTag object.");

  nfc.ontagfound = null;
  deactivateAndWaitForTechLost().then(() => toggleNFC(false)).then(runNextTest);
}

function tagFoundErrorCb(evt) {
  ok(false, "ontagfound shouldn't be called");
}

function testTagFound() {
  nfc.ontagfound = tagFoundCb;

  toggleNFC(true).then(() => NCI.activateRE(emulator.T1T_RE_INDEX));
}

function testTagFoundShouldNotFired() {
  nfc.ontagfound = tagFoundErrorCb;
  sysMsgHelper.waitForTechDiscovered(handleTechnologyDiscoveredRE0);

  toggleNFC(true).then(() => NCI.activateRE(emulator.P2P_RE_INDEX_0));
}

var tests = [
  testTagFound,
  testTagFoundShouldNotFired
];

SpecialPowers.pushPermissions(
  [{"type": "nfc", "allow": true, context: document},
   {'type': 'nfc-manager', 'allow': true, context: document}], runTests);
