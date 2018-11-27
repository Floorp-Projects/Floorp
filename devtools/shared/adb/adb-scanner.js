/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EventEmitter = require("devtools/shared/event-emitter");
const { dumpn } = require("devtools/shared/DevToolsUtils");
const { ADB } = require("devtools/shared/adb/adb");
const { adbDevicesRegistry } = require("devtools/shared/adb/adb-devices-registry");
const { AdbRuntime } = require("devtools/shared/adb/adb-runtime");

loader.lazyRequireGetter(this, "AdbDevice", "devtools/shared/adb/adb-device");

class ADBScanner extends EventEmitter {
  constructor() {
    super();
    this._runtimes = [];

    this._onDeviceConnected = this._onDeviceConnected.bind(this);
    this._onDeviceDisconnected = this._onDeviceDisconnected.bind(this);
    this._updateRuntimes = this._updateRuntimes.bind(this);
  }

  enable() {
    EventEmitter.on(ADB, "device-connected", this._onDeviceConnected);
    EventEmitter.on(ADB, "device-disconnected", this._onDeviceDisconnected);

    adbDevicesRegistry.on("register", this._updateRuntimes);
    adbDevicesRegistry.on("unregister", this._updateRuntimes);

    ADB.start().then(() => {
      ADB.trackDevices();
    });
    this._updateRuntimes();
  }

  disable() {
    EventEmitter.off(ADB, "device-connected", this._onDeviceConnected);
    EventEmitter.off(ADB, "device-disconnected", this._onDeviceDisconnected);
    adbDevicesRegistry.off("register", this._updateRuntimes);
    adbDevicesRegistry.off("unregister", this._updateRuntimes);
    this._updateRuntimes();
  }

  _emitUpdated() {
    this.emit("runtime-list-updated");
  }

  _onDeviceConnected(deviceId) {
    const device = new AdbDevice(deviceId);
    adbDevicesRegistry.register(deviceId, device);
  }

  _onDeviceDisconnected(deviceId) {
    adbDevicesRegistry.unregister(deviceId);
  }

  _updateRuntimes() {
    if (this._updatingPromise) {
      return this._updatingPromise;
    }
    this._runtimes = [];
    const promises = [];
    for (const id of adbDevicesRegistry.available()) {
      const device = adbDevicesRegistry.getByName(id);
      promises.push(this._detectRuntimes(device));
    }
    this._updatingPromise = Promise.all(promises);
    this._updatingPromise.then(() => {
      this._emitUpdated();
      this._updatingPromise = null;
    }, () => {
      this._updatingPromise = null;
    });
    return this._updatingPromise;
  }

  async _detectRuntimes(adbDevice) {
    const model = await adbDevice.getModel();
    const socketPaths = await adbDevice.getRuntimeSocketPaths();
    for (const socketPath of socketPaths) {
      const runtime = new AdbRuntime(adbDevice, model, socketPath);
      dumpn("Found " + runtime.name);
      this._runtimes.push(runtime);
    }
  }

  scan() {
    return this._updateRuntimes();
  }

  listRuntimes() {
    return this._runtimes;
  }
}

exports.ADBScanner = ADBScanner;
