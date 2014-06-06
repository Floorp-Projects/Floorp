/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

///////////////////////////////////////////////////////////////////////////////
// Test Purpose:
//   To verify that pairing process of BluetoothAdapter is correct.
//   Use B2G emulator commands to add/remove remote devices to simulate
//   discovering behavior. With current emulator implemation, the pair method
//   between adapter and remote device would be 'confirmation'.
//
// Test Coverage:
//   - BluetoothAdapter.startDiscovery()
//   - BluetoothAdapter.stopDiscovery()
//   - BluetoothAdapter.pair()
//   - BluetoothAdapter.unpair()
//   - BluetoothAdapter.onpairedstatuschanged()
//   - BluetoothAdapter.setPairingConfirmation()
//
///////////////////////////////////////////////////////////////////////////////

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

function replyPairingReq(aAdapter, aPairingEvent) {
  switch (aPairingEvent.method) {
    case 'confirmation':
      log("The pairing passkey is " + aPairingEvent.passkey);
      aAdapter.setPairingConfirmation(aPairingEvent.address, true);
      break;
    case 'pincode':
      let pincode = BT_PAIRING_PINCODE;
      aAdapter.setPinCode(aPairingEvent.address, pincode);
      break;
    case 'passkey':
      let passkey = BT_PAIRING_PASSKEY;
      aAdapter.setPasskey(aPairingEvent.address, passkey);
      break;
    default:
      ok(false, "Unsupported pairing method. [" + aPairingEvent.method + "]");
  }
}

startBluetoothTest(true, function testCaseMain(aAdapter) {
  log("Testing the pairing process of BluetoothAdapter ...");

  // listens to the system message BT_PAIRING_REQ
  navigator.mozSetMessageHandler(BT_PAIRING_REQ,
    (evt) => replyPairingReq(aAdapter, evt));

  return Promise.resolve()
    .then(() => removeEmulatorRemoteDevice(BDADDR_ALL))
    .then(() => addEmulatorRemoteDevice())
    .then((aRemoteAddress) =>
          startDiscoveryAndWaitDevicesFound(aAdapter, [aRemoteAddress]))
    .then((aRemoteAddresses) =>
          stopDiscovery(aAdapter).then(() => aRemoteAddresses))
    // 'aRemoteAddresses' is an arrary which contains addresses of discovered
    // remote devices.
    // Pairs with the first device in 'aRemoteAddresses' to test the functionality
    // of BluetoothAdapter.pair and BluetoothAdapter.onpairedstatuschanged.
    .then((aRemoteAddresses) => pairDeviceAndWait(aAdapter, aRemoteAddresses.pop()))
    .then(() => getPairedDevices(aAdapter))
    .then((aPairedDevices) => unpair(aAdapter, aPairedDevices.pop().address))
    .then(() => removeEmulatorRemoteDevice(BDADDR_ALL));
});
