/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { Ci, Cc, CC, Cr } = require("chrome");

// Ensure PSM is initialized to support TLS sockets
Cc["@mozilla.org/psm;1"].getService(Ci.nsISupports);

var DevToolsUtils = require("devtools/shared/DevToolsUtils");
var { dumpn } = DevToolsUtils;
loader.lazyRequireGetter(
  this,
  "WebSocketServer",
  "devtools/server/socket/websocket-server"
);
loader.lazyRequireGetter(
  this,
  "DebuggerTransport",
  "devtools/shared/transport/transport",
  true
);
loader.lazyRequireGetter(
  this,
  "WebSocketDebuggerTransport",
  "devtools/shared/transport/websocket-transport"
);
loader.lazyRequireGetter(
  this,
  "discovery",
  "devtools/shared/discovery/discovery"
);
loader.lazyRequireGetter(
  this,
  "Authenticators",
  "devtools/shared/security/auth",
  true
);
loader.lazyRequireGetter(
  this,
  "AuthenticationResult",
  "devtools/shared/security/auth",
  true
);
loader.lazyRequireGetter(
  this,
  "DevToolsSocketStatus",
  "resource://devtools/shared/security/DevToolsSocketStatus.jsm",
  true
);

loader.lazyRequireGetter(this, "EventEmitter", "devtools/shared/event-emitter");

DevToolsUtils.defineLazyGetter(this, "nsFile", () => {
  return CC("@mozilla.org/file/local;1", "nsIFile", "initWithPath");
});

DevToolsUtils.defineLazyGetter(this, "socketTransportService", () => {
  return Cc["@mozilla.org/network/socket-transport-service;1"].getService(
    Ci.nsISocketTransportService
  );
});

var DebuggerSocket = {};

/**
 * Connects to a devtools server socket.
 *
 * @param host string
 *        The host name or IP address of the devtools server.
 * @param port number
 *        The port number of the devtools server.
 * @param webSocket boolean (optional)
 *        Whether to use WebSocket protocol to connect. Defaults to false.
 * @param authenticator Authenticator (optional)
 *        |Authenticator| instance matching the mode in use by the server.
 *        Defaults to a PROMPT instance if not supplied.
 * @return promise
 *         Resolved to a DebuggerTransport instance.
 */
DebuggerSocket.connect = async function(settings) {
  // Default to PROMPT |Authenticator| instance if not supplied
  if (!settings.authenticator) {
    settings.authenticator = new (Authenticators.get().Client)();
  }
  _validateSettings(settings);
  // eslint-disable-next-line no-shadow
  const { host, port, authenticator } = settings;
  const transport = await _getTransport(settings);
  await authenticator.authenticate({
    host,
    port,
    transport,
  });
  transport.connectionSettings = settings;
  return transport;
};

/**
 * Validate that the connection settings have been set to a supported configuration.
 */
function _validateSettings(settings) {
  const { authenticator } = settings;

  authenticator.validateSettings(settings);
}

/**
 * Try very hard to create a DevTools transport, potentially making several
 * connect attempts in the process.
 *
 * @param host string
 *        The host name or IP address of the devtools server.
 * @param port number
 *        The port number of the devtools server.
 * @param webSocket boolean (optional)
 *        Whether to use WebSocket protocol to connect to the server. Defaults to false.
 * @param authenticator Authenticator
 *        |Authenticator| instance matching the mode in use by the server.
 *        Defaults to a PROMPT instance if not supplied.
 * @return transport DebuggerTransport
 *         A possible DevTools transport (if connection succeeded and streams
 *         are actually alive and working)
 */
var _getTransport = async function(settings) {
  const { host, port, webSocket } = settings;

  if (webSocket) {
    // Establish a connection and wait until the WebSocket is ready to send and receive
    const socket = await new Promise((resolve, reject) => {
      const s = new WebSocket(`ws://${host}:${port}`);
      s.onopen = () => resolve(s);
      s.onerror = err => reject(err);
    });

    return new WebSocketDebuggerTransport(socket);
  }

  const attempt = await _attemptTransport(settings);
  if (attempt.transport) {
    // Success
    return attempt.transport;
  }

  throw new Error("Connection failed");
};

/**
 * Make a single attempt to connect and create a DevTools transport.
 *
 * @param host string
 *        The host name or IP address of the devtools server.
 * @param port number
 *        The port number of the devtools server.
 * @param authenticator Authenticator
 *        |Authenticator| instance matching the mode in use by the server.
 *        Defaults to a PROMPT instance if not supplied.
 * @return transport DebuggerTransport
 *         A possible DevTools transport (if connection succeeded and streams
 *         are actually alive and working)
 * @return s nsISocketTransport
 *         Underlying socket transport, in case more details are needed.
 */
