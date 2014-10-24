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
const EventEmitter = require("devtools/toolkit/event-emitter");
const promise = require("promise");

const Strings = Services.strings.createBundle("chrome://browser/locale/devtools/webide.properties");

/**
 * Runtime and Scanner API
 *
 * |RuntimeScanners| maintains a set of |Scanner| objects that produce one or
 * more |Runtime|s to connect to.  Add-ons can extend the set of known runtimes
 * by registering additional |Scanner|s that emit them.
 *
 * Each |Scanner| must support the following API:
 *
 * enable()
 *   Bind any event handlers and start any background work the scanner needs to
 *   maintain an updated set of |Runtime|s.
 *   Called when the first consumer (such as WebIDE) actively interested in
 *   maintaining the |Runtime| list enables the registry.
 * disable()
 *   Unbind any event handlers and stop any background work the scanner needs to
 *   maintain an updated set of |Runtime|s.
 *   Called when the last consumer (such as WebIDE) actively interested in
 *   maintaining the |Runtime| list disables the registry.
 * emits "runtime-list-updated"
 *   If the set of runtimes a |Scanner| manages has changed, it must emit this
 *   event to notify consumers of changes.
 * scan()
 *   Actively refreshes the list of runtimes the scanner knows about.  If your
 *   scanner uses an active scanning approach (as opposed to listening for
 *   events when changes occur), the bulk of the work would be done here.
 *   @return Promise
 *           Should be resolved when scanning is complete.  If scanning has no
 *           well-defined end point, you can resolve immediately, as long as
 *           update event is emitted later when changes are noticed.
 * listRuntimes()
 *   Return the current list of runtimes known to the |Scanner| instance.
 *   @return Iterable
 *
 * Each |Runtime| must support the following API:
 *
 * |type| field
 *   The |type| must be one of the values from the |RuntimeTypes| object.  This
 *   is used for Telemetry and to support displaying sets of |Runtime|s
 *   categorized by type.
 * |id| field
 *   An identifier that is unique in the set of all runtimes with the same
 *   |type|.  WebIDE tries to save the last used runtime via type + id, and
 *   tries to locate it again in the next session, so this value should attempt
 *   to be stable across Firefox sessions.
 * |name| field
 *   A user-visible label to identify the runtime that will be displayed in a
 *   runtime list.
 * connect()
 *   Configure the passed |connection| object with any settings need to
 *   successfully connect to the runtime, and call the |connection|'s connect()
 *   method.
 *   @param  Connection connection
 *           A |Connection| object from the DevTools |ConnectionManager|.
 *   @return Promise
 *           Resolved once you've called the |connection|'s connect() method.
 */

/* SCANNER REGISTRY */

let RuntimeScanners = {

  _enabledCount: 0,
  _scanners: new Set(),

  get enabled() {
    return !!this._enabledCount;
  },

  add(scanner) {
    if (this.enabled) {
      // Enable any scanner added while globally enabled
      this._enableScanner(scanner);
    }
    this._scanners.add(scanner);
    this._emitUpdated();
  },

  remove(scanner) {
    this._scanners.delete(scanner);
    if (this.enabled) {
      // Disable any scanner removed while globally enabled
      this._disableScanner(scanner);
    }
    this._emitUpdated();
  },

  has(scanner) {
    return this._scanners.has(scanner);
  },

  scan() {
    if (!this.enabled) {
      return promise.resolve();
    }

    if (this._scanPromise) {
      return this._scanPromise;
    }

    let promises = [];

    for (let scanner of this._scanners) {
      promises.push(scanner.scan());
    }

    this._scanPromise = promise.all(promises);

    // Reset pending promise
    this._scanPromise.then(() => {
      this._scanPromise = null;
    }, () => {
      this._scanPromise = null;
    });

    return this._scanPromise;
  },

  listRuntimes: function*() {
    for (let scanner of this._scanners) {
      for (let runtime of scanner.listRuntimes()) {
        yield runtime;
      }
    }
  },

  _emitUpdated() {
    this.emit("runtime-list-updated");
  },

  enable() {
    if (this._enabledCount++ !== 0) {
      // Already enabled scanners during a previous call
      return;
    }
    this._emitUpdated = this._emitUpdated.bind(this);
    for (let scanner of this._scanners) {
      this._enableScanner(scanner);
    }
  },

  _enableScanner(scanner) {
    scanner.enable();
    scanner.on("runtime-list-updated", this._emitUpdated);
  },

  disable() {
    if (--this._enabledCount !== 0) {
      // Already disabled scanners during a previous call
      return;
    }
    for (let scanner of this._scanners) {
      this._disableScanner(scanner);
    }
  },

  _disableScanner(scanner) {
    scanner.off("runtime-list-updated", this._emitUpdated);
    scanner.disable();
  },

};

