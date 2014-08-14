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

let Promise =
  SpecialPowers.Cu.import("resource://gre/modules/Promise.jsm").Promise;

let bluetoothManager;

let pendingEmulatorCmdCount = 0;

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
    .then(function(aResults) {
      return aResults[0];
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
 * Get the boolean value which indicates defaultAdapter of bluetooth is enabled.
 *
 * Resolve if that defaultAdapter is enabled
 *
 * Fulfill params:
 *   A boolean value.
 * Reject params: (none)
 *
 * @return A deferred promise.
 */
function getBluetoothEnabled() {
  log("bluetoothManager.defaultAdapter.state: " + bluetoothManager.defaultAdapter.state);

  return (bluetoothManager.defaultAdapter.state == "enabled");
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
 * Wait for one named BluetoothManager event.
 *
 * Resolve if that named event occurs. Never reject.
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
 * Resolve if that named event occurs. Never reject.
 *
 * Fulfill params: the DOMEvent passed.
 *
 * @param aAdapter
 *        The BluetoothAdapter you want to use.
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
 * Wait for 'onattributechanged' events for state changes of BluetoothAdapter
 * with specified order.
 *
 * Resolve if those expected events occur in order. Never reject.
 *
 * Fulfill params: an array which contains every changed attributes during
 *                 the waiting.
 *
 * @param aAdapter
 *        The BluetoothAdapter you want to use.
 * @param aStateChangesInOrder
 *        An array which contains an expected order of BluetoothAdapterState.
 *        Example 1: [enabling, enabled]
 *        Example 2: [disabling, disabled]
 *
 * @return A deferred promise.
 */
function waitForAdapterStateChanged(aAdapter, aStateChangesInOrder) {
  let deferred = Promise.defer();

  let stateIndex = 0;
  let prevStateIndex = 0;
  let statesArray = [];
  let changedAttrs = [];
  aAdapter.onattributechanged = function(aEvent) {
    for (let i in aEvent.attrs) {
      changedAttrs.push(aEvent.attrs[i]);
      switch (aEvent.attrs[i]) {
        case "state":
          log("  'state' changed to " + aAdapter.state);

          // Received state change order may differ from expected one even though
          // state changes in expected order, because the value of state may change
          // again before we receive prior 'onattributechanged' event.
          //
          // For example, expected state change order [A,B,C] may result in
          // received ones:
          // - [A,C,C] if state becomes C before we receive 2nd 'onattributechanged'
          // - [B,B,C] if state becomes B before we receive 1st 'onattributechanged'
          // - [C,C,C] if state becomes C before we receive 1st 'onattributechanged'
          // - [A,B,C] if all 'onattributechanged' are received in perfect timing
          //
          // As a result, we ensure only following conditions instead of exactly
          // matching received and expected state change order.
          // - Received state change order never reverse expected one. For example,
          //   [B,A,C] should never occur with expected state change order [A,B,C].
          // - The changed value of state in received state change order never
          //   appears later than that in expected one. For example, [A,A,C] should
          //   never occur with expected state change order [A,B,C].
          let stateIndex = aStateChangesInOrder.indexOf(aAdapter.state);
          if (stateIndex >= prevStateIndex && stateIndex + 1 > statesArray.length) {
            statesArray.push(aAdapter.state);
            prevStateIndex = stateIndex;

            if (statesArray.length == aStateChangesInOrder.length) {
              aAdapter.onattributechanged = null;
              ok(true, "BluetoothAdapter event 'onattributechanged' got.");
              deferred.resolve(changedAttrs);
            }
          } else {
            ok(false, "The order of 'onattributechanged' events is unexpected.");
          }

          break;
        case "name":
          log("  'name' changed to " + aAdapter.name);
          if (aAdapter.state == "enabling") {
            isnot(aAdapter.name, "", "adapter.name");
          }
          else if (aAdapter.state == "disabling") {
            is(aAdapter.name, "", "adapter.name");
          }
          break;
        case "address":
          log("  'address' changed to " + aAdapter.address);
          if (aAdapter.state == "enabling") {
            isnot(aAdapter.address, "", "adapter.address");
          }
          else if (aAdapter.state == "disabling") {
            is(aAdapter.address, "", "adapter.address");
          }
          break;
        case "discoverable":
          log("  'discoverable' changed to " + aAdapter.discoverable);
          if (aAdapter.state == "enabling") {
            is(aAdapter.discoverable, true, "adapter.discoverable");
          }
          else if (aAdapter.state == "disabling") {
            is(aAdapter.discoverable, false, "adapter.discoverable");
          }
          break;
        case "discovering":
          log("  'discovering' changed to " + aAdapter.discovering);
          if (aAdapter.state == "enabling") {
            is(aAdapter.discovering, true, "adapter.discovering");
          }
          else if (aAdapter.state == "disabling") {
            is(aAdapter.discovering, false, "adapter.discovering");
          }
          break;
        case "unknown":
        default:
          ok(false, "Unknown attribute '" + aEvent.attrs[i] + "' changed." );
          break;
      }
    }
  };

  return deferred.promise;
}

/**
 * Wait for an 'onattributechanged' event for a specified attribute and compare
 * the new value with the expected value.
 *
 * Resolve if the specified event occurs. Never reject.
 *
 * Fulfill params: a BluetoothAttributeEvent with property attrs that contains
 *                 changed BluetoothAdapterAttributes.
 *
 * @param aAdapter
 *        The BluetoothAdapter you want to use.
 * @param aAttrName
 *        The name of the attribute of adapter.
 * @param aExpectedValue
 *        The expected new value of the attribute.
 *
 * @return A deferred promise.
 */
function waitForAdapterAttributeChanged(aAdapter, aAttrName, aExpectedValue) {
  let deferred = Promise.defer();

  aAdapter.onattributechanged = function(aEvent) {
    let i = aEvent.attrs.indexOf(aAttrName);
    if (i >= 0) {
      switch (aEvent.attrs[i]) {
        case "state":
          log("  'state' changed to " + aAdapter.state);
          is(aAdapter.state, aExpectedValue, "adapter.state");
          break;
        case "name":
          log("  'name' changed to " + aAdapter.name);
          is(aAdapter.name, aExpectedValue, "adapter.name");
          break;
        case "address":
          log("  'address' changed to " + aAdapter.address);
          is(aAdapter.address, aExpectedValue, "adapter.address");
          break;
        case "discoverable":
          log("  'discoverable' changed to " + aAdapter.discoverable);
          is(aAdapter.discoverable, aExpectedValue, "adapter.discoverable");
          break;
        case "discovering":
          log("  'discovering' changed to " + aAdapter.discovering);
          is(aAdapter.discovering, aExpectedValue, "adapter.discovering");
          break;
        case "unknown":
        default:
          ok(false, "Unknown attribute '" + aAttrName + "' changed." );
          break;
      }
      aAdapter.onattributechanged = null;
      deferred.resolve(aEvent);
    }
  };

  return deferred.promise;
}

/**
 * Wait for specified number of 'devicefound' events.
 *
 * Resolve if specified number of devices has been found. Never reject.
 *
 * Fulfill params: an array which contains BluetoothDeviceEvents that we
 *                 received from the BluetoothDiscoveryHandle.
 *
 * @param aDiscoveryHandle
 *        A BluetoothDiscoveryHandle which is used to notify application of
 *        discovered remote bluetooth devices.
 * @param aExpectedNumberOfDevices
 *        The number of remote devices we expect to discover.
 *
 * @return A deferred promise.
 */
function waitForDevicesFound(aDiscoveryHandle, aExpectedNumberOfDevices) {
  let deferred = Promise.defer();

  ok(aDiscoveryHandle instanceof BluetoothDiscoveryHandle,
    "aDiscoveryHandle should be a BluetoothDiscoveryHandle");

  let devicesArray = [];
  aDiscoveryHandle.ondevicefound = function onDeviceFound(aEvent) {
    ok(aEvent instanceof BluetoothDeviceEvent,
      "aEvent should be a BluetoothDeviceEvent");

    devicesArray.push(aEvent);
    if (devicesArray.length >= aExpectedNumberOfDevices) {
      aDiscoveryHandle.ondevicefound = null;
      deferred.resolve(devicesArray);
    }
  };

  return deferred.promise;
}

/**
 * Wait for 'devicefound' events of specified devices.
 *
 * Resolve if every specified device has been found. Never reject.
 *
 * Fulfill params: an array which contains the BluetoothDeviceEvents that we
 *                 expected to receive from the BluetoothDiscoveryHandle.
 *
 * @param aDiscoveryHandle
 *        A BluetoothDiscoveryHandle which is used to notify application of
 *        discovered remote bluetooth devices.
 * @param aRemoteAddresses
 *        An array which contains addresses of remote devices.
 *
 * @return A deferred promise.
 */
function waitForSpecifiedDevicesFound(aDiscoveryHandle, aRemoteAddresses) {
  let deferred = Promise.defer();

  ok(aDiscoveryHandle instanceof BluetoothDiscoveryHandle,
    "aDiscoveryHandle should be a BluetoothDiscoveryHandle");

  let devicesArray = [];
  aDiscoveryHandle.ondevicefound = function onDeviceFound(aEvent) {
    ok(aEvent instanceof BluetoothDeviceEvent,
      "aEvent should be a BluetoothDeviceEvent");

    if (aRemoteAddresses.indexOf(aEvent.device.address) != -1) {
      devicesArray.push(aEvent);
    }
    if (devicesArray.length == aRemoteAddresses.length) {
      aDiscoveryHandle.ondevicefound = null;
      ok(true, "BluetoothAdapter has found all remote devices.");
      deferred.resolve(devicesArray);
    }
  };

  return deferred.promise;
}

/**
 * Verify the correctness of 'devicepaired' or 'deviceunpaired' events.
 *
 * Use BluetoothAdapter.getPairedDevices() to get current device array.
 * Resolve if the device of from 'devicepaired' event exists in device array or
 * the device of address from 'deviceunpaired' event has been removed from
 * device array.
 * Otherwise, reject the promise.
 *
 * Fulfill params: the device event from 'devicepaired' or 'deviceunpaired'.
 *
 * @param aAdapter
 *        The BluetoothAdapter you want to use.
 * @param aDeviceEvent
 *        The device event from "devicepaired" or "deviceunpaired".
 *
 * @return A deferred promise.
 */
function verifyDevicePairedUnpairedEvent(aAdapter, aDeviceEvent) {
  let deferred = Promise.defer();

  let devices = aAdapter.getPairedDevices();
  let isPromisePending = true;
  if (aDeviceEvent.device) {
    log("  - Verify 'devicepaired' event");
    for (let i in devices) {
      if (devices[i].address == aDeviceEvent.device.address) {
        deferred.resolve(aDeviceEvent);
        isPromisePending = false;
      }
    }
    if (isPromisePending) {
      deferred.reject(aDeviceEvent);
      isPromisePending = false;
    }
  } else if (aDeviceEvent.address) {
    log("  - Verify 'deviceunpaired' event");
    for (let i in devices) {
      if (devices[i].address == aDeviceEvent.address) {
        deferred.reject(aDeviceEvent);
        isPromisePending = false;
      }
    }
    if (isPromisePending) {
      deferred.resolve(aDeviceEvent);
      isPromisePending = false;
    }
  } else {
    log("  - Exception occurs. Unexpected aDeviceEvent properties.");
    deferred.reject(aDeviceEvent);
    isPromisePending = false;
  }

  return deferred.promise;
}

/**
 * Add event handlers for pairing request listener.
 *
 * @param aAdapter
 *        The BluetoothAdapter you want to use.
 * @param aSpecifiedBdAddress (optional)
 *        Bluetooth address of the specified remote device which adapter wants
 *        to pair with. If aSpecifiedBdAddress is an empty string, 'null' or
 *        'undefined', this function accepts every pairing request.
 */
function addEventHandlerForPairingRequest(aAdapter, aSpecifiedBdAddress) {
  log("  - Add event handlers for pairing requests.");

  aAdapter.pairingReqs.ondisplaypasskeyreq = function onDisplayPasskeyReq(evt) {
    let passkey = evt.handle.passkey; // passkey to display
    ok(typeof passkey === 'string', "type checking for passkey.");
    log("  - Received 'ondisplaypasskeyreq' event with passkey: " + passkey);

    let device = evt.device;
    if (!aSpecifiedBdAddress || device.address == aSpecifiedBdAddress) {
      cleanupPairingListener(aAdapter.pairingReqs);
    }
  };

  aAdapter.pairingReqs.onenterpincodereq = function onEnterPinCodeReq(evt) {
    log("  - Received 'onenterpincodereq' event.");

    let device = evt.device;
    if (!aSpecifiedBdAddress || device.address == aSpecifiedBdAddress) {
      // TODO: Allow user to enter pincode.
      let UserEnteredPinCode = "0000";
      let pinCode = UserEnteredPinCode;

      evt.handle.setPinCode(pinCode).then(
        function onResolve() {
          log("  - 'setPinCode' resolve.");
          cleanupPairingListener(aAdapter.pairingReqs);
        },
        function onReject() {
          log("  - 'setPinCode' reject.");
          cleanupPairingListener(aAdapter.pairingReqs);
        });
    }
  };

  aAdapter.pairingReqs.onpairingconfirmationreq
    = function onPairingConfirmationReq(evt) {
    let passkey = evt.handle.passkey; // passkey for user to confirm
    ok(typeof passkey === 'string', "type checking for passkey.");
    log("  - Received 'onpairingconfirmationreq' event with passkey: " + passkey);

    let device = evt.device;
    if (!aSpecifiedBdAddress || device.address == aSpecifiedBdAddress) {
      let confirm = true;

      evt.handle.setPairingConfirmation(confirm).then(
        function onResolve() {
          log("  - 'setPairingConfirmation' resolve.");
          cleanupPairingListener(aAdapter.pairingReqs);
        },
        function onReject() {
          log("  - 'setPairingConfirmation' reject.");
          cleanupPairingListener(aAdapter.pairingReqs);
        });
    }
  };

  aAdapter.pairingReqs.onpairingconsentreq = function onPairingConsentReq(evt) {
    log("  - Received 'onpairingconsentreq' event.");

    let device = evt.device;
    if (!aSpecifiedBdAddress || device.address == aSpecifiedBdAddress) {
      cleanupPairingListener(aAdapter.pairingReqs);
    }
  };
}

/**
 * Clean up event handlers for pairing listener.
 *
 * @param aPairingReqs
 *        A BluetoothPairingListener with event handlers.
 */
function cleanupPairingListener(aPairingReqs) {
  aPairingReqs.ondisplaypasskeyreq = null;
  aPairingReqs.onenterpasskeyreq = null;
  aPairingReqs.onpairingconfirmationreq = null;
  aPairingReqs.onpairingconsentreq = null;
}

/**
 * Compare two uuid arrays to see if them are the same.
 *
 * @param aUuidArray1
 *        An array which contains uuid strings.
 * @param aUuidArray2
 *        Another array which contains uuid strings.
 *
 * @return A boolean value.
 */
function isUuidsEqual(aUuidArray1, aUuidArray2) {
  if (!Array.isArray(aUuidArray1) || !Array.isArray(aUuidArray2)) {
    return false;
  }

  if (aUuidArray1.length != aUuidArray2.length) {
    return false;
  }

  for (let i = 0, l = aUuidArray1.length; i < l; i++) {
    if (aUuidArray1[i] != aUuidArray2[i]) {
      return false;
    }
  }
  return true;
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
  startBluetoothTestBase([], function() {
    let origEnabled, needEnable;
    return Promise.resolve()
      .then(function() {
        origEnabled = getBluetoothEnabled();

        needEnable = !origEnabled;
        log("Original state of bluetooth is " + bluetoothManager.defaultAdapter.state);

        if (origEnabled && aReenable) {
          log("Disable Bluetooth ...");
          needEnable = true;

          isnot(bluetoothManager.defaultAdapter, null,
            "bluetoothManager.defaultAdapter")

          return bluetoothManager.defaultAdapter.disable();
        }
      })
      .then(function() {
        if (needEnable) {
          log("Enable Bluetooth ...");

          isnot(bluetoothManager.defaultAdapter, null,
            "bluetoothManager.defaultAdapter")

          return bluetoothManager.defaultAdapter.enable();
        }
      })
      .then(() => bluetoothManager.defaultAdapter)
      .then(aTestCaseMain)
      .then(function() {
        if (!origEnabled) {
          log("Disable Bluetooth ...");

          return bluetoothManager.defaultAdapter.disable();
        }
      });
  });
}