var _attemptTransport = async function(settings) {
  const { authenticator } = settings;
  // _attemptConnect only opens the streams.  Any failures at that stage
  // aborts the connection process immedidately.
  const { s, input, output } = await _attemptConnect(settings);

  // Check if the input stream is alive.
  let alive;
  try {
    const results = await _isInputAlive(input);
    alive = results.alive;
  } catch (e) {
    // For other unexpected errors, like NS_ERROR_CONNECTION_REFUSED, we reach
    // this block.
    input.close();
    output.close();
    throw e;
  }

  // The |Authenticator| examines the connection as well and may determine it
  // should be dropped.
  alive =
    alive &&
    authenticator.validateConnection({
      host: settings.host,
      port: settings.port,
      socket: s,
    });

  let transport;
  if (alive) {
    transport = new DebuggerTransport(input, output);
  } else {
    // Something went wrong, close the streams.
    input.close();
    output.close();
  }

  return { transport, s };
};

/**
 * Try to connect to a remote server socket.
 *
 * If successsful, the socket transport and its opened streams are returned.
 * Typically, this will only fail if the host / port is unreachable.  Other
 * problems, such as security errors, will allow this stage to succeed, but then
 * fail later when the streams are actually used.
 * @return s nsISocketTransport
 *         Underlying socket transport, in case more details are needed.
 * @return input nsIAsyncInputStream
 *         The socket's input stream.
 * @return output nsIAsyncOutputStream
 *         The socket's output stream.
 */
var _attemptConnect = async function({ host, port }) {
  const s = socketTransportService.createTransport([], host, port, null, null);

  // Force disabling IPV6 if we aren't explicitely connecting to an IPv6 address
  // It fails intermitently on MacOS when opening the Browser Toolbox (bug 1615412)
  if (!host.includes(":")) {
    s.connectionFlags |= Ci.nsISocketTransport.DISABLE_IPV6;
  }

  // By default the CONNECT socket timeout is very long, 65535 seconds,
  // so that if we race to be in CONNECT state while the server socket is still
  // initializing, the connection is stuck in connecting state for 18.20 hours!
  s.setTimeout(Ci.nsISocketTransport.TIMEOUT_CONNECT, 2);

  let input;
  let output;
  return new Promise((resolve, reject) => {
    s.setEventSink(
      {
        onTransportStatus(transport, status) {
          if (status != Ci.nsISocketTransport.STATUS_CONNECTING_TO) {
            return;
          }
          try {
            input = s.openInputStream(0, 0, 0);
          } catch (e) {
            reject(e);
          }
          resolve({ s, input, output });
        },
      },
      Services.tm.currentThread
    );

    // openOutputStream may throw NS_ERROR_NOT_INITIALIZED if we hit some race
    // where the nsISocketTransport gets shutdown in between its instantiation and
    // the call to this method.
    try {
      output = s.openOutputStream(0, 0, 0);
    } catch (e) {
      reject(e);
    }
  }).catch(e => {
    if (input) {
      input.close();
    }
    if (output) {
      output.close();
    }
    DevToolsUtils.reportException("_attemptConnect", e);
  });
};

/**
 * Check if the input stream is alive.
 */
function _isInputAlive(input) {
  return new Promise((resolve, reject) => {
    input.asyncWait(
      {
        onInputStreamReady(stream) {
          try {
            stream.available();
            resolve({ alive: true });
          } catch (e) {
            reject(e);
          }
        },
      },
      0,
      0,
      Services.tm.currentThread
    );
  });
}

/**
 * Creates a new socket listener for remote connections to the DevToolsServer.
 * This helps contain and organize the parts of the server that may differ or
 * are particular to one given listener mechanism vs. another.
 * This can be closed at any later time by calling |close|.
 * If remote connections are disabled, an error is thrown.
 *
 * @param {DevToolsServer} devToolsServer
 * @param {Object} socketOptions
 *        options of socket as follows
 *        {
 *          authenticator:
 *            Controls the |Authenticator| used, which hooks various socket steps to
 *            implement an authentication policy.  It is expected that different use
 *            cases may override pieces of the |Authenticator|.  See auth.js.
 *            We set the default |Authenticator|, which is |Prompt|.
 *          discoverable:
 *            Controls whether this listener is announced via the service discovery
 *            mechanism. Defaults is false.
 *          fromBrowserToolbox:
 *            Should only be passed when opening a socket for a Browser Toolbox
 *            session. DevToolsSocketStatus will track the socket separately to
 *            avoid triggering the visual cue in the URL bar.
 *          portOrPath:
 *            The port or path to listen on.
 *            If given an integer, the port to listen on.  Use -1 to choose any available
 *            port. Otherwise, the path to the unix socket domain file to listen on.
 *            Defaults is null.
 *          webSocket:
 *            Whether to use WebSocket protocol. Defaults is false.
 *        }
 */