EventEmitter.decorate(RuntimeScanners);

exports.RuntimeScanners = RuntimeScanners;

/* SCANNERS */

let SimulatorScanner = {

  _runtimes: [],

  enable() {
    this._updateRuntimes = this._updateRuntimes.bind(this);
    Simulator.on("register", this._updateRuntimes);
    Simulator.on("unregister", this._updateRuntimes);
    this._updateRuntimes();
  },

  disable() {
    Simulator.off("register", this._updateRuntimes);
    Simulator.off("unregister", this._updateRuntimes);
  },

  _emitUpdated() {
    this.emit("runtime-list-updated");
  },

  _updateRuntimes() {
    this._runtimes = [];
    for (let version of Simulator.availableVersions()) {
      this._runtimes.push(new SimulatorRuntime(version));
    }
    this._emitUpdated();
  },

  scan() {
    return promise.resolve();
  },

  listRuntimes: function() {
    return this._runtimes;
  }

};

EventEmitter.decorate(SimulatorScanner);
RuntimeScanners.add(SimulatorScanner);

/**
 * TODO: Remove this comaptibility layer in the future (bug 1085393)
 * This runtime exists to support the ADB Helper add-on below version 0.7.0.
 *
 * This scanner will list all ADB devices as runtimes, even if they may or may
 * not actually connect (since the |DeprecatedUSBRuntime| assumes a Firefox OS
 * device).
 */
let DeprecatedAdbScanner = {

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
    this._runtimes = [];
    for (let id of Devices.available()) {
      let runtime = new DeprecatedUSBRuntime(id);
      this._runtimes.push(runtime);
      runtime.updateNameFromADB().then(() => {
        this._emitUpdated();
      }, () => {});
    }
    this._emitUpdated();
  },

  scan() {
    return promise.resolve();
  },

  listRuntimes: function() {
    return this._runtimes;
  }

};

EventEmitter.decorate(DeprecatedAdbScanner);
RuntimeScanners.add(DeprecatedAdbScanner);

// ADB Helper 0.7.0 and later will replace this scanner on startup
exports.DeprecatedAdbScanner = DeprecatedAdbScanner;

let WiFiScanner = {

  _runtimes: [],

  init() {
    this.updateRegistration();
    Services.prefs.addObserver(this.ALLOWED_PREF, this, false);
  },

  enable() {
    this._updateRuntimes = this._updateRuntimes.bind(this);
    discovery.on("devtools-device-added", this._updateRuntimes);
    discovery.on("devtools-device-updated", this._updateRuntimes);
    discovery.on("devtools-device-removed", this._updateRuntimes);
    this._updateRuntimes();
  },

  disable() {
    discovery.off("devtools-device-added", this._updateRuntimes);
    discovery.off("devtools-device-updated", this._updateRuntimes);
    discovery.off("devtools-device-removed", this._updateRuntimes);
  },

  _emitUpdated() {
    this.emit("runtime-list-updated");
  },

  _updateRuntimes() {
    this._runtimes = [];
    for (let device of discovery.getRemoteDevicesWithService("devtools")) {
      this._runtimes.push(new WiFiRuntime(device));
    }
    this._emitUpdated();
  },

  scan() {
    discovery.scan();
    return promise.resolve();
  },

  listRuntimes: function() {
    return this._runtimes;
  },

  ALLOWED_PREF: "devtools.remote.wifi.scan",

  get allowed() {
    return Services.prefs.getBoolPref(this.ALLOWED_PREF);
  },

  updateRegistration() {
    if (this.allowed) {
      RuntimeScanners.add(WiFiScanner);
    } else {
      RuntimeScanners.remove(WiFiScanner);
    }
    this._emitUpdated();
  },

  observe(subject, topic, data) {
    if (data !== WiFiScanner.ALLOWED_PREF) {
      return;
    }
    WiFiScanner.updateRegistration();
  }

};

EventEmitter.decorate(WiFiScanner);
WiFiScanner.init();

exports.WiFiScanner = WiFiScanner;

let StaticScanner = {
  enable() {},
  disable() {},
  scan() { return promise.resolve(); },
  listRuntimes() { return [gRemoteRuntime, gLocalRuntime]; }
};

EventEmitter.decorate(StaticScanner);
RuntimeScanners.add(StaticScanner);

/* RUNTIMES */

