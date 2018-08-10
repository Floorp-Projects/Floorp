/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Wrapper around the ADB utility.

"use strict";

const { Cc, Ci } = require("chrome");
const EventEmitter = require("devtools/shared/event-emitter");
const client = require("./adb-client");
const { dumpn } = require("devtools/shared/DevToolsUtils");
const { getFileForBinary } = require("./adb-binary");
const { setTimeout } = require("resource://gre/modules/Timer.jsm");
const { PromiseUtils } = require("resource://gre/modules/PromiseUtils.jsm");
const { Services } = require("resource://gre/modules/Services.jsm");
loader.lazyRequireGetter(this, "check",
                         "devtools/shared/adb/adb-running-checker", true);

let ready = false;
let didRunInitially = false;

const OKAY = 0x59414b4f;

const ADB = {
  get didRunInitially() {
    return didRunInitially;
  },
  set didRunInitially(newVal) {
    didRunInitially = newVal;
  },

  get ready() {
    return ready;
  },
  set ready(newVal) {
    ready = newVal;
  },

  get adbFilePromise() {
    if (this._adbFilePromise) {
      return this._adbFilePromise;
    }
    this._adbFilePromise = getFileForBinary();
    return this._adbFilePromise;
  },

  async _runProcess(process, params) {
    return new Promise((resolve, reject) => {
      process.runAsync(params, params.length, {
        observe(subject, topic, data) {
          switch (topic) {
            case "process-finished":
              resolve();
              break;
            case "process-failed":
              reject();
              break;
          }
        }
      }, false);
    });
  },

  // Waits until a predicate returns true or re-tries the predicate calls
  // |retry| times, we wait for 100ms between each calls.
  async _waitUntil(predicate, retry = 20) {
    let count = 0;
    while (count++ < retry) {
      if (await predicate()) {
        return true;
      }
      // Wait for 100 milliseconds.
      await new Promise(resolve => setTimeout(resolve, 100));
    }
    // Timed out after trying too many times.
    return false;
  },

  // We startup by launching adb in server mode, and setting
  // the tcp socket preference to |true|
  async start() {
    return new Promise(async (resolve, reject) => {
      const onSuccessfulStart = () => {
        Services.obs.notifyObservers(null, "adb-ready");
        this.ready = true;
        resolve();
      };

      const isAdbRunning = await check();
      if (isAdbRunning) {
        this.didRunInitially = false;
        dumpn("Found ADB process running, not restarting");
        onSuccessfulStart();
        return;
      }
      dumpn("Didn't find ADB process running, restarting");

      this.didRunInitially = true;
      const process = Cc["@mozilla.org/process/util;1"]
                      .createInstance(Ci.nsIProcess);
      // FIXME: Bug 1481691 - We should avoid extracting files every time.
      const adbFile = await this.adbFilePromise;
      process.init(adbFile);
      // Hide command prompt window on Windows
      process.startHidden = true;
      process.noShell = true;
      const params = ["start-server"];
      let isStarted = false;
      try {
        await this._runProcess(process, params);
        isStarted = await this._waitUntil(check);
      } catch (e) {
      }

      if (isStarted) {
        onSuccessfulStart();
      } else {
        this.ready = false;
        reject();
      }
    });
  },

  /**
   * Stop the ADB server, but only if we started it.  If it was started before
   * us, we return immediately.
   *
   * @param boolean sync
   *        In case, we do need to kill the server, this param is passed through
   *        to kill to determine whether it's a sync operation.
   */
  async stop(sync) {
    if (!this.didRunInitially) {
      return; // We didn't start the server, nothing to do
    }
    await this.kill(sync);
    // Make sure the ADB server stops listening because kill() above doesn't
    // mean that the ADB server stops, it means that 'adb kill-server' command
    // just finished, so that it's possible that the ADB server is still alive.
    await this._waitUntil(async () => {
      return !await check();
    });
  },

  /**
   * Kill the ADB server.  We do this by running ADB again, passing it
   * the "kill-server" argument.
   *
   * @param {Boolean} sync
   *        Whether or not to kill the server synchronously.  In general,
   *        this should be false.  But on Windows, an add-on may fail to update
   *        if its copy of ADB is running when Firefox tries to update it.
   *        So add-ons who observe their own updates and kill the ADB server
   *        beforehand should do so synchronously on Windows to make sure
   *        the update doesn't race the killing.
   */
  async kill(sync) {
    const process = Cc["@mozilla.org/process/util;1"]
                    .createInstance(Ci.nsIProcess);
    const adbFile = await this.adbFilePromise;
    process.init(adbFile);
    // Hide command prompt window on Windows
    process.startHidden = true;
    process.noShell = true;
    const params = ["kill-server"];

    if (sync) {
      process.run(true, params, params.length);
      dumpn("adb kill-server: " + process.exitValue);
      this.ready = false;
      this.didRunInitially = false;
    } else {
      const self = this;
      process.runAsync(params, params.length, {
        observe(subject, topic, data) {
          switch (topic) {
            case "process-finished":
              dumpn("adb kill-server: " + process.exitValue);
              Services.obs.notifyObservers(null, "adb-killed");
              self.ready = false;
              self.didRunInitially = false;
              break;
            case "process-failed":
              dumpn("adb kill-server failure: " + process.exitValue);
              // It's hard to say whether or not ADB is ready at this point,
              // but it seems safer to assume that it isn't, so code that wants
              // to use it later will try to restart it.
              Services.obs.notifyObservers(null, "adb-killed");
              self.ready = false;
              self.didRunInitially = false;
              break;
          }
        }
      }, false);
    }
  },

  // Start tracking devices connecting and disconnecting from the host.
  // We can't reuse runCommand here because we keep the socket alive.
  // @return The socket used.
  trackDevices() {
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
            ADB.trackDevices(); // Re-track devices
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
  },

  // Sends back an array of device names.
  listDevices() {
    dumpn("listDevices");

    return this.runCommand("host:devices").then(
      function onSuccess(data) {
        const lines = data.split("\n");
        const res = [];
        lines.forEach(function(line) {
          if (line.length == 0) {
            return;
          }
          const [ device ] = line.split("\t");
          res.push(device);
        });
        return res;
      }
    );
  },

  // sends adb forward localPort devicePort
  forwardPort(localPort, devicePort) {
    dumpn("forwardPort " + localPort + " -- " + devicePort);
    // <host-prefix>:forward:<local>;<remote>

    return this.runCommand("host:forward:" + localPort + ";" + devicePort)
               .then(function onSuccess(data) {
                 return data;
               });
  },

  // Run a shell command
  shell(command) {
    const deferred = PromiseUtils.defer();
    let state;
    let stdout = "";

    dumpn("shell " + command);

    const shutdown = function() {
      dumpn("shell shutdown");
      socket.close();
      deferred.reject("BAD_RESPONSE");
    };

    const runFSM = function runFSM(data) {
      dumpn("runFSM " + state);
      let req;
      let ignoreResponseCode = false;
      switch (state) {
        case "start":
          state = "send-transport";
          runFSM();
          break;
        case "send-transport":
          req = client.createRequest("host:transport-any");
          socket.send(req);
          state = "wait-transport";
          break;
        case "wait-transport":
          if (!client.checkResponse(data, OKAY)) {
            shutdown();
            return;
          }
          state = "send-shell";
          runFSM();
          break;
        case "send-shell":
          req = client.createRequest("shell:" + command);
          socket.send(req);
          state = "rec-shell";
          break;
        case "rec-shell":
          if (!client.checkResponse(data, OKAY)) {
            shutdown();
            return;
          }
          state = "decode-shell";
          if (client.getBuffer(data).byteLength == 4) {
            break;
          }
          ignoreResponseCode = true;
          // eslint-disable-next-lined no-fallthrough
        case "decode-shell":
          const decoder = new TextDecoder();
          const text = new Uint8Array(client.getBuffer(data),
                                      ignoreResponseCode ? 4 : 0);
          stdout += decoder.decode(text);
          break;
        default:
          dumpn("shell Unexpected State: " + state);
          deferred.reject("UNEXPECTED_STATE");
      }
    };

    const socket = client.connect();
    socket.s.onerror = function(event) {
      dumpn("shell onerror");
      deferred.reject("SOCKET_ERROR");
    };

    socket.s.onopen = function(event) {
      dumpn("shell onopen");
      state = "start";
      runFSM();
    };

    socket.s.onclose = function(event) {
      deferred.resolve(stdout);
      dumpn("shell onclose");
    };

    socket.s.ondata = function(event) {
      dumpn("shell ondata");
      runFSM(event.data);
    };

    return deferred.promise;
  },

  // Asynchronously runs an adb command.
  // @param command The command as documented in
  // http://androidxref.com/4.0.4/xref/system/core/adb/SERVICES.TXT
  runCommand(command) {
    dumpn("runCommand " + command);
    const deferred = PromiseUtils.defer();
    if (!this.ready) {
      setTimeout(function() {
        deferred.reject("ADB_NOT_READY");
      });
      return deferred.promise;
    }

    const socket = client.connect();

    socket.s.onopen = function() {
      dumpn("runCommand onopen");
      const req = client.createRequest(command);
      socket.send(req);
    };

    socket.s.onerror = function() {
      dumpn("runCommand onerror");
      deferred.reject("NETWORK_ERROR");
    };

    socket.s.onclose = function() {
      dumpn("runCommand onclose");
    };

    socket.s.ondata = function(event) {
      dumpn("runCommand ondata");
      const data = event.data;

      const packet = client.unpackPacket(data, false);
      if (!client.checkResponse(data, OKAY)) {
        socket.close();
        dumpn("Error: " + packet.data);
        deferred.reject("PROTOCOL_ERROR");
        return;
      }

      deferred.resolve(packet.data);
    };

    return deferred.promise;
  }
};

exports.ADB = ADB;
