/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// https://github.com/mozilla-b2g/platform_external_qemu/blob/master/vl-android.c#L765
// static int bt_hci_parse(const char *str) {
//   ...
//   bdaddr.b[0] = 0x52;
//   bdaddr.b[1] = 0x54;
//   bdaddr.b[2] = 0x00;
//   bdaddr.b[3] = 0x12;
//   bdaddr.b[4] = 0x34;
//   bdaddr.b[5] = 0x56 + nb_hcis;
const EMULATOR_ADDRESS = "56:34:12:00:54:52";

// $ adb shell hciconfig /dev/ttyS2 name
// hci0:  Type: BR/EDR  Bus: UART
//        BD Address: 56:34:12:00:54:52  ACL MTU: 512:1  SCO MTU: 0:0
//        Name: 'Full Android on Emulator'
const EMULATOR_NAME = "Full Android on Emulator";

// $ adb shell hciconfig /dev/ttyS2 class
// hci0:  Type: BR/EDR  Bus: UART
//        BD Address: 56:34:12:00:54:52  ACL MTU: 512:1  SCO MTU: 0:0
//        Class: 0x58020c
//        Service Classes: Capturing, Object Transfer, Telephony
//        Device Class: Phone, Smart phone
const EMULATOR_CLASS = 0x58020c;

// Use same definition in QEMU for special bluetooth address,
// which were defined at external/qemu/hw/bt.h:
const BDADDR_ANY   = "00:00:00:00:00:00";
const BDADDR_ALL   = "ff:ff:ff:ff:ff:ff";
const BDADDR_LOCAL = "ff:ff:ff:00:00:00";

// A user friendly name for remote BT device.
const REMOTE_DEVICE_NAME = "Remote_BT_Device";

// A system message signature of pairing request event
const BT_PAIRING_REQ = "bluetooth-pairing-request";

// Passkey and pincode used to reply pairing requst
const BT_PAIRING_PASSKEY = 123456;
const BT_PAIRING_PINCODE = "ABCDEFG";

// Emulate Promise.jsm semantics.
Promise.defer = function() { return new Deferred(); }
function Deferred()  {
  this.promise = new Promise(function(resolve, reject) {
    this.resolve = resolve;
    this.reject = reject;
  }.bind(this));
  Object.freeze(this);
}

let bluetoothManager;

let pendingEmulatorCmdCount = 0;

/**
 * Send emulator command with safe guard.
 *
 * We should only call |finish()| after all emulator command transactions
 * end, so here comes with the pending counter.  Resolve when the emulator
 * gives positive response, and reject otherwise.
 *
 * Fulfill params:
 *   result -- an array of emulator response lines.
 *
 * Reject params:
 *   result -- an array of emulator response lines.
 *
 * @return A deferred promise.
 */
function runEmulatorCmdSafe(aCommand) {
  let deferred = Promise.defer();

  ++pendingEmulatorCmdCount;
  runEmulatorCmd(aCommand, function(aResult) {
    --pendingEmulatorCmdCount;

    ok(true, "Emulator response: " + JSON.stringify(aResult));
    if (Array.isArray(aResult) && aResult[aResult.length - 1] === "OK") {
      deferred.resolve(aResult);
    } else {
      ok(false, "Got an abnormal response from emulator.");
      log("Fail to execute emulator command: [" + aCommand + "]");
      deferred.reject(aResult);
    }
  });

  return deferred.promise;
}

/**
 * Wrap DOMRequest onsuccess/onerror events to Promise resolve/reject.
 *
 * Fulfill params: A DOMEvent.
 * Reject params: A DOMEvent.
 *
 * @param aRequest
 *        A DOMRequest instance.
 *
 * @return A deferred promise.
 */
function wrapDomRequestAsPromise(aRequest) {
  let deferred = Promise.defer();

  ok(aRequest instanceof DOMRequest,
     "aRequest is instanceof " + aRequest.constructor);

  aRequest.onsuccess = function(aEvent) {
    deferred.resolve(aEvent);
  };
  aRequest.onerror = function(aEvent) {
    deferred.reject(aEvent);
  };

  return deferred.promise;
}

