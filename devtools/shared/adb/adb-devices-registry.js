/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EventEmitter = require("devtools/shared/event-emitter");
const { adbAddon, ADB_ADDON_STATES } = require("devtools/shared/adb/adb-addon");

/**
 * Shared registry that will hold all the detected devices from ADB.
 * Extends EventEmitter and emits the following events:
 * - "register": a new device has been registered
 * - "unregister": a device has been unregistered
 */
class AdbDevicesRegistry extends EventEmitter {
  constructor() {
    super();

    // Internal object to store the discovered adb devices.
    this._devices = {};

    // When the addon is uninstalled, the repository should be emptied.
    // TODO: This should also be done when ADB is stopped.
    this._onAdbAddonUpdate = this._onAdbAddonUpdate.bind(this);
    adbAddon.on("update", this._onAdbAddonUpdate);
  }

  /**
   * Register a device (Device class defined in from adb-device.js) for the provided name.
   *
   * @param {String} name
   *        Name of the device.
   * @param {Device} device
   *        The device to register.
   */
  register(name, device) {
    this._devices[name] = device;
    this.emit("register");
  }

  /**
   * Unregister a device previously registered under the provided name.
   *
   * @param {String} name
   *        Name of the device.
   */
  unregister(name) {
    delete this._devices[name];
    this.emit("unregister");
  }

  /**
   * Returns an iterable containing the name of all the available devices, sorted by name.
   */
  available() {
    return Object.keys(this._devices).sort();
  }

  /**
   * Returns a device previously registered under the provided name.
   *
   * @param {String} name
   *        Name of the device.
   */
  getByName(name) {
    return this._devices[name];
  }

  _onAdbAddonUpdate() {
    const installed = adbAddon.status === ADB_ADDON_STATES.INSTALLED;
    if (!installed) {
      for (const name in this._devices) {
        this.unregister(name);
      }
    }
  }
}

exports.adbDevicesRegistry = new AdbDevicesRegistry();
