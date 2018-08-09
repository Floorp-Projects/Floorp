/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Wrapper around the ADB utility.

"use strict";

const { Cc, Ci } = require("chrome");
const EventEmitter = require("devtools/shared/event-emitter");
const client = require("./adb-client");
const { getFileForBinary } = require("./adb-binary");
const { setTimeout } = require("resource://gre/modules/Timer.jsm");
const { PromiseUtils } = require("resource://gre/modules/PromiseUtils.jsm");
const { OS } = require("resource://gre/modules/osfile.jsm");
const { Services } = require("resource://gre/modules/Services.jsm");
loader.lazyRequireGetter(this, "check",
                         "devtools/shared/adb/adb-running-checker", true);

let ready = false;
let didRunInitially = false;

const OKAY = 0x59414b4f;
// const FAIL = 0x4c494146;
// const STAT = 0x54415453;
const DATA = 0x41544144;
const DONE = 0x454e4f44;

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

  // We startup by launching adb in server mode, and setting
  // the tcp socket preference to |true|
  start() {
    return new Promise(async (resolve, reject) => {
      const onSuccessfulStart = () => {
        Services.obs.notifyObservers(null, "adb-ready");
        this.ready = true;
        resolve();
      };

      const isAdbRunning = await check();
      if (isAdbRunning) {
        this.didRunInitially = false;
        console.log("Found ADB process running, not restarting");
        onSuccessfulStart();
        return;
      }
      console.log("Didn't find ADB process running, restarting");

      this.didRunInitially = true;
      const process = Cc["@mozilla.org/process/util;1"]
                      .createInstance(Ci.nsIProcess);
      // FIXME: Bug 1481691 - We should avoid extracting files every time.
      const adbFile = await this.adbFilePromise;
      process.init(adbFile);
      // Hide command prompt window on Windows
      try {
        // startHidden is 55+
        process.startHidden = true;
        // noShell attribute is 58+
        process.noShell = true;
      } catch (e) {
      }
      const params = ["start-server"];
      const self = this;
      process.runAsync(params, params.length, {
        observe(subject, topic, data) {
          switch (topic) {
            case "process-finished":
              onSuccessfulStart();
              break;
            case "process-failed":
              self.ready = false;
              reject();
              break;
          }
        }
      }, false);
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
    // Make sure the ADB server stops listening.
    //
    // kill() above doesn't mean that the ADB server stops, it just means that
    // 'adb kill-server' command finished, so that it's possible that the ADB
    // server is still alive there.
    while (true) {
      const isAdbRunning = await check();
      if (!isAdbRunning) {
        break;
      }
    }
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
    try {
      // startHidden is 55+
      process.startHidden = true;
      // noShell attribute is 58+
      process.noShell = true;
    } catch (e) {
    }
    const params = ["kill-server"];

    if (sync) {
      process.run(true, params, params.length);
      console.log("adb kill-server: " + process.exitValue);
      this.ready = false;
      this.didRunInitially = false;
    } else {
      const self = this;
      process.runAsync(params, params.length, {
        observe(subject, topic, data) {
          switch (topic) {
            case "process-finished":
              console.log("adb kill-server: " + process.exitValue);
              Services.obs.notifyObservers(null, "adb-killed");
              self.ready = false;
              self.didRunInitially = false;
              break;
            case "process-failed":
              console.log("adb kill-server failure: " + process.exitValue);
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
    console.log("trackDevices");
    const socket = client.connect();
    let waitForFirst = true;
    const devices = {};

    socket.s.onopen = function() {
      console.log("trackDevices onopen");
      Services.obs.notifyObservers(null, "adb-track-devices-start");
      const req = client.createRequest("host:track-devices");
      socket.send(req);
    };

    socket.s.onerror = function(event) {
      console.log("trackDevices onerror: " + event);
      Services.obs.notifyObservers(null, "adb-track-devices-stop");
    };

    socket.s.onclose = function() {
      console.log("trackDevices onclose");

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
      console.log("trackDevices ondata");
      const data = event.data;
      console.log("length=" + data.byteLength);
      const dec = new TextDecoder();
      console.log(dec.decode(new Uint8Array(data)).trim());

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
    console.log("listDevices");

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
    console.log("forwardPort " + localPort + " -- " + devicePort);
    // <host-prefix>:forward:<local>;<remote>

    return this.runCommand("host:forward:" + localPort + ";" + devicePort)
               .then(function onSuccess(data) {
                 return data;
               });
  },

  // pulls a file from the device.
  // send "host:transport-any" why??
  // if !OKAY, return
  // send "sync:"
  // if !OKAY, return
  // send STAT + hex4(path.length) + path
  // recv STAT + 12 bytes (3 x 32 bits: mode, size, time)
  // send RECV + hex4(path.length) + path
  // while(needs data):
  //   recv DATA + hex4 + data
  // recv DONE + hex4(0)
  // send QUIT + hex4(0)
  pull(from, dest) {
    const deferred = PromiseUtils.defer();
    let state;
    let fileData = null;
    let currentPos = 0;
    let chunkSize = 0;
    let pkgData;
    const headerArray = new Uint32Array(2);
    let currentHeaderLength = 0;

    const encoder = new TextEncoder();
    let infoLengthPacket;

    console.log("pulling " + from + " -> " + dest);

    const shutdown = function() {
      console.log("pull shutdown");
      socket.close();
      deferred.reject("BAD_RESPONSE");
    };

    // extract chunk data header info. to headerArray.
    const extractChunkDataHeader = function(data) {
      const tmpArray = new Uint8Array(headerArray.buffer);
      for (let i = 0; i < 8 - currentHeaderLength; i++) {
        tmpArray[currentHeaderLength + i] = data[i];
      }
    };

    // chunk data header is 8 bytes length,
    // the first 4 bytes: hex4("DATA"), and
    // the second 4 bytes: hex4(chunk size)
    const checkChunkDataHeader = function(data) {
      if (data.length + currentHeaderLength >= 8) {
        extractChunkDataHeader(data);

        if (headerArray[0] != DATA) {
          shutdown();
          return false;
        }
        // remove header info. from socket package data
        pkgData = data.subarray(8 - currentHeaderLength, data.length);
        chunkSize = headerArray[1];
        currentHeaderLength = 0;
        return true;
      }

      // If chunk data header info. is separated into more than one
      // socket package, keep partial header info. in headerArray.
      const tmpArray = new Uint8Array(headerArray.buffer);
      for (let i = 0; i < data.length; i++) {
        tmpArray[currentHeaderLength + i] = data[i];
      }
      currentHeaderLength += data.length;
      return true;
    };

    // The last remaining package data contains 8 bytes,
    // they are "DONE(0x454e4f44)" and 0x0000.
    const checkDone = function(data) {
      if (data.length != 8) {
        return false;
      }

      const doneFlagArray = new Uint32Array(1);
      const tmpArray = new Uint8Array(doneFlagArray.buffer);
      for (let i = 0; i < 4; i++) {
        tmpArray[i] = data[i];
      }
      // Check DONE flag
      if (doneFlagArray[0] == DONE) {
        return true;
      }
      return false;
    };

    const runFSM = function runFSM(data) {
      console.log("runFSM " + state);
      let req;
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
          console.log("transport: OK");
          state = "send-sync";
          runFSM();
          break;
        case "send-sync":
          req = client.createRequest("sync:");
          socket.send(req);
          state = "wait-sync";
          break;
        case "wait-sync":
          if (!client.checkResponse(data, OKAY)) {
            shutdown();
            return;
          }
          console.log("sync: OK");
          state = "send-recv";
          runFSM();
          break;
        case "send-recv":
          infoLengthPacket = new Uint32Array(1);
          infoLengthPacket[0] = from.length;
          socket.send(encoder.encode("RECV"));
          socket.send(infoLengthPacket);
          socket.send(encoder.encode(from));

          state = "wait-recv";
          break;
        case "wait-recv":
          // After sending "RECV" command, adb server will send chunks data back,
          // Handle every single socket package here.
          // Note: One socket package maybe contain many chunks, and often
          // partial chunk at the end.
          pkgData = new Uint8Array(client.getBuffer(data));

          // Handle all data in a single socket package.
          while (pkgData.length > 0) {
            if (chunkSize == 0 && checkDone(pkgData)) {
              OS.File.writeAtomic(dest, fileData, {}).then(
                function onSuccess(number) {
                  console.log(number);
                  deferred.resolve("SUCCESS");
                },
                function onFailure(reason) {
                  console.log(reason);
                  deferred.reject("CANT_ACCESS_FILE");
                }
              );

              state = "send-quit";
              runFSM();
              return;
            }
            if (chunkSize == 0 && !checkChunkDataHeader(pkgData)) {
              shutdown();
              return;
            }
            // handle full chunk
            if (chunkSize > 0 && pkgData.length >= chunkSize) {
              const chunkData = pkgData.subarray(0, chunkSize);
              const tmpData = new Uint8Array(currentPos + chunkSize);
              if (fileData) {
                tmpData.set(fileData, 0);
              }
              tmpData.set(chunkData, currentPos);
              fileData = tmpData;
              pkgData = pkgData.subarray(chunkSize, pkgData.length);
              currentPos += chunkSize;
              chunkSize = 0;
            }
            // handle partial chunk at the end of socket package
            if (chunkSize > 0 && pkgData.length > 0 && pkgData.length < chunkSize) {
              const tmpData = new Uint8Array(currentPos + pkgData.length);
              if (fileData) {
                tmpData.set(fileData, 0);
              }
              tmpData.set(pkgData, currentPos);
              fileData = tmpData;
              currentPos += pkgData.length;
              chunkSize -= pkgData.length;
              break; // Break while loop.
            }
          }

          break;
        case "send-quit":
          infoLengthPacket = new Uint32Array(1);
          infoLengthPacket[0] = 0;
          socket.send(encoder.encode("QUIT"));
          socket.send(infoLengthPacket);

          state = "end";
          runFSM();
          break;
        case "end":
          socket.close();
          break;
        default:
          console.log("pull Unexpected State: " + state);
          deferred.reject("UNEXPECTED_STATE");
      }
    };

    const setupSocket = function() {
      socket.s.onerror = function(event) {
        console.log("pull onerror");
        deferred.reject("SOCKET_ERROR");
      };

      socket.s.onopen = function(event) {
        console.log("pull onopen");
        state = "start";
        runFSM();
      };

      socket.s.onclose = function(event) {
        console.log("pull onclose");
      };

      socket.s.ondata = function(event) {
        console.log("pull ondata:");
        runFSM(event.data);
      };
    };

    const socket = client.connect();
    setupSocket();

    return deferred.promise;
  },

  // pushes a file to the device.
  // from and dest are full paths.
  // XXX we should STAT the remote path before sending.
  push(from, dest) {
    const deferred = PromiseUtils.defer();
    let socket;
    let state;
    let fileSize;
    let fileData;
    let remaining;
    let currentPos = 0;
    let fileTime;

    console.log("pushing " + from + " -> " + dest);

    const shutdown = function() {
      console.log("push shutdown");
      socket.close();
      deferred.reject("BAD_RESPONSE");
    };

    const runFSM = function runFSM(data) {
      console.log("runFSM " + state);
      let req;
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
          console.log("transport: OK");
          state = "send-sync";
          runFSM();
          break;
        case "send-sync":
          req = client.createRequest("sync:");
          socket.send(req);
          state = "wait-sync";
          break;
        case "wait-sync":
          if (!client.checkResponse(data, OKAY)) {
            shutdown();
            return;
          }
          console.log("sync: OK");
          state = "send-send";
          runFSM();
          break;
        case "send-send":
          // need to send SEND + length($dest,$fileMode)
          // $fileMode is not the octal one there.
          const encoder = new TextEncoder();

          const infoLengthPacket = new Uint32Array(1), info = dest + ",33204";
          infoLengthPacket[0] = info.length;
          socket.send(encoder.encode("SEND"));
          socket.send(infoLengthPacket);
          socket.send(encoder.encode(info));

          // now sending file data.
          while (remaining > 0) {
            const toSend = remaining > 65536 ? 65536 : remaining;
            console.log("Sending " + toSend + " bytes");

            const dataLengthPacket = new Uint32Array(1);
            // We have to create a new ArrayBuffer for the fileData slice
            // because nsIDOMTCPSocket (or ArrayBufferInputStream) chokes on
            // reused buffers, even when we don't modify their contents.
            const dataPacket = new Uint8Array(new ArrayBuffer(toSend));
            dataPacket.set(new Uint8Array(fileData.buffer, currentPos, toSend));
            dataLengthPacket[0] = toSend;
            socket.send(encoder.encode("DATA"));
            socket.send(dataLengthPacket);
            socket.send(dataPacket);

            currentPos += toSend;
            remaining -= toSend;
          }

          // Ending up with DONE + mtime (wtf???)
          const fileTimePacket = new Uint32Array(1);
          fileTimePacket[0] = fileTime;
          socket.send(encoder.encode("DONE"));
          socket.send(fileTimePacket);

          state = "wait-done";
          break;
        case "wait-done":
          if (!client.checkResponse(data, OKAY)) {
            shutdown();
            return;
          }
          console.log("DONE: OK");
          state = "end";
          runFSM();
          break;
        case "end":
          socket.close();
          deferred.resolve("SUCCESS");
          break;
        default:
          console.log("push Unexpected State: " + state);
          deferred.reject("UNEXPECTED_STATE");
      }
    };

    const setupSocket = function() {
      socket.s.onerror = function(event) {
        console.log("push onerror");
        deferred.reject("SOCKET_ERROR");
      };

      socket.s.onopen = function(event) {
        console.log("push onopen");
        state = "start";
        runFSM();
      };

      socket.s.onclose = function(event) {
        console.log("push onclose");
      };

      socket.s.ondata = function(event) {
        console.log("push ondata");
        runFSM(event.data);
      };
    };
    // Stat the file, get its size.
    OS.File.stat(from).then(
      function onSuccess(stat) {
        if (stat.isDir) {
          // The path represents a directory
          deferred.reject("CANT_PUSH_DIR");
        } else {
          // The path represents a file, not a directory
          fileSize = stat.size;
          // We want seconds since epoch
          fileTime = stat.lastModificationDate.getTime() / 1000;
          remaining = fileSize;
          console.log(from + " size is " + fileSize);
          const readPromise = OS.File.read(from);
          readPromise.then(
            function readSuccess(data) {
              fileData = data;
              socket = client.connect();
              setupSocket();
            },
            function readError() {
              deferred.reject("READ_FAILED");
            }
          );
        }
      },
      function onFailure(reason) {
        console.log(reason);
        deferred.reject("CANT_ACCESS_FILE");
      }
    );

    return deferred.promise;
  },

  // Run a shell command
  shell(command) {
    const deferred = PromiseUtils.defer();
    let state;
    let stdout = "";

    console.log("shell " + command);

    const shutdown = function() {
      console.log("shell shutdown");
      socket.close();
      deferred.reject("BAD_RESPONSE");
    };

    const runFSM = function runFSM(data) {
      console.log("runFSM " + state);
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
        case "decode-shell":
          const decoder = new TextDecoder();
          const text = new Uint8Array(client.getBuffer(data),
                                      ignoreResponseCode ? 4 : 0);
          stdout += decoder.decode(text);
          break;
        default:
          console.log("shell Unexpected State: " + state);
          deferred.reject("UNEXPECTED_STATE");
      }
    };

    const socket = client.connect();
    socket.s.onerror = function(event) {
      console.log("shell onerror");
      deferred.reject("SOCKET_ERROR");
    };

    socket.s.onopen = function(event) {
      console.log("shell onopen");
      state = "start";
      runFSM();
    };

    socket.s.onclose = function(event) {
      deferred.resolve(stdout);
      console.log("shell onclose");
    };

    socket.s.ondata = function(event) {
      console.log("shell ondata");
      runFSM(event.data);
    };

    return deferred.promise;
  },

  reboot() {
    return this.shell("reboot");
  },

  rebootRecovery() {
    return this.shell("reboot recovery");
  },

  rebootBootloader() {
    return this.shell("reboot bootloader");
  },

  root() {
    const deferred = PromiseUtils.defer();
    let state;

    console.log("root");

    const shutdown = function() {
      console.log("root shutdown");
      socket.close();
      deferred.reject("BAD_RESPONSE");
    };

    const runFSM = function runFSM(data) {
      console.log("runFSM " + state);
      let req;
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
          state = "send-root";
          runFSM();
          break;
        case "send-root":
          req = client.createRequest("root:");
          socket.send(req);
          state = "rec-root";
          break;
        case "rec-root":
          // Nothing to do
          break;
        default:
          console.log("root Unexpected State: " + state);
          deferred.reject("UNEXPECTED_STATE");
      }
    };

    const socket = client.connect();
    socket.s.onerror = function(event) {
      console.log("root onerror");
      deferred.reject("SOCKET_ERROR");
    };

    socket.s.onopen = function(event) {
      console.log("root onopen");
      state = "start";
      runFSM();
    };

    socket.s.onclose = function(event) {
      deferred.resolve();
      console.log("root onclose");
    };

    socket.s.ondata = function(event) {
      console.log("root ondata");
      runFSM(event.data);
    };

    return deferred.promise;
  },

  // Asynchronously runs an adb command.
  // @param command The command as documented in
  // http://androidxref.com/4.0.4/xref/system/core/adb/SERVICES.TXT
  runCommand(command) {
    console.log("runCommand " + command);
    const deferred = PromiseUtils.defer();
    if (!this.ready) {
      setTimeout(function() {
        deferred.reject("ADB_NOT_READY");
      });
      return deferred.promise;
    }

    const socket = client.connect();

    socket.s.onopen = function() {
      console.log("runCommand onopen");
      const req = client.createRequest(command);
      socket.send(req);
    };

    socket.s.onerror = function() {
      console.log("runCommand onerror");
      deferred.reject("NETWORK_ERROR");
    };

    socket.s.onclose = function() {
      console.log("runCommand onclose");
    };

    socket.s.ondata = function(event) {
      console.log("runCommand ondata");
      const data = event.data;

      const packet = client.unpackPacket(data, false);
      if (!client.checkResponse(data, OKAY)) {
        socket.close();
        console.log("Error: " + packet.data);
        deferred.reject("PROTOCOL_ERROR");
        return;
      }

      deferred.resolve(packet.data);
    };

    return deferred.promise;
  }
};

exports.ADB = ADB;
