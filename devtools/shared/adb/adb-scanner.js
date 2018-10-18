/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EventEmitter = require("devtools/shared/event-emitter");
const { Devices } = require("devtools/shared/apps/Devices.jsm");
const { dumpn } = require("devtools/shared/DevToolsUtils");
const { RuntimeTypes } =
  require("devtools/client/webide/modules/runtime-types");
const { ADB } = require("devtools/shared/adb/adb");
loader.lazyRequireGetter(this, "Device", "devtools/shared/adb/adb-device");

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

    Devices.on("register", this._updateRuntimes);
    Devices.on("unregister", this._updateRuntimes);
    Devices.on("addon-status-updated", this._updateRuntimes);

    ADB.start().then(() => {
      ADB.trackDevices();
    });
    this._updateRuntimes();
  }

  disable() {
    EventEmitter.off(ADB, "device-connected", this._onDeviceConnected);
    EventEmitter.off(ADB, "device-disconnected", this._onDeviceDisconnected);
    Devices.off("register", this._updateRuntimes);
    Devices.off("unregister", this._updateRuntimes);
    Devices.off("addon-status-updated", this._updateRuntimes);
  }

  _emitUpdated() {
    this.emit("runtime-list-updated");
  }

  _onDeviceConnected(deviceId) {
    const device = new Device(deviceId);
    Devices.register(deviceId, device);
  }

  _onDeviceDisconnected(deviceId) {
    Devices.unregister(deviceId);
  }

  _updateRuntimes() {
    if (this._updatingPromise) {
      return this._updatingPromise;
    }
    this._runtimes = [];
    const promises = [];
    for (const id of Devices.available()) {
      const device = Devices.getByName(id);
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

  async _detectRuntimes(device) {
    const model = await device.getModel();
    const detectedRuntimes =
      await FirefoxOnAndroidRuntime.detect(device, model);
    this._runtimes.push(...detectedRuntimes);
  }

  scan() {
    return this._updateRuntimes();
  }

  listRuntimes() {
    return this._runtimes;
  }
}

function Runtime(device, model, socketPath) {
  this.device = device;
  this._model = model;
  this._socketPath = socketPath;
}

Runtime.prototype = {
  type: RuntimeTypes.USB,
  connect(connection) {
    return ADB.prepareTCPConnection(this._socketPath).then(port => {
      connection.host = "localhost";
      connection.port = port;
      connection.connect();
    });
  },
  get id() {
    return this.device.id + "|" + this._socketPath;
  },
};

function FirefoxOnAndroidRuntime(device, model, socketPath) {
  Runtime.call(this, device, model, socketPath);
}

// This requires Unix socket support from Firefox for Android (35+)
FirefoxOnAndroidRuntime.detect = async function(device, model) {
  const runtimes = [];
  // A matching entry looks like:
  // 00000000: 00000002 00000000 00010000 0001 01 6551588
  //  /data/data/org.mozilla.fennec/firefox-debugger-socket
  const query = "cat /proc/net/unix";
  const rawSocketInfo = await device.shell(query);
  let socketInfos = rawSocketInfo.split(/\r?\n/);
  // Filter to lines with "firefox-debugger-socket"
  socketInfos = socketInfos.filter(l => l.includes("firefox-debugger-socket"));
  // It's possible to have multiple lines with the same path, so de-dupe them
  const socketPaths = new Set();
  for (const socketInfo of socketInfos) {
    const socketPath = socketInfo.split(" ").pop();
    socketPaths.add(socketPath);
  }
  for (const socketPath of socketPaths) {
    const runtime = new FirefoxOnAndroidRuntime(device, model, socketPath);
    dumpn("Found " + runtime.name);
    runtimes.push(runtime);
  }
  return runtimes;
};

FirefoxOnAndroidRuntime.prototype = Object.create(Runtime.prototype);

FirefoxOnAndroidRuntime.prototype._channel = function() {
  const packageName = this._packageName();

  switch (packageName) {
    case "org.mozilla.firefox":
      return "";
    case "org.mozilla.firefox_beta":
      return "Beta";
    case "org.mozilla.fennec":
    case "org.mozilla.fennec_aurora":
      // This package name is now the one for Firefox Nightly distributed
      // through the Google Play Store since "dawn project"
      // cf. https://bugzilla.mozilla.org/show_bug.cgi?id=1357351#c8
      return "Nightly";
    default:
      return "Custom";
  }
};

FirefoxOnAndroidRuntime.prototype._packageName = function() {
  // If using abstract socket address, it is "@org.mozilla.firefox/..."
  // If using path base socket, it is "/data/data/<package>...""
  // Until Fennec 62 only supports path based UNIX domain socket, but
  // Fennec 63+ supports both path based and abstract socket.
  return this._socketPath.startsWith("@") ?
    this._socketPath.substr(1).split("/")[0] :
    this._socketPath.split("/")[3];
};

Object.defineProperty(FirefoxOnAndroidRuntime.prototype, "shortName", {
  get() {
    return `Firefox ${this._channel()}`;
  }
});

Object.defineProperty(FirefoxOnAndroidRuntime.prototype, "deviceName", {
  get() {
    return this._model || this.device.id;
  }
});

Object.defineProperty(FirefoxOnAndroidRuntime.prototype, "name", {
  get() {
    const channel = this._channel();
    return "Firefox " + channel + " on Android (" + this.deviceName + ")";
  }
});

exports.ADBScanner = ADBScanner;
