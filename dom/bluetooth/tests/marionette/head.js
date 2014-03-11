/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 sts=2 et filetype=javascript
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

let Promise =
  SpecialPowers.Cu.import("resource://gre/modules/Promise.jsm").Promise;

let bluetoothManager;

/**
 * Get mozSettings value specified by @aKey.
 *
 * Resolve if that mozSettings value is retrieved successfully, reject
 * otherwise.
 *
 * Fulfill params:
 *   The corresponding mozSettings value of the key.
 * Reject params: (none)
 *
 * @param aKey
 *        A string.
 *
 * @return A deferred promise.
 */
function getSettings(aKey) {
  let deferred = Promise.defer();

  let request = navigator.mozSettings.createLock().get(aKey);
  request.addEventListener("success", function(aEvent) {
    ok(true, "getSettings(" + aKey + ")");
    deferred.resolve(aEvent.target.result[aKey]);
  });
  request.addEventListener("error", function() {
    ok(false, "getSettings(" + aKey + ")");
    deferred.reject();
  });

  return deferred.promise;
}

/**
 * Set mozSettings values.
 *
 * Resolve if that mozSettings value is set successfully, reject otherwise.
 *
 * Fulfill params: (none)
 * Reject params: (none)
 *
 * @param aSettings
 *        An object of format |{key1: value1, key2: value2, ...}|.
 *
 * @return A deferred promise.
 */
function setSettings(aSettings) {
  let deferred = Promise.defer();

  let request = navigator.mozSettings.createLock().set(aSettings);
  request.addEventListener("success", function() {
    ok(true, "setSettings(" + JSON.stringify(aSettings) + ")");
    deferred.resolve();
  });
  request.addEventListener("error", function() {
    ok(false, "setSettings(" + JSON.stringify(aSettings) + ")");
    deferred.reject();
  });

  return deferred.promise;
}

/**
 * Get mozSettings value of 'bluetooth.enabled'.
 *
 * Resolve if that mozSettings value is retrieved successfully, reject
 * otherwise.
 *
 * Fulfill params:
 *   A boolean value.
 * Reject params: (none)
 *
 * @return A deferred promise.
 */
function getBluetoothEnabled() {
  return getSettings("bluetooth.enabled");
}

/**
 * Set mozSettings value of 'bluetooth.enabled'.
 *
 * Resolve if that mozSettings value is set successfully, reject otherwise.
 *
 * Fulfill params: (none)
 * Reject params: (none)
 *
 * @param aEnabled
 *        A boolean value.
 *
 * @return A deferred promise.
 */
function setBluetoothEnabled(aEnabled) {
  let obj = {};
  obj["bluetooth.enabled"] = aEnabled;
  return setSettings(obj);
}

/**
 * Push required permissions and test if |navigator.mozBluetooth| exists.
 * Resolve if it does, reject otherwise.
 *
 * Fulfill params:
 *   bluetoothManager -- an reference to navigator.mozBluetooth.
 * Reject params: (none)
 *
 * @param aPermissions
 *        Additional permissions to push before any test cases.  Could be either
 *        a string or an array of strings.
 *
 * @return A deferred promise.
 */
function ensureBluetoothManager(aPermissions) {
  let deferred = Promise.defer();

  let permissions = ["bluetooth"];
  if (aPermissions) {
    if (Array.isArray(aPermissions)) {
      permissions = permissions.concat(aPermissions);
    } else if (typeof aPermissions == "string") {
      permissions.push(aPermissions);
    }
  }

  let obj = [];
  for (let perm of permissions) {
    obj.push({
      "type": perm,
      "allow": 1,
      "context": document,
    });
  }

  SpecialPowers.pushPermissions(obj, function() {
    ok(true, "permissions pushed: " + JSON.stringify(permissions));

    bluetoothManager = window.navigator.mozBluetooth;
    log("navigator.mozBluetooth is " +
        (bluetoothManager ? "available" : "unavailable"));

    if (bluetoothManager instanceof BluetoothManager) {
      deferred.resolve(bluetoothManager);
    } else {
      deferred.reject();
    }
  });

  return deferred.promise;
}

/**
 * Wait for one named BluetoothManager event.
 *
 * Resolve if that named event occurs.  Never reject.
 *
 * Fulfill params: the DOMEvent passed.
 *
 * @return A deferred promise.
 */
