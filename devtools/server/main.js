/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Toolkit glue for the remote debugging protocol, loaded into the
 * debugging global.
 */
var { Ci, Cc } = require("chrome");
var Services = require("Services");
var { ActorPool, OriginalLocation, RegisteredActorFactory,
      ObservedActorFactory } = require("devtools/server/actors/common");
var { LocalDebuggerTransport, ChildDebuggerTransport, WorkerDebuggerTransport } =
  require("devtools/shared/transport/transport");
var DevToolsUtils = require("devtools/shared/DevToolsUtils");
var { dumpn } = DevToolsUtils;

DevToolsUtils.defineLazyGetter(this, "DebuggerSocket", () => {
  // eslint-disable-next-line no-shadow
  const { DebuggerSocket } = require("devtools/shared/security/socket");
  return DebuggerSocket;
});
DevToolsUtils.defineLazyGetter(this, "Authentication", () => {
  return require("devtools/shared/security/auth");
});
DevToolsUtils.defineLazyGetter(this, "generateUUID", () => {
  // eslint-disable-next-line no-shadow
  const { generateUUID } = Cc["@mozilla.org/uuid-generator;1"]
                           .getService(Ci.nsIUUIDGenerator);
  return generateUUID;
});

// Overload `Components` to prevent DevTools loader exception on Components
// object usage
// eslint-disable-next-line no-unused-vars
Object.defineProperty(this, "Components", {
  get() {
    return require("chrome").components;
  }
});

const CONTENT_PROCESS_SERVER_STARTUP_SCRIPT =
  "resource://devtools/server/startup/content-process.js";

function loadSubScript(url) {
  try {
    Services.scriptloader.loadSubScript(url, this);
  } catch (e) {
    const errorStr = "Error loading: " + url + ":\n" +
                   (e.fileName ? "at " + e.fileName + " : " + e.lineNumber + "\n" : "") +
                   e + " - " + e.stack + "\n";
    dump(errorStr);
    reportError(errorStr);
    throw e;
  }
}

loader.lazyRequireGetter(this, "EventEmitter", "devtools/shared/event-emitter");

var gRegisteredModules = Object.create(null);

/**
 * The ModuleAPI object is passed to modules loaded using the
 * DebuggerServer.registerModule() API.  Modules can use this
 * object to register actor factories.
 * Factories registered through the module API will be removed
 * when the module is unregistered or when the server is
 * destroyed.
 */
function ModuleAPI() {
  let activeTargetScopedActors = new Set();
  let activeGlobalActors = new Set();

  return {
    // See DebuggerServer.setRootActor for a description.
    setRootActor(factory) {
      DebuggerServer.setRootActor(factory);
    },

    // See DebuggerServer.addGlobalActor for a description.
    addGlobalActor(factory, name) {
      DebuggerServer.addGlobalActor(factory, name);
      activeGlobalActors.add(factory);
    },
    // See DebuggerServer.removeGlobalActor for a description.
    removeGlobalActor(factory) {
      DebuggerServer.removeGlobalActor(factory);
      activeGlobalActors.delete(factory);
    },

    // See DebuggerServer.addTargetScopedActor for a description.
    addTargetScopedActor(factory, name) {
      DebuggerServer.addTargetScopedActor(factory, name);
      activeTargetScopedActors.add(factory);
    },
    // See DebuggerServer.removeTargetScopedActor for a description.
    removeTargetScopedActor(factory) {
      DebuggerServer.removeTargetScopedActor(factory);
      activeTargetScopedActors.delete(factory);
    },

    // Destroy the module API object, unregistering any
    // factories registered by the module.
    destroy() {
      for (const factory of activeTargetScopedActors) {
        DebuggerServer.removeTargetScopedActor(factory);
      }
      activeTargetScopedActors = null;
      for (const factory of activeGlobalActors) {
        DebuggerServer.removeGlobalActor(factory);
      }
      activeGlobalActors = null;
    }
  };
}

/**
 * Public API
 */
