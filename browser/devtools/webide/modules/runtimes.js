/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {Cu} = require("chrome");
const {Devices} = Cu.import("resource://gre/modules/devtools/Devices.jsm");
const {Services} = Cu.import("resource://gre/modules/Services.jsm");
const {Simulator} = Cu.import("resource://gre/modules/devtools/Simulator.jsm");
const {ConnectionManager, Connection} = require("devtools/client/connection-manager");
const {DebuggerServer} = require("resource://gre/modules/devtools/dbg-server.jsm");
const discovery = require("devtools/toolkit/discovery/discovery");
const promise = require("promise");

const Strings = Services.strings.createBundle("chrome://webide/content/webide.properties");

function USBRuntime(id) {
  this.id = id;
}

USBRuntime.prototype = {
  connect: function(connection) {
    let device = Devices.getByName(this.id);
    if (!device) {
      return promise.reject("Can't find device: " + this.getName());
    }
    return device.connect().then((port) => {
      connection.host = "localhost";
      connection.port = port;
      connection.connect();
    });
  },
  getID: function() {
    return this.id;
  },
  getName: function() {
    return this._productModel || this.id;
  },
  updateNameFromADB: function() {
    if (this._productModel) {
      return promise.resolve();
    }
    let device = Devices.getByName(this.id);
    let deferred = promise.defer();
    if (device && device.shell) {
      device.shell("getprop ro.product.model").then(stdout => {
        this._productModel = stdout;
        deferred.resolve();
      }, () => {});
    } else {
      this._productModel = null;
      deferred.reject();
    }
    return deferred.promise;
  },
}

function WiFiRuntime(deviceName) {
  this.deviceName = deviceName;
}

WiFiRuntime.prototype = {
  connect: function(connection) {
    let service = discovery.getRemoteService("devtools", this.deviceName);
    if (!service) {
      return promise.reject("Can't find device: " + this.getName());
    }
    connection.host = service.host;
    connection.port = service.port;
    connection.connect();
    return promise.resolve();
  },
  getID: function() {
    return this.deviceName;
  },
  getName: function() {
    return this.deviceName;
  },
}

function SimulatorRuntime(version) {
  this.version = version;
}

SimulatorRuntime.prototype = {
  connect: function(connection) {
    let port = ConnectionManager.getFreeTCPPort();
    let simulator = Simulator.getByVersion(this.version);
    if (!simulator || !simulator.launch) {
      return promise.reject("Can't find simulator: " + this.getName());
    }
    return simulator.launch({port: port}).then(() => {
      connection.port = port;
      connection.keepConnecting = true;
      connection.connect();
    });
  },
  getID: function() {
    return this.version;
  },
  getName: function() {
    return Simulator.getByVersion(this.version).appinfo.label;
  },
}

let gLocalRuntime = {
  connect: function(connection) {
    if (!DebuggerServer.initialized) {
      DebuggerServer.init();
      DebuggerServer.addBrowserActors();
    }
    connection.port = null;
    connection.host = null; // Force Pipe transport
    connection.connect();
    return promise.resolve();
  },
  getName: function() {
    return Strings.GetStringFromName("local_runtime");
  },
}

let gRemoteRuntime = {
  connect: function(connection) {
    let win = Services.wm.getMostRecentWindow("devtools:webide");
    if (!win) {
      return promise.reject();
    }
    let ret = {value: connection.host + ":" + connection.port};
    Services.prompt.prompt(win,
                           Strings.GetStringFromName("remote_runtime_promptTitle"),
                           Strings.GetStringFromName("remote_runtime_promptMessage"),
                           ret, null, {});
    let [host,port] = ret.value.split(":");
    if (!host || !port) {
      return promise.reject();
    }
    connection.host = host;
    connection.port = port;
    connection.connect();
    return promise.resolve();
  },
  getName: function() {
    return Strings.GetStringFromName("remote_runtime");
  },
}

exports.USBRuntime = USBRuntime;
exports.WiFiRuntime = WiFiRuntime;
exports.SimulatorRuntime = SimulatorRuntime;
exports.gRemoteRuntime = gRemoteRuntime;
exports.gLocalRuntime = gLocalRuntime;
