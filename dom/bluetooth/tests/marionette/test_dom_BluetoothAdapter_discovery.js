/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

///////////////////////////////////////////////////////////////////////////////
// Test Purpose:
//   To verify that discovery process of BluetoothAdapter is correct.
//   Use B2G emulator commands to add/remove remote devices to simulate
//   discovering behavior.
//
// Test Coverage:
//   - BluetoothAdapter.startDiscovery()
//   - BluetoothAdapter.stopDiscovery()
//   - BluetoothAdapter.ondevicefound()
//   - BluetoothAdapter.discovering [Temporarily turned off until BT API update]
//
///////////////////////////////////////////////////////////////////////////////

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

startBluetoothTest(true, function testCaseMain(aAdapter) {
  log("Testing the discovery process of BluetoothAdapter ...");

  return Promise.resolve()
    .then(() => removeEmulatorRemoteDevice(BDADDR_ALL))
    .then(() => addEmulatorRemoteDevice(null))
    .then(function(aRemoteAddress) {
      let promises = [];
      promises.push(waitForAdapterEvent(aAdapter, "devicefound"));
      promises.push(startDiscovery(aAdapter));
      return Promise.all(promises)
        .then(function(aResults) {
          is(aResults[0].device.address, aRemoteAddress, "BluetoothDevice.address");
        });
    })
    .then(() => stopDiscovery(aAdapter))
    .then(() => removeEmulatorRemoteDevice(BDADDR_ALL));
});
