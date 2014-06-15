/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

function waitEitherEnabledOrDisabled() {
  let deferred = Promise.defer();

  function onEnabledDisabled(aEvent) {
    bluetoothManager.removeEventListener("adapteradded", onEnabledDisabled);
    bluetoothManager.removeEventListener("disabled", onEnabledDisabled);

    ok(true, "Got event " + aEvent.type);
    deferred.resolve(aEvent.type === "adapteradded");
  }

  // Listen 'adapteradded' rather than 'enabled' since the current API can't
  // disable BT before the BT adapter is initialized.
  // We should listen to 'enabled' when gecko can handle the case I mentioned
  // above, please refer to the follow-up bug 973482.
  bluetoothManager.addEventListener("adapteradded", onEnabledDisabled);
  bluetoothManager.addEventListener("disabled", onEnabledDisabled);

  return deferred.promise;
}

function test(aEnabled) {
  log("Testing 'bluetooth.enabled' => " + aEnabled);

  let deferred = Promise.defer();

  // Ensures that we can always receive that "enabled"/"disabled" event by
  // installing the event handler *before* we ever enable/disable Bluetooth. Or
  // we might just miss those events and get a timeout error.
  let promises = [];
  promises.push(waitEitherEnabledOrDisabled());
  promises.push(setBluetoothEnabled(aEnabled));
  Promise.all(promises)
    .then(function(aResults) {
      /* aResults is an array of two elements:
       *   [ <result of waitEitherEnabledOrDisabled>,
       *     <result of setBluetoothEnabled>]
       */
      log("  Examine results " + JSON.stringify(aResults));

      is(bluetoothManager.enabled, aEnabled, "bluetoothManager.enabled");
      is(aResults[0], aEnabled, "'adapteradded' event received");

      if (bluetoothManager.enabled === aEnabled && aResults[0] === aEnabled) {
        deferred.resolve();
      } else {
        deferred.reject();
      }
    });

  return deferred.promise;
}

startBluetoothTestBase(["settings-read", "settings-write"],
                       function testCaseMain() {
  return getBluetoothEnabled()
    .then(function(aEnabled) {
      log("Original 'bluetooth.enabled' is " + aEnabled);
      // Set to !aEnabled and reset back to aEnabled.
      return test(!aEnabled).then(test.bind(null, aEnabled));
    });
});
