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

const OKAY = 0x59414b4f;

// Start tracking devices connecting and disconnecting from the host.
// We can't reuse runCommand here because we keep the socket alive.
class TrackDevicesCommand extends EventEmitter {
  run() {
    this._waitForFirst = true;
    this._devices = {};
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
    for (const dev in this._devices) {
      this._devices[dev] = false;
      this.emit("device-disconnected", dev);
    }

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
      for (const dev in this._devices) {
        this._devices[dev] = false;
        this.emit("device-disconnected", dev);
      }
    } else {
      // One line per device, each line being $DEVICE\t(offline|device)
      const lines = packet.data.split("\n");
      const newDev = {};
      lines.forEach(function(line) {
        if (line.length == 0) {
          return;
        }

        const [dev, status] = line.split("\t");
        newDev[dev] = status !== "offline";
      });
      // Check which device changed state.
      for (const dev in newDev) {
        if (this._devices[dev] != newDev[dev]) {
          if (dev in this._devices || newDev[dev]) {
            const topic = newDev[dev] ? "device-connected"
                                      : "device-disconnected";
            this.emit(topic, dev);
          }
          this._devices[dev] = newDev[dev];
        }
      }
    }
  }
}
exports.TrackDevicesCommand = TrackDevicesCommand;
