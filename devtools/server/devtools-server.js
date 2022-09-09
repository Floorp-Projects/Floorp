/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { Ci } = require("chrome");
var { ActorRegistry } = require("devtools/server/actors/utils/actor-registry");
var DevToolsUtils = require("devtools/shared/DevToolsUtils");
var { dumpn } = DevToolsUtils;

loader.lazyRequireGetter(
  this,
  "DevToolsServerConnection",
  "devtools/server/devtools-server-connection",
  true
);
loader.lazyRequireGetter(
  this,
  "Authentication",
  "devtools/shared/security/auth"
);
loader.lazyRequireGetter(
  this,
  "LocalDebuggerTransport",
  "devtools/shared/transport/local-transport",
  true
);
loader.lazyRequireGetter(
  this,
  "ChildDebuggerTransport",
  "devtools/shared/transport/child-transport",
  true
);
loader.lazyRequireGetter(
  this,
  "JsWindowActorTransport",
  "devtools/shared/transport/js-window-actor-transport",
  true
);
loader.lazyRequireGetter(
  this,
  "WorkerThreadWorkerDebuggerTransport",
  "devtools/shared/transport/worker-transport",
  true
);

const CONTENT_PROCESS_SERVER_STARTUP_SCRIPT =
  "resource://devtools/server/startup/content-process.js";

loader.lazyRequireGetter(this, "EventEmitter", "devtools/shared/event-emitter");

/**
 * DevToolsServer is a singleton that has several responsibilities. It will
 * register the DevTools server actors that are relevant to the context.
 * It can also create other DevToolsServer, that will live in the same
 * environment as the debugged target (content page, worker...).
 *
 * For instance a regular Toolbox will be linked to DevToolsClient connected to
 * a DevToolsServer running in the same process as the Toolbox (main process).
 * But another DevToolsServer will be created in the same process as the page
 * targeted by the Toolbox.
 *
 * Despite being a singleton, the DevToolsServer still has a lifecycle and a
 * state. When a consumer needs to spawn a DevToolsServer, the init() method
 * should be called. Then you should either call registerAllActors or
 * registerActors to setup the server.
 * When the server is no longer needed, destroy() should be called.
 *
 */
