/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EventEmitter = require("devtools/shared/event-emitter");
const { ConnectionManager } =
  require("devtools/shared/client/connection-manager");
const { Devices } = require("devtools/shared/apps/Devices.jsm");
const { dumpn } = require("devtools/shared/DevToolsUtils");
const { RuntimeTypes } =
  require("devtools/client/webide/modules/runtime-types");

const ADBScanner = {

  _runtimes: [],

  enable() {
    this._updateRuntimes = this._updateRuntimes.bind(this);
    Devices.on("register", this._updateRuntimes);
    Devices.on("unregister", this._updateRuntimes);
    Devices.on("addon-status-updated", this._updateRuntimes);
    this._updateRuntimes();
  },

  disable() {
    Devices.off("register", this._updateRuntimes);
    Devices.off("unregister", this._updateRuntimes);
    Devices.off("addon-status-updated", this._updateRuntimes);
  },

  _emitUpdated() {
    this.emit("runtime-list-updated");
  },

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
  },

  _detectRuntimes: async function(device) {
    const model = await device.getModel();
    let detectedRuntimes = await FirefoxOSRuntime.detect(device, model);
    this._runtimes.push(...detectedRuntimes);
    detectedRuntimes = await FirefoxOnAndroidRuntime.detect(device, model);
    this._runtimes.push(...detectedRuntimes);
  },

  scan() {
    return this._updateRuntimes();
  },

  listRuntimes() {
    return this._runtimes;
  }

};

EventEmitter.decorate(ADBScanner);

function Runtime(device, model, socketPath) {
  this.device = device;
  this._model = model;
  this._socketPath = socketPath;
}

Runtime.prototype = {
  type: RuntimeTypes.USB,
  connect(connection) {
    const port = ConnectionManager.getFreeTCPPort();
    const local = "tcp:" + port;
    let remote;
    if (this._socketPath.startsWith("@")) {
      remote = "localabstract:" + this._socketPath.substring(1);
    } else {
      remote = "localfilesystem:" + this._socketPath;
    }
    return this.device.forwardPort(local, remote).then(() => {
      connection.host = "localhost";
      connection.port = port;
      connection.connect();
    });
  },
  get id() {
    return this.device.id + "|" + this._socketPath;
  },
};

// FIXME: Bug 1481691 - Drop code for support FirefoxOS.
function FirefoxOSRuntime(device, model) {
  Runtime.call(this, device, model, "/data/local/debugger-socket");
}

FirefoxOSRuntime.detect = async function(device, model) {
  const runtimes = [];
  const query = "test -f /system/b2g/b2g; echo $?";
  let b2gExists = await device.shell(query);
  // XXX: Sometimes we get an empty response back.  Likely a bug in our shell
  // code in this add-on.
  // There are also some Android devices that do not have `test` installed.
  for (let attempts = 3; attempts > 0; attempts--) {
    b2gExists = await device.shell(query);
    if (b2gExists.length == 3) {
      break;
    }
  }
  if (b2gExists === "0\r\n") {
    const runtime = new FirefoxOSRuntime(device, model);
    dumpn("Found " + runtime.name);
    runtimes.push(runtime);
  }
  return runtimes;
};

FirefoxOSRuntime.prototype = Object.create(Runtime.prototype);

Object.defineProperty(FirefoxOSRuntime.prototype, "name", {
  get() {
    return this._model || this.device.id;
  }
});

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

Object.defineProperty(FirefoxOnAndroidRuntime.prototype, "name", {
  get() {
    // If using abstract socket address, it is "@org.mozilla.firefox/..."
    // If using path base socket, it is "/data/data/<package>...""
    // Until Fennec 62 only supports path based UNIX domain socket, but
    // Fennec 63+ supports both path based and abstract socket.
    const packageName = this._socketPath.startsWith("@") ?
                        this._socketPath.substr(1).split("/")[0] :
                        this._socketPath.split("/")[3];
    let channel;
    switch (packageName) {
      case "org.mozilla.firefox":
        channel = "";
        break;
      case "org.mozilla.firefox_beta":
        channel = " Beta";
        break;
      case "org.mozilla.fennec_aurora":
        // This package name is now the one for Firefox Nightly distributed
        // through the Google Play Store since "dawn project"
        // cf. https://bugzilla.mozilla.org/show_bug.cgi?id=1357351#c8
        channel = " Nightly";
        break;
      case "org.mozilla.fennec":
        channel = " Nightly";
        break;
      default:
        channel = " Custom";
    }
    return "Firefox" + channel + " on Android (" +
           (this._model || this.device.id) + ")";
  }
});

exports.ADBScanner = ADBScanner;
