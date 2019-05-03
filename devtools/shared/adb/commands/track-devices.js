/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Wrapper around the ADB utility.

"use strict";

const EventEmitter = require("devtools/shared/event-emitter");
const { dumpn } = require("devtools/shared/DevToolsUtils");
const { setTimeout } = require("resource://gre/modules/Timer.jsm");

const { adbProcess } = require("../adb-process");
const client = require("../adb-client");

const ADB_STATUS_OFFLINE = "offline";
const OKAY = 0x59414b4f;

// Start tracking devices connecting and disconnecting from the host.
// We can't reuse runCommand here because we keep the socket alive.
class TrackDevicesCommand extends EventEmitter {
  run() {
    this._waitForFirst = true;
    // Hold device statuses. key: device id, value: status.
    this._devices = new Map();
    this._socket = client.connect();

    this._socket.s.onopen = this._onOpen.bind(this);
    this._socket.s.onerror = this._onError.bind(this);
    this._socket.s.onclose = this._onClose.bind(this);
    this._socket.s.ondata = this._onData.bind(this);
  }

  stop() {
    if (this._socket) {
      this._socket.close();

      this._socket.s.onopen = null;
      this._socket.s.onerror = null;
      this._socket.s.onclose = null;
      this._socket.s.ondata = null;
    }
  }

  _onOpen() {
    dumpn("trackDevices onopen");
    const req = client.createRequest("host:track-devices");
    this._socket.send(req);
  }

  _onError(event) {
    dumpn("trackDevices onerror: " + event);
  }

  _onClose() {
    dumpn("trackDevices onclose");

    // Report all devices as disconnected
    this._disconnectAllDevices();

    // When we lose connection to the server,
    // and the adb is still on, we most likely got our server killed
    // by local adb. So we do try to reconnect to it.

    // Give some time to the new adb to start
    setTimeout(() => {
      // Only try to reconnect/restart if the add-on is still enabled
      if (adbProcess.ready) {
        // try to connect to the new local adb server or spawn a new one
        adbProcess.start().then(() => {
          // Re-track devices
          this.run();
        });
      }
    }, 2000);
  }

  _onData(event) {
    dumpn("trackDevices ondata");
    const data = event.data;
    dumpn("length=" + data.byteLength);
    const dec = new TextDecoder();
    dumpn(dec.decode(new Uint8Array(data)).trim());

    // check the OKAY or FAIL on first packet.
    if (this._waitForFirst) {
      if (!client.checkResponse(data, OKAY)) {
        this._socket.close();
        return;
      }
    }

    const packet = client.unpackPacket(data, !this._waitForFirst);
    this._waitForFirst = false;

    if (packet.data == "") {
      // All devices got disconnected.
      this._disconnectAllDevices();
    } else {
      // One line per device, each line being $DEVICE\t(offline|device)
      const lines = packet.data.split("\n");
      const newDevices = new Map();
      lines.forEach(function(line) {
        if (line.length == 0) {
          return;
        }

        const [deviceId, status] = line.split("\t");
        newDevices.set(deviceId, status);
      });

      // Fire events if needed.
      const deviceIds = new Set([...this._devices.keys(), ...newDevices.keys()]);
      for (const deviceId of deviceIds) {
        const currentStatus = this._devices.get(deviceId);
        const newStatus = newDevices.get(deviceId);
        this._fireConnectionEventIfNeeded(deviceId, currentStatus, newStatus);
      }

      // Update devices.
      this._devices = newDevices;
    }
  }

  _disconnectAllDevices() {
    if (this._devices.size === 0) {
      // If no devices were detected, fire an event to let consumer resume.
      this.emit("no-devices-detected");
    } else {
      for (const [deviceId, status] of this._devices.entries()) {
        if (status !== ADB_STATUS_OFFLINE) {
          this.emit("device-disconnected", deviceId);
        }
      }
    }
    this._devices = new Map();
  }

  _fireConnectionEventIfNeeded(deviceId, currentStatus, newStatus) {
    const isCurrentOnline = !!(currentStatus && currentStatus !== ADB_STATUS_OFFLINE);
    const isNewOnline = !!(newStatus && newStatus !== ADB_STATUS_OFFLINE);

    if (isCurrentOnline === isNewOnline) {
      return;
    }

    if (isNewOnline) {
      this.emit("device-connected", deviceId);
    } else {
      this.emit("device-disconnected", deviceId);
    }
  }
}
exports.TrackDevicesCommand = TrackDevicesCommand;
