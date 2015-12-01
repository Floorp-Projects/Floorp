/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;
MARIONETTE_HEAD_JS = 'head.js';

function tagFoundCb(evt) {
  log("tagFoundCb called ");
  ok(evt.tag instanceof MozNFCTag, "Should get a NFCTag object.");

  NCI.deactivate();
}

function tagLostCb(evt) {
  log("tagLostCb called ");
  ok(true);

  nfc.ontagfound = null;
  nfc.ontaglost = null;
  toggleNFC(false).then(runNextTest);
}

function testTagLost() {
  nfc.ontagfound = tagFoundCb;
  nfc.ontaglost = tagLostCb;

  toggleNFC(true).then(() => NCI.activateRE(emulator.T1T_RE_INDEX));
}

var tests = [
  testTagLost
];

SpecialPowers.pushPermissions(
  [{"type": "nfc", "allow": true, context: document},
   {'type': 'nfc-manager', 'allow': true, context: document}], runTests);