var DevToolsServer = {
  _listeners: [],
  _initialized: false,
  // Map of global actor names to actor constructors.
  globalActorFactories: {},
  // Map of target-scoped actor names to actor constructors.
  targetScopedActorFactories: {},

  LONG_STRING_LENGTH: 10000,
  LONG_STRING_INITIAL_LENGTH: 1000,
  LONG_STRING_READ_LENGTH: 65 * 1024,

  /**
   * The windowtype of the chrome window to use for actors that use the global
   * window (i.e the global style editor). Set this to your main window type,
   * for example "navigator:browser".
   */
  chromeWindowType: "navigator:browser",

  /**
   * Allow debugging chrome of (parent or child) processes.
   */
  allowChromeProcess: false,

  /**
   * Flag used to check if the server can be destroyed when all connections have been
   * removed. Firefox on Android runs a single shared DevToolsServer, and should not be
   * closed even if no client is connected.
   */
  keepAlive: false,

  /**
   * We run a special server in child process whose main actor is an instance
   * of WindowGlobalTargetActor, but that isn't a root actor. Instead there is no root
   * actor registered on DevToolsServer.
   */
  get rootlessServer() {
    return !this.createRootActor;
  },

  /**
   * Initialize the devtools server.
   */
  init() {
    if (this.initialized) {
      return;
    }

    this._connections = {};
    ActorRegistry.init(this._connections);
    this._nextConnID = 0;

    this._initialized = true;
    this._onSocketListenerAccepted = this._onSocketListenerAccepted.bind(this);

    if (!isWorker) {
      // Mochitests watch this observable in order to register the custom actor
      // highlighter-test-actor.js.
      // Services.obs is not available in workers.
      const subject = { wrappedJSObject: ActorRegistry };
      Services.obs.notifyObservers(subject, "devtools-server-initialized");
    }
  },

  get protocol() {
    return require("devtools/shared/protocol");
  },

  get initialized() {
    return this._initialized;
  },

  hasConnection() {
    return this._connections && !!Object.keys(this._connections).length;
  },

  hasConnectionForPrefix(prefix) {
    return this._connections && !!this._connections[prefix + "/"];
  },
  /**
   * Performs cleanup tasks before shutting down the devtools server. Such tasks
   * include clearing any actor constructors added at runtime. This method
   * should be called whenever a devtools server is no longer useful, to avoid
   * memory leaks. After this method returns, the devtools server must be
   * initialized again before use.
   */
  destroy() {
    if (!this._initialized) {
      return;
    }
    this._initialized = false;

    for (const connection of Object.values(this._connections)) {
      connection.close();
    }

    ActorRegistry.destroy();
    this.closeAllSocketListeners();

    // Unregister all listeners
    this.off("connectionchange");

    dumpn("DevTools server is shut down.");
  },

  /**
   * Raises an exception if the server has not been properly initialized.
   */
  _checkInit() {
    if (!this._initialized) {
      throw new Error("DevToolsServer has not been initialized.");
    }

    if (!this.rootlessServer && !this.createRootActor) {
      throw new Error(
        "Use DevToolsServer.setRootActor() to add a root actor " +
          "implementation."
      );
    }
  },

  /**
   * Register different type of actors. Only register the one that are not already
   * registered.
   *
   * @param root boolean
   *        Registers the root actor from webbrowser module, which is used to
   *        connect to and fetch any other actor.
   * @param browser boolean
   *        Registers all the parent process actors useful for debugging the
   *        runtime itself, like preferences and addons actors.
   * @param target boolean
   *        Registers all the target-scoped actors like console, script, etc.
   *        for debugging a target context.
   */
  registerActors({ root, browser, target }) {
    if (browser) {
      ActorRegistry.addBrowserActors();
    }

    if (root) {
      const { createRootActor } = require("devtools/server/actors/webbrowser");
      this.setRootActor(createRootActor);
    }

    if (target) {
      ActorRegistry.addTargetScopedActors();
    }
  },

  /**
   * Register all possible actors for this DevToolsServer.
   */
  registerAllActors() {
    this.registerActors({ root: true, browser: true, target: true });
  },

  get listeningSockets() {
    return this._listeners.length;
  },

  /**
   * Add a SocketListener instance to the server's set of active
   * SocketListeners.  This is called by a SocketListener after it is opened.
   */
  addSocketListener(listener) {
    if (!Services.prefs.getBoolPref("devtools.debugger.remote-enabled")) {
      throw new Error("Can't add a SocketListener, remote debugging disabled");
    }
    this._checkInit();

    listener.on("accepted", this._onSocketListenerAccepted);
    this._listeners.push(listener);
  },

  /**
   * Remove a SocketListener instance from the server's set of active
   * SocketListeners.  This is called by a SocketListener after it is closed.
   */
  removeSocketListener(listener) {
    // Remove connections that were accepted in the listener.
    for (const connID of Object.getOwnPropertyNames(this._connections)) {
      const connection = this._connections[connID];
      if (connection.isAcceptedBy(listener)) {
        connection.close();
      }
    }

    this._listeners = this._listeners.filter(l => l !== listener);
    listener.off("accepted", this._onSocketListenerAccepted);
  },

  /**
   * Closes and forgets all previously opened listeners.
   *
   * @return boolean
   *         Whether any listeners were actually closed.
   */
  closeAllSocketListeners() {
    if (!this.listeningSockets) {
      return false;
    }

    for (const listener of this._listeners) {
      listener.close();
    }

    return true;
  },

  _onSocketListenerAccepted(transport, listener) {
    this._onConnection(transport, null, false, listener);
  },

  /**
   * Creates a new connection to the local debugger speaking over a fake
   * transport. This connection results in straightforward calls to the onPacket
   * handlers of each side.
   *
   * @param prefix string [optional]
   *    If given, all actors in this connection will have names starting
   *    with |prefix + '/'|.
   * @returns a client-side DebuggerTransport for communicating with
   *    the newly-created connection.
   */
  connectPipe(prefix) {
    this._checkInit();

    const serverTransport = new LocalDebuggerTransport();
    const clientTransport = new LocalDebuggerTransport(serverTransport);
    serverTransport.other = clientTransport;
    const connection = this._onConnection(serverTransport, prefix);

    // I'm putting this here because I trust you.
    //
    // There are times, when using a local connection, when you're going
    // to be tempted to just get direct access to the server.  Resist that
    // temptation!  If you succumb to that temptation, you will make the
    // fine developers that work on Fennec and Firefox OS sad.  They're
    // professionals, they'll try to act like they understand, but deep
    // down you'll know that you hurt them.
    //
    // This reference allows you to give in to that temptation.  There are
    // times this makes sense: tests, for example, and while porting a
    // previously local-only codebase to the remote protocol.
    //
    // But every time you use this, you will feel the shame of having
    // used a property that starts with a '_'.
    clientTransport._serverConnection = connection;

    return clientTransport;
  },

  /**
   * In a content child process, create a new connection that exchanges
   * nsIMessageSender messages with our parent process.
   *
   * @param prefix
   *    The prefix we should use in our nsIMessageSender message names and
   *    actor names. This connection will use messages named
   *    "debug:<prefix>:packet", and all its actors will have names
   *    beginning with "<prefix>/".
   */
  connectToParent(prefix, scopeOrManager) {
    this._checkInit();

    const transport = isWorker
      ? new WorkerThreadWorkerDebuggerTransport(scopeOrManager, prefix)
      : new ChildDebuggerTransport(scopeOrManager, prefix);

    return this._onConnection(transport, prefix, true);
  },

  connectToParentWindowActor(jsWindowChildActor, forwardingPrefix) {
    this._checkInit();
    const transport = new JsWindowActorTransport(
      jsWindowChildActor,
      forwardingPrefix
    );

    return this._onConnection(transport, forwardingPrefix, true);
  },

  /**
   * Check if the server is running in the child process.
   */
  get isInChildProcess() {
    return (
      Services.appinfo.processType != Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT
    );
  },

  /**
   * Create a new debugger connection for the given transport. Called after
   * connectPipe(), from connectToParent, or from an incoming socket
   * connection handler.
   *
   * If present, |forwardingPrefix| is a forwarding prefix that a parent
   * server is using to recognizes messages intended for this server. Ensure
   * that all our actors have names beginning with |forwardingPrefix + '/'|.
   * In particular, the root actor's name will be |forwardingPrefix + '/root'|.
   */
  _onConnection(
    transport,
    forwardingPrefix,
    noRootActor = false,
    socketListener = null
  ) {
    let connID;
    if (forwardingPrefix) {
      connID = forwardingPrefix + "/";
    } else {
      // Multiple servers can be started at the same time, and when that's the
      // case, they are loaded in separate devtools loaders.
      // So, use the current loader ID to prefix the connection ID and make it
      // unique.
      connID = "server" + loader.id + ".conn" + this._nextConnID++ + ".";
    }

    const conn = new DevToolsServerConnection(
      connID,
      transport,
      socketListener
    );
    this._connections[connID] = conn;

    // Create a root actor for the connection and send the hello packet.
    if (!noRootActor) {
      conn.rootActor = this.createRootActor(conn);
      if (forwardingPrefix) {
        conn.rootActor.actorID = forwardingPrefix + "/root";
      } else {
        conn.rootActor.actorID = "root";
      }
      conn.addActor(conn.rootActor);
      transport.send(conn.rootActor.sayHello());
    }
    transport.ready();

    this.emit("connectionchange", "opened", conn);
    return conn;
  },

  /**
   * Remove the connection from the debugging server.
   */
  _connectionClosed(connection) {
    delete this._connections[connection.prefix];
    this.emit("connectionchange", "closed", connection);

    // If keepAlive isn't explicitely set to true, destroy the server once its
    // last connection closes. Multiple JSWindowActor may use the same DevToolsServer
    // and in this case, let the server destroy itself once the last connection closes.
    // Otherwise we set keepAlive to true when starting a listening server, receiving
    // client connections. Typically when running server on phones, or on desktop
    // via `--start-debugger-server`.
    if (this.hasConnection() || this.keepAlive) {
      return;
    }

    this.destroy();
  },

  // DevToolsServer extension API.

  setRootActor(actorFactory) {
    this.createRootActor = actorFactory;
  },

  /**
   * Called when DevTools are unloaded to remove the contend process server startup script
   * for the list of scripts loaded for each new content process. Will also remove message
   * listeners from already loaded scripts.
   */
  removeContentServerScript() {
    Services.ppmm.removeDelayedProcessScript(
      CONTENT_PROCESS_SERVER_STARTUP_SCRIPT
    );
    try {
      Services.ppmm.broadcastAsyncMessage("debug:close-content-server");
    } catch (e) {
      // Nothing to do
    }
  },

  /**
   * Searches all active connections for an actor matching an ID.
   *
   * ⚠ TO BE USED ONLY FROM SERVER CODE OR TESTING ONLY! ⚠`
   *
   * This is helpful for some tests which depend on reaching into the server to check some
   * properties of an actor, and it is also used by the actors related to the
   * DevTools WebExtensions API to be able to interact with the actors created for the
   * panels natively provided by the DevTools Toolbox.
   */
  searchAllConnectionsForActor(actorID) {
    // NOTE: the actor IDs are generated with the following format:
    //
    //   `server${loaderID}.conn${ConnectionID}${ActorPrefix}${ActorID}`
    //
    // as an optimization we can come up with a regexp to query only
    // the right connection via its id.
    for (const connID of Object.getOwnPropertyNames(this._connections)) {
      const actor = this._connections[connID].getActor(actorID);
      if (actor) {
        return actor;
      }
    }
    return null;
  },
};

// Expose these to save callers the trouble of importing DebuggerSocket
DevToolsUtils.defineLazyGetter(DevToolsServer, "Authenticators", () => {
  return Authentication.Authenticators;
});
DevToolsUtils.defineLazyGetter(DevToolsServer, "AuthenticationResult", () => {
  return Authentication.AuthenticationResult;
});

EventEmitter.decorate(DevToolsServer);

exports.DevToolsServer = DevToolsServer;