/**
 * Add a Bluetooth remote device to scatternet and set its properties.
 *
 * Use QEMU command 'bt remote add' to add a virtual Bluetooth remote
 * and set its properties by setEmulatorDeviceProperty().
 *
 * Fulfill params:
 *   result -- bluetooth address of the remote device.
 * Reject params: (none)
 *
 * @param aProperies
 *        A javascript object with zero or several properties for initializing
 *        the remote device. By now, the properies could be 'name' or
 *        'discoverable'. It valid to put a null object or a javascript object
 *        which don't have any properies.
 *
 * @return A promise object.
 */
function addEmulatorRemoteDevice(aProperties) {
  let address;
  let promise = runEmulatorCmdSafe("bt remote add")
    .then(function(aResults) {
      address = aResults[0].toUpperCase();
    });

  for (let key in aProperties) {
    let value = aProperties[key];
    let propertyName = key;
    promise = promise.then(function() {
      return setEmulatorDeviceProperty(address, propertyName, value);
    });
  }

  return promise.then(function() {
    return address;
  });
}

/**
 * Remove Bluetooth remote devices in scatternet.
 *
 * Use QEMU command 'bt remote remove <addr>' to remove a specific virtual
 * Bluetooth remote device in scatternet or remove them all by  QEMU command
 * 'bt remote remove BDADDR_ALL'.
 *
 * @param aAddress
 *        The string of Bluetooth address with format xx:xx:xx:xx:xx:xx.
 *
 * Fulfill params:
 *   result -- an array of emulator response lines.
 * Reject params: (none)
 *
 * @return A promise object.
 */
function removeEmulatorRemoteDevice(aAddress) {
  let cmd = "bt remote remove " + aAddress;
  return runEmulatorCmdSafe(cmd)
    .then(function(aResults) {
      // 'bt remote remove <bd_addr>' returns a list of removed device one at a line.
      // The last item is "OK".
      return aResults.slice(0, -1);
    });
}

/**
 * Set a property for a Bluetooth device.
 *
 * Use QEMU command 'bt property <bd_addr> <prop_name> <value>' to set property.
 *
 * Fulfill params:
 *   result -- an array of emulator response lines.
 * Reject params:
 *   result -- an array of emulator response lines.
 *
 * @param aAddress
 *        The string of Bluetooth address with format xx:xx:xx:xx:xx:xx.
 * @param aPropertyName
 *        The property name of Bluetooth device.
 * @param aValue
 *        The new value of the specifc property.
 *
 * @return A deferred promise.
 */
function setEmulatorDeviceProperty(aAddress, aPropertyName, aValue) {
  let cmd = "bt property " + aAddress + " " + aPropertyName + " " + aValue;
  return runEmulatorCmdSafe(cmd);
}

/**
 * Get a property from a Bluetooth device.
 *
 * Use QEMU command 'bt property <bd_addr> <prop_name>' to get properties.
 *
 * Fulfill params:
 *   result -- a string with format <prop_name>: <value_of_prop>
 * Reject params:
 *   result -- an array of emulator response lines.
 *
 * @param aAddress
 *        The string of Bluetooth address with format xx:xx:xx:xx:xx:xx.
 * @param aPropertyName
 *        The property name of Bluetooth device.
 *
 * @return A deferred promise.
 */
function getEmulatorDeviceProperty(aAddress, aPropertyName) {
  let cmd = "bt property " + aAddress + " " + aPropertyName;
  return runEmulatorCmdSafe(cmd)
         .then(aResults => aResults[0]);
}

/**
 * Start discovering Bluetooth devices.
 *
 * Allows the device's adapter to start seeking for remote devices.
 *
 * Fulfill params: (none)
 * Reject params: a DOMError
 *
 * @param aAdapter
 *        A BluetoothAdapter which is used to interact with local BT device.
 *
 * @return A deferred promise.
 */
function startDiscovery(aAdapter) {
  let request = aAdapter.startDiscovery();

  return wrapDomRequestAsPromise(request)
    .then(function resolve() {
      // TODO (bug 892207): Make Bluetooth APIs available for 3rd party apps.
      //     Currently, discovering state wouldn't change immediately here.
      //     We would turn on this check when the redesigned API are landed.
      // is(aAdapter.discovering, false, "BluetoothAdapter.discovering");
      log("  Start discovery - Success");
    }, function reject(aEvent) {
      ok(false, "Start discovery - Fail");
      throw aEvent.target.error;
    });
}

