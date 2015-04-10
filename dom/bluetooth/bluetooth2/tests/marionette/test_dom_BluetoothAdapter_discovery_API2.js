/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

///////////////////////////////////////////////////////////////////////////////
// Test Purpose:
//   To verify the discovery process of BluetoothAdapter.
//   Testers have to put the B2G devices in an environment which is surrounded
//   by N discoverable remote devices. To pass this test, the number N has to be
//   greater or equals than EXPECTED_NUMBER_OF_REMOTE_DEVICES.
//
// Test Procedure:
//   [0] Set Bluetooth permission and enable default adapter.
//   [1] Start discovery and verify the correctness.
//   [2] Attach event handler for 'ondevicefound'.
//   [3] Stop discovery and verify the correctness.
//   [4] Mark the BluetoothDiscoveryHandle from [1] as expired.
//   [5] Start discovery and verify the correctness.
//   [6] Wait for 'devicefound' events.
//   [7] Stop discovery and verify the correctness.
//   [8] Call 'startDiscovery' twice continuously.
//   [9] Call 'stopDiscovery' twice continuously.
//   [10] Clean up the event handler of [2].
//
// Test Coverage:
//   - BluetoothAdapter.discovering
//   - BluetoothAdapter.startDiscovery()
//   - BluetoothAdapter.stopDiscovery()
//   - BluetoothAdapter.onattributechanged()
//   - BluetoothDiscoveryHandle.ondevicefound()
//
///////////////////////////////////////////////////////////////////////////////

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

const EXPECTED_NUMBER_OF_REMOTE_DEVICES = 2;

startBluetoothTest(true, function testCaseMain(aAdapter) {
  log("Checking adapter attributes ...");

  is(aAdapter.state, "enabled", "adapter.state");
  isnot(aAdapter.address, "", "adapter.address");

  // Since adapter has just been re-enabled, these properties should be 'false'.
  is(aAdapter.discovering, false, "adapter.discovering");
  is(aAdapter.discoverable, false, "adapter.discoverable");

  log("adapter.address: " + aAdapter.address);
  log("adapter.name: " + aAdapter.name);

  let discoveryHandle = null;
  return Promise.resolve()
    .then(function() {
      log("[1] Start discovery and verify the correctness ... ");
      let promises = [];
      promises.push(waitForAdapterAttributeChanged(aAdapter, "discovering", true));
      promises.push(aAdapter.startDiscovery());
      return Promise.all(promises);
    })
    .then(function(aResults) {
      log("[2] Attach event handler for 'ondevicefound' ... ");
      discoveryHandle = aResults[1];
      isHandleExpired = false;
      discoveryHandle.ondevicefound = function onDeviceFound(aEvent) {
        if (isHandleExpired) {
          ok(false, "Expired BluetoothDiscoveryHandle received an event.");
        }
      };
    })
    .then(function() {
      log("[3] Stop discovery and and verify the correctness ... ");
      let promises = [];
      if (aAdapter.discovering) {
        promises.push(waitForAdapterAttributeChanged(aAdapter, "discovering", false));
      }
      promises.push(aAdapter.stopDiscovery());
      return Promise.all(promises);
    })
    .then(function() {
      log("[4] Mark the BluetoothDiscoveryHandle from [1] as expired ... ");
      isHandleExpired = true;
    })
    .then(function() {
      log("[5] Start discovery and verify the correctness ... ");
      let promises = [];
      promises.push(waitForAdapterAttributeChanged(aAdapter, "discovering", true));
      promises.push(aAdapter.startDiscovery());
      return Promise.all(promises);
    })
    .then(function(aResults) {
      log("[6] Wait for 'devicefound' events ... ");
      return waitForDevicesFound(aResults[1], EXPECTED_NUMBER_OF_REMOTE_DEVICES);
    })
    .then(function() {
      log("[7] Stop discovery and and verify the correctness ... ");
      let promises = [];
      if (aAdapter.discovering) {
        promises.push(waitForAdapterAttributeChanged(aAdapter, "discovering", false));
      }
      promises.push(aAdapter.stopDiscovery());
      return Promise.all(promises);
    })
    .then(function() {
      log("[8] Call 'startDiscovery' twice continuously ... ");
      return aAdapter.startDiscovery()
        .then(() => aAdapter.startDiscovery())
        .then(() => ok(false, "Call startDiscovery() when adapter is discovering. - Fail"),
              () => ok(true, "Call startDiscovery() when adapter is discovering. - Success"));
    })
    .then(function() {
      log("[9] Call 'stopDiscovery' twice continuously ... ");
      return aAdapter.stopDiscovery()
        .then(() => aAdapter.stopDiscovery())
        .then(() => ok(true, "Call stopDiscovery() when adapter isn't discovering. - Success"),
              () => ok(false, "Call stopDiscovery() when adapter isn't discovering. - Fail"));
    })
    .then(function() {
      log("[10] Clean up the event handler of [2] ... ");
      if (discoveryHandle) {
        discoveryHandle.ondevicefound = null;
      }
    });
});
