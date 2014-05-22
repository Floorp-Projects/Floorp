/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 sts=2 et filetype=javascript
 * This Source Code Form is subject to the terms of the Mozilla Public
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
    .then(function(aRemoteAddress) {
      let promises = [];
      promises.push(waitForAdapterEvent(aAdapter, "devicefound"));
      promises.push(startDiscovery(aAdapter));
      return Promise.all(promises)
        .then(function(aResults) {
          is(aResults[0].device.address, aRemoteAddress, "BluetoothDevice.address");
          return aResults[0].device.address;
        });
    })
    .then(function(aRemoteAddress) {
      let promises = [];
      promises.push(stopDiscovery(aAdapter));
      promises.push(waitForAdapterEvent(aAdapter, "pairedstatuschanged"));
      promises.push(pair(aAdapter, aRemoteAddress));
      return Promise.all(promises);
    })
    .then(() => getPairedDevices(aAdapter))
    .then((aPairedDevices) => unpair(aAdapter, aPairedDevices.pop().address))
    .then(() => removeEmulatorRemoteDevice(BDADDR_ALL));
});
