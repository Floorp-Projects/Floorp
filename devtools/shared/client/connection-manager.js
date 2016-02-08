/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cc, Ci, Cu, Cr} = require("chrome");
const {setTimeout, clearTimeout} = require('sdk/timers');
const EventEmitter = require("devtools/shared/event-emitter");
const DevToolsUtils = require("devtools/shared/DevToolsUtils");
const { DebuggerServer } = require("devtools/server/main");
const { DebuggerClient } = require("devtools/shared/client/main");

Cu.import("resource://gre/modules/Services.jsm");
DevToolsUtils.defineLazyModuleGetter(this, "Task",
  "resource://gre/modules/Task.jsm");

const REMOTE_TIMEOUT = "devtools.debugger.remote-timeout";

/**
 * Connection Manager.
 *
 * To use this module:
 * const {ConnectionManager} = require("devtools/shared/client/connection-manager");
 *
 * # ConnectionManager
 *
 * Methods:
 *  . Connection createConnection(host, port)
 *  . void       destroyConnection(connection)
 *  . Number     getFreeTCPPort()
 *
 * Properties:
 *  . Array      connections
 *
 * # Connection
 *
 * A connection is a wrapper around a debugger client. It has a simple
 * API to instantiate a connection to a debugger server. Once disconnected,
 * no need to re-create a Connection object. Calling `connect()` again
 * will re-create a debugger client.
 *
 * Methods:
 *  . connect()             Connect to host:port. Expect a "connecting" event.
 *                          If no host is not specified, a local pipe is used
 *  . connect(transport)    Connect via transport. Expect a "connecting" event.
 *  . disconnect()          Disconnect if connected. Expect a "disconnecting" event
 *
 * Properties:
 *  . host                  IP address or hostname
 *  . port                  Port
 *  . logs                  Current logs. "newlog" event notifies new available logs
 *  . store                 Reference to a local data store (see below)
 *  . keepConnecting        Should the connection keep trying to connect?
 *  . timeoutDelay          When should we give up (in ms)?
 *                          0 means wait forever.
 *  . encryption            Should the connection be encrypted?
 *  . authentication        What authentication scheme should be used?
 *  . authenticator         The |Authenticator| instance used.  Overriding
 *                          properties of this instance may be useful to
 *                          customize authentication UX for a specific use case.
 *  . advertisement         The server's advertisement if found by discovery
 *  . status                Connection status:
 *                            Connection.Status.CONNECTED
 *                            Connection.Status.DISCONNECTED
 *                            Connection.Status.CONNECTING
 *                            Connection.Status.DISCONNECTING
 *                            Connection.Status.DESTROYED
 *
 * Events (as in event-emitter.js):
 *  . Connection.Events.CONNECTING      Trying to connect to host:port
 *  . Connection.Events.CONNECTED       Connection is successful
 *  . Connection.Events.DISCONNECTING   Trying to disconnect from server
 *  . Connection.Events.DISCONNECTED    Disconnected (at client request, or because of a timeout or connection error)
 *  . Connection.Events.STATUS_CHANGED  The connection status (connection.status) has changed
 *  . Connection.Events.TIMEOUT         Connection timeout
 *  . Connection.Events.HOST_CHANGED    Host has changed
 *  . Connection.Events.PORT_CHANGED    Port has changed
 *  . Connection.Events.NEW_LOG         A new log line is available
 *
 */

var ConnectionManager = {
  _connections: new Set(),
  createConnection: function(host, port) {
    let c = new Connection(host, port);
    c.once("destroy", (event) => this.destroyConnection(c));
    this._connections.add(c);
    this.emit("new", c);
    return c;
  },
  destroyConnection: function(connection) {
    if (this._connections.has(connection)) {
      this._connections.delete(connection);
      if (connection.status != Connection.Status.DESTROYED) {
        connection.destroy();
      }
    }
  },
  get connections() {
    return [...this._connections];
  },
  getFreeTCPPort: function () {
    let serv = Cc['@mozilla.org/network/server-socket;1']
                 .createInstance(Ci.nsIServerSocket);
    serv.init(-1, true, -1);
    let port = serv.port;
    serv.close();
    return port;
  },
}

