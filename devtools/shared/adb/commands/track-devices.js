/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Wrapper around the ADB utility.

"use strict";

const EventEmitter = require("devtools/shared/event-emitter");
const { dumpn } = require("devtools/shared/DevToolsUtils");
const { setTimeout } = require("resource://gre/modules/Timer.jsm");
const { Services } = require("resource://gre/modules/Services.jsm");

const { ADB } = require("../adb");
const client = require("../adb-client");

const OKAY = 0x59414b4f;

// Start tracking devices connecting and disconnecting from the host.
// We can't reuse runCommand here because we keep the socket alive.
// @return The socket used.
const trackDevices = function() {
  dumpn("trackDevices");
  const socket = client.connect();
  let waitForFirst = true;
  const devices = {};

  socket.s.onopen = function() {
    dumpn("trackDevices onopen");
    Services.obs.notifyObservers(null, "adb-track-devices-start");
    const req = client.createRequest("host:track-devices");
    socket.send(req);
  };

  socket.s.onerror = function(event) {
    dumpn("trackDevices onerror: " + event);
    Services.obs.notifyObservers(null, "adb-track-devices-stop");
  };

  socket.s.onclose = function() {
    dumpn("trackDevices onclose");

    // Report all devices as disconnected
    for (const dev in devices) {
      devices[dev] = false;
      EventEmitter.emit(ADB, "device-disconnected", dev);
    }

    Services.obs.notifyObservers(null, "adb-track-devices-stop");

    // When we lose connection to the server,
    // and the adb is still on, we most likely got our server killed
    // by local adb. So we do try to reconnect to it.
    setTimeout(function() { // Give some time to the new adb to start
      if (ADB.ready) { // Only try to reconnect/restart if the add-on is still enabled
        ADB.start().then(function() { // try to connect to the new local adb server
                                       // or, spawn a new one
          trackDevices(); // Re-track devices
        });
      }
    }, 2000);
  };

  socket.s.ondata = function(event) {
    dumpn("trackDevices ondata");
    const data = event.data;
    dumpn("length=" + data.byteLength);
    const dec = new TextDecoder();
    dumpn(dec.decode(new Uint8Array(data)).trim());

    // check the OKAY or FAIL on first packet.
    if (waitForFirst) {
      if (!client.checkResponse(data, OKAY)) {
        socket.close();
        return;
      }
    }

    const packet = client.unpackPacket(data, !waitForFirst);
    waitForFirst = false;

    if (packet.data == "") {
      // All devices got disconnected.
      for (const dev in devices) {
        devices[dev] = false;
        EventEmitter.emit(ADB, "device-disconnected", dev);
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
        if (devices[dev] != newDev[dev]) {
          if (dev in devices || newDev[dev]) {
            const topic = newDev[dev] ? "device-connected"
                                      : "device-disconnected";
            EventEmitter.emit(ADB, topic, dev);
          }
          devices[dev] = newDev[dev];
        }
      }
    }
  };
};
exports.trackDevices = trackDevices;