/**
 * Stop discovering Bluetooth devices.
 *
 * Allows the device's adapter to stop seeking for remote devices.
 *
 * Fulfill params: (none)
 * Reject params: a DOMError
 *
 * @param aAdapter
 *        A BluetoothAdapter which is used to interact with local BT device.
 *
 * @return A deferred promise.
 */
function stopDiscovery(aAdapter) {
  let request = aAdapter.stopDiscovery();

  return wrapDomRequestAsPromise(request)
    .then(function resolve() {
      // TODO (bug 892207): Make Bluetooth APIs available for 3rd party apps.
      //     Currently, discovering state wouldn't change immediately here.
      //     We would turn on this check when the redesigned API are landed.
      // is(aAdapter.discovering, false, "BluetoothAdapter.discovering");
      log("  Stop discovery - Success");
    }, function reject(aEvent) {
      ok(false, "Stop discovery - Fail");
      throw aEvent.target.error;
    });
}

/**
 * Wait for 'devicefound' event of specified devices.
 *
 * Resolve if that every specified devices has been found.  Never reject.
 *
 * Fulfill params: an array which contains addresses of remote devices.
 *
 * @param aAdapter
 *        A BluetoothAdapter which is used to interact with local BT device.
 * @param aRemoteAddresses
 *        An array which contains addresses of remote devices.
 *
 * @return A deferred promise.
 */
function waitForDevicesFound(aAdapter, aRemoteAddresses) {
  let deferred = Promise.defer();

  var addrArray = [];
  aAdapter.addEventListener("devicefound", function onevent(aEvent) {
    if(aRemoteAddresses.indexOf(aEvent.device.address) != -1) {
      addrArray.push(aEvent.device.address);
    }
    if(addrArray.length == aRemoteAddresses.length) {
      aAdapter.removeEventListener("devicefound", onevent);
      ok(true, "BluetoothAdapter has found all remote devices.");

      deferred.resolve(addrArray);
    }
  });

  return deferred.promise;
}

/**
 * Start discovering Bluetooth devices and wait for 'devicefound' events.
 *
 * Allows the device's adapter to start seeking for remote devices and wait for
 * the 'devicefound' events of specified devices.
 *
 * Fulfill params: an array of addresses of found devices.
 *
 * @param aAdapter
 *        A BluetoothAdapter which is used to interact with local BT device.
 * @param aRemoteAddresses
 *        An array which contains addresses of remote devices.
 *
 * @return A deferred promise.
 */
function startDiscoveryAndWaitDevicesFound(aAdapter, aRemoteAddresses) {
  let promises = [];

  promises.push(waitForDevicesFound(aAdapter, aRemoteAddresses));
  promises.push(startDiscovery(aAdapter));
  return Promise.all(promises)
         .then(aResults => aResults[0]);
}

/**
 * Start pairing a remote device.
 *
 * Start pairing a remote device with the device's adapter.
 *
 * Fulfill params: (none)
 * Reject params: a DOMError
 *
 * @param aAdapter
 *        A BluetoothAdapter which is used to interact with local BT device.
 * @param aDeviceAddress
 *        The string of remote Bluetooth address with format xx:xx:xx:xx:xx:xx.
 *
 * @return A deferred promise.
 */
function pair(aAdapter, aDeviceAddress) {
  let request = aAdapter.pair(aDeviceAddress);

  return wrapDomRequestAsPromise(request)
    .then(function resolve() {
      log("  Pair - Success");
    }, function reject(aEvent) {
      ok(false, "Pair - Fail");
      throw aEvent.target.error;
    });
}

/**
 * Remove the paired device from the paired device list.
 *
 * Remove the paired device from the paired device list of the device's adapter.
 *
 * Fulfill params: (none)
 * Reject params: a DOMError
 *
 * @param aAdapter
 *        A BluetoothAdapter which is used to interact with local BT device.
 * @param aDeviceAddress
 *        The string of remote Bluetooth address with format xx:xx:xx:xx:xx:xx.
 *
 * @return A deferred promise.
 */