function waitForManagerEvent(aEventName) {
  let deferred = Promise.defer();

  bluetoothManager.addEventListener(aEventName, function onevent(aEvent) {
    bluetoothManager.removeEventListener(aEventName, onevent);

    ok(true, "BluetoothManager event '" + aEventName + "' got.");
    deferred.resolve(aEvent);
  });

  return deferred.promise;
}

/**
 * Convenient function for setBluetoothEnabled and waitForManagerEvent
 * combined.
 *
 * Resolve if that named event occurs.  Reject if we can't set settings.
 *
 * Fulfill params: the DOMEvent passed.
 * Reject params: (none)
 *
 * @return A deferred promise.
 */
function setBluetoothEnabledAndWait(aEnabled) {
  let promises = [];

  // Bug 969109 -  Intermittent test_dom_BluetoothManager_adapteradded.js
  //
  // Here we want to wait for two events coming up -- Bluetooth "settings-set"
  // event and one of "enabled"/"disabled" events.  Special care is taken here
  // to ensure that we can always receive that "enabled"/"disabled" event by
  // installing the event handler *before* we ever enable/disable Bluetooth. Or
  // we might just miss those events and get a timeout error.
  promises.push(waitForManagerEvent(aEnabled ? "enabled" : "disabled"));
  promises.push(setBluetoothEnabled(aEnabled));

  return Promise.all(promises);
}

/**
 * Get default adapter.
 *
 * Resolve if that default adapter is got, reject otherwise.
 *
 * Fulfill params: a BluetoothAdapter instance.
 * Reject params: a DOMError, or null if if there is no adapter ready yet.
 *
 * @return A deferred promise.
 */
function getDefaultAdapter() {
  let deferred = Promise.defer();

  let request = bluetoothManager.getDefaultAdapter();
  request.onsuccess = function(aEvent) {
    let adapter = aEvent.target.result;
    if (!(adapter instanceof BluetoothAdapter)) {
      ok(false, "no BluetoothAdapter ready yet.");
      deferred.reject(null);
      return;
    }

    ok(true, "BluetoothAdapter got.");
    // TODO: We have an adapter instance now, but some of its attributes may
    // still remain unassigned/out-dated.  Here we waste a few seconds to
    // wait for the property changed events.
    //
    // See https://bugzilla.mozilla.org/show_bug.cgi?id=932914
    window.setTimeout(function() {
      deferred.resolve(adapter);
    }, 3000);
  };
  request.onerror = function(aEvent) {
    ok(false, "Failed to get default adapter.");
    deferred.reject(aEvent.target.error);
  };

  return deferred.promise;
}

/**
 * Flush permission settings and call |finish()|.
 */
function cleanUp() {
  SpecialPowers.flushPermissions(function() {
    // Use ok here so that we have at least one test run.
    ok(true, "permissions flushed");

    finish();
  });
}

function startBluetoothTestBase(aPermissions, aTestCaseMain) {
  ensureBluetoothManager(aPermissions)
    .then(aTestCaseMain)
    .then(cleanUp, function() {
      ok(false, "Unhandled rejected promise.");
      cleanUp();
    });
}

function startBluetoothTest(aReenable, aTestCaseMain) {
  startBluetoothTestBase(["settings-read", "settings-write"], function() {
    let origEnabled, needEnable;

    return getBluetoothEnabled()
      .then(function(aEnabled) {
        origEnabled = aEnabled;
        needEnable = !aEnabled;
        log("Original 'bluetooth.enabled' is " + origEnabled);

        if (aEnabled && aReenable) {
          log("  Disable 'bluetooth.enabled' ...");
          needEnable = true;
          return setBluetoothEnabledAndWait(false);
        }
      })
      .then(function() {
        if (needEnable) {
          log("  Enable 'bluetooth.enabled' ...");

          // See setBluetoothEnabledAndWait().  We must install all event
          // handlers *before* enabling Bluetooth.
          let promises = [];
          promises.push(waitForManagerEvent("adapteradded"));
          promises.push(setBluetoothEnabledAndWait(true));
          return Promise.all(promises);
        }
      })
      .then(getDefaultAdapter)
      .then(aTestCaseMain)
      .then(function() {
        if (!origEnabled) {
          return setBluetoothEnabledAndWait(false);
        }
      });
  });
}
