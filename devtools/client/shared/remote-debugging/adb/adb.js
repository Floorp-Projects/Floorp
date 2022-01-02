/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  clearInterval,
  setInterval,
} = require("resource://gre/modules/Timer.jsm");

const EventEmitter = require("devtools/shared/event-emitter");
const {
  adbProcess,
} = require("devtools/client/shared/remote-debugging/adb/adb-process");
const {
  adbAddon,
} = require("devtools/client/shared/remote-debugging/adb/adb-addon");
const AdbDevice = require("devtools/client/shared/remote-debugging/adb/adb-device");
const {
  AdbRuntime,
} = require("devtools/client/shared/remote-debugging/adb/adb-runtime");
const {
  TrackDevicesCommand,
} = require("devtools/client/shared/remote-debugging/adb/commands/track-devices");
loader.lazyRequireGetter(
  this,
  "check",
  "devtools/client/shared/remote-debugging/adb/adb-running-checker",
  true
);

// Duration in milliseconds of the runtime polling. We resort to polling here because we
// have no event to know when a runtime started on an already discovered ADB device.
const UPDATE_RUNTIMES_INTERVAL = 3000;

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
    this._onNoDevicesDetected = this._onNoDevicesDetected.bind(this);

    this._trackDevicesCommand.on("device-connected", this._onDeviceConnected);
    this._trackDevicesCommand.on(
      "device-disconnected",
      this._onDeviceDisconnected
    );
    this._trackDevicesCommand.on(
      "no-devices-detected",
      this._onNoDevicesDetected
    );
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

  getDevices() {
    return [...this._devices.values()];
  }

  async isProcessStarted() {
    return check();
  }

  async _startTracking() {
    this._isTrackingDevices = true;
    await adbProcess.start();

    this._trackDevicesCommand.run();

    // Device runtimes are detected by running a shell command and checking for
    // "firefox-debugger-socket" in the list of currently running processes.
    this._timer = setInterval(
      this.updateRuntimes.bind(this),
      UPDATE_RUNTIMES_INTERVAL
    );
  }

  async _stopTracking() {
    clearInterval(this._timer);
    this._isTrackingDevices = false;
    this._trackDevicesCommand.stop();

    this._devices = new Map();
    this._runtimes = [];
    this.updateRuntimes();
  }

  _shouldTrack() {
    return adbAddon.status === "installed" && this._listeners.size > 0;
  }

  /**
   * This method will emit "runtime-list-ready" to notify the consumer that the list of
   * runtimes is ready to be retrieved.
   */
  async _updateAdbProcess() {
    if (!this._isTrackingDevices && this._shouldTrack()) {
      const onRuntimesUpdated = this.once("runtime-list-updated");
      this._startTracking();
      // If we are starting to track runtimes, the list of runtimes will only be ready
      // once the first "runtime-list-updated" event has been processed.
      await onRuntimesUpdated;
    } else if (this._isTrackingDevices && !this._shouldTrack()) {
      this._stopTracking();
    }
    this.emit("runtime-list-ready");
  }

  async _onDeviceConnected(deviceId) {
    const adbDevice = new AdbDevice(deviceId);
    await adbDevice.initialize();
    this._devices.set(deviceId, adbDevice);
    this.updateRuntimes();
  }

  _onDeviceDisconnected(deviceId) {
    this._devices.delete(deviceId);
    this.updateRuntimes();
  }

  _onNoDevicesDetected() {
    this.updateRuntimes();
  }

  async _getDeviceRuntimes(device) {
    const socketPaths = [...(await device.getRuntimeSocketPaths())];
    const runtimes = [];
    for (const socketPath of socketPaths) {
      const runtime = new AdbRuntime(device, socketPath);
      await runtime.init();
      runtimes.push(runtime);
    }
    return runtimes;
  }
}

exports.adb = new Adb();
