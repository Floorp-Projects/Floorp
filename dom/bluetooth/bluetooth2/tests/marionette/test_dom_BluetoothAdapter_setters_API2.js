/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

///////////////////////////////////////////////////////////////////////////////
// Test Purpose:
//   To verify that all setters of BluetoothAdapter (except for pairing related
//   APIs) can change properties correctly.
//
// Test Procedure:
//   [0] Set Bluetooth permission and enable default adapter.
//   [1] Verify the functionality of 'setName'.
//   [2] Verify the functionality of 'setDiscoverable'.
//   [3] Disable Bluetooth and collect changed attributes.
//   [4] Verify the changes of attributes when BT is disabled.
//   [5] Set properties when Bluetooth is disabled.
//   [6] Enable Bluetooth and collect changed attributes.
//   [7] Verify the changes of attributes when BT is enabled.
//   [8] Restore the original 'adapter.name'.
//   [9] Check the validity of setting properties to their present value.
//
// Test Coverage:
//   - BluetoothAdapter.setName()
//   - BluetoothAdapter.setDiscoverable()
//   - BluetoothAdapter.onattributechanged()
//   - BluetoothAdapter.state
//   - BluetoothAdapter.name
//   - BluetoothAdapter.discoverable
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
  is(aAdapter.discoverable, false, "adapter.discoverable at step [0]");

  log("adapter.address: " + aAdapter.address);
  log("adapter.name: " + aAdapter.name);

  let originalName = aAdapter.name;
  return Promise.resolve()
    .then(function() {
      log("[1] Set 'name' ... ");
      let promises = [];
      promises.push(waitForAdapterAttributeChanged(aAdapter, "name", originalName + "_modified"));
      promises.push(aAdapter.setName(originalName + "_modified"));
      return Promise.all(promises);
    })
    .then(function() {
      log("[2] Set 'discoverable' ... ");
      let promises = [];
      promises.push(waitForAdapterAttributeChanged(aAdapter, "discoverable", !aAdapter.discoverable));
      promises.push(aAdapter.setDiscoverable(!aAdapter.discoverable));
      return Promise.all(promises);
    })
    .then(function() {
      log("[3] Disable Bluetooth ...");
      let promises = [];
      promises.push(waitForAdapterStateChanged(aAdapter, ["disabling", "disabled"]));
      promises.push(aAdapter.disable());
      return Promise.all(promises);
    })
    .then(function(aResults) {
      log("[4] Verify the changes of attributes ...");
      isnot(aResults[0].indexOf("name"), -1, "Indicator of 'name' changed event");
      isnot(aResults[0].indexOf("discoverable"), -1, "Indicator of 'discoverable' changed event");
      is(aAdapter.name, "", "adapter.name");
      is(aAdapter.discoverable, false, "adapter.discoverable at step [4]");
    })
    .then(() => log("[5] Set properties when Bluetooth is disabled ..."))
    .then(() => aAdapter.setName(originalName))
    .then(() => ok(false, "Failed to handle 'setName' when BT is disabled."),
          () => aAdapter.setDiscoverable(true))
    .then(() => ok(false, "Failed to handle 'setDiscoverable' when BT is disabled."),
          () => null)
    .then(function() {
      log("[6] Enable Bluetooth ...");
      let promises = [];
      promises.push(waitForAdapterStateChanged(aAdapter, ["enabling", "enabled"]));
      promises.push(aAdapter.enable());
      return Promise.all(promises);
    })
    .then(function(aResults) {
      log("[7] Verify the changes of attributes ...");
      isnot(aResults[0].indexOf("name"), -1, "Indicator of 'name' changed event");
      is(aAdapter.name, originalName + "_modified", "adapter.name");
      is(aAdapter.discoverable, false, "adapter.discoverable at step [7]");
    })
    .then(function() {
      log("[8] Restore the original 'name' ...");
      let promises = [];
      promises.push(waitForAdapterAttributeChanged(aAdapter, "name", originalName));
      promises.push(aAdapter.setName(originalName));
      return Promise.all(promises);
    })
    .then(function() {
      log("[9] Check the validity of setting properties to their present value ...");
      let promises = [];
      promises.push(aAdapter.setName(aAdapter.name));
      promises.push(aAdapter.setDiscoverable(aAdapter.discoverable));
      return Promise.all(promises);
    });
});
