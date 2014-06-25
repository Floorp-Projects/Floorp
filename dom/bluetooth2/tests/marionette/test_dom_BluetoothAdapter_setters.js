/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=2 ts=2 sts=2 et filetype=javascript
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

///////////////////////////////////////////////////////////////////////////////
// Test Purpose:
//   To verify that all setters of BluetoothAdapter (except for pairing related
//   APIs) can change properties correctly.
//
// Test Coverage:
//   - BluetoothAdapter.setName()
//   - BluetoothAdapter.setDiscoverable()
//   - BluetoothAdapter.setDiscoverableTimeout()
//
///////////////////////////////////////////////////////////////////////////////

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

const BT_DEVICE_NAME = "User friendly name of local BT device";

function setName(aAdapter, aName) {
  let deferred = Promise.defer();

  let request = aAdapter.setName(aName);
  request.onsuccess = function () {
    log("  setName succeed: " + aName);
    is(aAdapter.name, aName, "aAdapter.name");
    deferred.resolve();
  }
  request.onerror = function () {
    ok(false, "setName failed")
    deferred.reject();
  }

  return deferred.promise;
}

function setDiscoverable(aAdapter, aIsDiscoverable) {
  let deferred = Promise.defer();

  let request = aAdapter.setDiscoverable(aIsDiscoverable);
  request.onsuccess = function () {
    log("  setDiscoverable succeed: " + aIsDiscoverable);
    is(aAdapter.discoverable, aIsDiscoverable, "aAdapter.discoverable");
    deferred.resolve();
  }
  request.onerror = function () {
    ok(false, "setDiscoverable failed")
    deferred.reject();
  }

  return deferred.promise;
}

function setDiscoverableTimeout(aAdapter, aTimeout) {
  let deferred = Promise.defer();

  let request = aAdapter.setDiscoverableTimeout(aTimeout);
  request.onsuccess = function () {
    log("  setDiscoverableTimeout succeed: " + aTimeout);
    is(aAdapter.discoverableTimeout, aTimeout, "aAdapter.discoverableTimeout");
    deferred.resolve();
  }
  request.onerror = function () {
    ok(false, "setDiscoverableTimeout failed")
    deferred.reject();
  }

  return deferred.promise;
}

startBluetoothTest(true, function testCaseMain(aAdapter) {
  log("Testing BluetoothAdapter setters ...");

  return Promise.resolve()
    .then( () => setName(aAdapter, BT_DEVICE_NAME) )
    .then( () => setDiscoverableTimeout(aAdapter, 180) )
    .then( () => setDiscoverable(aAdapter, true) )
    .then( () => setName(aAdapter, EMULATOR_NAME) )
    .then( () => setDiscoverable(aAdapter, false) )
    .then( () => setDiscoverableTimeout(aAdapter, 120) );
});