var DebuggerServer = {
  _listeners: [],
  _initialized: false,
  // Flag to check if the content process server startup script was already loaded.
  _contentProcessServerStartupScriptLoaded: false,
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
   * We run a special server in child process whose main actor is an instance
   * of FrameTargetActor, but that isn't a root actor. Instead there is no root
   * actor registered on DebuggerServer.
   */
  get rootlessServer() {
    return !this.isModuleRegistered("devtools/server/actors/webbrowser");
  },

  /**
   * Initialize the debugger server.
   */
  init() {
    if (this.initialized) {
      return;
    }

    this._connections = {};
    this._nextConnID = 0;

    this._initialized = true;
  },

  get protocol() {
    return require("devtools/shared/protocol");
  },

  get initialized() {
    return this._initialized;
  },

  /**
   * Performs cleanup tasks before shutting down the debugger server. Such tasks
   * include clearing any actor constructors added at runtime. This method
   * should be called whenever a debugger server is no longer useful, to avoid
   * memory leaks. After this method returns, the debugger server must be
   * initialized again before use.
   */
  destroy() {
    if (!this._initialized) {
      return;
    }

    for (const connID of Object.getOwnPropertyNames(this._connections)) {
      this._connections[connID].close();
    }

    for (const id of Object.getOwnPropertyNames(gRegisteredModules)) {
      this.unregisterModule(id);
    }
    gRegisteredModules = Object.create(null);

    this.closeAllListeners();
    this.globalActorFactories = {};
    this.targetScopedActorFactories = {};
    this._initialized = false;

    dumpn("Debugger server is shut down.");
  },

  /**
   * Raises an exception if the server has not been properly initialized.
   */
  _checkInit() {
    if (!this._initialized) {
      throw new Error("DebuggerServer has not been initialized.");
    }

    if (!this.rootlessServer && !this.createRootActor) {
      throw new Error("Use DebuggerServer.addActors() to add a root actor " +
                      "implementation.");
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
      this._addBrowserActors();
    }

    if (root) {
      this.registerModule("devtools/server/actors/webbrowser");
    }

    if (target) {
      this._addTargetScopedActors();
    }
  },

  /**
   * Register all possible actors for this DebuggerServer.
   */
  registerAllActors() {
    this.registerActors({ root: true, browser: true, target: true });
  },

  /**
   * Load a subscript into the debugging global.
   *
   * @param url string A url that will be loaded as a subscript into the
   *        debugging global.  The user must load at least one script
   *        that implements a createRootActor() function to create the
   *        server's root actor.
   */
  addActors(url) {
    loadSubScript.call(this, url);
  },

  /**
   * Register a CommonJS module with the debugger server.
   * @param id string
   *        The ID of a CommonJS module.  This module must export 'register'
   *        and 'unregister' functions if no `options` argument is given.
   *        If `options` is set, the actor is going to be registered
   *        immediately, but loaded only when a client starts sending packets
   *        to an actor with the same id.
   *
   * @param options object (optional)
   *        This parameter is still optional, but not providing it is
   *        deprecated and will result in eagerly loading the actor module
   *        with the memory overhead that entails.
   *        An object with 3 mandatory attributes:
   *        - prefix (string):
   *          The prefix of an actor is used to compute:
   *          - the `actorID` of each new actor instance (ex: prefix1).
   *            (See ActorPool.addActor)
   *          - the actor name in the listTabs request. Sending a listTabs
   *            request to the root actor returns actor IDs. IDs are in
   *            dictionaries, with actor names as keys and actor IDs as values.
   *            The actor name is the prefix to which the "Actor" string is
   *            appended. So for an actor with the `console` prefix, the actor
   *            name will be `consoleActor`.
   *        - constructor (string):
   *          the name of the exported symbol to be used as the actor
   *          constructor.
   *        - type (a dictionary of booleans with following attribute names):
   *          - "global"
   *            registers a global actor instance, if true.
   *            A global actor has the root actor as its parent.
   *          - "target"
   *            registers a target-scoped actor instance, if true.
   *            A new actor will be created for each target, such as a tab.
   */
  registerModule(id, options) {
    if (id in gRegisteredModules) {
      return;
    }

    if (options) {
      // Lazy loaded actors
      const {prefix, constructor, type} = options;
      if (typeof (prefix) !== "string") {
        throw new Error(`Lazy actor definition for '${id}' requires a string ` +
                        `'prefix' option.`);
      }
      if (typeof (constructor) !== "string") {
        throw new Error(`Lazy actor definition for '${id}' requires a string ` +
                        `'constructor' option.`);
      }
      if (!("global" in type) && !("target" in type)) {
        throw new Error(`Lazy actor definition for '${id}' requires a dictionary ` +
                        `'type' option whose attributes can be 'global' or 'target'.`);
      }
      const name = prefix + "Actor";
      const mod = {
        id: id,
        prefix: prefix,
        constructorName: constructor,
        type: type,
        globalActor: type.global,
        targetScopedActor: type.target
      };
      gRegisteredModules[id] = mod;
      if (mod.targetScopedActor) {
        this.addTargetScopedActor(mod, name);
      }
      if (mod.globalActor) {
        this.addGlobalActor(mod, name);
      }
    } else {
      // Deprecated actors being loaded at startup
      const moduleAPI = ModuleAPI();
      const mod = require(id);
      mod.register(moduleAPI);
      gRegisteredModules[id] = {
        module: mod,
        api: moduleAPI
      };
    }
  },

  /**
   * Returns true if a module id has been registered.
   */
  isModuleRegistered(id) {
    return (id in gRegisteredModules);
  },

  /**
   * Unregister a previously-loaded CommonJS module from the debugger server.
   */
  unregisterModule(id) {
    const mod = gRegisteredModules[id];
    if (!mod) {
      throw new Error("Tried to unregister a module that was not previously registered.");
    }

    // Lazy actors
    if (mod.targetScopedActor) {
      this.removeTargetScopedActor(mod);
    }
    if (mod.globalActor) {
      this.removeGlobalActor(mod);
    }

    if (mod.module) {
      // Deprecated non-lazy module API
      mod.module.unregister(mod.api);
      mod.api.destroy();
    }

    delete gRegisteredModules[id];
  },

  /**
   * Install Firefox-specific actors.
   *
   * /!\ Be careful when adding a new actor, especially global actors.
   * Any new global actor will be exposed and returned by the root actor.
   */
  _addBrowserActors() {
    this.registerModule("devtools/server/actors/preference", {
      prefix: "preference",
      constructor: "PreferenceActor",
      type: { global: true }
    });
    this.registerModule("devtools/server/actors/actor-registry", {
      prefix: "actorRegistry",
      constructor: "ActorRegistryActor",
      type: { global: true }
    });
    this.registerModule("devtools/server/actors/addon/addons", {
      prefix: "addons",
      constructor: "AddonsActor",
      type: { global: true }
    });
    this.registerModule("devtools/server/actors/device", {
      prefix: "device",
      constructor: "DeviceActor",
      type: { global: true }
    });
    this.registerModule("devtools/server/actors/heap-snapshot-file", {
      prefix: "heapSnapshotFile",
      constructor: "HeapSnapshotFileActor",
      type: { global: true }
    });
    // Always register this as a global module, even while there is a pref turning
    // on and off the other performance actor. This actor shouldn't conflict with
    // the other one. These are also lazily loaded so there shouldn't be a performance
    // impact.
    this.registerModule("devtools/server/actors/perf", {
      prefix: "perf",
      constructor: "PerfActor",
      type: { global: true }
    });
  },

  /**
   * Install target-scoped actors.
   */
  _addTargetScopedActors() {
    this.registerModule("devtools/server/actors/webconsole", {
      prefix: "console",
      constructor: "WebConsoleActor",
      type: { target: true }
    });
    this.registerModule("devtools/server/actors/inspector/inspector", {
      prefix: "inspector",
      constructor: "InspectorActor",
      type: { target: true }
    });
    this.registerModule("devtools/server/actors/call-watcher", {
      prefix: "callWatcher",
      constructor: "CallWatcherActor",
      type: { target: true }
    });
    this.registerModule("devtools/server/actors/canvas", {
      prefix: "canvas",
      constructor: "CanvasActor",
      type: { target: true }
    });
    this.registerModule("devtools/server/actors/webgl", {
      prefix: "webgl",
      constructor: "WebGLActor",
      type: { target: true }
    });
    this.registerModule("devtools/server/actors/webaudio", {
      prefix: "webaudio",
      constructor: "WebAudioActor",
      type: { target: true }
    });
    this.registerModule("devtools/server/actors/stylesheets", {
      prefix: "styleSheets",
      constructor: "StyleSheetsActor",
      type: { target: true }
    });
    this.registerModule("devtools/server/actors/storage", {
      prefix: "storage",
      constructor: "StorageActor",
      type: { target: true }
    });
    this.registerModule("devtools/server/actors/gcli", {
      prefix: "gcli",
      constructor: "GcliActor",
      type: { target: true }
    });
    this.registerModule("devtools/server/actors/memory", {
      prefix: "memory",
      constructor: "MemoryActor",
      type: { target: true }
    });
    this.registerModule("devtools/server/actors/framerate", {
      prefix: "framerate",
      constructor: "FramerateActor",
      type: { target: true }
    });
    this.registerModule("devtools/server/actors/reflow", {
      prefix: "reflow",
      constructor: "ReflowActor",
      type: { target: true }
    });
    this.registerModule("devtools/server/actors/css-properties", {
      prefix: "cssProperties",
      constructor: "CssPropertiesActor",
      type: { target: true }
    });
    this.registerModule("devtools/server/actors/csscoverage", {
      prefix: "cssUsage",
      constructor: "CSSUsageActor",
      type: { target: true }
    });
    this.registerModule("devtools/server/actors/timeline", {
      prefix: "timeline",
      constructor: "TimelineActor",
      type: { target: true }
    });
    if ("nsIProfiler" in Ci &&
        !Services.prefs.getBoolPref("devtools.performance.new-panel-enabled", false)) {
      this.registerModule("devtools/server/actors/performance", {
        prefix: "performance",
        constructor: "PerformanceActor",
        type: { target: true }
      });
    }
    this.registerModule("devtools/server/actors/animation", {
      prefix: "animations",
      constructor: "AnimationsActor",
      type: { target: true }
    });
    this.registerModule("devtools/server/actors/promises", {
      prefix: "promises",
      constructor: "PromisesActor",
      type: { target: true }
    });
    this.registerModule("devtools/server/actors/emulation", {
      prefix: "emulation",
      constructor: "EmulationActor",
      type: { target: true }
    });
    this.registerModule("devtools/server/actors/addon/webextension-inspected-window", {
      prefix: "webExtensionInspectedWindow",
      constructor: "WebExtensionInspectedWindowActor",
      type: { target: true }
    });
    this.registerModule("devtools/server/actors/accessibility", {
      prefix: "accessibility",
      constructor: "AccessibilityActor",
      type: { target: true }
    });
  },

  /**
   * Passes a set of options to the AddonTargetActors for the given ID.
   *
   * @param id string
   *        The ID of the add-on to pass the options to
   * @param options object
   *        The options.
   * @return a promise that will be resolved when complete.
   */
  setAddonOptions(id, options) {
    if (!this._initialized) {
      return Promise.resolve();
    }

    const promises = [];

    // Pass to all connections
    for (const connID of Object.getOwnPropertyNames(this._connections)) {
      promises.push(this._connections[connID].setAddonOptions(id, options));
    }

    return Promise.all(promises);
  },

  get listeningSockets() {
    return this._listeners.length;
  },

  /**
   * Creates a socket listener for remote debugger connections.
   *
   * After calling this, set some socket options, such as the port / path to
   * listen on, and then call |open| on the listener.
   *
   * See SocketListener in devtools/shared/security/socket.js for available
   * options.
   *
   * @return SocketListener
   *         A SocketListener instance that is waiting to be configured and
   *         opened is returned.  This single listener can be closed at any
   *         later time by calling |close| on the SocketListener.  If remote
   *         connections are disabled, an error is thrown.
   */
  createListener() {
    if (!Services.prefs.getBoolPref("devtools.debugger.remote-enabled")) {
      throw new Error("Can't create listener, remote debugging disabled");
    }
    this._checkInit();
    return DebuggerSocket.createListener();
  },

  /**
   * Add a SocketListener instance to the server's set of active
   * SocketListeners.  This is called by a SocketListener after it is opened.
   */
  _addListener(listener) {
    this._listeners.push(listener);
  },

  /**
   * Remove a SocketListener instance from the server's set of active
   * SocketListeners.  This is called by a SocketListener after it is closed.
   */
  _removeListener(listener) {
    this._listeners = this._listeners.filter(l => l !== listener);
  },

  /**
   * Closes and forgets all previously opened listeners.
   *
   * @return boolean
   *         Whether any listeners were actually closed.
   */
  closeAllListeners() {
    if (!this.listeningSockets) {
      return false;
    }

    for (const listener of this._listeners) {
      listener.close();
    }

    return true;
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

    const transport = isWorker ?
                    new WorkerDebuggerTransport(scopeOrManager, prefix) :
                    new ChildDebuggerTransport(scopeOrManager, prefix);

    return this._onConnection(transport, prefix, true);
  },

  /**
   * Start a DevTools server in a content process (representing the entire process, not
   * just a single frame) and add it as a child server for an active connection.
   */
  connectToContentProcess(connection, mm, onDestroy) {
    return new Promise(resolve => {
      const prefix = connection.allocID("content-process");
      let actor, childTransport;

      mm.addMessageListener("debug:content-process-actor", function listener(msg) {
        // Arbitrarily choose the first content process to reply
        // XXX: This code needs to be updated if we use more than one content process
        mm.removeMessageListener("debug:content-process-actor", listener);

        // Pipe Debugger message from/to parent/child via the message manager
        childTransport = new ChildDebuggerTransport(mm, prefix);
        childTransport.hooks = {
          onPacket: connection.send.bind(connection),
          onClosed() {}
        };
        childTransport.ready();

        connection.setForwarding(prefix, childTransport);

        dumpn(`Start forwarding for process with prefix ${prefix}`);

        actor = msg.json.actor;

        resolve(actor);
      });

      // Load the content process server startup script only once.
      if (!this._contentProcessServerStartupScriptLoaded) {
        // Load the process script that will receive the debug:init-content-server message
        Services.ppmm.loadProcessScript(CONTENT_PROCESS_SERVER_STARTUP_SCRIPT, true);
        this._contentProcessServerStartupScriptLoaded = true;
      }

      // Send a message to the content process server startup script to forward it the
      // prefix.
      mm.sendAsyncMessage("debug:init-content-server", {
        prefix: prefix
      });

      function onClose() {
        Services.obs.removeObserver(onMessageManagerClose, "message-manager-close");
        EventEmitter.off(connection, "closed", onClose);
        if (childTransport) {
          // If we have a child transport, the actor has already
          // been created. We need to stop using this message manager.
          childTransport.close();
          childTransport = null;
          connection.cancelForwarding(prefix);

          // ... and notify the child process to clean the target-scoped actors.
          try {
            mm.sendAsyncMessage("debug:content-process-destroy");
          } catch (e) {
            // Nothing to do
          }
        }

        if (onDestroy) {
          onDestroy(mm);
        }
      }

      const onMessageManagerClose =
        DevToolsUtils.makeInfallible((subject, topic, data) => {
          if (subject == mm) {
            onClose();
            connection.send({ from: actor.actor, type: "tabDetached" });
          }
        });
      Services.obs.addObserver(onMessageManagerClose,
                               "message-manager-close");

      EventEmitter.on(connection, "closed", onClose);
    });
  },

  /**
   * Start a DevTools server in a worker and add it as a child server for an active
   * connection.
   */
  connectToWorker(connection, dbg, id, options) {
    return new Promise((resolve, reject) => {
      // Step 1: Ensure the worker debugger is initialized.
      if (!dbg.isInitialized) {
        dbg.initialize("resource://devtools/server/startup/worker.js");

        // Create a listener for rpc requests from the worker debugger. Only do
        // this once, when the worker debugger is first initialized, rather than
        // for each connection.
        const listener = {
          onClose: () => {
            dbg.removeListener(listener);
          },

          onMessage: (message) => {
            message = JSON.parse(message);
            if (message.type !== "rpc") {
              return;
            }

            Promise.resolve().then(() => {
              const method = {
                "fetch": DevToolsUtils.fetch,
              }[message.method];
              if (!method) {
                throw Error("Unknown method: " + message.method);
              }

              return method.apply(undefined, message.params);
            }).then((value) => {
              dbg.postMessage(JSON.stringify({
                type: "rpc",
                result: value,
                error: null,
                id: message.id
              }));
            }, (reason) => {
              dbg.postMessage(JSON.stringify({
                type: "rpc",
                result: null,
                error: reason,
                id: message.id
              }));
            });
          }
        };

        dbg.addListener(listener);
      }

      // Step 2: Send a connect request to the worker debugger.
      dbg.postMessage(JSON.stringify({
        type: "connect",
        id,
        options,
      }));

      // Steps 3-5 are performed on the worker thread (see worker.js).

      // Step 6: Wait for a connection response from the worker debugger.
      const listener = {
        onClose: () => {
          dbg.removeListener(listener);

          reject("closed");
        },

        onMessage: (message) => {
          message = JSON.parse(message);
          if (message.type !== "connected" || message.id !== id) {
            return;
          }

          // The initial connection message has been received, don't
          // need to listen any longer
          dbg.removeListener(listener);

          // Step 7: Create a transport for the connection to the worker.
          const transport = new WorkerDebuggerTransport(dbg, id);
          transport.ready();
          transport.hooks = {
            onClosed: () => {
              if (!dbg.isClosed) {
                // If the worker happens to be shutting down while we are trying
                // to close the connection, there is a small interval during
                // which no more runnables can be dispatched to the worker, but
                // the worker debugger has not yet been closed. In that case,
                // the call to postMessage below will fail. The onClosed hook on
                // DebuggerTransport is not supposed to throw exceptions, so we
                // need to make sure to catch these early.
                try {
                  dbg.postMessage(JSON.stringify({
                    type: "disconnect",
                    id,
                  }));
                } catch (e) {
                  // We can safely ignore these exceptions. The only time the
                  // call to postMessage can fail is if the worker is either
                  // shutting down, or has finished shutting down. In both
                  // cases, there is nothing to clean up, so we don't care
                  // whether this message arrives or not.
                }
              }

              connection.cancelForwarding(id);
            },

            onPacket: (packet) => {
              // Ensure that any packets received from the server on the worker
              // thread are forwarded to the client on the main thread, as if
              // they had been sent by the server on the main thread.
              connection.send(packet);
            }
          };

          // Ensure that any packets received from the client on the main thread
          // to actors on the worker thread are forwarded to the server on the
          // worker thread.
          connection.setForwarding(id, transport);

          resolve({
            threadActor: message.threadActor,
            consoleActor: message.consoleActor,
            transport: transport
          });
        }
      };
      dbg.addListener(listener);
    });
  },

  /**
   * Check if the server is running in the child process.
   */
  get isInChildProcess() {
    return Services.appinfo.processType != Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;
  },

  /**
   * In a chrome parent process, ask all content child processes
   * to execute a given module setup helper.
   *
   * @param module
   *        The module to be required
   * @param setupChild
   *        The name of the setup helper exported by the above module
   *        (setup helper signature: function ({mm}) { ... })
   * @param waitForEval (optional)
   *        If true, the returned promise only resolves once code in child
   *        is evaluated
   */
  setupInChild({ module, setupChild, args, waitForEval }) {
    if (this._childMessageManagers.size == 0) {
      return Promise.resolve();
    }
    return new Promise(done => {
      // If waitForEval is set, pass a unique id and expect child.js to send
      // a message back once the code in child is evaluated.
      if (typeof (waitForEval) != "boolean") {
        waitForEval = false;
      }
      let count = this._childMessageManagers.size;
      const id = waitForEval ? generateUUID().toString() : null;

      this._childMessageManagers.forEach(mm => {
        if (waitForEval) {
          // Listen for the end of each child execution
          const evalListener = msg => {
            if (msg.data.id !== id) {
              return;
            }
            mm.removeMessageListener("debug:setup-in-child-response", evalListener);
            if (--count === 0) {
              done();
            }
          };
          mm.addMessageListener("debug:setup-in-child-response", evalListener);
        }
        mm.sendAsyncMessage("debug:setup-in-child", {
          module: module,
          setupChild: setupChild,
          args: args,
          id: id,
        });
      });

      if (!waitForEval) {
        done();
      }
    });
  },

  /**
   * Live list of all currenctly attached child's message managers.
   */
  _childMessageManagers: new Set(),

  /**
   * Start a DevTools server in a remote frame's process and add it as a child server for
   * an active connection.
   *
   * @param object connection
   *        The debugger server connection to use.
   * @param Element frame
   *        The frame element with remote content to connect to.
   * @param function [onDestroy]
   *        Optional function to invoke when the child process closes or the connection
   *        shuts down. (Need to forget about the related target actor.)
   * @return object
   *         A promise object that is resolved once the connection is established.
   */
  connectToFrame(connection, frame, onDestroy, {addonId} = {}) {
    return new Promise(resolve => {
      // Get messageManager from XUL browser (which might be a specialized tunnel for RDM)
      // or else fallback to asking the frameLoader itself.
      let mm = frame.messageManager || frame.frameLoader.messageManager;
      mm.loadFrameScript("resource://devtools/server/startup/frame.js", false);

      const trackMessageManager = () => {
        frame.addEventListener("DevTools:BrowserSwap", onBrowserSwap);
        mm.addMessageListener("debug:setup-in-parent", onSetupInParent);
        if (!actor) {
          mm.addMessageListener("debug:actor", onActorCreated);
        }
        DebuggerServer._childMessageManagers.add(mm);
      };

      const untrackMessageManager = () => {
        frame.removeEventListener("DevTools:BrowserSwap", onBrowserSwap);
        mm.removeMessageListener("debug:setup-in-parent", onSetupInParent);
        if (!actor) {
          mm.removeMessageListener("debug:actor", onActorCreated);
        }
        DebuggerServer._childMessageManagers.delete(mm);
      };

      let actor, childTransport;
      const prefix = connection.allocID("child");
      // Compute the same prefix that's used by DebuggerServerConnection
      const connPrefix = prefix + "/";

      // provides hook to actor modules that need to exchange messages
      // between e10s parent and child processes
      const parentModules = [];
      const onSetupInParent = function(msg) {
        // We may have multiple connectToFrame instance running for the same frame and
        // need to filter the messages.
        if (msg.json.prefix != connPrefix) {
          return false;
        }

        const { module, setupParent } = msg.json;
        let m;

        try {
          m = require(module);

          if (!(setupParent in m)) {
            dumpn(`ERROR: module '${module}' does not export '${setupParent}'`);
            return false;
          }

          parentModules.push(m[setupParent]({ mm, prefix: connPrefix }));

          return true;
        } catch (e) {
          const errorMessage =
            "Exception during actor module setup running in the parent process: ";
          DevToolsUtils.reportException(errorMessage + e);
          dumpn(`ERROR: ${errorMessage}\n\t module: '${module}'\n\t ` +
                `setupParent: '${setupParent}'\n${DevToolsUtils.safeErrorString(e)}`);
          return false;
        }
      };

      const onActorCreated = DevToolsUtils.makeInfallible(function(msg) {
        if (msg.json.prefix != prefix) {
          return;
        }
        mm.removeMessageListener("debug:actor", onActorCreated);

        // Pipe Debugger message from/to parent/child via the message manager
        childTransport = new ChildDebuggerTransport(mm, prefix);
        childTransport.hooks = {
          onPacket: connection.send.bind(connection),
          onClosed() {}
        };
        childTransport.ready();

        connection.setForwarding(prefix, childTransport);

        dumpn(`Start forwarding for frame with prefix ${prefix}`);

        actor = msg.json.actor;
        resolve(actor);
      }).bind(this);

      // Listen for browser frame swap
      const onBrowserSwap = ({ detail: newFrame }) => {
        // Remove listeners from old frame and mm
        untrackMessageManager();
        // Update frame and mm to point to the new browser frame
        frame = newFrame;
        // Get messageManager from XUL browser (which might be a specialized tunnel for
        // RDM) or else fallback to asking the frameLoader itself.
        mm = frame.messageManager || frame.frameLoader.messageManager;
        // Add listeners to new frame and mm
        trackMessageManager();

        // provides hook to actor modules that need to exchange messages
        // between e10s parent and child processes
        parentModules.forEach(mod => {
          if (mod.onBrowserSwap) {
            mod.onBrowserSwap(mm);
          }
        });

        if (childTransport) {
          childTransport.swapBrowser(mm);
        }
      };

      const destroy = DevToolsUtils.makeInfallible(function() {
        EventEmitter.off(connection, "closed", destroy);
        Services.obs.removeObserver(onMessageManagerClose, "message-manager-close");

        // provides hook to actor modules that need to exchange messages
        // between e10s parent and child processes
        parentModules.forEach(mod => {
          if (mod.onDisconnected) {
            mod.onDisconnected();
          }
        });
        // TODO: Remove this deprecated path once it's no longer needed by add-ons.
        DebuggerServer.emit("disconnected-from-child:" + connPrefix,
                            { mm, prefix: connPrefix });

        if (childTransport) {
          // If we have a child transport, the actor has already
          // been created. We need to stop using this message manager.
          childTransport.close();
          childTransport = null;
          connection.cancelForwarding(prefix);

          // ... and notify the child process to clean the target-scoped actors.
          try {
            // Bug 1169643: Ignore any exception as the child process
            // may already be destroyed by now.
            mm.sendAsyncMessage("debug:disconnect", { prefix });
          } catch (e) {
            // Nothing to do
          }
        } else {
          // Otherwise, the frame has been closed before the actor
          // had a chance to be created, so we are not able to create
          // the actor.
          resolve(null);
        }
        if (actor) {
          // The FrameTargetActor within the child process doesn't necessary
          // have time to uninitialize itself when the frame is closed/killed.
          // So ensure telling the client that the related actor is detached.
          connection.send({ from: actor.actor, type: "tabDetached" });
          actor = null;
        }

        if (onDestroy) {
          onDestroy(mm);
        }

        // Cleanup all listeners
        untrackMessageManager();
      });

      // Listen for various messages and frame events
      trackMessageManager();

      // Listen for app process exit
      const onMessageManagerClose = function(subject, topic, data) {
        if (subject == mm) {
          destroy();
        }
      };
      Services.obs.addObserver(onMessageManagerClose,
                               "message-manager-close");

      // Listen for connection close to cleanup things
      // when user unplug the device or we lose the connection somehow.
      EventEmitter.on(connection, "closed", destroy);

      mm.sendAsyncMessage("debug:connect", { prefix, addonId });
    });
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
  _onConnection(transport, forwardingPrefix, noRootActor = false) {
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

    const conn = new DebuggerServerConnection(connID, transport);
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
  },

  // DebuggerServer extension API.

  setRootActor(actorFactory) {
    this.createRootActor = actorFactory;
  },

  /**
   * Registers handlers for new target-scoped request types defined dynamically.
   *
   * Note that the name or actorPrefix of the request type is not allowed to clash with
   * existing protocol packet properties, like 'title', 'url' or 'actor', since that would
   * break the protocol.
   *
   * @param actor object
   *        - constructorName: (required)
   *          name of actor constructor, which is also used when removing the actor.
   *        One of the following:
   *          - id:
   *            module ID that contains the actor
   *          - constructorFun:
   *            a function to construct the actor
   * @param name string
   *        The name of the new request type.
   */
  addTargetScopedActor(actor, name) {
    if (!name) {
      throw Error("addTargetScopedActor requires the `name` argument");
    }
    if (["title", "url", "actor"].includes(name)) {
      throw Error(name + " is not allowed");
    }
    if (DebuggerServer.targetScopedActorFactories.hasOwnProperty(name)) {
      throw Error(name + " already exists");
    }
    DebuggerServer.targetScopedActorFactories[name] =
      new RegisteredActorFactory(actor, name);
  },

  /**
   * Unregisters the handler for the specified target-scoped request type.
   *
   * When unregistering an existing target-scoped actor, we remove the actor factory as
   * well as all existing instances of the actor.
   *
   * @param actor object, string
   *        In case of object:
   *          The `actor` object being given to related addTargetScopedActor call.
   *        In case of string:
   *          The `name` string being given to related addTargetScopedActor call.
   */
  removeTargetScopedActor(actorOrName) {
    let name;
    if (typeof actorOrName == "string") {
      name = actorOrName;
    } else {
      const actor = actorOrName;
      for (const factoryName in DebuggerServer.targetScopedActorFactories) {
        const handler = DebuggerServer.targetScopedActorFactories[factoryName];
        if ((handler.name && handler.name == actor.name) ||
            (handler.id && handler.id == actor.id)) {
          name = factoryName;
          break;
        }
      }
    }
    if (!name) {
      return;
    }
    delete DebuggerServer.targetScopedActorFactories[name];
    for (const connID of Object.getOwnPropertyNames(this._connections)) {
      // DebuggerServerConnection in child process don't have rootActor
      if (this._connections[connID].rootActor) {
        this._connections[connID].rootActor.removeActorByName(name);
      }
    }
  },

  /**
   * Registers handlers for new browser-scoped request types defined dynamically.
   *
   * Note that the name or actorPrefix of the request type is not allowed to clash with
   * existing protocol packet properties, like 'from', 'tabs' or 'selected', since that
   * would break the protocol.
   *
   * @param actor object
   *        - constructorName: (required)
   *          name of actor constructor, which is also used when removing the actor.
   *        One of the following:
   *          - id:
   *            module ID that contains the actor
   *          - constructorFun:
   *            a function to construct the actor
   * @param name string
   *        The name of the new request type.
   */
  addGlobalActor(actor, name) {
    if (!name) {
      throw Error("addGlobalActor requires the `name` argument");
    }
    if (["from", "tabs", "selected"].includes(name)) {
      throw Error(name + " is not allowed");
    }
    if (DebuggerServer.globalActorFactories.hasOwnProperty(name)) {
      throw Error(name + " already exists");
    }
    DebuggerServer.globalActorFactories[name] = new RegisteredActorFactory(actor, name);
  },

  /**
   * Unregisters the handler for the specified browser-scoped request type.
   *
   * When unregistering an existing global actor, we remove the actor factory as well as
   * all existing instances of the actor.
   *
   * @param actor object, string
   *        In case of object:
   *          The `actor` object being given to related addGlobalActor call.
   *        In case of string:
   *          The `name` string being given to related addGlobalActor call.
   */
  removeGlobalActor(actorOrName) {
    let name;
    if (typeof actorOrName == "string") {
      name = actorOrName;
    } else {
      const actor = actorOrName;
      for (const factoryName in DebuggerServer.globalActorFactories) {
        const handler = DebuggerServer.globalActorFactories[factoryName];
        if ((handler.name && handler.name == actor.name) ||
            (handler.id && handler.id == actor.id)) {
          name = factoryName;
          break;
        }
      }
    }
    if (!name) {
      return;
    }
    delete DebuggerServer.globalActorFactories[name];
    for (const connID of Object.getOwnPropertyNames(this._connections)) {
      // DebuggerServerConnection in child process don't have rootActor
      if (this._connections[connID].rootActor) {
        this._connections[connID].rootActor.removeActorByName(name);
      }
    }
  },

  /**
   * Called when DevTools are unloaded to remove the contend process server startup script
   * for the list of scripts loaded for each new content process. Will also remove message
   * listeners from already loaded scripts.
   */
  removeContentServerScript() {
    Services.ppmm.removeDelayedProcessScript(CONTENT_PROCESS_SERVER_STARTUP_SCRIPT);
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
DevToolsUtils.defineLazyGetter(DebuggerServer, "Authenticators", () => {
  return Authentication.Authenticators;
});
DevToolsUtils.defineLazyGetter(DebuggerServer, "AuthenticationResult", () => {
  return Authentication.AuthenticationResult;
});

EventEmitter.decorate(DebuggerServer);

if (this.exports) {
  exports.DebuggerServer = DebuggerServer;
  exports.ActorPool = ActorPool;
  exports.OriginalLocation = OriginalLocation;
}

// Needed on B2G (See header note)
this.DebuggerServer = DebuggerServer;
this.ActorPool = ActorPool;
this.OriginalLocation = OriginalLocation;

// When using DebuggerServer.addActors, some symbols are expected to be in
// the scope of the added actor even before the corresponding modules are
// loaded, so let's explicitly bind the expected symbols here.
var includes = ["Components", "Ci", "Cu", "require", "Services", "DebuggerServer",
                "ActorPool", "DevToolsUtils"];
includes.forEach(name => {
  DebuggerServer[name] = this[name];
});

/**
 * Creates a DebuggerServerConnection.
 *
 * Represents a connection to this debugging global from a client.
 * Manages a set of actors and actor pools, allocates actor ids, and
 * handles incoming requests.
 *
 * @param prefix string
 *        All actor IDs created by this connection should be prefixed
 *        with prefix.
 * @param transport transport
 *        Packet transport for the debugging protocol.
 */
function DebuggerServerConnection(prefix, transport) {
  this._prefix = prefix;
  this._transport = transport;
  this._transport.hooks = this;
  this._nextID = 1;

  this._actorPool = new ActorPool(this);
  this._extraPools = [this._actorPool];

  // Responses to a given actor must be returned the the client
  // in the same order as the requests that they're replying to, but
  // Implementations might finish serving requests in a different
  // order.  To keep things in order we generate a promise for each
  // request, chained to the promise for the request before it.
  // This map stores the latest request promise in the chain, keyed
  // by an actor ID string.
  this._actorResponses = new Map();

  /*
   * We can forward packets to other servers, if the actors on that server
   * all use a distinct prefix on their names. This is a map from prefixes
   * to transports: it maps a prefix P to a transport T if T conveys
   * packets to the server whose actors' names all begin with P + "/".
   */
  this._forwardingPrefixes = new Map();
}

DebuggerServerConnection.prototype = {
  _prefix: null,
  get prefix() {
    return this._prefix;
  },

  _transport: null,
  get transport() {
    return this._transport;
  },

  /**
   * Message manager used to communicate with the parent process,
   * set by child.js. Is only defined for connections instantiated
   * within a child process.
   */
  parentMessageManager: null,

  close() {
    if (this._transport) {
      this._transport.close();
    }
  },

  send(packet) {
    this.transport.send(packet);
  },

  /**
   * Used when sending a bulk reply from an actor.
   * @see DebuggerTransport.prototype.startBulkSend
   */
  startBulkSend(header) {
    return this.transport.startBulkSend(header);
  },

  allocID(prefix) {
    return this.prefix + (prefix || "") + this._nextID++;
  },

  /**
   * Add a map of actor IDs to the connection.
   */
  addActorPool(actorPool) {
    this._extraPools.push(actorPool);
  },

  /**
   * Remove a previously-added pool of actors to the connection.
   *
   * @param ActorPool actorPool
   *        The ActorPool instance you want to remove.
   * @param boolean noCleanup [optional]
   *        True if you don't want to destroy each actor from the pool, false
   *        otherwise.
   */
  removeActorPool(actorPool, noCleanup) {
    // When a connection is closed, it removes each of its actor pools. When an
    // actor pool is removed, it calls the destroy method on each of its
    // actors. Some actors, such as ThreadActor, manage their own actor pools.
    // When the destroy method is called on these actors, they manually
    // remove their actor pools. Consequently, this method is reentrant.
    //
    // In addition, some actors, such as ThreadActor, perform asynchronous work
    // (in the case of ThreadActor, because they need to resume), before they
    // remove each of their actor pools. Since we don't wait for this work to
    // be completed, we can end up in this function recursively after the
    // connection already set this._extraPools to null.
    //
    // This is a bug: if the destroy method can perform asynchronous work,
    // then we should wait for that work to be completed before setting this.
    // _extraPools to null. As a temporary solution, it should be acceptable
    // to just return early (if this._extraPools has been set to null, all
    // actors pools for this connection should already have been removed).
    if (this._extraPools === null) {
      return;
    }
    const index = this._extraPools.lastIndexOf(actorPool);
    if (index > -1) {
      const pool = this._extraPools.splice(index, 1);
      if (!noCleanup) {
        pool.forEach(p => p.destroy());
      }
    }
  },

  /**
   * Add an actor to the default actor pool for this connection.
   */
  addActor(actor) {
    this._actorPool.addActor(actor);
  },

  /**
   * Remove an actor to the default actor pool for this connection.
   */
  removeActor(actor) {
    this._actorPool.removeActor(actor);
  },

  /**
   * Match the api expected by the protocol library.
   */
  unmanage(actor) {
    return this.removeActor(actor);
  },

  /**
   * Look up an actor implementation for an actorID.  Will search
   * all the actor pools registered with the connection.
   *
   * @param actorID string
   *        Actor ID to look up.
   */
  getActor(actorID) {
    const pool = this.poolFor(actorID);
    if (pool) {
      return pool.get(actorID);
    }

    if (actorID === "root") {
      return this.rootActor;
    }

    return null;
  },

  _getOrCreateActor(actorID) {
    let actor = this.getActor(actorID);
    if (!actor) {
      this.transport.send({ from: actorID ? actorID : "root",
                            error: "noSuchActor",
                            message: "No such actor for ID: " + actorID });
      return null;
    }

    // Dynamically-loaded actors have to be created lazily.
    if (actor instanceof ObservedActorFactory) {
      try {
        actor = actor.createActor();
      } catch (error) {
        const prefix = "Error occurred while creating actor '" + actor.name;
        this.transport.send(this._unknownError(actorID, prefix, error));
      }
    } else if (typeof (actor) !== "object") {
      // ActorPools should now contain only actor instances (i.e. objects)
      // or ObservedActorFactory instances.
      throw new Error("Unexpected actor constructor/function in ActorPool " +
                      "for actorID=" + actorID + ".");
    }

    return actor;
  },

  poolFor(actorID) {
    for (const pool of this._extraPools) {
      if (pool.has(actorID)) {
        return pool;
      }
    }
    return null;
  },

  _unknownError(from, prefix, error) {
    const errorString = prefix + ": " + DevToolsUtils.safeErrorString(error);
    reportError(errorString);
    dumpn(errorString);
    return {
      from,
      error: "unknownError",
      message: errorString
    };
  },

  _queueResponse: function(from, type, responseOrPromise) {
    const pendingResponse = this._actorResponses.get(from) || Promise.resolve(null);
    const responsePromise = pendingResponse.then(() => {
      return responseOrPromise;
    }).then(response => {
      if (!this.transport) {
        throw new Error(`Connection closed, pending response from ${from}, ` +
                        `type ${type} failed`);
      }
      if (!response.from) {
        response.from = from;
      }
      this.transport.send(response);
    }).catch((error) => {
      if (!this.transport) {
        throw new Error(`Connection closed, pending error from ${from}, ` +
                        `type ${type} failed`);
      }

      const prefix = "error occurred while processing '" + type;
      this.transport.send(this._unknownError(from, prefix, error));
    });

    this._actorResponses.set(from, responsePromise);
  },

  /**
   * Passes a set of options to the AddonTargetActors for the given ID.
   *
   * @param id string
   *        The ID of the add-on to pass the options to
   * @param options object
   *        The options.
   * @return a promise that will be resolved when complete.
   */
  setAddonOptions(id, options) {
    const addonList = this.rootActor._parameters.addonList;
    if (!addonList) {
      return Promise.resolve();
    }
    return addonList.getList().then((addonTargetActors) => {
      for (const actor of addonTargetActors) {
        if (actor.id != id) {
          continue;
        }
        actor.setOptions(options);
        return;
      }
    });
  },

  /* Forwarding packets to other transports based on actor name prefixes. */

  /*
   * Arrange to forward packets to another server. This is how we
   * forward debugging connections to child processes.
   *
   * If we receive a packet for an actor whose name begins with |prefix|
   * followed by '/', then we will forward that packet to |transport|.
   *
   * This overrides any prior forwarding for |prefix|.
   *
   * @param prefix string
   *    The actor name prefix, not including the '/'.
   * @param transport object
   *    A packet transport to which we should forward packets to actors
   *    whose names begin with |(prefix + '/').|
   */
  setForwarding(prefix, transport) {
    this._forwardingPrefixes.set(prefix, transport);
  },

  /*
   * Stop forwarding messages to actors whose names begin with
   * |prefix+'/'|. Such messages will now elicit 'noSuchActor' errors.
   */
  cancelForwarding(prefix) {
    this._forwardingPrefixes.delete(prefix);

    // Notify the client that forwarding in now cancelled for this prefix.
    // There could be requests in progress that the client should abort rather leaving
    // handing indefinitely.
    if (this.rootActor) {
      this.send(this.rootActor.forwardingCancelled(prefix));
    }
  },

  sendActorEvent(actorID, eventName, event = {}) {
    event.from = actorID;
    event.type = eventName;
    this.send(event);
  },

  // Transport hooks.

  /**
   * Called by DebuggerTransport to dispatch incoming packets as appropriate.
   *
   * @param packet object
   *        The incoming packet.
   */
  onPacket(packet) {
    // If the actor's name begins with a prefix we've been asked to
    // forward, do so.
    //
    // Note that the presence of a prefix alone doesn't indicate that
    // forwarding is needed: in DebuggerServerConnection instances in child
    // processes, every actor has a prefixed name.
    if (this._forwardingPrefixes.size > 0) {
      let to = packet.to;
      let separator = to.lastIndexOf("/");
      while (separator >= 0) {
        to = to.substring(0, separator);
        const forwardTo = this._forwardingPrefixes.get(packet.to.substring(0, separator));
        if (forwardTo) {
          forwardTo.send(packet);
          return;
        }
        separator = to.lastIndexOf("/");
      }
    }

    const actor = this._getOrCreateActor(packet.to);
    if (!actor) {
      return;
    }

    let ret = null;

    // handle "requestTypes" RDP request.
    if (packet.type == "requestTypes") {
      ret = { from: actor.actorID, requestTypes: Object.keys(actor.requestTypes) };
    } else if (actor.requestTypes && actor.requestTypes[packet.type]) {
      // Dispatch the request to the actor.
      try {
        this.currentPacket = packet;
        ret = actor.requestTypes[packet.type].bind(actor)(packet, this);
      } catch (error) {
        const prefix = "error occurred while processing '" + packet.type;
        this.transport.send(this._unknownError(actor.actorID, prefix, error));
      } finally {
        this.currentPacket = undefined;
      }
    } else {
      ret = { error: "unrecognizedPacketType",
              message: ("Actor " + actor.actorID +
                        " does not recognize the packet type " +
                        packet.type) };
    }

    // There will not be a return value if a bulk reply is sent.
    if (ret) {
      this._queueResponse(packet.to, packet.type, ret);
    }
  },

  /**
   * Called by the DebuggerTransport to dispatch incoming bulk packets as
   * appropriate.
   *
   * @param packet object
   *        The incoming packet, which contains:
   *        * actor:  Name of actor that will receive the packet
   *        * type:   Name of actor's method that should be called on receipt
   *        * length: Size of the data to be read
   *        * stream: This input stream should only be used directly if you can
   *                  ensure that you will read exactly |length| bytes and will
   *                  not close the stream when reading is complete
   *        * done:   If you use the stream directly (instead of |copyTo|
   *                  below), you must signal completion by resolving /
   *                  rejecting this deferred.  If it's rejected, the transport
   *                  will be closed.  If an Error is supplied as a rejection
   *                  value, it will be logged via |dumpn|.  If you do use
   *                  |copyTo|, resolving is taken care of for you when copying
   *                  completes.
   *        * copyTo: A helper function for getting your data out of the stream
   *                  that meets the stream handling requirements above, and has
   *                  the following signature:
   *          @param  output nsIAsyncOutputStream
   *                  The stream to copy to.
   *          @return Promise
   *                  The promise is resolved when copying completes or rejected
   *                  if any (unexpected) errors occur.
   *                  This object also emits "progress" events for each chunk
   *                  that is copied.  See stream-utils.js.
   */
  onBulkPacket(packet) {
    const { actor: actorKey, type } = packet;

    const actor = this._getOrCreateActor(actorKey);
    if (!actor) {
      return;
    }

    // Dispatch the request to the actor.
    let ret;
    if (actor.requestTypes && actor.requestTypes[type]) {
      try {
        ret = actor.requestTypes[type].call(actor, packet);
      } catch (error) {
        const prefix = "error occurred while processing bulk packet '" + type;
        this.transport.send(this._unknownError(actorKey, prefix, error));
        packet.done.reject(error);
      }
    } else {
      const message = "Actor " + actorKey +
                    " does not recognize the bulk packet type " + type;
      ret = { error: "unrecognizedPacketType",
              message: message };
      packet.done.reject(new Error(message));
    }

    // If there is a JSON response, queue it for sending back to the client.
    if (ret) {
      this._queueResponse(actorKey, type, ret);
    }
  },

  /**
   * Called by DebuggerTransport when the underlying stream is closed.
   *
   * @param status nsresult
   *        The status code that corresponds to the reason for closing
   *        the stream.
   */
  onClosed(status) {
    dumpn("Cleaning up connection.");
    if (!this._actorPool) {
      // Ignore this call if the connection is already closed.
      return;
    }
    this._actorPool = null;

    EventEmitter.emit(this, "closed", status);

    this._extraPools.forEach(p => p.destroy());
    this._extraPools = null;

    this.rootActor = null;
    this._transport = null;
    DebuggerServer._connectionClosed(this);
  },

  /*
   * Debugging helper for inspecting the state of the actor pools.
   */
  _dumpPools() {
    dumpn("/-------------------- dumping pools:");
    if (this._actorPool) {
      dumpn("--------------------- actorPool actors: " +
            uneval(Object.keys(this._actorPool._actors)));
    }
    for (const pool of this._extraPools) {
      if (pool !== this._actorPool) {
        dumpn("--------------------- extraPool actors: " +
              uneval(Object.keys(pool._actors)));
      }
    }
  },

  /*
   * Debugging helper for inspecting the state of an actor pool.
   */
  _dumpPool(pool) {
    dumpn("/-------------------- dumping pool:");
    dumpn("--------------------- actorPool actors: " +
          uneval(Object.keys(pool._actors)));
  },

  /**
   * In a content child process, ask the DebuggerServer in the parent process
   * to execute a given module setup helper.
   *
   * @param module
   *        The module to be required
   * @param setupParent
   *        The name of the setup helper exported by the above module
   *        (setup helper signature: function ({mm}) { ... })
   * @return boolean
   *         true if the setup helper returned successfully
   */
  setupInParent({ module, setupParent }) {
    if (!this.parentMessageManager) {
      return false;
    }

    const { sendSyncMessage } = this.parentMessageManager;

    return sendSyncMessage("debug:setup-in-parent", {
      prefix: this.prefix,
      module: module,
      setupParent: setupParent
    });
  },
};
