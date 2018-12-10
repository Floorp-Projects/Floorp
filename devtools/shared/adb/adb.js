/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EventEmitter = require("devtools/shared/event-emitter");
const { adbProcess } = require("devtools/shared/adb/adb-process");
const { adbAddon } = require("devtools/shared/adb/adb-addon");
const AdbDevice = require("devtools/shared/adb/adb-device");
const { AdbRuntime } = require("devtools/shared/adb/adb-runtime");
const { TrackDevicesCommand } = require("devtools/shared/adb/commands/track-devices");

class Adb extends EventEmitter {
  constructor() {
    super();

    this._trackDevicesCommand = new TrackDevicesCommand();

    this._isTrackingDevices = false;
    this._isUpdatingRuntimes = false;

    this._listeners = new Set();
    this._devices = new Map();
    this._runtimes = [];

    this._updateAdbProcess = this._updateAdbProcess.bind(this);
    this._onDeviceConnected = this._onDeviceConnected.bind(this);
    this._onDeviceDisconnected = this._onDeviceDisconnected.bind(this);

    this._trackDevicesCommand.on("device-connected", this._onDeviceConnected);
    this._trackDevicesCommand.on("device-disconnected", this._onDeviceDisconnected);
    adbAddon.on("update", this._updateAdbProcess);
  }

  registerListener(listener) {
    this._listeners.add(listener);
    this.on("runtime-list-updated", listener);
    this._updateAdbProcess();
  }

  unregisterListener(listener) {
    this._listeners.delete(listener);
    this.off("runtime-list-updated", listener);
    this._updateAdbProcess();
  }

  async updateRuntimes() {
    try {
      const devices = [...this._devices.values()];
      const promises = devices.map(d => this._getDeviceRuntimes(d));
      const allRuntimes = await Promise.all(promises);

      this._runtimes = allRuntimes.flat();
      this.emit("runtime-list-updated");
    } catch (e) {
      console.error(e);
    }
  }

  getRuntimes() {
    return this._runtimes;
  }

  async _startAdb() {
    this._isTrackingDevices = true;
    await adbProcess.start();

    this._trackDevicesCommand.run();
  }

  async _stopAdb() {
    this._isTrackingDevices = false;
    this._trackDevicesCommand.stop();
    await adbProcess.stop();

    this._devices = new Map();
    this._runtimes = [];
    this.updateRuntimes();
  }

  _shouldTrack() {
    return adbAddon.status === "installed" && this._listeners.size > 0;
  }

  _updateAdbProcess() {
    if (!this._isTrackingDevices && this._shouldTrack()) {
      this._startAdb();
    } else if (this._isTrackingDevices && !this._shouldTrack()) {
      this._stopAdb();
    }
  }

  _onDeviceConnected(deviceId) {
    this._devices.set(deviceId, new AdbDevice(deviceId));
    this.updateRuntimes();
  }

  _onDeviceDisconnected(deviceId) {
    this._devices.delete(deviceId);
    this.updateRuntimes();
  }

  async _getDeviceRuntimes(device) {
    const model = await device.getModel();
    const socketPaths = await device.getRuntimeSocketPaths();
    return [...socketPaths].map(socketPath => new AdbRuntime(device, model, socketPath));
  }
}

exports.adb = new Adb();