EventEmitter.decorate(ConnectionManager);

var lastID = -1;

function Connection(host, port) {
  EventEmitter.decorate(this);
  this.uid = ++lastID;
  this.host = host;
  this.port = port;
  this._setStatus(Connection.Status.DISCONNECTED);
  this._onDisconnected = this._onDisconnected.bind(this);
  this._onConnected = this._onConnected.bind(this);
  this._onTimeout = this._onTimeout.bind(this);
  this.resetOptions();
}

Connection.Status = {
  CONNECTED: "connected",
  DISCONNECTED: "disconnected",
  CONNECTING: "connecting",
  DISCONNECTING: "disconnecting",
  DESTROYED: "destroyed",
}

Connection.Events = {
  CONNECTED: Connection.Status.CONNECTED,
  DISCONNECTED: Connection.Status.DISCONNECTED,
  CONNECTING: Connection.Status.CONNECTING,
  DISCONNECTING: Connection.Status.DISCONNECTING,
  DESTROYED: Connection.Status.DESTROYED,
  TIMEOUT: "timeout",
  STATUS_CHANGED: "status-changed",
  HOST_CHANGED: "host-changed",
  PORT_CHANGED: "port-changed",
  NEW_LOG: "new_log"
}

Connection.prototype = {
  logs: "",
  log: function(str) {
    let d = new Date();
    let hours = ("0" + d.getHours()).slice(-2);
    let minutes = ("0" + d.getMinutes()).slice(-2);
    let seconds = ("0" + d.getSeconds()).slice(-2);
    let timestamp = [hours, minutes, seconds].join(":") + ": ";
    str = timestamp + str;
    this.logs +=  "\n" + str;
    this.emit(Connection.Events.NEW_LOG, str);
  },

  get client() {
    return this._client
  },

  get host() {
    return this._host
  },

  set host(value) {
    if (this._host && this._host == value)
      return;
    this._host = value;
    this.emit(Connection.Events.HOST_CHANGED);
  },

  get port() {
    return this._port
  },

  set port(value) {
    if (this._port && this._port == value)
      return;
    this._port = value;
    this.emit(Connection.Events.PORT_CHANGED);
  },

  get authentication() {
    return this._authentication;
  },

  set authentication(value) {
    this._authentication = value;
    // Create an |Authenticator| of this type
    if (!value) {
      this.authenticator = null;
      return;
    }
    let AuthenticatorType = DebuggerClient.Authenticators.get(value);
    this.authenticator = new AuthenticatorType.Client();
  },

  get advertisement() {
    return this._advertisement;
  },

  set advertisement(advertisement) {
    // The full advertisement may contain more info than just the standard keys
    // below, so keep a copy for use during connection later.
    this._advertisement = advertisement;
    if (advertisement) {
      ["host", "port", "encryption", "authentication"].forEach(key => {
        this[key] = advertisement[key];
      });
    }
  },

  /**
   * Settings to be passed to |socketConnect| at connection time.
   */
  get socketSettings() {
    let settings = {};
    if (this.advertisement) {
      // Use the advertisement as starting point if it exists, as it may contain
      // extra data, like the server's cert.
      Object.assign(settings, this.advertisement);
    }
    Object.assign(settings, {
      host: this.host,
      port: this.port,
      encryption: this.encryption,
      authenticator: this.authenticator
    });
    return settings;
  },

  timeoutDelay: Services.prefs.getIntPref(REMOTE_TIMEOUT),

  resetOptions() {
    this.keepConnecting = false;
    this.timeoutDelay = Services.prefs.getIntPref(REMOTE_TIMEOUT);
    this.encryption = false;
    this.authentication = null;
    this.advertisement = null;
  },

  disconnect: function(force) {
    if (this.status == Connection.Status.DESTROYED) {
      return;
    }
    clearTimeout(this._timeoutID);
    if (this.status == Connection.Status.CONNECTED ||
        this.status == Connection.Status.CONNECTING) {
      this.log("disconnecting");
      this._setStatus(Connection.Status.DISCONNECTING);
      if (this._client) {
        this._client.close();
      }
    }
  },

  connect: function(transport) {
    if (this.status == Connection.Status.DESTROYED) {
      return;
    }
    if (!this._client) {
      this._customTransport = transport;
      if (this._customTransport) {
        this.log("connecting (custom transport)");
      } else {
        this.log("connecting to " + this.host + ":" + this.port);
      }
      this._setStatus(Connection.Status.CONNECTING);

      if (this.timeoutDelay > 0) {
        this._timeoutID = setTimeout(this._onTimeout, this.timeoutDelay);
      }
      this._clientConnect();
    } else {
      let msg = "Can't connect. Client is not fully disconnected";
      this.log(msg);
      throw new Error(msg);
    }
  },

  destroy: function() {
    this.log("killing connection");
    clearTimeout(this._timeoutID);
    this.keepConnecting = false;
    if (this._client) {
      this._client.close();
      this._client = null;
    }
    this._setStatus(Connection.Status.DESTROYED);
  },

  _getTransport: Task.async(function*() {
    if (this._customTransport) {
      return this._customTransport;
    }
    if (!this.host) {
      return DebuggerServer.connectPipe();
    }
    let settings = this.socketSettings;
    let transport = yield DebuggerClient.socketConnect(settings);
    return transport;
  }),

  _clientConnect: function () {
    this._getTransport().then(transport => {
      if (!transport) {
        return;
      }
      this._client = new DebuggerClient(transport);
      this._client.addOneTimeListener("closed", this._onDisconnected);
      this._client.connect().then(this._onConnected);
    }, e => {
      // If we're continuously trying to connect, we expect the connection to be
      // rejected a couple times, so don't log these.
      if (!this.keepConnecting || e.result !== Cr.NS_ERROR_CONNECTION_REFUSED) {
        console.error(e);
      }
      // In some cases, especially on Mac, the openOutputStream call in
      // DebuggerClient.socketConnect may throw NS_ERROR_NOT_INITIALIZED.
      // It occurs when we connect agressively to the simulator,
      // and keep trying to open a socket to the server being started in
      // the simulator.
      this._onDisconnected();
    });
  },

  get status() {
    return this._status
  },

  _setStatus: function(value) {
    if (this._status && this._status == value)
      return;
    this._status = value;
    this.emit(value);
    this.emit(Connection.Events.STATUS_CHANGED, value);
  },

  _onDisconnected: function() {
    this._client = null;
    this._customTransport = null;

    if (this._status == Connection.Status.CONNECTING && this.keepConnecting) {
      setTimeout(() => this._clientConnect(), 100);
      return;
    }

    clearTimeout(this._timeoutID);

    switch (this.status) {
      case Connection.Status.CONNECTED:
        this.log("disconnected (unexpected)");
        break;
      case Connection.Status.CONNECTING:
        this.log("connection error. Possible causes: USB port not connected, port not forwarded (adb forward), wrong host or port, remote debugging not enabled on the device.");
        break;
      default:
        this.log("disconnected");
    }
    this._setStatus(Connection.Status.DISCONNECTED);
  },

  _onConnected: function() {
    this.log("connected");
    clearTimeout(this._timeoutID);
    this._setStatus(Connection.Status.CONNECTED);
  },

  _onTimeout: function() {
    this.log("connection timeout. Possible causes: didn't click on 'accept' (prompt).");
    this.emit(Connection.Events.TIMEOUT);
    this.disconnect();
  },
}

exports.ConnectionManager = ConnectionManager;
exports.Connection = Connection;