function SocketListener(devToolsServer, socketOptions) {
  this._devToolsServer = devToolsServer;

  // Set socket options with default value
  this._socketOptions = {
    authenticator:
      socketOptions.authenticator || new (Authenticators.get().Server)(),
    discoverable: !!socketOptions.discoverable,
    fromBrowserToolbox: !!socketOptions.fromBrowserToolbox,
    portOrPath: socketOptions.portOrPath || null,
    webSocket: !!socketOptions.webSocket,
  };

  EventEmitter.decorate(this);
}

SocketListener.prototype = {
  get authenticator() {
    return this._socketOptions.authenticator;
  },

  get discoverable() {
    return this._socketOptions.discoverable;
  },

  get fromBrowserToolbox() {
    return this._socketOptions.fromBrowserToolbox;
  },

  get portOrPath() {
    return this._socketOptions.portOrPath;
  },

  get webSocket() {
    return this._socketOptions.webSocket;
  },

  /**
   * Validate that all options have been set to a supported configuration.
   */
  _validateOptions() {
    if (this.portOrPath === null) {
      throw new Error("Must set a port / path to listen on.");
    }
    if (this.discoverable && !Number(this.portOrPath)) {
      throw new Error("Discovery only supported for TCP sockets.");
    }
  },

  /**
   * Listens on the given port or socket file for remote debugger connections.
   */
  open() {
    this._validateOptions();
    this._devToolsServer.addSocketListener(this);

    let flags = Ci.nsIServerSocket.KeepWhenOffline;
    // A preference setting can force binding on the loopback interface.
    if (Services.prefs.getBoolPref("devtools.debugger.force-local")) {
      flags |= Ci.nsIServerSocket.LoopbackOnly;
    }

    const self = this;
    return (async function() {
      const backlog = 4;
      self._socket = self._createSocketInstance();
      if (self.isPortBased) {
        const port = Number(self.portOrPath);
        self._socket.initSpecialConnection(port, flags, backlog);
      } else if (self.portOrPath.startsWith("/")) {
        const file = nsFile(self.portOrPath);
        if (file.exists()) {
          file.remove(false);
        }
        self._socket.initWithFilename(file, parseInt("666", 8), backlog);
      } else {
        // Path isn't absolute path, so we use abstract socket address
        self._socket.initWithAbstractAddress(self.portOrPath, backlog);
      }
      self._socket.asyncListen(self);
      dumpn("Socket listening on: " + (self.port || self.portOrPath));
    })()
      .then(() => {
        DevToolsSocketStatus.notifySocketOpened({
          fromBrowserToolbox: self.fromBrowserToolbox,
        });
        this._advertise();
      })
      .catch(e => {
        dumpn(
          "Could not start debugging listener on '" +
            this.portOrPath +
            "': " +
            e
        );
        this.close();
      });
  },

  _advertise() {
    if (!this.discoverable || !this.port) {
      return;
    }

    const advertisement = {
      port: this.port,
    };

    this.authenticator.augmentAdvertisement(this, advertisement);

    discovery.addService("devtools", advertisement);
  },

  _createSocketInstance() {
    return Cc["@mozilla.org/network/server-socket;1"].createInstance(
      Ci.nsIServerSocket
    );
  },

  /**
   * Closes the SocketListener.  Notifies the server to remove the listener from
   * the set of active SocketListeners.
   */
  close() {
    if (this.discoverable && this.port) {
      discovery.removeService("devtools");
    }
    if (this._socket) {
      this._socket.close();
      this._socket = null;

      DevToolsSocketStatus.notifySocketClosed({
        fromBrowserToolbox: this.fromBrowserToolbox,
      });
    }
    this._devToolsServer.removeSocketListener(this);
  },

  get host() {
    if (!this._socket) {
      return null;
    }
    if (Services.prefs.getBoolPref("devtools.debugger.force-local")) {
      return "127.0.0.1";
    }
    return "0.0.0.0";
  },

  /**
   * Gets whether this listener uses a port number vs. a path.
   */
  get isPortBased() {
    return !!Number(this.portOrPath);
  },

  /**
   * Gets the port that a TCP socket listener is listening on, or null if this
   * is not a TCP socket (so there is no port).
   */
  get port() {
    if (!this.isPortBased || !this._socket) {
      return null;
    }
    return this._socket.port;
  },

  onAllowedConnection(transport) {
    dumpn("onAllowedConnection, transport: " + transport);
    this.emit("accepted", transport, this);
  },

  // nsIServerSocketListener implementation

  onSocketAccepted: DevToolsUtils.makeInfallible(function(
    socket,
    socketTransport
  ) {
    const connection = new ServerSocketConnection(this, socketTransport);
    connection.once("allowed", this.onAllowedConnection.bind(this));
  },
  "SocketListener.onSocketAccepted"),

  onStopListening(socket, status) {
    dumpn("onStopListening, status: " + status);
  },
};

