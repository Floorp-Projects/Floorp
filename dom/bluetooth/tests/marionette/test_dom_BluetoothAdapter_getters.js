/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

///////////////////////////////////////////////////////////////////////////////
// Test Purpose:
//   To verify that the properties of BluetoothAdapter can be updated and
//   retrieved correctly. Use B2G emulator commands to set properties for this
//   test case.
//
// Test Coverage:
//   - BluetoothAdapter.name
//   - BluetoothAdapter.address
//   - BluetoothAdapter.class
//   - BluetoothAdapter.discoverable
//   - BluetoothAdapter.discovering
//   ( P.S. Don't include [BluetoothAdapter.uuids], [BluetoothAdapter.devices] )
//
///////////////////////////////////////////////////////////////////////////////

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

function testAdapterGetter(aAdapter, aPropertyName, aParamName, aExpected) {
  let cmd = "bt property " + BDADDR_LOCAL + " " + aParamName;
  return runEmulatorCmdSafe(cmd)
    .then(function(aResults) {
      is(aResults[1], "OK", "The status report from emulator command.");
      log("  Got adapter " + aResults[0]);
      is(aResults[0], aParamName + ": " + aExpected, "BluetoothAdapter." + aPropertyName);
    });
}

startBluetoothTest(true, function testCaseMain(aAdapter) {
  log("Checking the correctness of BluetoothAdapter properties ...");

  return Promise.resolve()
    .then(() => testAdapterGetter(aAdapter, "name",         "name",         aAdapter.name))
    .then(() => testAdapterGetter(aAdapter, "address",      "address",      aAdapter.address))
    .then(() => testAdapterGetter(aAdapter, "class",        "cod",          "0x" + aAdapter.class.toString(16)))
    .then(() => testAdapterGetter(aAdapter, "discoverable", "discoverable", aAdapter.discoverable.toString()))
    .then(() => testAdapterGetter(aAdapter, "discovering",  "discovering",  aAdapter.discovering.toString()));
});
