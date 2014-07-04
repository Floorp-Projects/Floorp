/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

///////////////////////////////////////////////////////////////////////////////
// Test Purpose:
//   To verify the basic functionality of BluetoothManager.
//
// Test Coverage:
//   - BluetoothManager.defaultAdapter
//   - BluetoothManager.getAdapters()
// TODO:
//   - BluetoothManager.onattributechanged()
//   - BluetoothManager.onadapteradded()
//   - BluetoothManager.onadapterremoved()
//
///////////////////////////////////////////////////////////////////////////////

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

// TODO: Listens to 'onattributechanged' when B2G supports the feature that
//       allows user to add/remove Bluetooth adapter.
// Currently, B2G recognizes build-in Bluetooth chip as default adapter and
// don't support adding additional Bluetooth dongles in gonk layer.
// Therefore, the 'onattributechanged' would be triggered *only* when the
// instance of BluetoothManager is created.
function waitForManagerAttributeChanged() {
  let deferred = Promise.defer();

  bluetoothManager.onattributechanged = function(aEvent) {
    if(aEvent.attrs.indexOf("defaultAdapter")) {
      bluetoothManager.onattributechanged = null;
      ok(true, "BluetoothManager event 'onattributechanged' got.");
      deferred.resolve(aEvent);
    }
  };

  return deferred.promise;
}

startBluetoothTestBase(["settings-read", "settings-write"],
                       function testCaseMain() {
  let adapters = bluetoothManager.getAdapters();
  ok(Array.isArray(adapters), "Can not got the array of adapters");
  ok(adapters.length, "The number of adapters should not be zero");
  ok(bluetoothManager.defaultAdapter, "defaultAdapter should not be null.");
});