// These type strings are used for logging events to Telemetry.
// You must update Histograms.json if new types are added.
let RuntimeTypes = exports.RuntimeTypes = {
  USB: "USB",
  WIFI: "WIFI",
  SIMULATOR: "SIMULATOR",
  REMOTE: "REMOTE",
  LOCAL: "LOCAL",
  OTHER: "OTHER"
};

/**
 * TODO: Remove this comaptibility layer in the future (bug 1085393)
 * This runtime exists to support the ADB Helper add-on below version 0.7.0.
 *
 * This runtime assumes it is connecting to a Firefox OS device.
 */
function DeprecatedUSBRuntime(id) {
  this._id = id;
}

DeprecatedUSBRuntime.prototype = {
  type: RuntimeTypes.USB,
  get device() {
    return Devices.getByName(this._id);
  },
  connect: function(connection) {
    if (!this.device) {
      return promise.reject("Can't find device: " + this.name);
    }
    return this.device.connect().then((port) => {
      connection.host = "localhost";
      connection.port = port;
      connection.connect();
    });
  },
  get id() {
    return this._id;
  },
  get name() {
    return this._productModel || this._id;
  },
  updateNameFromADB: function() {
    if (this._productModel) {
      return promise.reject();
    }
    let deferred = promise.defer();
    if (this.device && this.device.shell) {
      this.device.shell("getprop ro.product.model").then(stdout => {
        this._productModel = stdout;
        deferred.resolve();
      }, () => {});
    } else {
      this._productModel = null;
      deferred.reject();
    }
    return deferred.promise;
  },
};

// For testing use only
exports._DeprecatedUSBRuntime = DeprecatedUSBRuntime;

function WiFiRuntime(deviceName) {
  this.deviceName = deviceName;
}

WiFiRuntime.prototype = {
  type: RuntimeTypes.WIFI,
  connect: function(connection) {
    let service = discovery.getRemoteService("devtools", this.deviceName);
    if (!service) {
      return promise.reject("Can't find device: " + this.name);
    }
    connection.host = service.host;
    connection.port = service.port;
    connection.connect();
    return promise.resolve();
  },
  get id() {
    return this.deviceName;
  },
  get name() {
    return this.deviceName;
  },
};

// For testing use only
exports._WiFiRuntime = WiFiRuntime;

function SimulatorRuntime(version) {
  this.version = version;
}

SimulatorRuntime.prototype = {
  type: RuntimeTypes.SIMULATOR,
  connect: function(connection) {
    let port = ConnectionManager.getFreeTCPPort();
    let simulator = Simulator.getByVersion(this.version);
    if (!simulator || !simulator.launch) {
      return promise.reject("Can't find simulator: " + this.name);
    }
    return simulator.launch({port: port}).then(() => {
      connection.host = "localhost";
      connection.port = port;
      connection.keepConnecting = true;
      connection.once(Connection.Events.DISCONNECTED, simulator.close);
      connection.connect();
    });
  },
  get id() {
    return this.version;
  },
  get name() {
    let simulator = Simulator.getByVersion(this.version);
    if (!simulator) {
      return "Unknown";
    }
    return Simulator.getByVersion(this.version).appinfo.label;
  },
};

// For testing use only
exports._SimulatorRuntime = SimulatorRuntime;

let gLocalRuntime = {
  type: RuntimeTypes.LOCAL,
  connect: function(connection) {
    if (!DebuggerServer.initialized) {
      DebuggerServer.init();
      DebuggerServer.addBrowserActors();
    }
    connection.host = null; // Force Pipe transport
    connection.port = null;
    connection.connect();
    return promise.resolve();
  },
  get id() {
    return "local";
  },
  get name() {
    return Strings.GetStringFromName("local_runtime");
  },
};

// For testing use only
exports._gLocalRuntime = gLocalRuntime;

let gRemoteRuntime = {
  type: RuntimeTypes.REMOTE,
  connect: function(connection) {
    let win = Services.wm.getMostRecentWindow("devtools:webide");
    if (!win) {
      return promise.reject();
    }
    let ret = {value: connection.host + ":" + connection.port};
    let title = Strings.GetStringFromName("remote_runtime_promptTitle");
    let message = Strings.GetStringFromName("remote_runtime_promptMessage");
    let ok = Services.prompt.prompt(win, title, message, ret, null, {});
    let [host,port] = ret.value.split(":");
    if (!ok) {
      return promise.reject({canceled: true});
    }
    if (!host || !port) {
      return promise.reject();
    }
    connection.host = host;
    connection.port = port;
    connection.connect();
    return promise.resolve();
  },
  get name() {
    return Strings.GetStringFromName("remote_runtime");
  },
};

// For testing use only
exports._gRemoteRuntime = gRemoteRuntime;