function unpair(aAdapter, aDeviceAddress) {
  let request = aAdapter.unpair(aDeviceAddress);

  return wrapDomRequestAsPromise(request)
    .then(function resolve() {
      log("  Unpair - Success");
    }, function reject(aEvent) {
      ok(false, "Unpair - Fail");
      throw aEvent.target.error;
    });
}

/**
 * Start pairing a remote device and wait for "pairedstatuschanged" event.
 *
 * Start pairing a remote device with the device's adapter and wait for
 * "pairedstatuschanged" event.
 *
 * Fulfill params:  an array of promise results contains the fulfilled params of
 *                  'waitForAdapterEvent' and 'pair'.
 *
 * @param aAdapter
 *        A BluetoothAdapter which is used to interact with local BT device.
 * @param aDeviceAddress
 *        The string of remote Bluetooth address with format xx:xx:xx:xx:xx:xx.
 *
 * @return A deferred promise.
 */
function pairDeviceAndWait(aAdapter, aDeviceAddress) {
  let promises = [];
  promises.push(waitForAdapterEvent(aAdapter, "pairedstatuschanged"));
  promises.push(pair(aAdapter, aDeviceAddress));
  return Promise.all(promises);
}

/**
 * Get paried Bluetooth devices.
 *
 * The getPairedDevices method is used to retrieve the full list of all devices
 * paired with the device's adapter.
 *
 * Fulfill params: a shallow copy of the Array of paired BluetoothDevice
 *                 objects.
 * Reject params: a DOMError
 *
 * @param aAdapter
 *        A BluetoothAdapter which is used to interact with local BT device.
 *
 * @return A deferred promise.
 */
function getPairedDevices(aAdapter) {
  let request = aAdapter.getPairedDevices();

  return wrapDomRequestAsPromise(request)
    .then(function resolve() {
      log("  getPairedDevices - Success");
      let pairedDevices = request.result.slice();
      return pairedDevices;
    }, function reject(aEvent) {
      ok(false, "getPairedDevices - Fail");
      throw aEvent.target.error;
    });
}

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
  let request = navigator.mozSettings.createLock().get(aKey);

  return wrapDomRequestAsPromise(request)
    .then(function resolve(aEvent) {
      ok(true, "getSettings(" + aKey + ")");
      return aEvent.target.result[aKey];
    }, function reject(aEvent) {
      ok(false, "getSettings(" + aKey + ")");
      throw aEvent.target.error;
    });
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
  let lock = window.navigator.mozSettings.createLock();
  let request = lock.set(aSettings);
  let deferred = Promise.defer();
  lock.onsettingstransactionsuccess = function () {
      ok(true, "setSettings(" + JSON.stringify(aSettings) + ")");
    deferred.resolve();
  };
  lock.onsettingstransactionfailure = function (aEvent) {
      ok(false, "setSettings(" + JSON.stringify(aSettings) + ")");
    deferred.reject();
  };
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
 * @param aEventName
 *        The name of the EventHandler.
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
 * Wait for one named BluetoothAdapter event.
 *
 * Resolve if that named event occurs.  Never reject.
 *
 * Fulfill params: the DOMEvent passed.
 *
 * @param aAdapter
 *        A BluetoothAdapter which is used to interact with local BT device.
 * @param aEventName
 *        The name of the EventHandler.
 *
 * @return A deferred promise.
 */
function waitForAdapterEvent(aAdapter, aEventName) {
  let deferred = Promise.defer();

  aAdapter.addEventListener(aEventName, function onevent(aEvent) {
    aAdapter.removeEventListener(aEventName, onevent);

    ok(true, "BluetoothAdapter event '" + aEventName + "' got.");
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
 * Fulfill params: an array of promise results contains the fulfill params of
 *                 'waitForManagerEvent' and 'setBluetoothEnabled'.
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
 * Wait for pending emulator transactions and call |finish()|.
 */
function cleanUp() {
  // Use ok here so that we have at least one test run.
  ok(true, ":: CLEANING UP ::");

  waitFor(finish, function() {
    return pendingEmulatorCmdCount === 0;
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
  startBluetoothTestBase(["settings-read", "settings-write", "settings-api-read", "settings-api-write"], function() {
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
