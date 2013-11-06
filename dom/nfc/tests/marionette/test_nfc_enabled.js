/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;

function toggleNFC(enabled, callback) {
  isnot(callback, null);
  var settings = window.navigator.mozSettings;
  isnot(settings, null);
  ok(settings instanceof SettingsManager,
     'settings instanceof ' + settings.constructor +
     ', expected SettingsManager');

  let req = settings.createLock().get('nfc.enabled');
  req.onsuccess = function() {
    if (req.result['nfc.enabled'] === enabled) {
      callback();
    } else {
      let req = settings.createLock().set({'nfc.enabled': enabled});
      req.onsuccess = function() {
        window.setTimeout(callback, 5000); // give emulator time to toggle NFC
      };
      req.onerror = function() {
        ok(false,
           'Setting \'nfc.enabled\' to \'' + enabled +
           '\' failed, error ' + req.error.name);
        finish();
      };
    }
  };
  req.onerror = function() {
    ok(false, 'Getting \'nfc.enabled\' failed, error ' + req.error.name);
    finish();
  };
}

function testEnableNFC() {
  toggleNFC(true, runNextTest);
}

function testDisableNFC() {
  toggleNFC(false, runNextTest);
}

let tests = [
  testEnableNFC,
  testDisableNFC
];

function runNextTest() {
  let test = tests.shift();
  if (!test) {
    cleanUp();
    return;
  }
  test();
}

function cleanUp() {
  finish();
}

SpecialPowers.pushPermissions(
  [{'type': 'settings', 'allow': true, 'context': document}], runNextTest);
