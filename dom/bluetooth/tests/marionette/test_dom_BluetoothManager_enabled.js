/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 sts=2 et filetype=javascript
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

let enabledEventReceived;
function onEnabled() {
  enabledEventReceived = true;
}

let disabledEventReceived;
function onDisabled() {
  disabledEventReceived = true;
}

function test(aEnabled) {
  log("Testing 'bluetooth.enabled' => " + aEnabled);

  let deferred = Promise.defer();

  enabledEventReceived = false;
  disabledEventReceived = false;

  setBluetoothEnabled(aEnabled).then(function() {
    log("  Settings set. Waiting 3 seconds and examine results.");

    window.setTimeout(function() {
      is(bluetoothManager.enabled, aEnabled, "bluetoothManager.enabled");
      is(enabledEventReceived, aEnabled, "enabledEventReceived");
      is(disabledEventReceived, !aEnabled, "disabledEventReceived");

      if (bluetoothManager.enabled === aEnabled &&
          enabledEventReceived === aEnabled &&
          disabledEventReceived === !aEnabled) {
        deferred.resolve();
      } else {
        deferred.reject();
      }
    }, 3000);
  });

  return deferred.promise;
}

startBluetoothTestBase(["settings-read", "settings-write"],
                       function testCaseMain() {
  bluetoothManager.addEventListener("enabled", onEnabled);
  bluetoothManager.addEventListener("disabled", onDisabled);

  return getBluetoothEnabled()
    .then(function(aEnabled) {
      log("Original 'bluetooth.enabled' is " + aEnabled);
      // Set to !aEnabled and reset back to aEnabled.
      return test(!aEnabled).then(test.bind(null, aEnabled));
    })
    .then(function() {
      bluetoothManager.removeEventListener("enabled", onEnabled);
      bluetoothManager.removeEventListener("disabled", onDisabled);
    });
});
