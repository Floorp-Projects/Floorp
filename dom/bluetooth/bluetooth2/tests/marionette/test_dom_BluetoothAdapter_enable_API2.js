/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

///////////////////////////////////////////////////////////////////////////////
// Test Purpose:
//   To verify that enable/disable process of BluetoothAdapter is correct.
//
// Test Procedure:
//   [0] Set Bluetooth permission and enable default adapter.
//   [1] Disable Bluetooth and check the correctness of 'onattributechanged'.
//   [2] Enable Bluetooth and check the correctness of 'onattributechanged'.
//
// Test Coverage:
//   - BluetoothAdapter.enable()
//   - BluetoothAdapter.disable()
//   - BluetoothAdapter.onattributechanged()
//   - BluetoothAdapter.address
//   - BluetoothAdapter.state
//
///////////////////////////////////////////////////////////////////////////////

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

startBluetoothTest(true, function testCaseMain(aAdapter) {
  log("Checking adapter attributes ...");

  is(aAdapter.state, "enabled", "adapter.state");
  isnot(aAdapter.address, "", "adapter.address");

  // Since adapter has just been re-enabled, these properties should be 'false'.
  is(aAdapter.discovering, false, "adapter.discovering");
  is(aAdapter.discoverable, false, "adapter.discoverable");
  // TODO: Check the correctness of name and address if we use emulator.
  // is(aAdapter.name, EMULATOR_NAME, "adapter.name");
  // is(aAdapter.address, EMULATOR_ADDRESS, "adapter.address");

  log("  adapter.address: " + aAdapter.address);
  log("  adapter.name: " + aAdapter.name);

  let originalAddr = aAdapter.address;
  let originalName = aAdapter.name;

  return Promise.resolve()
    .then(function() {
      log("[1] Disable Bluetooth and check the correctness of 'onattributechanged'");
      let promises = [];
      promises.push(waitForAdapterStateChanged(aAdapter, ["disabling", "disabled"]));
      promises.push(aAdapter.disable());
      return Promise.all(promises);
    })
    .then(function(aResults) {
      isnot(aResults[0].indexOf("address"), -1, "Indicator of 'address' changed event");
      if (originalName != "") {
        isnot(aResults[0].indexOf("name"), -1, "Indicator of 'name' changed event");
      }
      is(aAdapter.address, "", "adapter.address");
      is(aAdapter.name, "", "adapter.name");
    })
    .then(function() {
      log("[2] Enable Bluetooth and check the correctness of 'onattributechanged'");
      let promises = [];
      promises.push(waitForAdapterStateChanged(aAdapter, ["enabling", "enabled"]));
      promises.push(aAdapter.enable());
      return Promise.all(promises);
    })
    .then(function(aResults) {
      isnot(aResults[0].indexOf("address"), -1, "Indicator of 'address' changed event");
      if (originalName != "") {
        isnot(aResults[0].indexOf("name"), -1, "Indicator of 'name' changed event");
      }
      is(aAdapter.address, originalAddr, "adapter.address");
      is(aAdapter.name, originalName, "adapter.name");
    })
});