/**
 * A |ServerSocketConnection| is created by a |SocketListener| for each accepted
 * incoming socket.
 */
function ServerSocketConnection(listener, socketTransport) {
  this._listener = listener;
  this._socketTransport = socketTransport;
  this._handle();
  EventEmitter.decorate(this);
}

ServerSocketConnection.prototype = {
  get authentication() {
    return this._listener.authenticator.mode;
  },

  get host() {
    return this._socketTransport.host;
  },

  get port() {
    return this._socketTransport.port;
  },

  get address() {
    return this.host + ":" + this.port;
  },

  get client() {
    const client = {
      host: this.host,
      port: this.port,
    };
    return client;
  },

  get server() {
    const server = {
      host: this._listener.host,
      port: this._listener.port,
    };
    return server;
  },

  /**
   * This is the main authentication workflow.  If any pieces reject a promise,
   * the connection is denied.  If the entire process resolves successfully,
   * the connection is finally handed off to the |DevToolsServer|.
   */
  async _handle() {
    dumpn("Debugging connection starting authentication on " + this.address);
    try {
      await this._createTransport();
      await this._authenticate();
      this.allow();
    } catch (e) {
      this.deny(e);
    }
  },

  /**
   * We need to open the streams early on, as that is required in the case of
   * TLS sockets to keep the handshake moving.
   */
  async _createTransport() {
    const input = this._socketTransport.openInputStream(0, 0, 0);
    const output = this._socketTransport.openOutputStream(0, 0, 0);

    if (this._listener.webSocket) {
      const socket = await WebSocketServer.accept(
        this._socketTransport,
        input,
        output
      );
      this._transport = new WebSocketDebuggerTransport(socket);
    } else {
      this._transport = new DebuggerTransport(input, output);
    }

    // Start up the transport to observe the streams in case they are closed
    // early.  This allows us to clean up our state as well.
    this._transport.hooks = {
      onTransportClosed: reason => {
        this.deny(reason);
      },
    };
    this._transport.ready();
  },

  async _authenticate() {
    const result = await this._listener.authenticator.authenticate({
      client: this.client,
      server: this.server,
      transport: this._transport,
    });

    // If result is fine, we can stop here
    if (
      result === AuthenticationResult.ALLOW ||
      result === AuthenticationResult.ALLOW_PERSIST
    ) {
      return;
    }

    if (result === AuthenticationResult.DISABLE_ALL) {
      this._listener._devToolsServer.closeAllSocketListeners();
      Services.prefs.setBoolPref("devtools.debugger.remote-enabled", false);
    }

    // If we got an error (DISABLE_ALL, DENY, …), let's throw a NS_ERROR_CONNECTION_REFUSED
    // exception
    throw Components.Exception("", Cr.NS_ERROR_CONNECTION_REFUSED);
  },

  deny(result) {
    if (this._destroyed) {
      return;
    }
    let errorName = result;
    for (const name in Cr) {
      if (Cr[name] === result) {
        errorName = name;
        break;
      }
    }
    dumpn(
      "Debugging connection denied on " + this.address + " (" + errorName + ")"
    );
    if (this._transport) {
      this._transport.hooks = null;
      this._transport.close(result);
    }
    this._socketTransport.close(result);
    this.destroy();
  },

  allow() {
    if (this._destroyed) {
      return;
    }
    dumpn("Debugging connection allowed on " + this.address);
    this.emit("allowed", this._transport);
    this.destroy();
  },

  destroy() {
    this._destroyed = true;
    this._listener = null;
    this._socketTransport = null;
    this._transport = null;
  },
};

exports.DebuggerSocket = DebuggerSocket;
exports.SocketListener = SocketListener;
