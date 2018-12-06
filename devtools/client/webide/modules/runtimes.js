/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const {DebuggerServer} = require("devtools/server/main");
const discovery = require("devtools/shared/discovery/discovery");
const EventEmitter = require("devtools/shared/event-emitter");
const {RuntimeTypes} = require("devtools/client/webide/modules/runtime-types");
const promise = require("promise");

loader.lazyRequireGetter(this, "adb", "devtools/shared/adb/adb", true);

loader.lazyRequireGetter(this, "AuthenticationResult",
  "devtools/shared/security/auth", true);
loader.lazyRequireGetter(this, "DevToolsUtils",
  "devtools/shared/DevToolsUtils");

const Strings = Services.strings.createBundle("chrome://devtools/locale/webide.properties");

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
 * |prolongedConnection| field
 *   A boolean value which should be |true| if the connection process is
 *   expected to take a unknown or large amount of time.  A UI may use this as a
 *   hint to skip timeouts or other time-based code paths.
 * connect()
 *   Configure the passed |connection| object with any settings need to
 *   successfully connect to the runtime, and call the |connection|'s connect()
 *   method.
 *   @param  Connection connection
 *           A |Connection| object from the DevTools |ConnectionManager|.
 *   @return Promise
 *           Resolved once you've called the |connection|'s connect() method.
 * configure() OPTIONAL
 *   Show a configuration screen if the runtime is configurable.
 */

/* SCANNER REGISTRY */

var RuntimeScanners = {

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

    const promises = [];

    for (const scanner of this._scanners) {
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

  listRuntimes: function* () {
    for (const scanner of this._scanners) {
      for (const runtime of scanner.listRuntimes()) {
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
    for (const scanner of this._scanners) {
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
    for (const scanner of this._scanners) {
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

var UsbScanner = {
  init() {
    this._emitUpdated = this._emitUpdated.bind(this);
  },
  enable() {
    adb.registerListener(this._emitUpdated);
  },
  disable() {
    adb.unregisterListener(this._emitUpdated);
  },
  scan() {
    return adb.updateRuntimes();
  },
  listRuntimes() {
    return adb.getRuntimes();
  },
  _emitUpdated() {
    this.emit("runtime-list-updated");
  },
};
EventEmitter.decorate(UsbScanner);
UsbScanner.init();
RuntimeScanners.add(UsbScanner);

var WiFiScanner = {

  _runtimes: [],

  init() {
    this.updateRegistration();
    Services.prefs.addObserver(this.ALLOWED_PREF, this);
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
    for (const device of discovery.getRemoteDevicesWithService("devtools")) {
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
  },

};

EventEmitter.decorate(WiFiScanner);
WiFiScanner.init();

exports.WiFiScanner = WiFiScanner;

var StaticScanner = {
  enable() {},
  disable() {},
  scan() {
    return promise.resolve();
  },
  listRuntimes() {
    const runtimes = [gRemoteRuntime];
    if (Services.prefs.getBoolPref("devtools.webide.enableLocalRuntime")) {
      runtimes.push(gLocalRuntime);
    }
    return runtimes;
  },
};

EventEmitter.decorate(StaticScanner);
RuntimeScanners.add(StaticScanner);

exports.RuntimeTypes = RuntimeTypes;

function WiFiRuntime(deviceName) {
  this.deviceName = deviceName;
}

WiFiRuntime.prototype = {
  type: RuntimeTypes.WIFI,
  // Mark runtime as taking a long time to connect
  prolongedConnection: true,
  connect: function(connection) {
    const service = discovery.getRemoteService("devtools", this.deviceName);
    if (!service) {
      return promise.reject(new Error("Can't find device: " + this.name));
    }
    connection.advertisement = service;
    connection.authenticator.sendOOB = this.sendOOB;
    // Disable the default connection timeout, since QR scanning can take an
    // unknown amount of time.  This prevents spurious errors (even after
    // eventual success) from being shown.
    connection.timeoutDelay = 0;
    connection.connect();
    return promise.resolve();
  },
  get id() {
    return this.deviceName;
  },
  get name() {
    return this.deviceName;
  },

  /**
   * During OOB_CERT authentication, a notification dialog like this is used to
   * to display a token which the user must transfer through some mechanism to the
   * server to authenticate the devices.
   *
   * This implementation presents the token as text for the user to transfer
   * manually.  For a mobile device, you should override this implementation with
   * something more convenient, such as displaying a QR code.
   *
   * This method receives an object containing:
   * @param host string
   *        The host name or IP address of the debugger server.
   * @param port number
   *        The port number of the debugger server.
   * @param cert object (optional)
   *        The server's cert details.
   * @param authResult AuthenticationResult
   *        Authentication result sent from the server.
   * @param oob object (optional)
   *        The token data to be transferred during OOB_CERT step 8:
   *        * sha256: hash(ClientCert)
   *        * k     : K(random 128-bit number)
   * @return object containing:
   *         * close: Function to hide the notification
   */
  sendOOB(session) {
    const WINDOW_ID = "devtools:wifi-auth";
    const { authResult } = session;
    // Only show in the PENDING state
    if (authResult != AuthenticationResult.PENDING) {
      throw new Error("Expected PENDING result, got " + authResult);
    }

    // Listen for the window our prompt opens, so we can close it programatically
    let promptWindow;
    const windowListener = {
      onOpenWindow(xulWindow) {
        const win = xulWindow.docShell.domWindow;
        win.addEventListener("load", function() {
          if (win.document.documentElement.getAttribute("id") != WINDOW_ID) {
            return;
          }
          // Found the window
          promptWindow = win;
          Services.wm.removeListener(windowListener);
        }, {once: true});
      },
      onCloseWindow() {},
    };
    Services.wm.addListener(windowListener);

    // |openDialog| is typically a blocking API, so |executeSoon| to get around this
    DevToolsUtils.executeSoon(() => {
      // Height determines the size of the QR code.  Force a minimum size to
      // improve scanability.
      const MIN_HEIGHT = 600;
      const win = Services.wm.getMostRecentWindow("devtools:webide");
      const width = win.outerWidth * 0.8;
      const height = Math.max(win.outerHeight * 0.5, MIN_HEIGHT);
      win.openDialog("chrome://webide/content/wifi-auth.xhtml",
                     WINDOW_ID,
                     "modal=yes,width=" + width + ",height=" + height, session);
    });

    return {
      close() {
        if (!promptWindow) {
          return;
        }
        promptWindow.close();
        promptWindow = null;
      },
    };
  },
};

// For testing use only
exports._WiFiRuntime = WiFiRuntime;

var gLocalRuntime = {
  type: RuntimeTypes.LOCAL,
  connect: function(connection) {
    DebuggerServer.init();
    DebuggerServer.registerAllActors();
    DebuggerServer.allowChromeProcess = true;
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

var gRemoteRuntime = {
  type: RuntimeTypes.REMOTE,
  connect: function(connection) {
    const win = Services.wm.getMostRecentWindow("devtools:webide");
    if (!win) {
      return promise.reject(new Error("No WebIDE window found"));
    }
    const ret = {value: connection.host + ":" + connection.port};
    const title = Strings.GetStringFromName("remote_runtime_promptTitle");
    const message = Strings.GetStringFromName("remote_runtime_promptMessage");
    const ok = Services.prompt.prompt(win, title, message, ret, null, {});
    const [host, port] = ret.value.split(":");
    if (!ok) {
      return promise.reject({canceled: true});
    }
    if (!host || !port) {
      return promise.reject(new Error("Invalid host or port"));
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
